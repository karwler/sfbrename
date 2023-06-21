#ifndef SETTINGS_H
#define SETTINGS_H

#ifndef CONSOLE
#include "utils.h"

typedef struct Settings {
	char* rscPath;
	size_t rlen;
	int width, height;
	bool maximized;
	bool autoPreview;
	bool showDetails;
} Settings;

char* loadTextAsset(Settings* set, const char* name, size_t* olen);
void loadSettings(Settings* set);
void saveSettings(const Settings* set);
#endif

#endif
