#ifndef SETTINGS_H
#define SETTINGS_H

#ifndef CONSOLE
#include "utils.h"

#define TDIRC_NAME "templates/"
#define MAX_RECENT_TEMPLATES 16

typedef struct Settings {
	char* rscPath;
	size_t rlen;
	char* cfgPath;
	size_t clen;
	char* templates[MAX_RECENT_TEMPLATES];
	int width, height;
	bool maximized;
	bool autoPreview;
	bool showDetails;
} Settings;

char* loadTextAsset(Settings* set, const char* name, size_t* olen);
GtkBuilder* loadUi(Settings* set, const char* name);
void loadSettings(Settings* set);
void saveSettings(Settings* set);
void freeSettings(Settings* set);
#endif

#endif
