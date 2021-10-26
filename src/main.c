#include "arguments.h"
#include "main.h"
#include "rename.h"
#include <ctype.h>
#include <sys/stat.h>
#ifndef CONSOLE
#include <zlib.h>
#endif
#ifdef __MINGW32__
#include <windows.h>
#endif

#define MAIN_GLADE_PATH "share/sfbrename/main.glade.gz"
#ifdef APPIMAGE
#define WINDOW_ICON_PATH "sfbrename.png"
#else
#define WINDOW_ICON_PATH "share/sfbrename/sfbrename.png"
#endif
#define INFLATED_SIZE 40000
#define INFLATE_INCREMENT 10000
#define FILE_URI_PREFIX "file://"
#define LINE_BREAK_CHARS "\r\n"
#define DEFAULT_PRINT_BUFFER_SIZE 1024
#define COLOSSAL_FUCKUP_MESSAGE "Failed to build message"
#ifdef __MINGW32__
#define EXECUTABLE_NAME "sfbrename.exe"
#endif

#ifndef CONSOLE
typedef struct AsyncMessage {
	Window* win;
	char* text;
	GMutex mutex;
	GCond cond;
	ResponseType response;
	MessageType message;
	ButtonsType buttons;
} AsyncMessage;

typedef struct KeyPos {
	char* key;
	size_t pos;
} KeyPos;

typedef struct KeyDirPos {
	char* file;
	char* dir;
	size_t pos;
} KeyDirPos;
#endif

#ifdef __MINGW32__
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

#ifndef CONSOLE
static uint8* readGzip(const char* path, size_t* olen) {
	gzFile file = gzopen(path, "rb");
	if (!file)
		return NULL;

	*olen = 0;
	size_t siz = INFLATED_SIZE;
	uint8* str = malloc(siz);
	for (;;) {
		*olen += (size_t)gzread(file, str + *olen, (uint)(siz - *olen));
		if (*olen < siz)
			break;
		siz += INFLATE_INCREMENT;
		str = realloc(str, siz);
	}
	gzclose(file);
	return str;
}

static void addFile(Window* win, const char* file) {
	if (!g_utf8_validate(file, -1, NULL)) {
		showMessage(win, MESSAGE_ERROR, BUTTONS_OK, "Invalid UTF-8 input path");
		return;
	}
	size_t plen = strlen(file);
	if (plen >= PATH_MAX) {
		showMessage(win, MESSAGE_ERROR, BUTTONS_OK, "Path '%s' is too long", file);
		return;
	}
	char path[PATH_MAX];
	memcpy(path, file, (plen + 1) * sizeof(char));

	char* sep = memrchr(path, '/', plen * sizeof(char));
	sep = sep ? sep + 1 : path;
	size_t nlen = (size_t)(path + plen - sep);
	if (nlen >= FILENAME_MAX) {
		showMessage(win, MESSAGE_ERROR, BUTTONS_OK, "Filename '%s' is too long", sep);
		return;
	}
	char name[FILENAME_MAX];
	memcpy(name, sep, (nlen + 1) * sizeof(char));
	*sep = '\0';

	GtkTreeIter it;
	gtk_list_store_append(win->lsFiles, &it);
	gtk_list_store_set(win->lsFiles, &it, FCOL_OLD_NAME, name, FCOL_NEW_NAME, name, FCOL_DIRECTORY, path, FCOL_INVALID);
}

static void processArgumentFiles(Window* win) {
	size_t plen = 0;
	char path[PATH_MAX];
	if (getcwd(path, PATH_MAX)) {
		plen = strlen(path);
		if (path[plen - 1] != '/')
			path[plen++] = '/';
	} else
		g_printerr("Current working directory path too long\n");

	Arguments* arg = win->args;
	if (!arg->noGui && arg->files) {
#ifdef __MINGW32__
		char buf[PATH_MAX];
#endif
		for (size_t i = 0; i < arg->nFiles; ++i) {
			const char* file = g_file_peek_path(arg->files[i]);
			if (file) {
#ifdef __MINGW32__
				size_t fsiz = strlen(file) + 1;
				if (fsiz >= PATH_MAX) {
					g_printerr("Filepath '%s' is too long\n", file);
					continue;
				}
				memcpy(buf, file, fsiz * sizeof(char));
				unbackslashify(buf);
				addFile(win, buf);
#else
				addFile(win, file);
#endif
			}
		}
	}
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
static ResponseType windowMessage(Window* win, MessageType type, ButtonsType buttons, char* message) {
	ResponseType rc;
	if (win) {
		GtkMessageDialog* dialog = GTK_MESSAGE_DIALOG(gtk_message_dialog_new(GTK_WINDOW(win->window), GTK_DIALOG_DESTROY_WITH_PARENT, (GtkMessageType)type, (GtkButtonsType)buttons, "%s", message));
		rc = gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(GTK_WIDGET(dialog));
	} else
		rc = consoleMessage(type, buttons, message);
	return rc;
}

static gboolean asyncMessage(AsyncMessage* am) {
	g_mutex_lock(&am->mutex);
	am->response = windowMessage(am->win, am->message, am->buttons, am->text);
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
			.win = win,
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
		rc = am.response;
		g_mutex_unlock(&am.mutex);
		g_cond_clear(&am.cond);
		g_mutex_clear(&am.mutex);
	} else
		rc = windowMessage(win, type, buttons, str);
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

#ifndef CONSOLE
static void runAddDialog(Window* win, const char* title, GtkFileChooserAction action) {
	GtkFileChooserDialog* dialog = GTK_FILE_CHOOSER_DIALOG(gtk_file_chooser_dialog_new(title, GTK_WINDOW(win->window), action, "Cancel", GTK_RESPONSE_CANCEL, "Add", GTK_RESPONSE_ACCEPT, NULL));
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), g_get_home_dir());
	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), true);

#ifdef __MINGW32__
	char path[PATH_MAX];
#endif
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		GSList* files = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));
		for (GSList* it = files; it; it = it->next) {
#ifdef __MINGW32__
			size_t plen = strlen(it->data);
			if (plen < PATH_MAX) {
				memcpy(path, it->data, (plen + 1) * sizeof(char));
				unbackslashify(path);
				addFile(win, path);
			} else
				showMessage(win, MESSAGE_ERROR, BUTTONS_OK, "Path '%s' is too long", (char*)files->data);
#else
			addFile(win, it->data);
#endif
		}
		g_slist_free(files);
		autoPreview(win);
	}
	gtk_widget_destroy(GTK_WIDGET(dialog));
}

G_MODULE_EXPORT void clickAddFiles(GtkButton* button, Window* win) {
	runAddDialog(win, "Add Files", GTK_FILE_CHOOSER_ACTION_OPEN);
}

G_MODULE_EXPORT void clickAddFolders(GtkButton* button, Window* win) {
	runAddDialog(win, "Add Folders", GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
}

static void toggleAutoPreview(GtkCheckMenuItem* checkmenuitem, Window* win) {
	win->autoPreview = gtk_check_menu_item_get_active(checkmenuitem);
	autoPreview(win);
}

static void toggleSingleThread(GtkCheckMenuItem* checkmenuitem, Window* win) {
	win->singleThread = gtk_check_menu_item_get_active(checkmenuitem);
}

static void activateClear(GtkMenuItem* item, Window* win) {
	gtk_list_store_clear(win->lsFiles);
}

static void activateReset(GtkMenuItem* item, Window* win) {
	Arguments* arg = win->args;
	gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbExtensionMode), (int)arg->extensionMode);
	gtk_entry_set_text(win->etExtension, arg->extensionName ? arg->extensionName : "");
	gtk_entry_set_text(win->etExtensionReplace, arg->extensionReplace ? arg->extensionReplace : "");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->cbExtensionCi), arg->extensionCi);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->cbExtensionRegex), arg->extensionRegex);
	gtk_spin_button_set_value(win->sbExtensionElements, (double)arg->extensionElements);
	gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbRenameMode), (int)arg->renameMode);
	gtk_entry_set_text(win->etRename, arg->rename ? arg->rename : "");
	gtk_entry_set_text(win->etReplace, arg->replace ? arg->replace : "");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->cbReplaceCi), arg->replaceCi);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->cbReplaceRegex), arg->replaceRegex);
	gtk_spin_button_set_value(win->sbRemoveFrom, (double)arg->removeFrom);
	gtk_spin_button_set_value(win->sbRemoveTo, (double)arg->removeTo);
	gtk_spin_button_set_value(win->sbRemoveFirst, (double)arg->removeFirst);
	gtk_spin_button_set_value(win->sbRemoveLast, (double)arg->removeLast);
	gtk_entry_set_text(win->etAddInsert, arg->addInsert ? arg->addInsert : "");
	gtk_spin_button_set_value(win->sbAddAt, (double)arg->addAt);
	gtk_entry_set_text(win->etAddPrefix, arg->addPrefix ? arg->addPrefix : "");
	gtk_entry_set_text(win->etAddSuffix, arg->addSuffix ? arg->addSuffix : "");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->cbNumber), arg->number);
	gtk_spin_button_set_value(win->sbNumberLocation, (double)arg->numberLocation);
	gtk_spin_button_set_value(win->sbNumberStart, (double)arg->numberStart);
	gtk_spin_button_set_value(win->sbNumberStep, (double)arg->numberStep);
	gtk_spin_button_set_value(win->sbNumberBase, (double)arg->numberBase);
	gtk_spin_button_set_value(win->sbNumberPadding, (double)arg->numberPadding);
	gtk_entry_set_text(win->etNumberPadding, arg->numberPadStr ? arg->numberPadStr : "");
	gtk_entry_set_text(win->etNumberPrefix, arg->numberPrefix ? arg->numberPrefix : "");
	gtk_entry_set_text(win->etNumberSuffix, arg->numberSuffix ? arg->numberSuffix : "");
	gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbDestinationMode), (int)arg->destinationMode);
	gtk_entry_set_text(win->etDestination, arg->destination ? arg->destination : "");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->cbDestinationForward), !arg->backwards);
	win->autoPreview = !arg->noAutoPreview;
}

G_MODULE_EXPORT void clickOptions(GtkButton* button, Window* win) {
	GtkCheckMenuItem* miPreview = GTK_CHECK_MENU_ITEM(gtk_check_menu_item_new_with_label("Auto Preview"));
	gtk_check_menu_item_set_active(miPreview, win->autoPreview);
	g_signal_connect(miPreview, "toggled", G_CALLBACK(toggleAutoPreview), win);

	GtkCheckMenuItem* miThread = GTK_CHECK_MENU_ITEM(gtk_check_menu_item_new_with_label("Single Thread"));
	gtk_check_menu_item_set_active(miThread, win->singleThread);
	g_signal_connect(miThread, "toggled", G_CALLBACK(toggleSingleThread), win);

	GtkMenuItem* miClear = GTK_MENU_ITEM(gtk_menu_item_new_with_label("Clear"));
	g_signal_connect(miClear, "activate", G_CALLBACK(activateClear), win);

	GtkMenuItem* miReset = GTK_MENU_ITEM(gtk_menu_item_new_with_label("Reset"));
	g_signal_connect(miReset, "activate", G_CALLBACK(activateReset), win);

	GtkMenu* menu = GTK_MENU(gtk_menu_new());
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), GTK_WIDGET(miPreview));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), GTK_WIDGET(miThread));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), GTK_WIDGET(miClear));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), GTK_WIDGET(miReset));
	gtk_widget_show_all(GTK_WIDGET(menu));
	gtk_menu_popup_at_widget(menu, GTK_WIDGET(button), GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL);
}

G_MODULE_EXPORT void dragEndTblFiles(GtkWidget* widget, GdkDragContext* context, Window* win) {
	GtkTargetEntry targetEntry = { "text/uri-list", 0, 0 };
	gtk_tree_view_enable_model_drag_source(GTK_TREE_VIEW(widget), GDK_BUTTON1_MASK, &targetEntry, 1, GDK_ACTION_MOVE);
	gtk_tree_view_enable_model_drag_dest(GTK_TREE_VIEW(widget), &targetEntry, 1, GDK_ACTION_COPY | GDK_ACTION_MOVE);
	if (context)
		autoPreview(win);
}

static bool addFileUris(Window* win, char** uris) {
	if (!uris)
		return false;

	size_t flen = strlen(FILE_URI_PREFIX);
	for (int i = 0; uris[i]; ++i)
		if (!strncmp(uris[i], FILE_URI_PREFIX, flen)) {
#ifdef __MINGW32__
			const char* file = uris[i] + flen;
			addFile(win, file + (file[0] == '/' && isalnum(file[1]) && file[2] == ':' && file[3] == '/'));
#else
			addFile(win, uris[i] + flen);
#endif
		}
	g_strfreev(uris);
	return true;
}

G_MODULE_EXPORT void dropTblFiles(GtkWidget* widget, GdkDragContext* context, int x, int y, GtkSelectionData* data, uint info, uint time, Window* win) {
	if (addFileUris(win, gtk_selection_data_get_uris(data))) {
		gtk_drag_finish(context, true, false, time);
		autoPreview(win);
	}
}

static GtkTreeRowReference** getTreeViewSelectedRowRefs(GtkTreeView* view, GtkTreeModel** model, uint* cnt) {
	GtkTreeSelection* selc = gtk_tree_view_get_selection(view);
	GList* rows = gtk_tree_selection_get_selected_rows(selc, model);
	uint rnum = g_list_length(rows);
	if (!rnum) {
		g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);
		return NULL;
	}

	GtkTreeRowReference** refs = malloc(rnum * sizeof(GtkTreeRowReference*));
	*cnt = 0;
	for (GList* it = rows; it; it = it->next) {
		refs[*cnt] = gtk_tree_row_reference_new(*model, it->data);
		if (refs[*cnt])
			++*cnt;
	}
	g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);
	return refs;
}

static void removeTblFilesRowRefs(Window* win, GtkTreeModel* model, GtkTreeRowReference** refs, uint cnt) {
	for (uint i = 0; i < cnt; ++i) {
		GtkTreeIter iter;
		if (gtk_tree_model_get_iter(model, &iter, gtk_tree_row_reference_get_path(refs[i])))
			gtk_list_store_remove(win->lsFiles, &iter);
		gtk_tree_row_reference_free(refs[i]);
	}
	free(refs);
	autoPreview(win);
}

static void activateTblDel(GtkMenuItem* item, Window* win) {
	GtkTreeModel* model;
	uint cnt;
	GtkTreeRowReference** refs = getTreeViewSelectedRowRefs(win->tblFiles, &model, &cnt);
	if (refs)
		removeTblFilesRowRefs(win, model, refs, cnt);
}

G_MODULE_EXPORT gboolean buttonPressTblFiles(GtkWidget* widget, GdkEvent* event, Window* win) {
	switch (event->button.button) {
	case GDK_BUTTON_PRIMARY:
		gtk_tree_view_set_reorderable(GTK_TREE_VIEW(widget), true);
		break;
	case GDK_BUTTON_MIDDLE: {
		GtkTreeView* view = GTK_TREE_VIEW(widget);
		GtkTreePath* path;
		if (gtk_tree_view_get_path_at_pos(view, (int)event->button.x, (int)event->button.y, &path, NULL, NULL, NULL)) {
			GtkTreeIter iter;
			if (gtk_tree_model_get_iter(gtk_tree_view_get_model(view), &iter, path))
				gtk_list_store_remove(win->lsFiles, &iter);
			gtk_tree_path_free(path);
		}
		break; }
	case GDK_BUTTON_SECONDARY: {
		GtkTreeView* view = GTK_TREE_VIEW(widget);
		GtkTreePath* path;
		if (gtk_tree_view_get_path_at_pos(view, (int)event->button.x, (int)event->button.y, &path, NULL, NULL, NULL)) {
			gtk_tree_view_set_cursor(view, path, NULL, false);
			gtk_tree_path_free(path);
		}

		GtkMenuItem* miAddFiles = GTK_MENU_ITEM(gtk_menu_item_new_with_label("Add Files"));
		g_signal_connect(miAddFiles, "activate", G_CALLBACK(clickAddFiles), win);

		GtkMenuItem* miAddFolders = GTK_MENU_ITEM(gtk_menu_item_new_with_label("Add Folders"));
		g_signal_connect(miAddFolders, "activate", G_CALLBACK(clickAddFolders), win);

		GtkMenuItem* miDel = NULL;
		if (gtk_tree_selection_count_selected_rows(gtk_tree_view_get_selection(view))) {
			miDel = GTK_MENU_ITEM(gtk_menu_item_new_with_label("Delete"));
			g_signal_connect(miDel, "activate", G_CALLBACK(activateTblDel), win);
		}

		GtkMenuItem* miClear = GTK_MENU_ITEM(gtk_menu_item_new_with_label("Clear"));
		g_signal_connect(miClear, "activate", G_CALLBACK(activateClear), win);

		GtkMenu* menu = GTK_MENU(gtk_menu_new());
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), GTK_WIDGET(miAddFiles));
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), GTK_WIDGET(miAddFolders));
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
		if (miDel)
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), GTK_WIDGET(miDel));
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), GTK_WIDGET(miClear));
		gtk_widget_show_all(GTK_WIDGET(menu));
		gtk_menu_popup_at_pointer(menu, event);
		break; }
	}
	return false;
}

G_MODULE_EXPORT gboolean buttonReleaseTblFiles(GtkWidget* widget, GdkEvent* event, Window* win) {
	if (event->button.button == GDK_BUTTON_PRIMARY)
		dragEndTblFiles(widget, NULL, win);
	return false;
}

static void appendTblFilesTextName(char** text, size_t* tlen, size_t *tmax, const char* name) {
	size_t len = strlen(name);
	if (*tlen + len >= *tmax) {
		*tmax += PATH_MAX;
		*text = realloc(*text, *tmax * sizeof(char));
	}
	memcpy(*text + *tlen, name, len * sizeof(char));
	*tlen += len;
}

static void setTblFilesClipboardText(GtkTreeModel* model, GtkTreeRowReference** refs, uint cnt) {
	size_t tlen = 0, tmax = PATH_MAX;
	char* text = malloc(tmax * sizeof(char));
	for (uint i = 0; i < cnt; ++i) {
		GtkTreeIter iter;
		if (gtk_tree_model_get_iter(model, &iter, gtk_tree_row_reference_get_path(refs[i]))) {
			char* name;
			char* dirc;
			gtk_tree_model_get(model, &iter, FCOL_OLD_NAME, &name, FCOL_DIRECTORY, &dirc, FCOL_INVALID);
			appendTblFilesTextName(&text, &tlen, &tmax, dirc);
			appendTblFilesTextName(&text, &tlen, &tmax, name);
#ifdef __MINGW32__
			appendTblFilesTextName(&text, &tlen, &tmax, LINE_BREAK_CHARS);
#else
			appendTblFilesTextName(&text, &tlen, &tmax, "\n");
#endif
			g_free(dirc);
			g_free(name);
		}
	}
	text[tlen] = '\0';
	gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), text, (int)tlen);
	free(text);
}

G_MODULE_EXPORT gboolean keyPressTblFiles(GtkWidget* widget, GdkEvent* event, Window* win) {
	switch (event->key.keyval) {
	case GDK_KEY_x:
		if (event->key.state & GDK_CONTROL_MASK) {
			GtkTreeModel* model;
			uint cnt;
			GtkTreeRowReference** refs = getTreeViewSelectedRowRefs(GTK_TREE_VIEW(widget), &model, &cnt);
			if (refs) {
				setTblFilesClipboardText(model, refs, cnt);
				removeTblFilesRowRefs(win, model, refs, cnt);
			}
		}
		break;
	case GDK_KEY_c:
		if (event->key.state & GDK_CONTROL_MASK) {
			GtkTreeModel* model;
			uint cnt;
			GtkTreeRowReference** refs = getTreeViewSelectedRowRefs(GTK_TREE_VIEW(widget), &model, &cnt);
			if (refs) {
				setTblFilesClipboardText(model, refs, cnt);
				for (uint i = 0; i < cnt; ++i)
					gtk_tree_row_reference_free(refs[i]);
				free(refs);
				autoPreview(win);
			}
		}
		break;
	case GDK_KEY_v:
		if (event->key.state & GDK_CONTROL_MASK) {
			GtkClipboard* clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
			char** uris = gtk_clipboard_wait_for_uris(clipboard);
			if (uris) {
				if (addFileUris(win, uris))
					autoPreview(win);
			} else {
				char* text = gtk_clipboard_wait_for_text(clipboard);
				if (text) {
					for (char* pos = text + strspn(text, LINE_BREAK_CHARS); *pos; pos += strspn(pos, LINE_BREAK_CHARS)) {
						size_t i = strcspn(pos, LINE_BREAK_CHARS);
						bool more = pos[i] != '\0';
						pos[i] = '\0';
						addFile(win, pos);
						pos += i + more;
					}
					g_free(text);
					autoPreview(win);
				}
			}
		}
		break;
	case GDK_KEY_Delete:
		activateTblDel(NULL, win);
	}
	return false;
}

static int strcmpNameAsc(const void* a, const void* b) {
	return strcmp(((const KeyPos*)a)->key, ((const KeyPos*)b)->key);
}

static int strcmpNameDsc(const void* a, const void* b) {
	return -strcmp(((const KeyPos*)a)->key, ((const KeyPos*)b)->key);
}

static int strcmpDirAsc(const void* a, const void* b) {
	const KeyDirPos* l = a;
	const KeyDirPos* r = b;
	int rc = strcmp(l->dir, r->dir);
	return !rc ? strcmp(l->file, r->file) : rc;
}

static int strcmpDirDsc(const void* a, const void* b) {
	return -strcmpDirAsc(a, b);
}

static size_t sortColumnInit(GtkTreeViewColumn* column, Window* win, Sorting* sort, GtkTreeViewColumn* cother, Sorting* sother, GtkTreeModel** model, GtkTreeIter* it) {
	*sother = SORT_NONE;
	gtk_tree_view_column_set_sort_indicator(cother, false);
	if (*sort == SORT_DSC) {
		*sort = SORT_NONE;
		gtk_tree_view_column_set_sort_indicator(column, false);
		return 0;
	}
	gtk_tree_view_column_set_sort_order(column, ++*sort == SORT_ASC ? GTK_SORT_ASCENDING : GTK_SORT_DESCENDING);
	gtk_tree_view_column_set_sort_indicator(column, true);

	*model = gtk_tree_view_get_model(win->tblFiles);
	return gtk_tree_model_get_iter_first(*model, it) ? (size_t)gtk_tree_model_iter_n_children(*model, NULL) : 0;
}

static void sortColumnFinish(Window* win, int* order, void* keys) {
	gtk_list_store_reorder(win->lsFiles, order);
	free(order);
	free(keys);
	autoPreview(win);
}

G_MODULE_EXPORT void clickColumnTblFilesName(GtkTreeViewColumn* treeviewcolumn, Window* win) {
	GtkTreeModel* model;
	GtkTreeIter it;
	size_t num = sortColumnInit(treeviewcolumn, win, &win->nameSort, win->tblFilesDirectory, &win->directorySort, &model, &it);
	if (!num)
		return;

	KeyPos* keys = malloc(num * sizeof(KeyPos));
	int* order = malloc(num * sizeof(int));
	size_t i = 0;
	do {
		char* name;
		gtk_tree_model_get(model, &it, FCOL_OLD_NAME, &name, FCOL_INVALID);
		keys[i].pos = i;
		keys[i++].key = g_utf8_collate_key_for_filename(name, -1);
		g_free(name);
	} while (gtk_tree_model_iter_next(model, &it));
	num = i;
	qsort(keys, num, sizeof(KeyPos), win->nameSort == SORT_ASC ? strcmpNameAsc : strcmpNameDsc);

	for (i = 0; i < num; ++i) {
		order[i] = (int)keys[i].pos;
		g_free(keys[i].key);
	}
	sortColumnFinish(win, order, keys);
}

G_MODULE_EXPORT void clickColumnTblFilesDirectory(GtkTreeViewColumn* treeviewcolumn, Window* win) {
	GtkTreeModel* model;
	GtkTreeIter it;
	size_t num = sortColumnInit(treeviewcolumn, win, &win->directorySort, win->tblFilesName, &win->nameSort, &model, &it);
	if (!num)
		return;

	KeyDirPos* keys = malloc(num * sizeof(KeyDirPos));
	int* order = malloc(num * sizeof(int));
	size_t i = 0;
	do {
		char* file;
		char* dir;
		gtk_tree_model_get(model, &it, FCOL_OLD_NAME, &file, FCOL_DIRECTORY, &dir, FCOL_INVALID);
		keys[i].pos = i;
		keys[i].file = g_utf8_collate_key_for_filename(file, -1);
		keys[i++].dir = g_utf8_collate_key_for_filename(dir, -1);
		g_free(file);
		g_free(dir);
	} while (gtk_tree_model_iter_next(model, &it));
	num = i;
	qsort(keys, num, sizeof(KeyDirPos), win->directorySort == SORT_ASC ? strcmpDirAsc : strcmpDirDsc);

	for (i = 0; i < num; ++i) {
		order[i] = (int)keys[i].pos;
		g_free(keys[i].file);
		g_free(keys[i].dir);
	}
	sortColumnFinish(win, order, keys);
}

G_MODULE_EXPORT void valueChangeEtGeneric(GtkEditable* editable, Window* win) {
	autoPreview(win);
}

G_MODULE_EXPORT void valueChangeCmbGeneric(GtkComboBox* comboBox, Window* win) {
	autoPreview(win);
}

G_MODULE_EXPORT void valueChangeSbGeneric(GtkSpinButton* spinButton, Window* win) {
	autoPreview(win);
}

G_MODULE_EXPORT void toggleCbGeneric(GtkToggleButton* toggleButton, Window* win) {
	autoPreview(win);
}

G_MODULE_EXPORT void valueChangeEtExtension(GtkEditable* editable, Window* win) {
	RenameMode mode = (uint)gtk_combo_box_get_active(GTK_COMBO_BOX(win->cmbExtensionMode));
	if (mode != RENAME_RENAME && mode != RENAME_REPLACE)
		gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbExtensionMode), RENAME_RENAME);
	autoPreview(win);
}

G_MODULE_EXPORT void valueChangeEtExtensionReplace(GtkEditable* editable, Window* win) {
	gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbExtensionMode), RENAME_REPLACE);
	autoPreview(win);
}

G_MODULE_EXPORT void toggleCbExtensionReplace(GtkToggleButton* toggleButton, Window* win) {
	gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbExtensionMode), RENAME_REPLACE);
	autoPreview(win);
}

G_MODULE_EXPORT void valueChangeEtRename(GtkEditable* editable, Window* win) {
	RenameMode mode = (uint)gtk_combo_box_get_active(GTK_COMBO_BOX(win->cmbRenameMode));
	if (mode != RENAME_RENAME && mode != RENAME_REPLACE)
		gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbRenameMode), RENAME_RENAME);
	autoPreview(win);
}

G_MODULE_EXPORT void valueChangeEtReplace(GtkEditable* editable, Window* win) {
	gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbRenameMode), RENAME_REPLACE);
	autoPreview(win);
}

G_MODULE_EXPORT void toggleCbReplace(GtkToggleButton* editable, Window* win) {
	gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbRenameMode), RENAME_REPLACE);
	autoPreview(win);
}

G_MODULE_EXPORT void valueChangeSbNumber(GtkSpinButton* spinButton, Window* win) {
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->cbNumber), true);
	autoPreview(win);
}

G_MODULE_EXPORT void valueChangeEtNumber(GtkEditable* editable, Window* win) {
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->cbNumber), true);
	autoPreview(win);
}

G_MODULE_EXPORT void valueChangeEtDestination(GtkEditable* editable, Window* win) {
	if (gtk_combo_box_get_active(GTK_COMBO_BOX(win->cmbDestinationMode)) == DESTINATION_IN_PLACE)
		gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbDestinationMode), DESTINATION_MOVE);
	autoPreview(win);
}

G_MODULE_EXPORT void clickOpenDestination(GtkButton* button, Window* win) {
	GtkFileChooserDialog* dialog = GTK_FILE_CHOOSER_DIALOG(gtk_file_chooser_dialog_new("Pick Destination", GTK_WINDOW(win->window), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, "Cancel", GTK_RESPONSE_CANCEL, "Open", GTK_RESPONSE_ACCEPT, NULL));
	const char* dst = gtk_entry_get_text(win->etDestination);
	struct stat ps;
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), !stat(dst, &ps) && S_ISDIR(ps.st_mode) ? dst : g_get_home_dir());
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		char* dirc = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		if (dirc) {
			gtk_entry_set_text(win->etDestination, dirc);
			g_free(dirc);

			if (gtk_combo_box_get_active(GTK_COMBO_BOX(win->cmbDestinationMode)) == DESTINATION_IN_PLACE)
				gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbDestinationMode), DESTINATION_MOVE);
		}
	}
	gtk_widget_destroy(GTK_WIDGET(dialog));
}

G_MODULE_EXPORT void clickPreview(GtkButton* button, Window* win) {
	windowPreview(win);
}

static void setThreadCode(Process* prc, ThreadCode code) {
	g_mutex_lock(&prc->mutex);
	prc->threadCode = code;
	g_mutex_unlock(&prc->mutex);
}

G_MODULE_EXPORT void clickRename(GtkButton* button, Window* win) {
	Process* prc = win->proc;
	if (!prc->thread)
		windowRename(win);
	else
		setThreadCode(prc, THREAD_ABORT);
}

G_MODULE_EXPORT gboolean closeWindow(GtkApplicationWindow* window, GdkEvent* event, Window* win) {
	Process* prc = win->proc;
	if (prc->thread) {
		if (showMessage(win, MESSAGE_QUESTION, BUTTONS_YES_NO, "A process is still running.\nDo you want to abort it?") == RESPONSE_NO)
			return true;
		setThreadCode(prc, THREAD_DISCARD);
		joinThread(prc);
	}
	return false;
}

void autoPreview(Window* win) {
	if (win->autoPreview) {
		win->dryAuto = true;
		clickPreview(NULL, win);
		win->dryAuto = false;
	}
}
#endif

static void runConsole(Window* win) {
	Arguments* arg = win->args;
	processArgumentOptions(arg);
	if (!arg->dry)
		consoleRename(win);
	else
		consolePreview(win);
}

#ifdef CONSOLE
static void initWindow(GApplication* app, Window* win) {
	runConsole(win);
}

static void initWindowOpen(GApplication* app, GFile** files, int nFiles, const char* hint, Window* win) {
	Arguments* arg = win->args;
	arg->files = files;
	arg->nFiles = (size_t)nFiles;
	runConsole(win);
}
#else
static void initWindow(GtkApplication* app, Window* win) {
	Arguments* arg = win->args;
	if (arg->noGui) {
		runConsole(win);
		return;
	}
	processArgumentOptions(arg);

	const char* err = gtk_check_version(3, 10, 0);
	if (err) {
		g_printerr("%s\n", err);
		g_application_quit(G_APPLICATION(app));
		return;
	}

	char path[PATH_MAX];
#ifdef __MINGW32__
	wchar* wpath = malloc(PATH_MAX * sizeof(wchar));
	size_t plen = GetModuleFileNameW(NULL, wpath, PATH_MAX) + 1;
	if (plen > 1 && plen <= PATH_MAX)
		plen = WideCharToMultiByte(CP_UTF8, 0, wpath, plen, path, PATH_MAX, NULL, NULL);
	free(wpath);
#else
	size_t plen = (size_t)readlink("/proc/self/exe", path, PATH_MAX);
#endif
	if (plen > 1 && plen <= PATH_MAX) {
#ifdef __MINGW32__
		unbackslashify(path);
#endif
		char* pos = memrchr(path, '/', --plen);
		if (pos)
			pos = memrchr(path, '/', (size_t)(pos - path - (pos != path)));

		plen = (size_t)(pos - path);
		if (pos && plen + strlen(MAIN_GLADE_PATH) + 2 < PATH_MAX)
			++plen;
		else {
			strcpy(path, "../");
			plen = strlen(path);
		}
	} else {
		strcpy(path, "../");
		plen = strlen(path);
	}
	strcpy(path + plen, MAIN_GLADE_PATH);

	size_t glen;
	char* glade = (char*)readGzip(path, &glen);
	if (!glade) {
		g_printerr("Failed to read main.glade\n");
		return;
	}

	GtkBuilder* builder = gtk_builder_new();
	GError* error = NULL;
	if (!gtk_builder_add_from_string(builder, glade, glen, &error)) {
		free(glade);
		g_printerr("Failed to load main.glade: %s\n", error->message);
		g_clear_error(&error);
		return;
	}
	free(glade);
	win->window = GTK_APPLICATION_WINDOW(gtk_builder_get_object(builder, "window"));
	win->btAddFiles = GTK_BUTTON(gtk_builder_get_object(builder, "btAddFiles"));
	win->btAddFolders = GTK_BUTTON(gtk_builder_get_object(builder, "btAddFolders"));
	win->btOptions = GTK_BUTTON(gtk_builder_get_object(builder, "btOptions"));
	win->tblFiles = GTK_TREE_VIEW(gtk_builder_get_object(builder, "tblFiles"));
	win->lsFiles = GTK_LIST_STORE(gtk_builder_get_object(builder, "lsFiles"));
	win->tblFilesName = GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(builder, "tblFilesName"));
	win->tblFilesDirectory = GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(builder, "tblFilesDirectory"));
	win->cmbExtensionMode = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "cmbExtensionMode"));
	win->etExtension = GTK_ENTRY(gtk_builder_get_object(builder, "etExtension"));
	win->etExtensionReplace = GTK_ENTRY(gtk_builder_get_object(builder, "etExtensionReplace"));
	win->cbExtensionCi = GTK_CHECK_BUTTON(gtk_builder_get_object(builder, "cbExtensionCi"));
	win->cbExtensionRegex = GTK_CHECK_BUTTON(gtk_builder_get_object(builder, "cbExtensionRegex"));
	win->sbExtensionElements = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "sbExtensionElements"));
	win->cmbRenameMode = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "cmbRenameMode"));
	win->etRename = GTK_ENTRY(gtk_builder_get_object(builder, "etRename"));
	win->etReplace = GTK_ENTRY(gtk_builder_get_object(builder, "etReplace"));
	win->cbReplaceCi = GTK_CHECK_BUTTON(gtk_builder_get_object(builder, "cbReplaceCi"));
	win->cbReplaceRegex = GTK_CHECK_BUTTON(gtk_builder_get_object(builder, "cbReplaceRegex"));
	win->sbRemoveFrom = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "sbRemoveFrom"));
	win->sbRemoveTo = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "sbRemoveTo"));
	win->sbRemoveFirst = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "sbRemoveFirst"));
	win->sbRemoveLast = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "sbRemoveLast"));
	win->etAddInsert = GTK_ENTRY(gtk_builder_get_object(builder, "etAddInsert"));
	win->sbAddAt = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "sbAddAt"));
	win->etAddPrefix = GTK_ENTRY(gtk_builder_get_object(builder, "etAddPrefix"));
	win->etAddSuffix = GTK_ENTRY(gtk_builder_get_object(builder, "etAddSuffix"));
	win->cbNumber = GTK_CHECK_BUTTON(gtk_builder_get_object(builder, "cbNumber"));
	win->sbNumberLocation = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "sbNumberLocation"));
	win->sbNumberBase = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "sbNumberBase"));
	win->sbNumberStart = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "sbNumberStart"));
	win->sbNumberStep = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "sbNumberStep"));
	win->sbNumberPadding = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "sbNumberPadding"));
	win->etNumberPadding = GTK_ENTRY(gtk_builder_get_object(builder, "etNumberPadding"));
	win->etNumberPrefix = GTK_ENTRY(gtk_builder_get_object(builder, "etNumberPrefix"));
	win->etNumberSuffix = GTK_ENTRY(gtk_builder_get_object(builder, "etNumberSuffix"));
	win->cmbDestinationMode = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "cmbDestinationMode"));
	win->etDestination = GTK_ENTRY(gtk_builder_get_object(builder, "etDestination"));
	win->btDestination = GTK_BUTTON(gtk_builder_get_object(builder, "btDestination"));
	win->btPreview = GTK_BUTTON(gtk_builder_get_object(builder, "btPreview"));
	win->btRename = GTK_BUTTON(gtk_builder_get_object(builder, "btRename"));
	win->cbDestinationForward = GTK_CHECK_BUTTON(gtk_builder_get_object(builder, "cbDestinationForward"));
	win->pbRename = GTK_PROGRESS_BAR(gtk_builder_get_object(builder, "pbRename"));

	gtk_widget_set_sensitive(GTK_WIDGET(win->btRename), !arg->dry);
	dragEndTblFiles(GTK_WIDGET(win->tblFiles), NULL, win);

	GtkLabel* numberLabel = GTK_LABEL(gtk_bin_get_child(GTK_BIN(win->cbNumber)));
	PangoAttrList* attrs = gtk_label_get_attributes(numberLabel);
	if (!attrs)
		attrs = pango_attr_list_new();
	pango_attr_list_change(attrs, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
	gtk_label_set_attributes(numberLabel, attrs);
	pango_attr_list_unref(attrs);

	processArgumentFiles(win);
	activateReset(NULL, win);
	if (plen + strlen(WINDOW_ICON_PATH) < PATH_MAX) {
		strcpy(path + plen, WINDOW_ICON_PATH);
		gtk_window_set_icon_from_file(GTK_WINDOW(win->window), path, NULL);
	}

	gtk_builder_connect_signals(builder, win);
	g_object_unref(builder);
	gtk_application_add_window(app, GTK_WINDOW(win->window));
	gtk_widget_show_all(GTK_WIDGET(win->window));
}

static void initWindowOpen(GtkApplication* app, GFile** files, int nFiles, const char* hint, Window* win) {
	Arguments* arg = win->args;
	arg->files = files;
	arg->nFiles = (size_t)nFiles;
	if (!arg->noGui)
		initWindow(app, win);
	else
		runConsole(win);
}
#endif

int main(int argc, char** argv) {
	Window win = {
		.proc = malloc(sizeof(Process)),
#ifdef CONSOLE
		.app = g_application_new(NULL, G_APPLICATION_HANDLES_OPEN),
#else
		.app = gtk_application_new(NULL, G_APPLICATION_HANDLES_OPEN),
		.singleThread = false,
#endif
		.args = malloc(sizeof(Arguments))
	};
	Process* prc = win.proc;
	memset(prc, 0, sizeof(Process));

	Arguments* arg = win.args;
	g_signal_connect(win.app, "activate", G_CALLBACK(initWindow), &win);
	g_signal_connect(win.app, "open", G_CALLBACK(initWindowOpen), &win);
	initCommandLineArguments(G_APPLICATION(win.app), arg, argc, argv);
	int rc = g_application_run(G_APPLICATION(win.app), argc, argv);
	g_object_unref(win.app);

	g_free(arg->extensionName);
	g_free(arg->extensionReplace);
	g_free(arg->rename);
	g_free(arg->replace);
	g_free(arg->addInsert);
	g_free(arg->addPrefix);
	g_free(arg->addSuffix);
	g_free(arg->numberPadStr);
	g_free(arg->numberPrefix);
	g_free(arg->numberSuffix);
	g_free(arg->destination);
	free(arg);
	free(prc);
	return rc;
}
