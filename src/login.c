#ifndef CONSOLE
#include "login.h"
#include <zlib.h>

#define INFLATED_SIZE 40000
#define INFLATE_INCREMENT 10000
#define WINDOW_ICON_PATH "share/sfbrename/sfbrename.png"

static char* readGzip(const char* path, size_t* olen) {
	gzFile file = gzopen(path, "rb");
	if (!file)
		return NULL;

	*olen = 0;
	size_t siz = INFLATED_SIZE;
	char* str = malloc(siz * sizeof(char));
	for (;;) {
		*olen += (size_t)gzread(file, str + *olen, (uint)(siz - *olen) * sizeof(char));
		if (*olen < siz)
			break;
		siz += INFLATE_INCREMENT;
		str = realloc(str, siz);
	}
	gzclose(file);
	return str;
}

static char* readFile(const char* path, size_t* olen) {
	FILE* file = fopen(path, "rb");
	if (!file || fseek(file, 0, SEEK_END))
		return NULL;
	*olen = ftell(file);
	if (*olen == SIZE_MAX || fseek(file, 0, SEEK_SET))
		return NULL;

	char* str = malloc(*olen * sizeof(char));
	size_t rlen = fread(str, sizeof(char), *olen, file);
	if (!rlen) {
		free(str);
		return NULL;
	}
	if (rlen < *olen) {
		*olen = rlen;
		str = realloc(str, rlen * sizeof(char));
	}
	return str;
}

GtkBuilder* loadUiFile(const char* relPath, char* path, size_t* pplen) {
	size_t relPathLen = strlen(relPath);
#ifdef _WIN32
	wchar* wpath = malloc(PATH_MAX * sizeof(wchar));
	size_t plen = GetModuleFileNameW(NULL, wpath, PATH_MAX) + 1;
	if (plen > 1 && plen <= PATH_MAX)
		plen = WideCharToMultiByte(CP_UTF8, 0, wpath, plen, path, PATH_MAX, NULL, NULL);
	free(wpath);
#else
	size_t plen = (size_t)readlink("/proc/self/exe", path, PATH_MAX);
#endif
	if (plen > 1 && plen <= PATH_MAX) {
#ifdef _WIN32
		unbackslashify(path);
#endif
		char* pos = memrchr(path, '/', --plen);
		if (pos)
			pos = memrchr(path, '/', (size_t)(pos - path - (pos != path)));

		plen = (size_t)(pos - path);
		if (pos && plen + relPathLen + 2 < PATH_MAX)
			++plen;
		else {
			strcpy(path, "../");
			plen = strlen(path);
		}
	} else {
		strcpy(path, "../");
		plen = strlen(path);
	}
	memcpy(path + plen, relPath, (relPathLen + 1) * sizeof(char));

	size_t glen;
	char* glade = readGzip(path, &glen);
	if (!glade) {
		path[plen + relPathLen - 3] = '\0';
		glade = readFile(path, &glen);
		if (!glade) {
			g_printerr("Failed to read main.glade\n");
			return NULL;
		}
	}

	GtkBuilder* builder = gtk_builder_new();
	GError* error = NULL;
	if (!gtk_builder_add_from_string(builder, glade, glen, &error)) {
		free(glade);
		g_printerr("Failed to load main.glade: %s\n", error->message);
		g_clear_error(&error);
		return NULL;
	}
	free(glade);
	*pplen = plen;
	return builder;
}

void setWindowIcon(GtkWindow* window, char* path, size_t plen) {
	if (plen + strlen(WINDOW_ICON_PATH) < PATH_MAX) {
		strcpy(path + plen, WINDOW_ICON_PATH);
		gtk_window_set_icon_from_file(window, path, NULL);
	}
}

G_MODULE_EXPORT void clicLoginOk(GtkButton* button, LoginDialog* ld) {
	gtk_dialog_response(ld->dialog, GTK_RESPONSE_OK);
}

G_MODULE_EXPORT void clicLoginCancel(GtkButton* button, LoginDialog* ld) {
	gtk_dialog_response(ld->dialog, GTK_RESPONSE_CANCEL);
}

GtkResponseType runLoginDialog(const char* name, LoginDialog* ld) {
	char path[PATH_MAX];
	size_t plen;
	GtkBuilder* builder = loadUiFile("share/sfbrename/login.glade.gz", path, &plen);
	if (!builder)
		return GTK_RESPONSE_REJECT;

	ld->dialog = GTK_DIALOG(gtk_builder_get_object(builder, "dialog"));
	ld->etUser = GTK_ENTRY(gtk_builder_get_object(builder, "etUser"));
	ld->etPassword = GTK_ENTRY(gtk_builder_get_object(builder, "etPassword"));
	ld->etDomain = GTK_ENTRY(gtk_builder_get_object(builder, "etDomain"));

	GtkLabel* lblTitle = GTK_LABEL(gtk_builder_get_object(builder, "lblTitle"));
	const char* titlePrefix = gtk_label_get_text(lblTitle);
	size_t titlePrefixLen = strlen(titlePrefix);
	size_t nameLen = strlen(name);
	char* finTitle = malloc((titlePrefixLen + nameLen + 1) * sizeof(char));
	memcpy(finTitle, titlePrefix, titlePrefixLen * sizeof(char));
	memcpy(finTitle + titlePrefixLen, name, (nameLen + 1) * sizeof(char));
	gtk_label_set_text(lblTitle, finTitle);
	free(finTitle);

	setWindowIcon(GTK_WINDOW(ld->dialog), path, plen);
	gtk_builder_connect_signals(builder, ld);
	g_object_unref(builder);
	gtk_widget_show_all(GTK_WIDGET(ld->dialog));
	return gtk_dialog_run(ld->dialog);
}
#endif
