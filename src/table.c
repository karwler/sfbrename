#ifndef CONSOLE
#include "rename.h"
#include "table.h"
#include "window.h"
#ifdef _WIN32
#include <ctype.h>
#endif

#define FILE_URI_PREFIX "file://"
#define LINE_BREAK_CHARS "\r\n"

typedef struct FileEntry {
	Window* win;
	FileInfo* info;
	size_t dlen;
	char file[];
} FileEntry;

typedef struct FileDetails {
	GtkListStore* lsFiles;
	GtkTreeIter it;
	FileInfo* info;
} FileDetails;

typedef struct ArgumentFiles {
	Window* win;
	char* files[];
} ArgumentFiles;

typedef struct ClipboardFiles {
	Window* win;
	union {
		char** uris;
		char* text;
	};
	bool list;
} ClipboardFiles;

typedef struct DialogFiles {
	Window* win;
	GSList* files;
} DialogFiles;

typedef struct DragFiles {
	Window* win;
	char** uris;
	GdkDragContext* ctx;
	uint time;
} DragFiles;

typedef struct KeyDirPos {
	char* file;
	char* dir;
	size_t pos;
} KeyDirPos;

static gboolean appendFile(FileEntry* entry) {
	Window* win = entry->win;
	FileInfo* info = entry->info;
	gtk_list_store_insert_after(win->lsFiles, &win->lastFile, win->lastFilePtr);
	if (info) {
		gtk_list_store_set(win->lsFiles, &win->lastFile,
			FCOL_OLD_NAME, entry->file + entry->dlen,
			FCOL_NEW_NAME, entry->file + entry->dlen,
			FCOL_DIRECTORY, entry->file,
			FCOL_SIZE, info->size,
			FCOL_CREATE, info->create,
			FCOL_MODIFY, info->modify,
			FCOL_ACCESS, info->access,
			FCOL_CHANGE, info->change,
			FCOL_PERMISSIONS, info->perms,
			FCOL_USER, info->user,
			FCOL_GROUP, info->group,
			FCOL_INVALID
		);
		freeFileInfo(info);
	} else
		gtk_list_store_set(win->lsFiles, &win->lastFile, FCOL_OLD_NAME, entry->file + entry->dlen, FCOL_NEW_NAME, entry->file + entry->dlen, FCOL_DIRECTORY, entry->file, FCOL_INVALID);

	win->lastFilePtr = &win->lastFile;
	free(entry);
	return G_SOURCE_REMOVE;
}

static void addFile(Window* win, const char* file) {
	if (!g_utf8_validate(file, -1, NULL)) {
		showMessage(win, MESSAGE_ERROR, BUTTONS_OK, "Invalid UTF-8 input path");
		return;
	}
	size_t flen = strlen(file);
	const char* sep = memrchr(file, '/', flen * sizeof(char));
	sep = sep ? sep + 1 : file;
	size_t dlen = sep - file, nlen = flen - dlen;
	if (dlen >= PATH_MAX) {
		showMessage(win, MESSAGE_ERROR, BUTTONS_OK, "Path '%s' is too long", file);
		return;
	}
	if (nlen >= FILENAME_MAX) {
		showMessage(win, MESSAGE_ERROR, BUTTONS_OK, "Filename '%s' is too long", file + dlen);
		return;
	}

	FileEntry* entry = malloc(sizeof(FileEntry) + (flen + 2) * sizeof(char));
	entry->win = win;
	entry->info = NULL;
	entry->dlen = dlen + 1;
	memcpy(entry->file, file, dlen * sizeof(char));
	entry->file[dlen] = '\0';
	memcpy(entry->file + entry->dlen, file + dlen, (nlen + 1) * sizeof(char));
	if (win->sets.showDetails) {
		entry->info = malloc(sizeof(FileInfo));
		setFileInfo(file, entry->info);
	}
	g_idle_add(G_SOURCE_FUNC(appendFile), entry);
}

static void startFileAppend(Window* win) {
	setWidgetsSensitive(win, false);
	GtkTreeModel* model = gtk_tree_view_get_model(win->tblFiles);
	uint num = gtk_tree_model_iter_n_children(model, NULL);
	if (num) {
		gtk_tree_model_iter_nth_child(model, &win->lastFile, NULL, num - 1);
		win->lastFilePtr = &win->lastFile;
	} else
		win->lastFilePtr = NULL;
}

static gboolean runAddDialogFinish(DialogFiles* dgf) {
	finishThread(dgf->win);
	autoPreview(dgf->win);
	setWidgetsSensitive(dgf->win, true);
	g_slist_free(dgf->files);
	free(dgf);
	return G_SOURCE_REMOVE;
}

static void* runAddDialogProc(DialogFiles* dgf) {
#ifdef _WIN32
	char path[PATH_MAX];
#endif
	Window* win = dgf->win;
	Process* prc = win->proc;
	prc->id = 0;
	for (GSList* it = dgf->files; it && win->threadCode == THREAD_POPULATE; it = it->next) {
#ifdef _WIN32
		size_t plen = strlen(it->data);
		if (plen < PATH_MAX) {
			memcpy(path, it->data, (plen + 1) * sizeof(char));
			unbackslashify(path);
			addFile(dgf->win, path);
		} else
			showMessage(dgf->win, MESSAGE_ERROR, BUTTONS_OK, "Path '%s' is too long", (char*)dgf->files->data);
#else
		addFile(dgf->win, it->data);
#endif
		++prc->id;
		g_idle_add(G_SOURCE_FUNC(updateProgressBar), win);
	}
	g_idle_add(G_SOURCE_FUNC(runAddDialogFinish), dgf);
	return NULL;
}

void addFilesFromDialog(Window* win, GSList* files) {
	DialogFiles* dgf = malloc(sizeof(DialogFiles));
	dgf->win = win;
	dgf->files = files;

	Process* prc = win->proc;
	prc->forward = true;
	prc->total = 0;
	for (GSList* it = dgf->files; it; it = it->next, ++win->proc->total);
	startFileAppend(win);
	runThread(win, THREAD_POPULATE, (GThreadFunc)runAddDialogProc, G_SOURCE_FUNC(runAddDialogFinish), dgf);
}

static gboolean processArgumentFilesFinish(ArgumentFiles* af) {
	finishThread(af->win);
	setWidgetsSensitive(af->win, true);
	for (size_t i = 0; i < af->win->proc->total; ++i)
		free(af->files[i]);
	free(af);
	return G_SOURCE_REMOVE;
}

static void* processArgumentFilesProc(ArgumentFiles* af) {
	Window* win = af->win;
	Process* prc = win->proc;
#ifdef _WIN32
	char buf[PATH_MAX];
#endif
	for (prc->id = 0; prc->id < prc->total && win->threadCode == THREAD_POPULATE;) {
#ifdef _WIN32
		size_t fsiz = strlen(af->files[prc->id]) + 1;
		if (fsiz >= PATH_MAX) {
			g_printerr("Filepath '%s' is too long\n", af->files[prc->id]);
			continue;
		}
		memcpy(buf, af->files[prc->id], fsiz * sizeof(char));
		unbackslashify(buf);
		addFile(win, buf);
#else
		addFile(win, af->files[prc->id]);
#endif
		++prc->id;
		g_idle_add(G_SOURCE_FUNC(updateProgressBar), win);
	}
	g_idle_add(G_SOURCE_FUNC(processArgumentFilesFinish), af);
	return NULL;
}

void addFilesFromArguments(Window* win, GFile** files, size_t nFiles) {
	ArgumentFiles* af = malloc(sizeof(ArgumentFiles) + (sizeof(char*) * nFiles));
	af->win = win;
	for (size_t i = 0; i < nFiles; ++i)
		af->files[i] = strdup(g_file_peek_path(files[i]));

	Process* prc = win->proc;
	prc->forward = true;
	prc->total = nFiles;
	startFileAppend(win);
	runThread(win, THREAD_POPULATE, (GThreadFunc)processArgumentFilesProc, G_SOURCE_FUNC(processArgumentFilesFinish), af);
}

static FileDetails* newFileDetails(Window* win, FileInfo* info) {
	FileDetails* details = malloc(sizeof(FileDetails));
	details->lsFiles = win->lsFiles;
	details->it = win->proc->it;
	details->info = info;
	return details;
}

static gboolean setFileDetails(FileDetails* details) {
	FileInfo* info = details->info;
	if (info) {
		gtk_list_store_set(details->lsFiles, &details->it,
			FCOL_SIZE, info->size,
			FCOL_CREATE, info->create,
			FCOL_MODIFY, info->modify,
			FCOL_ACCESS, info->access,
			FCOL_CHANGE, info->change,
			FCOL_PERMISSIONS, info->perms,
			FCOL_USER, info->user,
			FCOL_GROUP, info->group,
			FCOL_INVALID
		);
		freeFileInfo(info);
	} else
		gtk_list_store_set(details->lsFiles, &details->it, FCOL_SIZE, NULL, FCOL_CREATE, NULL, FCOL_MODIFY, NULL, FCOL_ACCESS, NULL, FCOL_CHANGE, NULL, FCOL_PERMISSIONS, NULL, FCOL_USER, NULL, FCOL_GROUP, NULL, FCOL_INVALID);
	free(details);
	return G_SOURCE_REMOVE;
}

static gboolean toggleShowDetailsFinish(Window* win) {
	finishThread(win);
	setWidgetsSensitive(win, true);
	return G_SOURCE_REMOVE;
}

static void* toggleShowDetailsProc(Window* win) {
	Process* prc = win->proc;
	prc->id = 9;
	if (win->sets.showDetails) {
		char* name;
		char* dirc;
		char path[PATH_MAX];
		do {
			gtk_tree_model_get(prc->model, &prc->it, FCOL_OLD_NAME, &name, FCOL_DIRECTORY, &dirc, FCOL_INVALID);
			size_t nlen = strlen(name), dlen = strlen(dirc);
			if (nlen + dlen < PATH_MAX) {
				memcpy(path, dirc, dlen * sizeof(char));
				memcpy(path + dlen, name, (nlen + 1) * sizeof(char));

				FileDetails* details = newFileDetails(win, malloc(sizeof(FileInfo)));
				setFileInfo(path, details->info);
				g_idle_add(G_SOURCE_FUNC(setFileDetails), details);
			}
			g_free(name);
			g_free(dirc);
			++prc->id;
			g_idle_add(G_SOURCE_FUNC(updateProgressBar), win);
		} while (win->threadCode == THREAD_POPULATE && gtk_tree_model_iter_next(prc->model, &prc->it));
	} else {
		do {
			g_idle_add(G_SOURCE_FUNC(setFileDetails), newFileDetails(win, NULL));
			++prc->id;
			g_idle_add(G_SOURCE_FUNC(updateProgressBar), win);
		} while (win->threadCode == THREAD_POPULATE && gtk_tree_model_iter_next(prc->model, &prc->it));
	}
	g_idle_add(G_SOURCE_FUNC(toggleShowDetailsFinish), win);
	return NULL;
}

void setDetailsVisible(Window* win) {
	Settings* set = &win->sets;
	gtk_tree_view_column_set_visible(win->tblFilesSize, set->showDetails);
#ifndef _WIN32
	gtk_tree_view_column_set_visible(win->tblFilesCreate, set->showDetails);
#endif
	gtk_tree_view_column_set_visible(win->tblFilesModify, set->showDetails);
	gtk_tree_view_column_set_visible(win->tblFilesAccess, set->showDetails);
	gtk_tree_view_column_set_visible(win->tblFilesChange, set->showDetails);
	gtk_tree_view_column_set_visible(win->tblFilesPermissions, set->showDetails);
#ifndef _WIN32
	gtk_tree_view_column_set_visible(win->tblFilesUser, set->showDetails);
	gtk_tree_view_column_set_visible(win->tblFilesGroup, set->showDetails);
#endif

	Process* prc = win->proc;
	prc->model = gtk_tree_view_get_model(win->tblFiles);
	if (gtk_tree_model_get_iter_first(prc->model, &prc->it)) {
		prc->forward = true;
		prc->total = gtk_tree_model_iter_n_children(prc->model, NULL);
		setWidgetsSensitive(win, false);
		runThread(win, THREAD_POPULATE, (GThreadFunc)toggleShowDetailsProc, G_SOURCE_FUNC(toggleShowDetailsFinish), win);
	}
}

G_MODULE_EXPORT void dragEndTblFiles(GtkWidget* widget, GdkDragContext* context, Window* win) {
	GtkTargetEntry targetEntry = { "text/uri-list", 0, 0 };
	gtk_tree_view_enable_model_drag_source(GTK_TREE_VIEW(widget), GDK_BUTTON1_MASK, &targetEntry, 1, GDK_ACTION_MOVE);
	gtk_tree_view_enable_model_drag_dest(GTK_TREE_VIEW(widget), &targetEntry, 1, GDK_ACTION_COPY | GDK_ACTION_MOVE);
	if (context)
		autoPreview(win);
}

static void addFileUris(Window* win, char** uris) {
	Process* prc = win->proc;
	size_t flen = strlen(FILE_URI_PREFIX);
	for (prc->id = 0; uris[prc->id] && win->threadCode == THREAD_POPULATE;) {
		if (!strncmp(uris[prc->id], FILE_URI_PREFIX, flen)) {
			char* fpath = g_uri_unescape_string(uris[prc->id] + flen, "/");
			if (fpath) {
#ifdef _WIN32
				addFile(win, fpath + (fpath[0] == '/' && isalnum(fpath[1]) && fpath[2] == ':' && fpath[3] == '/'));
#else
				addFile(win, fpath);
#endif
				g_free(fpath);
			} else
				g_printerr("Failed to unescape '%s'\n", uris[prc->id]);
		}
		++prc->id;
		g_idle_add(G_SOURCE_FUNC(updateProgressBar), win);
	}
}

static gboolean dropTblFilesFinish(DragFiles* dgf) {
	finishThread(dgf->win);
	gtk_drag_finish(dgf->ctx, true, false, dgf->time);
	autoPreview(dgf->win);
	setWidgetsSensitive(dgf->win, true);
	g_strfreev(dgf->uris);
	free(dgf);
	return G_SOURCE_REMOVE;
}

static void* dropTblFilesProc(DragFiles* dgf) {
	addFileUris(dgf->win, dgf->uris);
	g_idle_add(G_SOURCE_FUNC(dropTblFilesFinish), dgf);
	return NULL;
}

G_MODULE_EXPORT void dropTblFiles(GtkWidget* widget, GdkDragContext* context, int x, int y, GtkSelectionData* data, uint info, uint time, Window* win) {
	if (win->threadCode != THREAD_NONE)
		return;

	DragFiles* dgf = malloc(sizeof(DragFiles));
	dgf->win = win;
	dgf->uris = gtk_selection_data_get_uris(data);
	dgf->ctx = context;
	dgf->time = time;
	if (dgf->uris) {
		Process* prc = win->proc;
		prc->forward = true;
		for (prc->total = 0; dgf->uris[prc->total]; ++prc->total);
		startFileAppend(win);
		runThread(win, THREAD_POPULATE, (GThreadFunc)dropTblFilesProc, G_SOURCE_FUNC(dropTblFilesFinish), dgf);
	} else
		free(dgf);
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

G_MODULE_EXPORT gboolean buttonPressTblFiles(GtkWidget* widget, GdkEventButton* event, Window* win) {
	switch (event->button) {
	case GDK_BUTTON_PRIMARY:
		gtk_tree_view_set_reorderable(GTK_TREE_VIEW(widget), true);
		break;
	case GDK_BUTTON_MIDDLE: {
		GtkTreeView* view = GTK_TREE_VIEW(widget);
		GtkTreePath* path;
		if (gtk_tree_view_get_path_at_pos(view, (int)event->x, (int)event->y, &path, NULL, NULL, NULL)) {
			GtkTreeIter iter;
			if (gtk_tree_model_get_iter(gtk_tree_view_get_model(view), &iter, path))
				gtk_list_store_remove(win->lsFiles, &iter);
			gtk_tree_path_free(path);
		}
		break; }
	case GDK_BUTTON_SECONDARY: {
		GtkTreeView* view = GTK_TREE_VIEW(widget);
		GtkTreePath* path;
		if (gtk_tree_view_get_path_at_pos(view, (int)event->x, (int)event->y, &path, NULL, NULL, NULL)) {
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
		gtk_menu_popup_at_pointer(menu, (GdkEvent*)event);
	} }
	return FALSE;
}

G_MODULE_EXPORT gboolean buttonReleaseTblFiles(GtkWidget* widget, GdkEventButton* event, Window* win) {
	if (event->button == GDK_BUTTON_PRIMARY)
		dragEndTblFiles(widget, NULL, win);
	return FALSE;
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
#ifdef _WIN32
			appendTblFilesTextName(&text, &tlen, &tmax, LINE_BREAK_CHARS);
#else
			appendTblFilesTextName(&text, &tlen, &tmax, "\n");
#endif
			g_free(dirc);
			g_free(name);
		}
	}
	text[tlen] = '\0';
	gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), text, tlen);
	free(text);
}

static gboolean addClipboardFilesFinish(ClipboardFiles* cbf) {
	finishThread(cbf->win);
	autoPreview(cbf->win);
	setWidgetsSensitive(cbf->win, true);
	if (cbf->list)
		g_strfreev(cbf->uris);
	else
		g_free(cbf->text);
	free(cbf);
	return G_SOURCE_REMOVE;
}

static void* addClipboardFilesProc(ClipboardFiles* cbf) {
	Window* win = cbf->win;
	if (cbf->list)
		addFileUris(win, cbf->uris);
	else {
		Process* prc = win->proc;
		prc->id = 0;
		for (char* pos = cbf->text + strspn(cbf->text, LINE_BREAK_CHARS); *pos && win->threadCode == THREAD_POPULATE; pos += strspn(pos, LINE_BREAK_CHARS)) {
			size_t i = strcspn(pos, LINE_BREAK_CHARS);
			bool more = pos[i] != '\0';
			pos[i] = '\0';
			addFile(cbf->win, pos);
			pos += i + more;
			++prc->id;
			g_idle_add(G_SOURCE_FUNC(updateProgressBar), win);
		}
	}
	g_idle_add(G_SOURCE_FUNC(addClipboardFilesFinish), cbf);
	return NULL;
}

G_MODULE_EXPORT gboolean keyPressTblFiles(GtkWidget* widget, GdkEventKey* event, Window* win) {
	switch (event->keyval) {
	case GDK_KEY_x:
		if (event->state & GDK_CONTROL_MASK) {
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
		if (event->state & GDK_CONTROL_MASK) {
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
		if (event->state & GDK_CONTROL_MASK && win->threadCode == THREAD_NONE) {
			GtkClipboard* clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
			ClipboardFiles* cbf = malloc(sizeof(ClipboardFiles));
			cbf->win = win;
			cbf->uris = gtk_clipboard_wait_for_uris(clipboard);
			cbf->list = cbf->uris;

			Process* prc = win->proc;
			prc->forward = true;
			prc->total = 0;
			if (cbf->uris)
				for (; cbf->uris[prc->total]; ++prc->total);
			else {
				cbf->text = gtk_clipboard_wait_for_text(clipboard);
				if (!cbf->text) {
					free(cbf);
					return FALSE;
				}
				for (char* pos = cbf->text + strspn(cbf->text, LINE_BREAK_CHARS); *pos; pos += strspn(pos, LINE_BREAK_CHARS), ++prc->total) {
					size_t i = strcspn(pos, LINE_BREAK_CHARS);
					pos += i + pos[i] != '\0';
				}
			}
			startFileAppend(win);
			runThread(win, THREAD_POPULATE, (GThreadFunc)addClipboardFilesProc, G_SOURCE_FUNC(addClipboardFilesFinish), cbf);
		}
		break;
	case GDK_KEY_Delete:
		activateTblDel(NULL, win);
	}
	return FALSE;
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

static bool sortColumnInit(GtkTreeViewColumn* column, Sorting* sort, GtkTreeViewColumn* cother, Sorting* sother) {
	*sother = SORT_NONE;
	gtk_tree_view_column_set_sort_indicator(cother, false);
	if (*sort == SORT_DSC) {
		*sort = SORT_NONE;
		gtk_tree_view_column_set_sort_indicator(column, false);
		return false;
	}
	gtk_tree_view_column_set_sort_order(column, ++*sort == SORT_ASC ? GTK_SORT_ASCENDING : GTK_SORT_DESCENDING);
	gtk_tree_view_column_set_sort_indicator(column, true);
	return true;
}

G_MODULE_EXPORT void clickColumnTblFilesName(GtkTreeViewColumn* treeviewcolumn, Window* win) {
	if (sortColumnInit(treeviewcolumn, &win->nameSort, win->tblFilesDirectory, &win->directorySort)) {
		sortTreeViewColumn(win->tblFiles, win->lsFiles, FCOL_OLD_NAME, win->nameSort == SORT_ASC);
		autoPreview(win);
	}
}

G_MODULE_EXPORT void clickColumnTblFilesDirectory(GtkTreeViewColumn* treeviewcolumn, Window* win) {
	if (!sortColumnInit(treeviewcolumn, &win->directorySort, win->tblFilesName, &win->nameSort))
		return;
	GtkTreeIter it;
	GtkTreeModel* model = gtk_tree_view_get_model(win->tblFiles);
	if (!gtk_tree_model_get_iter_first(model, &it))
		return;

	size_t num = gtk_tree_model_iter_n_children(model, NULL);
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
		order[i] = keys[i].pos;
		g_free(keys[i].file);
		g_free(keys[i].dir);
	}
	gtk_list_store_reorder(win->lsFiles, order);
	free(order);
	free(keys);
	autoPreview(win);
}
#endif
