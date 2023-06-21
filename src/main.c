#include "arguments.h"
#include "main.h"
#include "rename.h"
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <grp.h>
#include <pwd.h>
#endif

#define MAIN_GLADE_NAME "main.glade.gz"
#define WINDOW_ICON_NAME "sfbrename.png"
#define LICENSE_NAME "LICENSE.gz"
#define FILE_URI_PREFIX "file://"
#define LINE_BREAK_CHARS "\r\n"

#ifndef CONSOLE
typedef struct FileEntry {
	GtkListStore* lsFiles;
	FileInfo* info;
	size_t dlen;
	char file[];
} FileEntry;

typedef struct FileDetails {
	GtkListStore* lsFiles;
	GtkTreeIter it;
	FileInfo* info;
} FileDetails;

typedef struct ClipboardFiles {
	Window* win;
	union {
		char** uris;
		char* text;
	};
	bool list;
} ClipboardFiles;

typedef struct DragFiles {
	Window* win;
	char** uris;
	GdkDragContext* ctx;
	uint time;
} DragFiles;

typedef struct DialogFiles {
	Window* win;
	GSList* files;
} DialogFiles;

typedef struct KeyPos {
	char* key;
	size_t pos;
} KeyPos;

typedef struct KeyDirPos {
	char* file;
	char* dir;
	size_t pos;
} KeyDirPos;

static gboolean appendFile(FileEntry* entry) {
	FileInfo* info = entry->info;
	GtkTreeIter it;
	gtk_list_store_append(entry->lsFiles, &it);
	if (info) {
		gtk_list_store_set(entry->lsFiles, &it,
			FCOL_OLD_NAME, entry->file + entry->dlen,
			FCOL_NEW_NAME, entry->file + entry->dlen,
			FCOL_DIRECTORY, entry->file,
			FCOL_SIZE, info->size,
#ifndef _WIN32
			FCOL_CREATE, info->create,
#endif
			FCOL_MODIFY, info->modify,
			FCOL_ACCESS, info->access,
			FCOL_CHANGE, info->change,
			FCOL_PERMISSIONS, info->perms,
#ifndef _WIN32
			FCOL_USER, info->pwd ? info->pwd->pw_name : NULL,
			FCOL_GROUP, info->grp ? info->grp->gr_name : NULL,
#endif
			FCOL_INVALID
		);
		g_free(info->size);
		free(info);
	} else
		gtk_list_store_set(entry->lsFiles, &it, FCOL_OLD_NAME, entry->file + entry->dlen, FCOL_NEW_NAME, entry->file + entry->dlen, FCOL_DIRECTORY, entry->file, FCOL_INVALID);
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
	entry->lsFiles = win->lsFiles;
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

static gboolean processArgumentFilesFinish(Window* win) {
	setWidgetsSensitive(win, true);
	return G_SOURCE_REMOVE;
}

static void* processArgumentFilesProc(Window* win) {
	Arguments* arg = win->args;
	Process* prc = win->proc;
#ifdef _WIN32
	char buf[PATH_MAX];
#endif
	for (prc->id = 0; prc->id < arg->nFiles && win->threadCode == THREAD_POPULATE;) {
		const char* file = g_file_peek_path(arg->files[prc->id]);
		if (file) {
#ifdef _WIN32
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
		++prc->id;
		g_idle_add(G_SOURCE_FUNC(updateProgressBar), win);
	}
	g_idle_add(G_SOURCE_FUNC(processArgumentFilesFinish), win);
	return NULL;
}

static gboolean runAddDialogFinish(DialogFiles* dgf) {
	finishThread(dgf->win);
	autoPreview(dgf->win);
	setWidgetsSensitive(dgf->win, true);
	g_slist_free(dgf->files);
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

static void runAddDialog(Window* win, const char* title, GtkFileChooserAction action) {
	GtkFileChooserDialog* dialog = GTK_FILE_CHOOSER_DIALOG(gtk_file_chooser_dialog_new(title, GTK_WINDOW(win->window), action, "Cancel", GTK_RESPONSE_CANCEL, "Add", GTK_RESPONSE_ACCEPT, NULL));
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), g_get_home_dir());
	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), true);
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		DialogFiles* dgf = malloc(sizeof(DialogFiles));
		dgf->win = win;
		dgf->files = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));

		Process* prc = win->proc;
		prc->forward = true;
		prc->total = 0;
		for (GSList* it = dgf->files; it; it = it->next, ++win->proc->total);
		setWidgetsSensitive(win, false);
		runThread(win, THREAD_POPULATE, (GThreadFunc)runAddDialogProc, G_SOURCE_FUNC(runAddDialogFinish), dgf);
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
	win->sets.autoPreview = gtk_check_menu_item_get_active(checkmenuitem);
	autoPreview(win);
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
#ifndef _WIN32
			FCOL_CREATE, info->create,
#endif
			FCOL_MODIFY, info->modify,
			FCOL_ACCESS, info->access,
			FCOL_CHANGE, info->change,
			FCOL_PERMISSIONS, info->perms,
#ifndef _WIN32
			FCOL_USER, info->pwd ? info->pwd->pw_name : NULL,
			FCOL_GROUP, info->grp ? info->grp->gr_name : NULL,
#endif
			FCOL_INVALID
		);
		g_free(info->size);
		free(info);
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

static void toggleShowDetails(GtkCheckMenuItem* checkmenuitem, Window* win) {
	Settings* set = &win->sets;
	set->showDetails = gtk_check_menu_item_get_active(checkmenuitem);

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

static void activateClear(GtkMenuItem* item, Window* win) {
	gtk_list_store_clear(win->lsFiles);
}

static void activateReset(GtkMenuItem* item, Window* win) {
	Arguments* arg = win->args;
	gtk_entry_set_text(win->etExtension, arg->extensionName ? arg->extensionName : "");
	gtk_entry_set_text(win->etExtensionReplace, arg->extensionReplace ? arg->extensionReplace : "");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->cbExtensionCi), arg->extensionCi);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->cbExtensionRegex), arg->extensionRegex);
	gtk_spin_button_set_value(win->sbExtensionElements, (double)arg->extensionElements);
	gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbExtensionMode), arg->extensionMode);
	gtk_entry_set_text(win->etRename, arg->rename ? arg->rename : "");
	gtk_entry_set_text(win->etReplace, arg->replace ? arg->replace : "");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->cbReplaceCi), arg->replaceCi);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->cbReplaceRegex), arg->replaceRegex);
	gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbRenameMode), arg->renameMode);
	gtk_spin_button_set_value(win->sbRemoveFrom, (double)arg->removeFrom);
	gtk_spin_button_set_value(win->sbRemoveTo, (double)arg->removeTo);
	gtk_spin_button_set_value(win->sbRemoveFirst, (double)arg->removeFirst);
	gtk_spin_button_set_value(win->sbRemoveLast, (double)arg->removeLast);
	gtk_entry_set_text(win->etAddInsert, arg->addInsert ? arg->addInsert : "");
	gtk_spin_button_set_value(win->sbAddAt, (double)arg->addAt);
	gtk_entry_set_text(win->etAddPrefix, arg->addPrefix ? arg->addPrefix : "");
	gtk_entry_set_text(win->etAddSuffix, arg->addSuffix ? arg->addSuffix : "");
	gtk_spin_button_set_value(win->sbNumberLocation, (double)arg->numberLocation);
	gtk_spin_button_set_value(win->sbNumberStart, (double)arg->numberStart);
	gtk_spin_button_set_value(win->sbNumberStep, (double)arg->numberStep);
	gtk_spin_button_set_value(win->sbNumberBase, (double)arg->numberBase);
	gtk_spin_button_set_value(win->sbNumberPadding, (double)arg->numberPadding);
	gtk_entry_set_text(win->etNumberPadding, arg->numberPadStr ? arg->numberPadStr : "");
	gtk_entry_set_text(win->etNumberPrefix, arg->numberPrefix ? arg->numberPrefix : "");
	gtk_entry_set_text(win->etNumberSuffix, arg->numberSuffix ? arg->numberSuffix : "");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->cbNumber), arg->number);
	gtk_entry_set_text(win->etDateFormat, arg->dateFormat ? arg->dateFormat : DEFAULT_DATE_FORMAT);
	gtk_spin_button_set_value(win->sbDateLocation, (double)arg->dateLocation);
	gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbDateMode), arg->dateMode);
	gtk_entry_set_text(win->etDestination, arg->destination ? arg->destination : "");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->cbDestinationForward), !arg->backwards);
	gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbDestinationMode), arg->destinationMode);
	win->sets.autoPreview = !arg->noAutoPreview;
	win->sets.showDetails = !arg->noShowDetails;
}

static void activateAbout(GtkMenuItem* item, Window* win) {
	Settings* set = &win->sets;
	strcpy(set->rscPath + set->rlen, WINDOW_ICON_NAME);
	GdkPixbuf* icon = gdk_pixbuf_new_from_file(set->rscPath, NULL);
	char* license = loadTextAsset(set, LICENSE_NAME, NULL);
	const char* authors[] = { "karwler", NULL };
	gtk_show_about_dialog(GTK_WINDOW(win->window),
		"authors", authors,
		"comments", "For the date format field see the documentation for strftime",
		"license", license,
		"license-type", license ? GTK_LICENSE_CUSTOM : GTK_LICENSE_UNKNOWN,
		"logo", icon,
		"program-name", "sfbrename",
		"version", "1.1.0",
		"website", "https://github.com/karwler/sfbrename",
		"website-label", "simple fucking bulk rename",
		NULL
	);
	free(license);
}

G_MODULE_EXPORT void clickOptions(GtkButton* button, Window* win) {
	GtkCheckMenuItem* miPreview = GTK_CHECK_MENU_ITEM(gtk_check_menu_item_new_with_label("Auto Preview"));
	gtk_check_menu_item_set_active(miPreview, win->sets.autoPreview);
	g_signal_connect(miPreview, "toggled", G_CALLBACK(toggleAutoPreview), win);

	GtkCheckMenuItem* miDetails = GTK_CHECK_MENU_ITEM(gtk_check_menu_item_new_with_label("Show Details"));
	gtk_check_menu_item_set_active(miDetails, win->sets.showDetails);
	g_signal_connect(miDetails, "toggled", G_CALLBACK(toggleShowDetails), win);

	GtkMenuItem* miClear = GTK_MENU_ITEM(gtk_menu_item_new_with_label("Clear"));
	g_signal_connect(miClear, "activate", G_CALLBACK(activateClear), win);

	GtkMenuItem* miReset = GTK_MENU_ITEM(gtk_menu_item_new_with_label("Reset"));
	g_signal_connect(miReset, "activate", G_CALLBACK(activateReset), win);

	GtkMenuItem* miAbout = GTK_MENU_ITEM(gtk_menu_item_new_with_label("About"));
	g_signal_connect(miAbout, "activate", G_CALLBACK(activateAbout), win);

	GtkMenu* menu = GTK_MENU(gtk_menu_new());
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), GTK_WIDGET(miPreview));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), GTK_WIDGET(miDetails));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), GTK_WIDGET(miClear));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), GTK_WIDGET(miReset));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), GTK_WIDGET(miAbout));
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
		setWidgetsSensitive(win, false);
		runThread(win, THREAD_POPULATE, (GThreadFunc)dropTblFilesProc, G_SOURCE_FUNC(dropTblFilesFinish), dgf);
	} else
		free(dgf);
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
		if (event->key.state & GDK_CONTROL_MASK && win->threadCode == THREAD_NONE) {
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
					return false;
				}
				for (char* pos = cbf->text + strspn(cbf->text, LINE_BREAK_CHARS); *pos; pos += strspn(pos, LINE_BREAK_CHARS), ++prc->total) {
					size_t i = strcspn(pos, LINE_BREAK_CHARS);
					pos += i + pos[i] != '\0';
				}
			}
			setWidgetsSensitive(win, false);
			runThread(win, THREAD_POPULATE, (GThreadFunc)addClipboardFilesProc, G_SOURCE_FUNC(addClipboardFilesFinish), cbf);
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
	return gtk_tree_model_get_iter_first(*model, it) ? gtk_tree_model_iter_n_children(*model, NULL) : 0;
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
		order[i] = keys[i].pos;
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
		order[i] = keys[i].pos;
		g_free(keys[i].file);
		g_free(keys[i].dir);
	}
	sortColumnFinish(win, order, keys);
}

G_MODULE_EXPORT void valueChangeGeneric(GtkWidget* widget, Window* win) {
	autoPreview(win);
}

static void validateEtFilename(GtkEntry* entry) {
	char* str = validateFilename(gtk_entry_get_text(entry));
	if (str) {
		gtk_entry_set_text(entry, str);
		g_free(str);
	}
}

G_MODULE_EXPORT void valueChangeEtGeneric(GtkEntry* entry, Window* win) {
	validateEtFilename(entry);
	autoPreview(win);
}

G_MODULE_EXPORT void valueChangeEtExtension(GtkEntry* entry, Window* win) {
	RenameMode mode = gtk_combo_box_get_active(GTK_COMBO_BOX(win->cmbExtensionMode));
	if (mode != RENAME_RENAME && mode != RENAME_REPLACE)
		gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbExtensionMode), RENAME_RENAME);
	validateEtFilename(entry);
	autoPreview(win);
}

G_MODULE_EXPORT void valueChangeEtExtensionReplace(GtkEntry* entry, Window* win) {
	gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbExtensionMode), RENAME_REPLACE);
	validateEtFilename(entry);
	autoPreview(win);
}

G_MODULE_EXPORT void toggleCbExtensionReplace(GtkToggleButton* toggleButton, Window* win) {
	gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbExtensionMode), RENAME_REPLACE);
	autoPreview(win);
}

G_MODULE_EXPORT void valueChangeEtRename(GtkEntry* entry, Window* win) {
	RenameMode mode = gtk_combo_box_get_active(GTK_COMBO_BOX(win->cmbRenameMode));
	if (mode != RENAME_RENAME && mode != RENAME_REPLACE)
		gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbRenameMode), RENAME_RENAME);
	validateEtFilename(entry);
	autoPreview(win);
}

G_MODULE_EXPORT void valueChangeEtReplace(GtkEntry* entry, Window* win) {
	gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbRenameMode), RENAME_REPLACE);
	validateEtFilename(entry);
	autoPreview(win);
}

G_MODULE_EXPORT void toggleCbReplace(GtkToggleButton* toggleButton, Window* win) {
	gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbRenameMode), RENAME_REPLACE);
	autoPreview(win);
}

G_MODULE_EXPORT void valueChangeSbNumber(GtkSpinButton* spinButton, Window* win) {
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->cbNumber), true);
	autoPreview(win);
}

G_MODULE_EXPORT void valueChangeEtNumber(GtkEntry* entry, Window* win) {
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->cbNumber), true);
	validateEtFilename(entry);
	autoPreview(win);
}

G_MODULE_EXPORT void valueChangeEtDestination(GtkEntry* entry, Window* win) {
	if (gtk_combo_box_get_active(GTK_COMBO_BOX(win->cmbDestinationMode)) == DESTINATION_IN_PLACE)
		gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbDestinationMode), DESTINATION_MOVE);
	autoPreview(win);
}

static void valueChangeDate(Window* win) {
	if (gtk_combo_box_get_active(GTK_COMBO_BOX(win->cmbDateMode)) == DATE_NONE)
		gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbDateMode), DATE_MODIFY);
	autoPreview(win);
}

G_MODULE_EXPORT void valueChangeEtDate(GtkEntry* entry, Window* win) {
	validateEtFilename(entry);
	valueChangeDate(win);
}

G_MODULE_EXPORT void valueChangeSbDate(GtkSpinButton* spinButton, Window* win) {
	valueChangeDate(win);
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

G_MODULE_EXPORT void clickRename(GtkButton* button, Window* win) {
	if (!win->thread)
		windowRename(win);
	else
		win->threadCode = THREAD_ABORT;
}

G_MODULE_EXPORT gboolean closeWindow(GtkApplicationWindow* window, GdkEvent* event, Window* win) {
	if (win->thread) {
		if (showMessage(win, MESSAGE_QUESTION, BUTTONS_YES_NO, "A process is still running.\nDo you want to abort it?") == RESPONSE_NO)
			return true;
		win->threadCode = THREAD_ABORT;
		g_thread_join(win->thread);
	}

	Settings* set = &win->sets;
	gtk_window_get_size(GTK_WINDOW(window), &set->width, &set->height);
	set->maximized = gtk_window_is_maximized(GTK_WINDOW(window));
	return false;
}

G_MODULE_EXPORT void destroyWindow(GtkApplicationWindow* window, Window* win) {
	if (win->threadCode != THREAD_NONE)
		while (gtk_events_pending())
			gtk_main_iteration_do(true);
}

void autoPreview(Window* win) {
	if (win->sets.autoPreview) {
		win->dryAuto = true;
		clickPreview(NULL, win);
		win->dryAuto = false;
	}
}

static void setLabelWeight(GtkLabel* label, PangoWeight weight) {
	PangoAttrList* attrs = gtk_label_get_attributes(label);
	if (!attrs)
		attrs = pango_attr_list_new();
	pango_attr_list_change(attrs, pango_attr_weight_new(weight));
	gtk_label_set_attributes(label, attrs);
	pango_attr_list_unref(attrs);
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
	arg->nFiles = nFiles;
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

	Settings* set = &win->sets;
	size_t glen;
	char* glade = loadTextAsset(set, MAIN_GLADE_NAME, &glen);
	if (!glade)
		return;

	GtkBuilder* builder = gtk_builder_new();
	GError* error = NULL;
	bool ok = gtk_builder_add_from_string(builder, glade, glen, &error);
	free(glade);
	if (!ok) {
		g_printerr("Failed to load main.glade: %s\n", error->message);
		g_clear_error(&error);
		return;
	}
	win->window = GTK_APPLICATION_WINDOW(gtk_builder_get_object(builder, "window"));
	win->btAddFiles = GTK_BUTTON(gtk_builder_get_object(builder, "btAddFiles"));
	win->btAddFolders = GTK_BUTTON(gtk_builder_get_object(builder, "btAddFolders"));
	win->btOptions = GTK_BUTTON(gtk_builder_get_object(builder, "btOptions"));
	win->tblFiles = GTK_TREE_VIEW(gtk_builder_get_object(builder, "tblFiles"));
	win->lsFiles = GTK_LIST_STORE(gtk_builder_get_object(builder, "lsFiles"));
	win->tblFilesName = GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(builder, "tblFilesName"));
	win->tblFilesDirectory = GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(builder, "tblFilesDirectory"));
	win->tblFilesSize = GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(builder, "tblFilesSize"));
	win->tblFilesCreate = GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(builder, "tblFilesCreate"));
	win->tblFilesModify = GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(builder, "tblFilesModify"));
	win->tblFilesAccess = GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(builder, "tblFilesAccess"));
	win->tblFilesChange = GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(builder, "tblFilesChange"));
	win->tblFilesPermissions = GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(builder, "tblFilesPermissions"));
	win->tblFilesUser = GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(builder, "tblFilesUser"));
	win->tblFilesGroup = GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(builder, "tblFilesGroup"));
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
	win->cmbDateMode = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "cmbDateMode"));
	win->etDateFormat = GTK_ENTRY(gtk_builder_get_object(builder, "etDateFormat"));
	win->sbDateLocation = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "sbDateLocation"));
	win->cmbDestinationMode = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "cmbDestinationMode"));
	win->etDestination = GTK_ENTRY(gtk_builder_get_object(builder, "etDestination"));
	win->btDestination = GTK_BUTTON(gtk_builder_get_object(builder, "btDestination"));
	win->btPreview = GTK_BUTTON(gtk_builder_get_object(builder, "btPreview"));
	win->btRename = GTK_BUTTON(gtk_builder_get_object(builder, "btRename"));
	win->cbDestinationForward = GTK_CHECK_BUTTON(gtk_builder_get_object(builder, "cbDestinationForward"));
	win->pbRename = GTK_PROGRESS_BAR(gtk_builder_get_object(builder, "pbRename"));

#ifdef _WIN32
	gtk_combo_box_text_remove(win->cmbDateMode, 1);
	gtk_tree_view_column_set_visible(win->tblFilesCreate, false);
	gtk_tree_view_column_set_visible(win->tblFilesUser, false);
	gtk_tree_view_column_set_visible(win->tblFilesGroup, false);
#endif
	setLabelWeight(GTK_LABEL(gtk_bin_get_child(GTK_BIN(win->cbNumber))), PANGO_WEIGHT_BOLD);
	gtk_widget_set_sensitive(GTK_WIDGET(win->btRename), !arg->dry);
	dragEndTblFiles(GTK_WIDGET(win->tblFiles), NULL, win);

	activateReset(NULL, win);
	strcpy(set->rscPath + set->rlen, WINDOW_ICON_NAME);
	gtk_window_set_icon_from_file(GTK_WINDOW(win->window), set->rscPath, NULL);
	gtk_window_resize(GTK_WINDOW(win->window), set->width, set->height);
	if (set->maximized)
		gtk_window_maximize(GTK_WINDOW(win->window));

	gtk_builder_connect_signals(builder, win);
	g_object_unref(builder);
	gtk_application_add_window(app, GTK_WINDOW(win->window));
	gtk_widget_show_all(GTK_WIDGET(win->window));
	if (!arg->noGui && arg->files) {
		win->proc->forward = true;
		win->proc->total = arg->nFiles;
		setWidgetsSensitive(win, false);
		runThread(win, THREAD_POPULATE, (GThreadFunc)processArgumentFilesProc, G_SOURCE_FUNC(processArgumentFilesFinish), win);
	}
}

static void initWindowOpen(GtkApplication* app, GFile** files, int nFiles, const char* hint, Window* win) {
	Arguments* arg = win->args;
	arg->files = files;
	arg->nFiles = nFiles;
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
#endif
		.args = malloc(sizeof(Arguments))
	};
	Process* prc = win.proc;
	memset(prc, 0, sizeof(Process));
#ifndef CONSOLE
	loadSettings(&win.sets);
#endif

	Arguments* arg = win.args;
	g_signal_connect(win.app, "activate", G_CALLBACK(initWindow), &win);
	g_signal_connect(win.app, "open", G_CALLBACK(initWindowOpen), &win);
	initCommandLineArguments(G_APPLICATION(win.app), arg, argc, argv);
	int rc = g_application_run(G_APPLICATION(win.app), argc, argv);
	g_object_unref(win.app);
#ifndef CONSOLE
	saveSettings(&win.sets);
#endif

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
	g_free(arg->dateFormat);
	g_free(arg->destination);
#ifndef CONSOLE
	free(win.sets.rscPath);
#endif
	free(arg);
	free(prc);
	return rc;
}
