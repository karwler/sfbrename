#ifndef CONSOLE
#include "arguments.h"
#include "rename.h"
#include "table.h"
#include "templates.h"
#include "window.h"
#include <sys/stat.h>

#define MAIN_UI_NAME "main.ui.gz"
#define WINDOW_ICON_NAME "sfbrename.png"
#define LICENSE_NAME "LICENSE.gz"

void setWidgetsSensitive(Window* win, bool sensitive) {
	Process* prc = win->proc;
	gtk_widget_set_sensitive(GTK_WIDGET(win->btAddFiles), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->btAddFolders), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->btOptions), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->tblFiles), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->cmbExtensionMode), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->etExtension), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->etExtensionReplace), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->cbExtensionCi), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->cbExtensionRegex), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->sbExtensionElements), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->cmbRenameMode), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->etRename), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->etReplace), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->cbReplaceCi), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->cbReplaceRegex), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->sbRemoveFrom), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->sbRemoveTo), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->sbRemoveFirst), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->sbRemoveLast), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->etAddInsert), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->sbAddAt), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->etAddPrefix), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->etAddSuffix), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->cbNumber), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->sbNumberLocation), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->sbNumberBase), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->cbNumberUpper), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->sbNumberStart), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->sbNumberStep), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->sbNumberPadding), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->etNumberPadding), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->etNumberPrefix), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->etNumberSuffix), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->cmbDateMode), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->etDateFormat), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->sbDateLocation), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->cmbDestinationMode), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->etDestination), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->btDestination), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->btPreview), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->cbDestinationForward), sensitive);
	if (sensitive)
		gtk_button_set_label(win->btRename, "Rename");
	else {
		gtk_button_set_label(win->btRename, "Abort");
		setProgressBar(win->pbRename, 0, prc->total, true);
	}
}

static void runAddDialog(Window* win, const char* title, GtkFileChooserAction action) {
	GtkFileChooserDialog* dialog = GTK_FILE_CHOOSER_DIALOG(gtk_file_chooser_dialog_new(title, GTK_WINDOW(win->window), action, "Cancel", GTK_RESPONSE_CANCEL, "Add", GTK_RESPONSE_ACCEPT, NULL));
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), g_get_home_dir());
	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), true);
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
		addFilesFromDialog(win, gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog)));
	gtk_widget_destroy(GTK_WIDGET(dialog));
}

G_MODULE_EXPORT void clickAddFiles(GtkButton* button, Window* win) {
	runAddDialog(win, "Add Files", GTK_FILE_CHOOSER_ACTION_OPEN);
}

G_MODULE_EXPORT void clickAddFolders(GtkButton* button, Window* win) {
	runAddDialog(win, "Add Folders", GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
}

static void resetNameParameters(Window* win, const Arguments* arg) {
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
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->cbNumberUpper), !arg->numberLower);
	gtk_spin_button_set_value(win->sbNumberPadding, (double)arg->numberPadding);
	gtk_entry_set_text(win->etNumberPadding, arg->numberPadStr ? arg->numberPadStr : "");
	gtk_entry_set_text(win->etNumberPrefix, arg->numberPrefix ? arg->numberPrefix : "");
	gtk_entry_set_text(win->etNumberSuffix, arg->numberSuffix ? arg->numberSuffix : "");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->cbNumber), arg->number);
	gtk_entry_set_text(win->etDateFormat, arg->dateFormat ? arg->dateFormat : DEFAULT_DATE_FORMAT);
	gtk_spin_button_set_value(win->sbDateLocation, (double)arg->dateLocation);
	gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbDateMode), arg->dateMode);
}

static bool resetParameters(Window* win, Arguments* arg) {
	if (arg) {
		Settings* set = &win->sets;
		bool tmp = set->autoPreview;
		set->autoPreview = false;
		resetNameParameters(win, arg);
		freeArguments(arg);
		set->autoPreview = tmp;
		autoPreview(win);
	}
	return arg;
}

static void activateTemplates(GtkMenuItem* item, Window* win) {
	resetParameters(win, openTemplatesDialog(win));
}

static void activateRecentTemplate(GtkMenuItem* item, Window* win) {
	Settings* set = &win->sets;
	char* error;
	const char* name = gtk_menu_item_get_label(item);
	if (!resetParameters(win, loadTemplateFile(&win->sets, name, &error))) {
		if (showMessage(win, MESSAGE_ERROR, BUTTONS_YES_NO, "%s\nDo you want to remove this entry?", error) == RESPONSE_YES)
			for (uint i = 0;; ++i)
				if (!strcmp(set->templates[i], name)) {
					free(set->templates[i]);
					uint j;
					for (j = i + 1; j < MAX_RECENT_TEMPLATES && set->templates[j]; ++j);
					memmove(set->templates + i, set->templates + 1, (j - i - 1) * sizeof(char));
					set->templates[j - 1] = NULL;
					break;
				}
		g_free(error);
	}
}

static void toggleAutoPreview(GtkCheckMenuItem* checkmenuitem, Window* win) {
	win->sets.autoPreview = gtk_check_menu_item_get_active(checkmenuitem);
	autoPreview(win);
}

static void toggleShowDetails(GtkCheckMenuItem* checkmenuitem, Window* win) {
	win->sets.showDetails = gtk_check_menu_item_get_active(checkmenuitem);
	setDetailsVisible(win);
}

void activateClear(GtkMenuItem* item, Window* win) {
	gtk_list_store_clear(win->lsFiles);
}

static void resetAllParameters(Window* win) {
	const Arguments* arg = win->args;
	Settings* set = &win->sets;
	bool tmp = set->autoPreview;
	set->autoPreview = false;
	resetNameParameters(win, arg);
	gtk_entry_set_text(win->etDestination, arg->destination ? arg->destination : "");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->cbDestinationForward), !arg->backwards);
	gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbDestinationMode), arg->destinationMode);
	set->autoPreview = tmp;
}

static void activateReset(GtkMenuItem* item, Window* win) {
	resetAllParameters(win);
	autoPreview(win);
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
	Settings* set = &win->sets;
	GtkMenu* templates = GTK_MENU(gtk_menu_new());
	for (uint i = 0; i < MAX_RECENT_TEMPLATES && set->templates[i]; ++i) {
		GtkMenuItem* item = GTK_MENU_ITEM(gtk_menu_item_new_with_label(set->templates[i]));
		g_signal_connect(item, "activate", G_CALLBACK(activateRecentTemplate), win);
		gtk_menu_shell_append(GTK_MENU_SHELL(templates), GTK_WIDGET(item));
	}

	GtkMenuItem* miTemplates = GTK_MENU_ITEM(gtk_menu_item_new_with_label("Templates"));
	g_signal_connect(miTemplates, "activate", G_CALLBACK(activateTemplates), win);

	GtkMenuItem* miRecent = GTK_MENU_ITEM(gtk_menu_item_new_with_label("Recent Templates"));
	gtk_menu_item_set_submenu(miRecent, GTK_WIDGET(templates));

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
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), GTK_WIDGET(miTemplates));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), GTK_WIDGET(miRecent));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
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

G_MODULE_EXPORT void valueChangeNumberGeneric(GtkWidget* widget, Window* win) {
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->cbNumber), true);
	autoPreview(win);
}

G_MODULE_EXPORT void textChangeSbNumber(GtkSpinButton* spinButton, Window* win) {
	char* str = sanitizeNumber(gtk_entry_get_text(GTK_ENTRY(spinButton)), gtk_spin_button_get_value_as_int(win->sbNumberBase));
	if (str) {
		gtk_entry_set_text(GTK_ENTRY(spinButton), str);
		g_free(str);
	}
}

G_MODULE_EXPORT int inputSbNumber(GtkSpinButton* spinButton, double* newValue, Window* win) {
	*newValue = strToLlong(gtk_entry_get_text(GTK_ENTRY(spinButton)), gtk_spin_button_get_value_as_int(win->sbNumberBase));
	return TRUE;
}

G_MODULE_EXPORT gboolean outputSbNumber(GtkSpinButton* spinButton, Window* win) {
	char buf[MAX_DIGITS_I64B + 2];
	llongToStr(buf,
		(llong)gtk_adjustment_get_value(gtk_spin_button_get_adjustment(spinButton)),
		gtk_spin_button_get_value_as_int(win->sbNumberBase),
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->cbNumberUpper))
	);
	if (strcmp(buf, gtk_entry_get_text(GTK_ENTRY(spinButton))))
		gtk_entry_set_text(GTK_ENTRY(spinButton), buf);
	return TRUE;
}

G_MODULE_EXPORT void valueChangeNumberBase(GtkWidget* widget, Window* win) {
	outputSbNumber(win->sbNumberStart, win);
	outputSbNumber(win->sbNumberStep, win);
	valueChangeNumberGeneric(widget, win);
}

G_MODULE_EXPORT void valueChangeEtNumber(GtkEntry* entry, Window* win) {
	validateEtFilename(entry);
	valueChangeNumberGeneric(GTK_WIDGET(entry), win);
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
		windowPreview(win);
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

Window* openWindow(GtkApplication* app, const Arguments* arg, Process* prc, GFile** files, size_t nFiles) {
	const char* err = gtk_check_version(3, 10, 0);
	if (err) {
		g_printerr("%s\n", err);
		g_application_quit(G_APPLICATION(app));
		return NULL;
	}

	Window* win = malloc(sizeof(Window));
	memset(win, 0, sizeof(Window));
	win->proc = prc;
	win->args = arg;
	Settings* set = &win->sets;
	loadSettings(set);
	GtkBuilder* builder = loadUi(set, MAIN_UI_NAME);
	if (!builder) {
		free(win);
		return NULL;
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
	win->cbNumberUpper = GTK_CHECK_BUTTON(gtk_builder_get_object(builder, "cbNumberUpper"));
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

	setLabelWeight(GTK_LABEL(gtk_bin_get_child(GTK_BIN(win->cbNumber))), PANGO_WEIGHT_BOLD);
	gtk_widget_set_sensitive(GTK_WIDGET(win->btRename), !arg->dry);
	dragEndTblFiles(GTK_WIDGET(win->tblFiles), NULL, win);
	resetAllParameters(win);
	outputSbNumber(win->sbNumberStart, win);
	outputSbNumber(win->sbNumberStep, win);

	strcpy(set->rscPath + set->rlen, WINDOW_ICON_NAME);
	gtk_window_set_icon_from_file(GTK_WINDOW(win->window), set->rscPath, NULL);
	gtk_window_resize(GTK_WINDOW(win->window), set->width, set->height);
	if (set->maximized)
		gtk_window_maximize(GTK_WINDOW(win->window));
	gtk_builder_connect_signals(builder, win);
	g_object_unref(builder);
	gtk_application_add_window(app, GTK_WINDOW(win->window));
	gtk_widget_show_all(GTK_WIDGET(win->window));
	if (files)
		addFilesFromArguments(win, files, nFiles);
	return win;
}

void freeWindow(Window* win) {
	if (win) {
		saveSettings(&win->sets);
		freeSettings(&win->sets);
		free(win);
	}
}
#endif
