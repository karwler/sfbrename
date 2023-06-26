#ifndef CONSOLE
#include "arguments.h"
#include "templates.h"
#include "window.h"
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>

#define TEMPLATES_UI_NAME "templates.ui.gz"
#define TEMPLATE_VERSION 0

typedef struct Templates {
	Window* win;
	Arguments* result;
	GFileMonitor* monitor;

	GtkDialog* dialog;
	GtkListStore* lsTemplates;
	GtkTreeView* tvTemplates;
	GtkButton* btSave;
	GtkButton* btLoad;
	GtkButton* btDelete;
} Templates;

#define stDlgMessageNoArg(parent, type, str) { \
	GtkMessageDialog* dialog = GTK_MESSAGE_DIALOG(gtk_message_dialog_new(parent, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, type, GTK_BUTTONS_OK, str)); \
	gtk_dialog_run(GTK_DIALOG(dialog)); \
	gtk_widget_destroy(GTK_WIDGET(dialog)); \
}

#define stDlgMessage(parent, type, format, ...) { \
	GtkMessageDialog* dialog = GTK_MESSAGE_DIALOG(gtk_message_dialog_new(parent, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, type, GTK_BUTTONS_OK, format, __VA_ARGS__)); \
	gtk_dialog_run(GTK_DIALOG(dialog)); \
	gtk_widget_destroy(GTK_WIDGET(dialog)); \
}

static void findTreeModelEntryNext(GtkTreeModel* model, GtkTreeIter* it, int* in, int mid) {
	for (; *in > mid; --*in)
		gtk_tree_model_iter_previous(model, it);
	for (; *in < mid; ++*in)
		gtk_tree_model_iter_next(model, it);
}

static int findTreeModelEntry(GtkTreeModel* model, GtkTreeIter* it, const char* name) {
	int rat = -1;
	char* str;
	int lo = 0;
	int hi = gtk_tree_model_iter_n_children(model, NULL) - 1;
	int in = lo + (hi - lo) / 2;
	gtk_tree_model_iter_nth_child(model, it, NULL, in);

	while (lo <= hi) {
		int mid = lo + (hi - lo) / 2;
		findTreeModelEntryNext(model, it, &in, mid);
		gtk_tree_model_get(model, it, 0, &str, -1);
		rat = g_utf8_collate(name, str);
		g_free(str);

		if (!rat)
			break;
		if (rat > 0)
			lo = mid + 1;
		else
			hi = mid - 1;
	}
	return rat;
}

static void insertNameAt(GtkListStore* store, GtkTreeIter* it, GtkTreeIter* pos, int rel, const char* name) {
	if (rel < 0)
		gtk_list_store_insert_before(store, it, pos);
	else
		gtk_list_store_insert_after(store, it, pos);
	gtk_list_store_set(store, it, 0, name, -1);
}

static void changeMonitor(GFileMonitor* monitor, GFile* file, GFile* otherFile, GFileMonitorEvent event, Templates* tpl) {
	GtkTreeIter oldIt, newIt;
	switch (event) {
	case G_FILE_MONITOR_EVENT_DELETED: case G_FILE_MONITOR_EVENT_MOVED_OUT: {
		char* name = g_file_get_basename(file);
		if (!findTreeModelEntry(gtk_tree_view_get_model(tpl->tvTemplates), &oldIt, name))
			gtk_list_store_remove(tpl->lsTemplates, &oldIt);
		g_free(name);
		break; }
	case G_FILE_MONITOR_EVENT_CREATED: case G_FILE_MONITOR_EVENT_MOVED_IN: {
		char* name = g_file_get_basename(file);
		int rs = findTreeModelEntry(gtk_tree_view_get_model(tpl->tvTemplates), &oldIt, name);
		if (rs)
			insertNameAt(tpl->lsTemplates, &newIt, &oldIt, rs, name);
		g_free(name);
		break; }
	case G_FILE_MONITOR_EVENT_RENAMED: {
		GtkTreeModel* model = gtk_tree_view_get_model(tpl->tvTemplates);
		char* oldName = g_file_get_basename(file);
		if (!findTreeModelEntry(model, &oldIt, oldName)) {
			char* newName = g_file_get_basename(otherFile);
			int rs = findTreeModelEntry(model, &newIt, newName);
			if (rs) {
				if (rs < 0)
					gtk_list_store_move_before(tpl->lsTemplates, &oldIt, &newIt);
				else
					gtk_list_store_move_after(tpl->lsTemplates, &oldIt, &newIt);
				gtk_list_store_set(tpl->lsTemplates, &oldIt, 0, newName, -1);
			} else
				gtk_list_store_remove(tpl->lsTemplates, &oldIt);
			g_free(newName);
		}
		g_free(oldName);
	} }
}

static void setMonitor(Templates* tpl) {
	if (!tpl->monitor) {
		GFile* dirc = g_file_new_for_path(tpl->win->sets.cfgPath);
		if (dirc) {
			tpl->monitor = g_file_monitor_directory(dirc, G_FILE_MONITOR_WATCH_MOVES, NULL, NULL);
			if (tpl->monitor)
				g_signal_connect(tpl->monitor, "changed", G_CALLBACK(changeMonitor), tpl);
			g_object_unref(dirc);
		}
	}
}

static void listTemplates(Templates* tpl) {
	DIR* dir = opendir(tpl->win->sets.cfgPath);
	if (!dir) {
		if (errno != ENOENT)
			stDlgMessage(GTK_WINDOW(tpl->dialog), GTK_MESSAGE_ERROR, "Failed to list directory: %s", strerror(errno))
		return;
	}

#ifdef _WIN32
	size_t dlen = strlen(tpl->win->sets.cfgPath);
	char* path = malloc(PATH_MAX);
	memcpy(path, tpl->win->sets.cfgPath, dlen * sizeof(char));
#endif
	struct stat ps;
	GtkTreeIter it, *pit = NULL;
	for (struct dirent* entry; (entry = readdir(dir));) {
#ifdef _WIN32
		size_t nlen = strlen(entry->d_name);
		if (dlen + nlen >= PATH_MAX)
			continue;
		memcpy(path + dlen, entry->d_name, (nlen + 1) * sizeof(char));
		if (!stat(path, &ps) && S_ISREG(ps.st_mode)) {
#else
		if (entry->d_type == DT_REG || (entry->d_type == DT_LNK && !fstatat(dirfd(dir), entry->d_name, &ps, 0) && S_ISREG(ps.st_mode))) {
#endif
			gtk_list_store_insert_after(tpl->lsTemplates, &it, pit);
			gtk_list_store_set(tpl->lsTemplates, &it, 0, entry->d_name, -1);
			pit = &it;
		}
	}
	closedir(dir);
#ifdef _WIN32
	free(path);
#endif
	sortTreeViewColumn(tpl->tvTemplates, tpl->lsTemplates, 0, true);
	setMonitor(tpl);
}

static void setMostRecentTemplate(Settings* set, const char* name) {
	uint i;
	for (i = 0; i < MAX_RECENT_TEMPLATES && set->templates[i]; ++i)
		if (!strcmp(set->templates[i], name)) {
			char* str = set->templates[i];
			memmove(set->templates + 1, set->templates, i * sizeof(char*));
			set->templates[0] = str;
			return;
		}

	if (i == MAX_RECENT_TEMPLATES)
		free(set->templates[--i]);
	memmove(set->templates + 1, set->templates, i * sizeof(char*));
	set->templates[0] = strdup(name);
}

#define defnReadInt(name, type) \
	static type read ## name(FILE* fd) { \
		type val; \
		fread(&val, sizeof(type), 1, fd); \
		return val; \
	}

defnReadInt(Uint8, uint8_t)
defnReadInt(Sint16, int16_t)
defnReadInt(Uint16, uint16_t)
defnReadInt(Uint32, uint32_t)
defnReadInt(Sint64, int64_t)

static char* readString(FILE* fd) {
	uint16_t len;
	fread(&len, sizeof(len), 1, fd);
	if (!len)
		return NULL;

	char* str = g_malloc((len + 1) * sizeof(char));
	fread(str, sizeof(char), len, fd);
	str[len] = '\0';
	return str;
}

Arguments* loadTemplateFile(Settings* set, const char* name, char** error) {
	*error = NULL;
	char* path = newStrncat(3, set->cfgPath, set->clen, TDIRC_NAME, strlen(TDIRC_NAME), name, strlen(name));
	FILE* fd = fopen(path, "rb");
	if (!fd) {
		*error = g_strdup_printf("Failed to open file: %s", strerror(errno));
		free(path);
		return NULL;
	}
	free(path);

	uint32_t version = readUint32(fd);
	if (version != TEMPLATE_VERSION) {
		*error = g_strdup_printf("Invalid file Version. Expected: %u Got: %u", TEMPLATE_VERSION, version);
		fclose(fd);
		return NULL;
	}
	Arguments* arg = malloc(sizeof(Arguments));
	memset(arg, 0, sizeof(Arguments));

	arg->extensionMode = readUint8(fd);
	arg->extensionName = readString(fd);
	arg->extensionReplace = readString(fd);
	arg->extensionCi = readUint8(fd);
	arg->extensionRegex = readUint8(fd);
	arg->extensionElements = readSint16(fd);

	arg->renameMode = readUint8(fd);
	arg->rename = readString(fd);
	arg->replace = readString(fd);
	arg->replaceCi = readUint8(fd);
	arg->replaceRegex = readUint8(fd);

	arg->removeFrom = readSint16(fd);
	arg->removeTo = readSint16(fd);
	arg->removeFirst = readUint16(fd);
	arg->removeLast = readUint16(fd);

	arg->addInsert = readString(fd);
	arg->addAt = readSint16(fd);
	arg->addPrefix = readString(fd);
	arg->addSuffix = readString(fd);

	arg->number = readUint8(fd);
	arg->numberLocation = readSint16(fd);
	arg->numberBase = readUint8(fd);
	arg->numberLower = !readUint8(fd);
	arg->numberStart = readSint64(fd);
	arg->numberStep = readSint64(fd);
	arg->numberPadding = readUint8(fd);
	arg->numberPadStr = readString(fd);
	arg->numberPrefix = readString(fd);
	arg->numberSuffix = readString(fd);

	arg->dateMode = readUint8(fd);
	arg->dateFormat = readString(fd);
	arg->dateLocation = readSint16(fd);

	if (ferror(fd)) {
		*error = g_strdup("Failed to read file");
		freeArguments(arg);
		fclose(fd);
		return NULL;
	}
	fclose(fd);
	setMostRecentTemplate(set, name);
	return arg;
}

static void loadFile(Templates* tpl, const char* name) {
	char* error;
	tpl->result = loadTemplateFile(&tpl->win->sets, name, &error);
	if (tpl->result)
		gtk_dialog_response(tpl->dialog, GTK_RESPONSE_APPLY);
	else {
		stDlgMessage(GTK_WINDOW(tpl->dialog), GTK_MESSAGE_ERROR, "%s", error)
		g_free(error);
	}
}

#define defnWriteInt(name, type) \
	static void write ## name(FILE* fd, type val) { \
		fwrite(&val, sizeof(type), 1, fd); \
	}

defnWriteInt(Uint8, uint8_t)
defnWriteInt(Sint16, int16_t)
defnWriteInt(Uint16, uint16_t)
defnWriteInt(Uint32, uint32_t)
defnWriteInt(Sint64, int64_t)

static void writeString(FILE* fd, const char* str) {
	uint16_t len = strlen(str);
	fwrite(&len, sizeof(len), 1, fd);
	fwrite(str, sizeof(char), len, fd);
}

static bool saveFile(Templates* tpl, const char* name) {
	Window* win = tpl->win;
	Settings* set = &win->sets;
	set->cfgPath[set->clen - 1] = '\0';
	mkdirCommon(set->cfgPath);
	set->cfgPath[set->clen - 1] = '/';
	mkdirCommon(set->cfgPath);
	setMonitor(tpl);

	char* path = newStrncat(2, set->cfgPath, strlen(set->cfgPath), name, strlen(name));
	FILE* fd = fopen(path, "wb");
	if (!fd) {
		stDlgMessage(GTK_WINDOW(tpl->dialog), GTK_MESSAGE_ERROR, "Failed to create file: %s", strerror(errno))
		free(path);
		return false;
	}
	free(path);

	writeUint32(fd, TEMPLATE_VERSION);

	writeUint8(fd, gtk_combo_box_get_active(GTK_COMBO_BOX(win->cmbExtensionMode)));
	writeString(fd, gtk_entry_get_text(win->etExtension));
	writeString(fd, gtk_entry_get_text(win->etExtensionReplace));
	writeUint8(fd, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->cbExtensionCi)));
	writeUint8(fd, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->cbExtensionRegex)));
	writeSint16(fd, gtk_spin_button_get_value_as_int(win->sbExtensionElements));

	writeUint8(fd, gtk_combo_box_get_active(GTK_COMBO_BOX(win->cmbRenameMode)));
	writeString(fd, gtk_entry_get_text(win->etRename));
	writeString(fd, gtk_entry_get_text(win->etReplace));
	writeUint8(fd, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->cbReplaceCi)));
	writeUint8(fd, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->cbReplaceRegex)));

	writeSint16(fd, gtk_spin_button_get_value_as_int(win->sbRemoveFrom));
	writeSint16(fd, gtk_spin_button_get_value_as_int(win->sbRemoveTo));
	writeUint16(fd, gtk_spin_button_get_value_as_int(win->sbRemoveFirst));
	writeUint16(fd, gtk_spin_button_get_value_as_int(win->sbRemoveLast));

	writeString(fd, gtk_entry_get_text(win->etAddInsert));
	writeSint16(fd, gtk_spin_button_get_value_as_int(win->sbAddAt));
	writeString(fd, gtk_entry_get_text(win->etAddPrefix));
	writeString(fd, gtk_entry_get_text(win->etAddSuffix));

	writeUint8(fd, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->cbNumber)));
	writeSint16(fd, gtk_spin_button_get_value_as_int(win->sbNumberLocation));
	writeUint8(fd, gtk_spin_button_get_value_as_int(win->sbNumberBase));
	writeUint8(fd, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->cbNumberUpper)));
	writeSint64(fd, gtk_spin_button_get_value_as_int(win->sbNumberStart));
	writeSint64(fd, gtk_spin_button_get_value_as_int(win->sbNumberStep));
	writeUint8(fd, gtk_spin_button_get_value_as_int(win->sbNumberPadding));
	writeString(fd, gtk_entry_get_text(win->etNumberPadding));
	writeString(fd, gtk_entry_get_text(win->etNumberPrefix));
	writeString(fd, gtk_entry_get_text(win->etNumberSuffix));

	writeUint8(fd, gtk_combo_box_get_active(GTK_COMBO_BOX(win->cmbDateMode)));
	writeString(fd, gtk_entry_get_text(win->etDateFormat));
	writeSint16(fd, gtk_spin_button_get_value_as_int(win->sbDateLocation));

	if (ferror(fd)) {
		stDlgMessageNoArg(GTK_WINDOW(tpl->dialog), GTK_MESSAGE_ERROR, "Failed to write file")
		fclose(fd);
		return false;
	}
	fclose(fd);
	return true;
}

G_MODULE_EXPORT void changTsTemplates(GtkTreeSelection* selection, Templates* tpl) {
	int cnt = gtk_tree_selection_count_selected_rows(selection);
	gtk_widget_set_sensitive(GTK_WIDGET(tpl->btSave), cnt == 1);
	gtk_widget_set_sensitive(GTK_WIDGET(tpl->btLoad), cnt == 1);
	gtk_widget_set_sensitive(GTK_WIDGET(tpl->btDelete), cnt >= 1);
}

static char* getNameFromPath(GtkTreeModel* model, GtkTreePath* path) {
	char* name = NULL;
	GtkTreeRowReference* ref = gtk_tree_row_reference_new(model, path);
	GtkTreeIter it;
	if (gtk_tree_model_get_iter(model, &it, gtk_tree_row_reference_get_path(ref)))
		gtk_tree_model_get(model, &it, 0, &name, -1);
	gtk_tree_row_reference_free(ref);
	return name;
}

static char* getFirstSelectedName(GtkTreeView* treeView) {
	GtkTreeModel* model;
	GList* rows = gtk_tree_selection_get_selected_rows(gtk_tree_view_get_selection(treeView), &model);
	char* name = g_list_length(rows) ? getNameFromPath(model, rows->data) : NULL;
	g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);
	return name;
}

G_MODULE_EXPORT void rowActivateTvTemplates(GtkTreeView* treeView, GtkTreePath* path, GtkTreeViewColumn* column, Templates* tpl) {
	char* name = getNameFromPath(gtk_tree_view_get_model(treeView), path);
	if (name) {
		loadFile(tpl, name);
		g_free(name);
	}
}

G_MODULE_EXPORT void clickNew(GtkButton* button, Templates* tpl) {
	GtkTreeModel* model = gtk_tree_view_get_model(tpl->tvTemplates);
	GtkTreeIter it;
	char* name = NULL;
	int rel;
	do {
		char* text = showInputText(GTK_WINDOW(tpl->dialog), "New Template", "Name", name);
		if (!text) {
			free(name);
			return;
		}

		rel = findTreeModelEntry(model, &it, text);
		if (!rel)
			showMessage(tpl->win, MESSAGE_INFO, BUTTONS_OK, "The name '%s' is already taken.", text);
		free(name);
		name = text;
	} while (!rel);

	if (saveFile(tpl, name)) {
		setMostRecentTemplate(&tpl->win->sets, name);

		GtkTreeIter new;
		insertNameAt(tpl->lsTemplates, &new, &it, rel, name);

		GtkTreeSelection* selc = gtk_tree_view_get_selection(tpl->tvTemplates);
		gtk_tree_selection_unselect_all(selc);
		gtk_tree_selection_select_iter(selc, &new);
	}
	free(name);
}

G_MODULE_EXPORT void clickSave(GtkButton* button, Templates* tpl) {
	char* name = getFirstSelectedName(tpl->tvTemplates);
	if (name) {
		saveFile(tpl, name);
		setMostRecentTemplate(&tpl->win->sets, name);
		g_free(name);
	}
}

G_MODULE_EXPORT void clickLoad(GtkButton* button, Templates* tpl) {
	char* name = getFirstSelectedName(tpl->tvTemplates);
	if (name) {
		loadFile(tpl, name);
		g_free(name);
	}
}

G_MODULE_EXPORT void clickDelete(GtkButton* button, Templates* tpl) {
	Settings* set = &tpl->win->sets;
	size_t tlen = set->clen + strlen(TDIRC_NAME);
	size_t brsv = tlen + 128;
	char* buf = malloc(brsv * sizeof(char));
	memcpy(buf, set->cfgPath, tlen * sizeof(char));

	GtkTreeModel* model;
	uint cnt;
	GtkTreeRowReference** refs = getTreeViewSelectedRowRefs(tpl->tvTemplates, &model, &cnt);
	for (uint i = 0; i < cnt; ++i) {
		GtkTreeIter iter;
		if (gtk_tree_model_get_iter(model, &iter, gtk_tree_row_reference_get_path(refs[i]))) {
			char* name;
			gtk_tree_model_get(model, &iter, 0, &name, -1);
			gtk_list_store_remove(tpl->lsTemplates, &iter);

			size_t nlen = strlen(name) + 1;
			if (tlen + nlen > brsv) {
				brsv = tlen + nlen;
				buf = realloc(buf, brsv * sizeof(char));
			}
			memcpy(buf + tlen, name, nlen * sizeof(char));
			g_free(name);
			remove(buf);
		}
		gtk_tree_row_reference_free(refs[i]);
	}
	free(refs);
	free(buf);
}

G_MODULE_EXPORT void clickClose(GtkButton* button, Templates* tpl) {
	gtk_dialog_response(tpl->dialog, GTK_RESPONSE_CLOSE);
}

G_MODULE_EXPORT gboolean closeTemplates(GtkDialog* dialog, GdkEvent* event, Templates* tpl) {
	return false;
}

Arguments* openTemplatesDialog(Window* win) {
	Settings* set = &win->sets;
	GtkBuilder* builder = loadUi(set, TEMPLATES_UI_NAME);
	if (!builder)
		return NULL;

	Templates tpl = {
		.win = win,
		.dialog = GTK_DIALOG(gtk_builder_get_object(builder, "dialog")),
		.lsTemplates = GTK_LIST_STORE(gtk_builder_get_object(builder, "lsTemplates")),
		.tvTemplates = GTK_TREE_VIEW(gtk_builder_get_object(builder, "tvTemplates")),
		.btSave = GTK_BUTTON(gtk_builder_get_object(builder, "btSave")),
		.btLoad = GTK_BUTTON(gtk_builder_get_object(builder, "btLoad")),
		.btDelete = GTK_BUTTON(gtk_builder_get_object(builder, "btDelete"))
	};
	gtk_window_set_transient_for(GTK_WINDOW(tpl.dialog), GTK_WINDOW(win->window));

	strcpy(set->cfgPath + set->clen, TDIRC_NAME);
	listTemplates(&tpl);
	gtk_builder_connect_signals(builder, &tpl);
	g_object_unref(builder);

	gtk_dialog_run(tpl.dialog);
	gtk_widget_destroy(GTK_WIDGET(tpl.dialog));
	g_object_unref(tpl.monitor);
	return tpl.result;
}
#endif
