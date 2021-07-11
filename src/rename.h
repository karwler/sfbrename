#ifndef RENAME_H
#define RENAME_H

#include <stdbool.h>
#include <stddef.h>

typedef struct Process Process;
typedef struct Window Window;

void windowRename(Window* win);
void consoleRename(Window* win);
void windowPreview(Window* win);
void consolePreview(Window* win);

#endif
