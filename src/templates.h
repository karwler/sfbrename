#ifndef TEMPLATES_H
#define TEMPLATES_H

#ifndef CONSOLE
#include "utils.h"

Arguments* loadTemplateFile(Settings* set, const char* name, char** error);
Arguments* openTemplatesDialog(Window* win);

#endif

#endif
