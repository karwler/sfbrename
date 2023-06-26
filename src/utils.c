#ifdef CONSOLE
#include "utils.h"
#else
#include "window.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <aclapi.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#endif
#endif
#include <ctype.h>

static const uint8_t CHAR2DIGIT_UPPER[128] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,					// 0 - 15
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,					// 16 - 31
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 62, 0, 0,				// 32 ' ' - 47 '/'
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0,					// 48 '0' - 63 '?'
	0, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,	// 64 '@' - 79 'O'
	25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 0, 0, 0, 0, 0,		// 80 'P' - 95 '_'
	0, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50,	// 96 '`' - 111 '0'
	51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0, 0, 0, 0, 0,		// 112 'p' - 127
};

static const uint8_t CHAR2DIGIT_BASE64URL[128] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,					// 0 - 15
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,					// 16 - 31
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 62, 0, 0,				// 32 ' ' - 47 '/'
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0, 0, 0, 0, 0, 0,		// 48 '0' - 63 '?'
	0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,			// 64 '@' - 79 'O'
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 0, 0, 0, 0, 63,		// 80 'P' - 95 '_'
	0, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,	// 96 '`' - 111 '0'
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 0, 0, 0, 0, 0,		// 112 'p' - 127
};

#ifndef CONSOLE
typedef struct KeyPos {
	char* key;
	size_t pos;
} KeyPos;

void runThread(Window* win, ThreadCode code, GThreadFunc proc, GSourceFunc fin, void* data) {
	GError* err = NULL;
	win->threadCode = code;
	win->thread = g_thread_try_new(NULL, proc, data, &err);
	if (!win->thread) {
		ResponseType rc = showMessage(win, MESSAGE_ERROR, BUTTONS_YES_NO, "Failed to create thread: %s\nRun on main thread?", err->message);
		g_clear_error(&err);
		if (rc == RESPONSE_YES)
			proc(data);
		else
			fin(data);
	}
}

void finishThread(Window* win) {
	g_thread_join(win->thread);
	win->thread = NULL;
	win->threadCode = THREAD_NONE;
}

#ifdef _WIN32
static void timespecToStr(FILETIME ft, char* str, FILETIME lf, SYSTEMTIME st) {
	if (FileTimeToLocalFileTime(&ft, &lf) && FileTimeToSystemTime(&lf, &st))
		snprintf(str, 20, "%hu-%02hu-%02hu %02hu:%02hu:%02hu", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
	else
		*str = '\0';
}
#else
static void timespecToStr(time_t time, char* str) {
	const struct tm* date = localtime(&time);
	if (date)
		snprintf(str, 20, "%u-%02u-%02u %02u:%02u:%02u", 1900 + date->tm_year, date->tm_mon, date->tm_mday, date->tm_hour, date->tm_min, date->tm_sec);
	else
		*str = '\0';
}

static char formatXperm(mode_t pm, mode_t xperm, mode_t sperm, char low, char upp) {
	if (pm & xperm)
		return pm & sperm ? low : 'x';
	return pm & sperm ? upp : '-';
}

static void formatPermissions(mode_t mode, char* str) {
	mode_t pm = mode & ~S_IFMT;
	switch (mode & S_IFMT) {
	case S_IFIFO:
		str[0] = 'f';
		break;
	case S_IFCHR:
		str[0] = 'c';
		break;
	case S_IFDIR:
		str[0] = 'd';
		break;
	case S_IFBLK:
		str[0] = 'b';
		break;
	case S_IFREG:
		str[0] = '-';
		break;
	case S_IFLNK:
		str[0] = 'l';
		break;
	case S_IFSOCK:
		str[0] = 's';
		break;
	default:
		str[0] = 'u';
	}
	str[1] = pm & S_IRUSR ? 'r' : '-';
	str[2] = pm & S_IWUSR ? 'w' : '-';
	str[3] = formatXperm(pm, S_IXUSR, S_ISUID, 's', 'S');
	str[4] = pm & S_IRGRP ? 'r' : '-';
	str[5] = pm & S_IWGRP ? 'w' : '-';
	str[6] = formatXperm(pm, S_IXGRP, S_ISGID, 's', 'S');
	str[7] = pm & S_IROTH ? 'r' : '-';
	str[8] = pm & S_IWOTH ? 'w' : '-';
	str[9] = formatXperm(pm, S_IXOTH, S_ISVTX, 't', 'T');
	str[10] = '\0';
}
#endif

void setFileInfo(const char* file, FileInfo* info) {
#ifdef _WIN32
	wchar_t* path = stow(file);
	HANDLE fh = CreateFileW(path, FILE_READ_ATTRIBUTES | STANDARD_RIGHTS_READ | SYNCHRONIZE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	free(path);
	if (fh != INVALID_HANDLE_VALUE) {
		PSID sid;
		PSECURITY_DESCRIPTOR sd;
		if (!GetSecurityInfo(fh, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION, &sid, NULL, NULL, NULL, &sd)) {
			DWORD nlen = 0, dlen = 0;
			SID_NAME_USE use;
			LookupAccountSidW(NULL, sid, NULL, &nlen, NULL, &dlen, &use);
			wchar_t* user = malloc(nlen * sizeof(wchar_t));
			wchar_t* domn = malloc(dlen * sizeof(wchar_t));
			if (LookupAccountSidW(NULL, sid, user, &nlen, domn, &dlen, &use)) {
				info->user = wtos(user);
				info->group = wtos(domn);
			} else
				info->user = info->group = NULL;
			free(user);
			free(domn);
			LocalFree(sd);
		} else
			info->user = info->group = NULL;

		BY_HANDLE_FILE_INFORMATION fi;
		if (GetFileInformationByHandle(fh, &fi)) {
			FILETIME lf;
			SYSTEMTIME st;
			uint64_t size = fi.nFileSizeLow | ((uint64_t)fi.nFileSizeHigh << 32);
			info->size = size >= 1024 ? g_format_size_full(size, G_FORMAT_SIZE_IEC_UNITS) : g_strdup_printf("%u B", (uint)size);
			timespecToStr(fi.ftCreationTime, info->create, lf, st);
			timespecToStr(fi.ftLastWriteTime, info->modify, lf, st);
			timespecToStr(fi.ftLastAccessTime, info->access, lf, st);
			timespecToStr(fi.ftLastWriteTime, info->modify, lf, st);
			info->perms[0] = fi.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT ? 'l' : fi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ? 'd' : '-';
			info->perms[1] = '\0';
		} else {
			info->size = NULL;
			info->create[0] = info->modify[0] = info->access[0] = info->change[0] = info->perms[0] = '\0';
		}
		CloseHandle(fh);
	} else
		memset(info, 0, sizeof(FileInfo));
#else
	struct statx ps;
	if (!statx(-1, file, AT_SYMLINK_NOFOLLOW | AT_STATX_SYNC_AS_STAT, STATX_TYPE | STATX_MODE | STATX_UID | STATX_GID | STATX_ATIME | STATX_MTIME | STATX_CTIME | STATX_SIZE | STATX_BTIME, &ps)) {
		const struct passwd* pwd = getpwuid(ps.stx_uid);
		const struct group* grp = getgrgid(ps.stx_gid);
		info->size = ps.stx_size >= 1024 ? g_format_size_full(ps.stx_size, G_FORMAT_SIZE_IEC_UNITS) : g_strdup_printf("%u B", (uint)ps.stx_size);
		info->user = pwd ? strdup(pwd->pw_name) : NULL;
		info->group = grp ? strdup(grp->gr_name) : NULL;
		timespecToStr(ps.stx_btime.tv_sec, info->create);
		timespecToStr(ps.stx_mtime.tv_sec, info->modify);
		timespecToStr(ps.stx_atime.tv_sec, info->access);
		timespecToStr(ps.stx_ctime.tv_sec, info->change);
		formatPermissions(ps.stx_mode, info->perms);
	} else
		memset(info, 0, sizeof(FileInfo));
#endif
}

void freeFileInfo(FileInfo* info) {
	free(info->size);
	free(info->user);
	free(info->group);
	free(info);
}

GtkTreeRowReference** getTreeViewSelectedRowRefs(GtkTreeView* view, GtkTreeModel** model, uint* cnt) {
	*cnt = 0;
	GtkTreeSelection* selc = gtk_tree_view_get_selection(view);
	GList* rows = gtk_tree_selection_get_selected_rows(selc, model);
	uint rnum = g_list_length(rows);
	if (!rnum) {
		g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);
		return NULL;
	}

	GtkTreeRowReference** refs = malloc(rnum * sizeof(GtkTreeRowReference*));
	for (GList* it = rows; it; it = it->next) {
		GtkTreeRowReference* ref = gtk_tree_row_reference_new(*model, it->data);
		if (ref)
			refs[(*cnt)++] = ref;
	}
	g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);
	return refs;
}

static int strcmpNameAsc(const void* a, const void* b) {
	return strcmp(((const KeyPos*)a)->key, ((const KeyPos*)b)->key);
}

static int strcmpNameDsc(const void* a, const void* b) {
	return -strcmp(((const KeyPos*)a)->key, ((const KeyPos*)b)->key);
}

void sortTreeViewColumn(GtkTreeView* treeView, GtkListStore* listStore, int colId, bool ascending) {
	GtkTreeModel* model = gtk_tree_view_get_model(treeView);
	GtkTreeIter it;
	if (!gtk_tree_model_get_iter_first(model, &it))
		return;

	size_t num = gtk_tree_model_iter_n_children(model, NULL);
	KeyPos* keys = malloc(num * sizeof(KeyPos));
	int* order = malloc(num * sizeof(int));
	size_t i = 0;
	do {
		char* name;
		gtk_tree_model_get(model, &it, colId, &name, FCOL_INVALID);
		keys[i].pos = i;
		keys[i++].key = g_utf8_collate_key_for_filename(name, -1);
		g_free(name);
	} while (gtk_tree_model_iter_next(model, &it));
	num = i;
	qsort(keys, num, sizeof(KeyPos), ascending ? strcmpNameAsc : strcmpNameDsc);

	for (i = 0; i < num; ++i) {
		order[i] = keys[i].pos;
		g_free(keys[i].key);
	}
	gtk_list_store_reorder(listStore, order);
	free(order);
	free(keys);
}

static void activateInputText(GtkEntry* entry, GtkDialog* dlg) {
	gtk_dialog_response(dlg, GTK_RESPONSE_ACCEPT);
}

char* showInputText(GtkWindow* parent, const char* title, const char* label, const char* text)  {
	GtkLabel* lbl = GTK_LABEL(gtk_label_new(label));
	gtk_widget_set_valign(GTK_WIDGET(lbl), GTK_ALIGN_CENTER);

	GtkEntry* ent = GTK_ENTRY(gtk_entry_new());
	gtk_entry_set_max_length(ent, FILENAME_MAX - 1);
	gtk_widget_set_valign(GTK_WIDGET(ent), GTK_ALIGN_CENTER);
	atk_object_set_name(gtk_widget_get_accessible(GTK_WIDGET(ent)), "name");
	if (text)
		gtk_entry_set_text(ent, text);

	GtkBox* box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8));
	gtk_container_add(GTK_CONTAINER(box), GTK_WIDGET(lbl));
	gtk_container_add(GTK_CONTAINER(box), GTK_WIDGET(ent));
	gtk_box_set_child_packing(box, GTK_WIDGET(ent), true, true, 0, GTK_PACK_START);
	gtk_widget_show_all(GTK_WIDGET(box));

	GtkDialog* dlg = GTK_DIALOG(gtk_dialog_new_with_buttons(title, parent, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, "_OK", GTK_RESPONSE_ACCEPT, "_Cancel", GTK_RESPONSE_CANCEL, NULL));
	GtkBox* root = GTK_BOX(gtk_dialog_get_content_area(dlg));
	gtk_box_set_spacing(root, 4);
	gtk_container_add(GTK_CONTAINER(root), GTK_WIDGET(box));
	gtk_box_set_child_packing(root, GTK_WIDGET(box), true, true, 0, GTK_PACK_START);
	g_signal_connect(ent, "activate", G_CALLBACK(activateInputText), dlg);

	char* str = NULL;
	if (gtk_dialog_run(dlg) == GTK_RESPONSE_ACCEPT) {
		text = gtk_entry_get_text(ent);
		if (text && *text)
			str = strdup(text);
	}
	gtk_widget_destroy(GTK_WIDGET(dlg));
	return str;
}
#endif

static ResponseType consoleMessage(MessageType type, ButtonsType buttons, char* message) {
	void (*printer)(const char*, ...) = type == MESSAGE_INFO || type == MESSAGE_QUESTION || type == MESSAGE_OTHER ? g_print : g_printerr;
	if (buttons == BUTTONS_YES_NO || buttons == BUTTONS_OK_CANCEL)
		printer("%s [Y/n]\n", message);
	else {
		printer("%s\n", message);
		return RESPONSE_NONE;
	}

	char ch;
	scanf("%c", &ch);
	bool ok = toupper(ch) == 'Y' || ch == '\n';
	if (buttons == BUTTONS_YES_NO)
		return ok ? RESPONSE_YES : RESPONSE_NO;
	return ok ? RESPONSE_OK : RESPONSE_CANCEL;
}

#ifndef CONSOLE
static ResponseType windowMessage(GtkWindow* parent, MessageType type, ButtonsType buttons, char* message) {
	ResponseType rc;
	if (parent) {
		GtkMessageDialog* dialog = GTK_MESSAGE_DIALOG(gtk_message_dialog_new(parent, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, (GtkMessageType)type, (GtkButtonsType)buttons, "%s", message));
		rc = gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(GTK_WIDGET(dialog));
	} else
		rc = consoleMessage(type, buttons, message);
	return rc;
}

static gboolean asyncMessage(AsyncMessage* am) {
	g_mutex_lock(&am->mutex);
	am->response = windowMessage(am->parent, am->message, am->buttons, am->text);
	g_cond_signal(&am->cond);
	g_mutex_unlock(&am->mutex);
	return G_SOURCE_REMOVE;
}
#endif

ResponseType showMessageV(Window* win, MessageType type, ButtonsType buttons, const char* format, va_list args) {
	char* str = g_strdup_vprintf(format, args);
	ResponseType rc;
#ifdef CONSOLE
	rc = consoleMessage(type, buttons, str);
#else
	if (g_thread_self()->func) {
		AsyncMessage am = {
			.parent = GTK_WINDOW(win->window),
			.text = str,
			.response = RESPONSE_NONE,
			.message = type,
			.buttons = buttons
		};
		g_mutex_init(&am.mutex);
		g_cond_init(&am.cond);
		g_idle_add(G_SOURCE_FUNC(asyncMessage), &am);

		g_mutex_lock(&am.mutex);
		g_cond_wait(&am.cond, &am.mutex);
		do {
			rc = am.response;
		} while (rc == RESPONSE_WAIT);
		g_mutex_unlock(&am.mutex);
		g_cond_clear(&am.cond);
		g_mutex_clear(&am.mutex);
	} else
		rc = windowMessage(GTK_WINDOW(win->window), type, buttons, str);
#endif
	g_free(str);
	return rc;
}

ResponseType showMessage(Window* win, MessageType type, ButtonsType buttons, const char* format, ...) {
	va_list args;
	va_start(args, format);
	ResponseType rc = showMessageV(win, type, buttons, format, args);
	va_end(args);
	return rc;
}

char* sanitizeNumber(const char* text, uint8_t base) {
	char digits[65] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_";
	if (base <= 10 || base > 36)
		digits[base] = '\0';
	else {
		uint8_t cnt = base - 10;
		memmove(digits + base, digits + 36, cnt);
		digits[10 + cnt * 2] = '\0';
	}
	size_t len = strspn(text, digits);
	if (!text[len])
		return NULL;

	const char* pos = text;
	char* str = g_malloc(strlen(text) * sizeof(char));
	char* out = str;
	do {
		memcpy(out, pos, len);
		out += len;
		pos += len;
		pos += strcspn(pos, digits);
		len = strspn(pos, digits);
	} while (pos[len]);
	memcpy(out, pos, (len + 1) * sizeof(char));
	return str;
}

#define strToLlongLoop(str, num, base, digits, mod) { \
	for (const char* pos = str; *pos; ++pos) { \
		uchar ch = mod(*pos); \
		num = num * base + digits[ch & 0x7F]; \
	} \
}

llong strToLlong(const char* str, uint8_t base) {
	const uint8_t* digits = base < 64 ? CHAR2DIGIT_UPPER : CHAR2DIGIT_BASE64URL;
	llong num = 0;
	if (base <= 10 || base > 36)
		strToLlongLoop(str, num, base, digits, noop)
	else
		strToLlongLoop(str, num, base, digits, toupper)
	return num;
}

size_t llongToRevStr(char* buf, llong num, uint8_t base, const char* digits) {
	size_t blen = 0;
	if (num) {
		for (uint64_t val = num > 0 ? num : -num; val; val /= base)
			buf[blen++] = digits[val % base];
	} else
		buf[blen++] = '0';
	return blen;
}

size_t llongToStr(char* buf, llong num, uint8_t base, bool upper) {
	bool negative = num < 0;
	size_t blen = llongToRevStr(buf + negative, num, base, pickDigitChars(base, upper));
	if (negative)
		buf[0] = '-';
	buf[blen] = '\0';

	char* a = buf + negative;
	char* b = a + blen - 1;
	for (; a < b; ++a, --b) {
		char t = *a;
		*a = *b;
		*b = t;
	}
	return blen;
}

char* newStrncat(uint n, ...) {
	va_list first, second;
	va_start(first, n);
	va_copy(second, first);

	size_t i = 0;
	for (uint c = n; c; --c) {
		va_arg(first, char*);
		i += va_arg(first, size_t);
	}
	va_end(first);

	char* str = malloc((i + 1) * sizeof(char));
	for (i = 0; n; --n) {
		char* s = va_arg(second, char*);
		size_t l = va_arg(second, size_t);
		memcpy(str + i, s, l * sizeof(char));
		i += l;
	}
	va_end(second);
	str[i] = '\0';
	return str;
}

#ifdef _WIN32
void* memrchr(const void* s, int c, size_t n) {
	uint8_t* p = (uint8_t*)s;
	if (n)
		do {
			if (p[--n] == c)
				return p + n;
		} while (n);
	return NULL;
}

wchar_t* stow(const char* src) {
	wchar_t* dst;
	int len = MultiByteToWideChar(CP_UTF8, 0, src, -1, NULL, 0);
	if (len > 1) {
		dst = malloc(len * sizeof(wchar_t));
		MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, len);
	} else {
		dst = malloc(sizeof(wchar_t));
		dst[0] = '\0';
	}
	return dst;
}

char* wtos(const wchar_t* src) {
	char* dst;
	int len = WideCharToMultiByte(CP_UTF8, 0, src, -1, NULL, 0, NULL, NULL);
	if (len > 1) {
		dst = malloc(len * sizeof(char));
		WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, len, NULL, NULL);
	} else {
		dst = malloc(sizeof(char));
		dst[0] = '\0';
	}
	return dst;
}

void unbackslashify(char* path) {
	for (; *path; ++path)
		if (*path == '\\')
			*path = '/';
}
#endif
