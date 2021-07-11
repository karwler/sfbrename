#ifndef ARGUMENTS_H
#define ARGUMENTS_H

typedef struct Arguments Arguments;
typedef struct _GtkApplication GtkApplication;
typedef struct Window Window;

void processArguments(Window* win);
void initCommandLineArguments(GtkApplication* app, Arguments* args, int argc, char** argv);

#endif
