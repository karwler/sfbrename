#ifdef CONSOLE
#include "utils.h"
#else
#include "main.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <zlib.h>
#ifndef _WIN32
#include <grp.h>
#include <pwd.h>
#endif
#endif
#include <ctype.h>

#define DEFAULT_PRINT_BUFFER_SIZE 1024
#define COLOSSAL_FUCKUP_MESSAGE "Failed to build message"

#ifndef CONSOLE
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

static void timespecToStr(time_t time, char* str) {
	const struct tm* date = localtime(&time);
	if (date)
		snprintf(str, 20, "%u-%02u-%02u %02u:%02u:%02u", 1900 + date->tm_year, date->tm_mon, date->tm_mday, date->tm_hour, date->tm_min, date->tm_sec);
	else
		*str = '\0';
}

#ifndef _WIN32
static char formatXperm(mode_t pm, mode_t xperm, mode_t sperm, char low, char upp) {
	if (pm & xperm)
		return pm & sperm ? low : 'x';
	return pm & sperm ? upp : '-';
}
#endif

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
#ifndef _WIN32
	case S_IFLNK:
		str[0] = 'l';
		break;
	case S_IFSOCK:
		str[0] = 's';
		break;
#endif
	default:
		str[0] = 'u';
	}
	str[1] = pm & S_IRUSR ? 'r' : '-';
	str[2] = pm & S_IWUSR ? 'w' : '-';
#ifdef _WIN32
	str[3] = pm & S_IXUSR ? 'x' : '-';
#else
	str[3] = formatXperm(pm, S_IXUSR, S_ISUID, 's', 'S');
#endif
	str[4] = pm & S_IRGRP ? 'r' : '-';
	str[5] = pm & S_IWGRP ? 'w' : '-';
#ifdef _WIN32
	str[6] = pm & S_IXGRP ? 'x' : '-';
#else
	str[6] = formatXperm(pm, S_IXGRP, S_ISGID, 's', 'S');
#endif
	str[7] = pm & S_IROTH ? 'r' : '-';
	str[8] = pm & S_IWOTH ? 'w' : '-';
#ifdef _WIN32
	str[9] = pm & S_IXOTH ? 'x' : '-';
#else
	str[9] = formatXperm(pm, S_IXOTH, S_ISVTX, 't', 'T');
#endif
	str[10] = '\0';
}

void setFileInfo(const char* file, FileInfo* info) {
#ifdef _WIN32
	struct stat ps;
	if (!stat(file, &ps)) {
		info->size = ps.st_size >= 1024 ? g_format_size_full(ps.st_size, G_FORMAT_SIZE_IEC_UNITS) : g_strdup_printf("%u B", (uint)ps.st_size);
		timespecToStr(ps.st_mtime, info->modify);
		timespecToStr(ps.st_atime, info->access);
		timespecToStr(ps.st_ctime, info->change);
		formatPermissions(ps.st_mode, info->perms);
	} else
		memset(info, 0, sizeof(FileInfo));
#else
	struct statx ps;
	if (!statx(-1, file, AT_SYMLINK_NOFOLLOW | AT_STATX_SYNC_AS_STAT, STATX_TYPE | STATX_MODE | STATX_UID | STATX_GID | STATX_ATIME | STATX_MTIME | STATX_CTIME | STATX_SIZE | STATX_BTIME, &ps)) {
		info->size = ps.stx_size >= 1024 ? g_format_size_full(ps.stx_size, G_FORMAT_SIZE_IEC_UNITS) : g_strdup_printf("%u B", (uint)ps.stx_size);
		timespecToStr(ps.stx_btime.tv_sec, info->create);
		timespecToStr(ps.stx_mtime.tv_sec, info->modify);
		timespecToStr(ps.stx_atime.tv_sec, info->access);
		timespecToStr(ps.stx_ctime.tv_sec, info->change);
		formatPermissions(ps.stx_mode, info->perms);
		info->pwd = getpwuid(ps.stx_uid);
		info->grp = getgrgid(ps.stx_gid);
	} else
		memset(info, 0, sizeof(FileInfo));
#endif
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
		GtkMessageDialog* dialog = GTK_MESSAGE_DIALOG(gtk_message_dialog_new(parent, GTK_DIALOG_DESTROY_WITH_PARENT, (GtkMessageType)type, (GtkButtonsType)buttons, "%s", message));
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
	char* str = malloc(DEFAULT_PRINT_BUFFER_SIZE * sizeof(char));
	int len = vsnprintf(str, DEFAULT_PRINT_BUFFER_SIZE, format, args);
	if (len >= DEFAULT_PRINT_BUFFER_SIZE) {
		str = realloc(str, (size_t)(len + 1) * sizeof(char));
		len = vsnprintf(str, (size_t)len + 1, format, args);
	}
	if (len < 0) {
		str = realloc(str, sizeof(COLOSSAL_FUCKUP_MESSAGE));
		strcpy(str, COLOSSAL_FUCKUP_MESSAGE);
		type = MESSAGE_ERROR;
		buttons = BUTTONS_OK;
	}

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
	free(str);
	return rc;
}

ResponseType showMessage(Window* win, MessageType type, ButtonsType buttons, const char* format, ...) {
	va_list args;
	va_start(args, format);
	ResponseType rc = showMessageV(win, type, buttons, format, args);
	va_end(args);
	return rc;
}

#ifdef _WIN32
void* memrchr(const void* s, int c, size_t n) {
	uint8* p = (uint8*)s;
	if (n)
		do {
			if (p[--n] == c)
				return p + n;
		} while (n);
	return NULL;
}

void unbackslashify(char* path) {
	for (; *path; ++path)
		if (*path == '\\')
			*path = '/';
}
#endif
