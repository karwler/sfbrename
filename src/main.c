#include "arguments.h"
#include "rename.h"
#include "main.h"
#include <sys/stat.h>
#include <zlib.h>

#define MAIN_GLADE_PATH "share/sfbrename/main.glade.gz"
#ifdef APPIMAGE
#define WINDOW_ICON_PATH "sfbrename.png"
#else
#define WINDOW_ICON_PATH "share/sfbrename/sfbrename.png"
#endif
#define DEFAULT_PRINT_BUFFER_SIZE 1024
#define INFLATED_SIZE 40000
#define INFLATE_INCREMENT 10000

static void autoPreview(Window* win);

static uint8* readGzip(const char* path, size_t* olen) {
	gzFile file = gzopen(path, "rb");
	if (!file)
		return NULL;

	*olen = 0;
	size_t siz = INFLATED_SIZE;
	uint8* str = malloc(siz);
	for (;;) {
		*olen += gzread(file, str + *olen, siz - *olen);
		if (*olen < siz)
			break;
		siz += INFLATE_INCREMENT;
		str = realloc(str, siz);
	}
	gzclose(file);
	return str;
}

void addFile(Window* win, const char* file) {
	if (!g_utf8_validate(file, -1, NULL)) {
		g_printerr("invalid UTF-8 input '%s'\n", file);
		return;
	}
	char path[PATH_MAX];
	if (!realpath(file, path))
		strcpy(path, file);

	struct stat ps;
	if (lstat(path, &ps) || S_ISDIR(ps.st_mode) || S_ISLNK(ps.st_mode)) {
		g_printerr("file '%s' is invalid\n", path);
		return;
	}

	size_t plen = strlen(path);
	const char* sep = memrchr(path, '/', plen * sizeof(char));
	sep = sep ? sep + 1 : path;
	size_t dlen = sep - path;
	size_t nlen = plen - dlen;
	if (dlen >= PATH_MAX) {
		g_printerr("directory of '%s' is too long\n", path);
		return;
	}
	if (!dlen && path[0] == '/')
		++dlen;
	if (nlen >= FILENAME_MAX) {
		g_printerr("'%s' is too long\n", sep + 1);
		return;
	}
	char name[FILENAME_MAX];
	char dirc[PATH_MAX];
	memcpy(name, sep, (nlen + 1) * sizeof(char));
	memcpy(dirc, path, dlen * sizeof(char));
	dirc[dlen] = '\0';

	GtkTreeIter it;
	gtk_list_store_append(win->lsFiles, &it);
	gtk_list_store_set(win->lsFiles, &it, FCOL_OLD_NAME, name, FCOL_NEW_NAME, name, FCOL_DIRECTORY, dirc, FCOL_INVALID);
}

int showMessageBox(Window* win, GtkMessageType type, GtkButtonsType buttons, const char* format, ...) {
	va_list args;
	va_start(args, format);
	char* str = malloc(DEFAULT_PRINT_BUFFER_SIZE * sizeof(char));
	int len = vsnprintf(str, DEFAULT_PRINT_BUFFER_SIZE, format, args);
	if (len >= DEFAULT_PRINT_BUFFER_SIZE) {
		str = realloc(str, (len + 1) * sizeof(char));
		len = vsnprintf(str, len + 1, format, args);
	}
	va_end(args);
	if (len < 0) {
		g_printerr("failed to create message box\n");
		free(str);
		return 0;
	}

	int rc;
	if (win) {
		GtkMessageDialog* dialog = GTK_MESSAGE_DIALOG(gtk_message_dialog_new(win->window, GTK_DIALOG_DESTROY_WITH_PARENT, type, buttons, "%s", str));
		rc = gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(GTK_WIDGET(dialog));
	} else {
		if (type == GTK_MESSAGE_INFO || type == GTK_MESSAGE_QUESTION || type == GTK_MESSAGE_OTHER)
			g_print("%s\n", str);
		else
			g_printerr("%s\n", str);
		rc = 0;
	}
	free(str);
	return rc;
}

G_MODULE_EXPORT void activateOpen(GtkMenuItem* menuitem, Window* win) {
	GtkFileChooserDialog* dialog = GTK_FILE_CHOOSER_DIALOG(gtk_file_chooser_dialog_new("Open File", win->window, GTK_FILE_CHOOSER_ACTION_OPEN, "Cancel", GTK_RESPONSE_CANCEL, "Open", GTK_RESPONSE_ACCEPT, NULL));
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), g_get_home_dir());
	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), true);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		GSList* files = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));
		for (GSList* it = files; it; it = it->next)
			addFile(win, it->data);
		g_slist_free(files);
	}
	gtk_widget_destroy(GTK_WIDGET(dialog));
	autoPreview(win);
}

G_MODULE_EXPORT void activateQuit(GtkMenuItem* menuitem, Window* win) {
	g_application_quit(G_APPLICATION(win->app));
}

G_MODULE_EXPORT void activateClear(GtkMenuItem* menuitem, Window* win) {
	gtk_list_store_clear(win->lsFiles);
}

G_MODULE_EXPORT void activateAutoPreview(GtkMenuItem* menuitem, Window* win) {
	autoPreview(win);
}

G_MODULE_EXPORT void dropTblFiles(GtkWidget* widget, GdkDragContext* context, int x, int y, GtkSelectionData* data, uint info, uint time, Window* win) {
	char** uris = gtk_selection_data_get_uris(data);
	if (uris) {
		for (int i = 0; uris[i]; ++i)
			if (!strncmp(uris[i], "file://", 7))
				addFile(win, uris[i] + 7);
		g_strfreev(uris);
		autoPreview(win);
	}
}

G_MODULE_EXPORT gboolean keyPressTblFiles(GtkWidget* widget, GdkEvent* event, Window* win) {
	if (event->key.keyval != GDK_KEY_Delete)
		return false;

	GtkTreeView* view = GTK_TREE_VIEW(widget);
	GtkTreeSelection* selc = gtk_tree_view_get_selection(view);
	GtkTreeModel* model;
	GList* rows = gtk_tree_selection_get_selected_rows(selc, &model);
	uint rnum = g_list_length(rows);
	if (rnum) {
		GtkTreeRowReference** refs = malloc(rnum * sizeof(GtkTreeRowReference*));
		uint cnt = 0;
		for (GList* it = rows; it; it = it->next) {
			refs[cnt] = gtk_tree_row_reference_new(model, it->data);
			if (refs[cnt])
				++cnt;
		}

		for (uint i = 0; i < cnt; ++i) {
			GtkTreeIter iter;
			if (gtk_tree_model_get_iter(model, &iter, gtk_tree_row_reference_get_path(refs[i])))
				gtk_list_store_remove(win->lsFiles, &iter);
			gtk_tree_row_reference_free(refs[i]);
		}
		free(refs);
		autoPreview(win);
	}
	g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);
	return false;
}

G_MODULE_EXPORT void valueChangeEtGeneric(GtkEditable* editable, Window* win) {
	autoPreview(win);
}

G_MODULE_EXPORT void valueChangeCmbGeneric(GtkComboBox* widget, Window* win) {
	autoPreview(win);
}

G_MODULE_EXPORT void valueChangeSbGeneric(GtkSpinButton* spinButton, Window* win) {
	autoPreview(win);
}

G_MODULE_EXPORT void toggleCbGeneric(GtkToggleButton* togglebutton, Window* win) {
	autoPreview(win);
}

G_MODULE_EXPORT void valueChangeEtExtension(GtkEditable* editable, Window* win) {
	RenameMode mode = gtk_combo_box_get_active(GTK_COMBO_BOX(win->cmbExtensionMode));
	if (mode != RENAME_RENAME && mode != RENAME_REPLACE)
		gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbExtensionMode), RENAME_RENAME);
	autoPreview(win);
}

G_MODULE_EXPORT void valueChangeEtExtensionReplace(GtkEditable* editable, Window* win) {
	gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbExtensionMode), RENAME_REPLACE);
	autoPreview(win);
}

G_MODULE_EXPORT void toggleCbExtensionReplace(GtkToggleButton* togglebutton, Window* win) {
	gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbExtensionMode), RENAME_REPLACE);
	autoPreview(win);
}

G_MODULE_EXPORT void valueChangeEtRename(GtkEditable* editable, Window* win) {
	RenameMode mode = gtk_combo_box_get_active(GTK_COMBO_BOX(win->cmbRenameMode));
	if (mode != RENAME_RENAME && mode != RENAME_REPLACE)
		gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbRenameMode), RENAME_RENAME);
	autoPreview(win);
}

G_MODULE_EXPORT void valueChangeEtReplace(GtkEditable* editable, Window* win) {
	gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbRenameMode), RENAME_REPLACE);
	autoPreview(win);
}

G_MODULE_EXPORT void toggleCbReplace(GtkToggleButton* togglebutton, Window* win) {
	gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbRenameMode), RENAME_REPLACE);
	autoPreview(win);
}

G_MODULE_EXPORT void valueChangeSbRemoveFrom(GtkSpinButton* spinButton, Window* win) {
	int from = gtk_spin_button_get_value_as_int(spinButton);
	int to = gtk_spin_button_get_value_as_int(win->sbRemoveTo);
	if (from > to)
		gtk_spin_button_set_value(win->sbRemoveTo, from);
	autoPreview(win);
}

G_MODULE_EXPORT void valueChangeSbRemoveTo(GtkSpinButton* spinButton, Window* win) {
	int from = gtk_spin_button_get_value_as_int(win->sbRemoveFrom);
	int to = gtk_spin_button_get_value_as_int(spinButton);
	if (to < from)
		gtk_spin_button_set_value(win->sbRemoveFrom, to);
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
	GtkFileChooserDialog* dialog = GTK_FILE_CHOOSER_DIALOG(gtk_file_chooser_dialog_new("Pick Destination", win->window, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, "Cancel", GTK_RESPONSE_CANCEL, "Open", GTK_RESPONSE_ACCEPT, NULL));

	const char* dst = gtk_entry_get_text(win->etDestination);
	struct stat ps;
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), !lstat(dst, &ps) && S_ISDIR(ps.st_mode) ? dst : getenv("HOME"));

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

G_MODULE_EXPORT void clickRename(GtkButton* button, Window* win) {
	windowRename(win);
}

G_MODULE_EXPORT void clickPreview(GtkButton* button, Window* win) {
	windowPreview(win);
}

static void autoPreview(Window* win) {
	if (gtk_check_menu_item_get_active(win->mbAutoPreview))
		clickPreview(NULL, win);
}

static void runConsole(Window* win) {
	if (!win->args->dry)
		consoleRename(win);
	else
		consolePreview(win);
}

static void initWindow(GtkApplication* app, Window* win) {
	Arguments* args = win->args;
	if (args->noGui) {
		runConsole(win);
		return;
	}

	const char* err = gtk_check_version(3, 4, 0);
	if (err) {
		g_printerr("%s\n", err);
		g_application_quit(G_APPLICATION(app));
		return;
	}

	char path[PATH_MAX];
	size_t plen = readlink("/proc/self/exe", path, PATH_MAX);
	if (plen < PATH_MAX) {
		char* pos = memrchr(path, '/', --plen);
		if (pos)
			pos = memrchr(path, '/', pos - path - (pos != path));
		plen = pos && pos - path + strlen(MAIN_GLADE_PATH) + 2 < PATH_MAX ? pos - path + 1 : 0;
	} else
		plen = 0;
	strcpy(path + plen, MAIN_GLADE_PATH);

	size_t glen;
	char* glade = (char*)readGzip(path, &glen);
	if (!glade) {
		g_printerr("Failed to read %s\n", MAIN_GLADE_PATH);
		return;
	}

	GtkBuilder* builder = gtk_builder_new();
	GError* error = NULL;
	if (!gtk_builder_add_from_string(builder, glade, glen, &error)) {
		free(glade);
		g_printerr("Failed to load %s: %s\n", MAIN_GLADE_PATH, error->message);
		g_clear_error(&error);
		return;
	}
	free(glade);
	win->window = GTK_WINDOW(gtk_builder_get_object(builder, "window"));
	win->mbAutoPreview = GTK_CHECK_MENU_ITEM(gtk_builder_get_object(builder, "mbAutoPreview"));
	win->tblFiles = GTK_TREE_VIEW(gtk_builder_get_object(builder, "tblFiles"));
	win->lsFiles = GTK_LIST_STORE(gtk_builder_get_object(builder, "lsFiles"));
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
	win->cbDestinationForward = GTK_CHECK_BUTTON(gtk_builder_get_object(builder, "cbDestinationForward"));
	GtkCheckMenuItem* mbAutoPreview = GTK_CHECK_MENU_ITEM(gtk_builder_get_object(builder, "mbAutoPreview"));

	GtkLabel* numberLabel = GTK_LABEL(gtk_bin_get_child(GTK_BIN(win->cbNumber)));
	PangoAttrList* attrs = gtk_label_get_attributes(numberLabel);
	if (!attrs)
		attrs = pango_attr_list_new();
	pango_attr_list_change(attrs, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
	gtk_label_set_attributes(numberLabel, attrs);
	pango_attr_list_unref(attrs);

	GtkTargetEntry target = { "text/uri-list", 0, 0 };
	gtk_tree_view_enable_model_drag_dest(win->tblFiles, &target, 1, GDK_ACTION_COPY | GDK_ACTION_MOVE);
	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(win->tblFiles), GTK_SELECTION_MULTIPLE);

	processArguments(win);
	if (args->extensionMode) {
		gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbExtensionMode), args->gotExtensionMode);
		g_free(args->extensionMode);
	}
	if (args->extensionName) {
		gtk_entry_set_text(win->etExtension, args->extensionName);
		g_free(args->extensionName);
	}
	if (args->extensionReplace) {
		gtk_entry_set_text(win->etExtensionReplace, args->extensionReplace);
		g_free(args->extensionReplace);
	}
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->cbExtensionCi), args->extensionCi);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->cbExtensionRegex), args->extensionRegex);
	gtk_spin_button_set_value(win->sbExtensionElements, args->extensionElements);
	if (args->renameMode) {
		gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbRenameMode), args->gotRenameMode);
		g_free(args->renameMode);
	}
	if (args->rename) {
		gtk_entry_set_text(win->etRename, args->rename);
		g_free(args->rename);
	}
	if (args->replace) {
		gtk_entry_set_text(win->etReplace, args->replace);
		g_free(args->replace);
	}
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->cbReplaceCi), args->replaceCi);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->cbReplaceRegex), args->replaceRegex);
	gtk_spin_button_set_value(win->sbRemoveFrom, args->removeFrom);
	gtk_spin_button_set_value(win->sbRemoveTo, args->removeTo);
	gtk_spin_button_set_value(win->sbRemoveFirst, args->removeFirst);
	gtk_spin_button_set_value(win->sbRemoveLast, args->removeLast);
	if (args->addInsert) {
		gtk_entry_set_text(win->etAddInsert, args->addInsert);
		g_free(args->addInsert);
	}
	gtk_spin_button_set_value(win->sbAddAt, args->addAt);
	if (args->addPrefix) {
		gtk_entry_set_text(win->etAddPrefix, args->addPrefix);
		g_free(args->addPrefix);
	}
	if (args->addSuffix) {
		gtk_entry_set_text(win->etAddSuffix, args->addSuffix);
		g_free(args->addSuffix);
	}
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->cbNumber), args->number);
	gtk_spin_button_set_value(win->sbNumberLocation, args->numberLocation);
	gtk_spin_button_set_value(win->sbNumberStart, args->numberStart);
	gtk_spin_button_set_value(win->sbNumberStep, args->numberStep);
	gtk_spin_button_set_value(win->sbNumberBase, args->numberBase);
	gtk_spin_button_set_value(win->sbNumberPadding, args->numberPadding);
	if (args->numberPadStr) {
		gtk_entry_set_text(win->etNumberPadding, args->numberPadStr);
		g_free(args->numberPadStr);
	}
	if (args->numberPrefix) {
		gtk_entry_set_text(win->etNumberPrefix, args->numberPrefix);
		g_free(args->numberPrefix);
	}
	if (args->numberSuffix) {
		gtk_entry_set_text(win->etNumberSuffix, args->numberSuffix);
		g_free(args->numberSuffix);
	}
	if (args->destinationMode) {
		gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbDestinationMode), args->gotDestinationMode);
		g_free(args->destinationMode);
	}
	if (args->destination) {
		gtk_entry_set_text(win->etDestination, args->destination);
		g_free(args->destination);
	}
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->cbDestinationForward), !args->backwards);
	gtk_check_menu_item_set_active(mbAutoPreview, !args->noAutoPreview);
	free(win->args);
	win->args = NULL;

	if (plen + strlen(WINDOW_ICON_PATH) + 1 < PATH_MAX) {
		strcpy(path + plen, WINDOW_ICON_PATH);
		gtk_window_set_icon_from_file(win->window, path, NULL);
	}

	gtk_builder_connect_signals(builder, win);
	g_object_unref(builder);
	gtk_application_add_window(app, GTK_WINDOW(win->window));
	gtk_widget_show_all(GTK_WIDGET(win->window));
}

static void initWindowOpen(GApplication* app, GFile** files, int nFiles, const char* hint, Window* win) {
	win->args->files = files;
	win->args->nFiles = nFiles;
	if (!win->args->noGui)
		initWindow(GTK_APPLICATION(app), win);
	else
		runConsole(win);
}

int main(int argc, char** argv) {
	Window win = {
		.app = gtk_application_new(NULL, G_APPLICATION_HANDLES_OPEN),
		.args = malloc(sizeof(Arguments))
	};
	g_signal_connect(win.app, "activate", G_CALLBACK(initWindow), &win);
	g_signal_connect(win.app, "open", G_CALLBACK(initWindowOpen), &win);
	initCommandLineArguments(win.app, win.args, argc, argv);
	int rc = g_application_run(G_APPLICATION(win.app), argc, argv);
	g_object_unref(win.app);
	return rc;
}
