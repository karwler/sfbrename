#include "arguments.h"
#include "rename.h"
#include "window.h"

typedef struct Program {
#ifdef CONSOLE
	GApplication* app;
#else
	GtkApplication* app;
	Window* win;
#endif
	Arguments args;
	Process proc;
} Program;

static void runConsole(Program* prog, GFile** files, int nFiles) {
	Arguments* arg = &prog->args;
	if (arg->dry)
		consolePreview(&prog->proc, arg, files, nFiles);
	else
		consoleRename(&prog->proc, arg, files, nFiles);
}

static void openApplication(GtkApplication* app, GFile** files, int nFiles, const char* hint, Program* prog) {
	Arguments* arg = &prog->args;
	Process* prc = &prog->proc;
	processArgumentOptions(arg);
	prc->messageBehavior = arg->msgAbort ? MSGBEHAVIOR_ABORT : arg->msgContinue ? MSGBEHAVIOR_CONTINUE : MSGBEHAVIOR_ASK;
#ifdef CONSOLE
	runConsole(prog, files, nFiles);
#else
	if (arg->noGui)
		runConsole(prog, files, nFiles);
	else
		prog->win = openWindow(prog->app, arg, prc, files, nFiles);
#endif
}

static void activateApplication(GtkApplication* app, Program* prog) {
	openApplication(app, NULL, 0, NULL, prog);
}

int main(int argc, char** argv) {
	Program* prog = malloc(sizeof(Program));
	memset(prog, 0, sizeof(Program));
#ifdef CONSOLE
	prog->app = g_application_new(NULL, G_APPLICATION_HANDLES_OPEN);
#else
	prog->app = gtk_application_new(NULL, G_APPLICATION_HANDLES_OPEN);
#endif
	g_signal_connect(prog->app, "activate", G_CALLBACK(activateApplication), prog);
	g_signal_connect(prog->app, "open", G_CALLBACK(openApplication), prog);
	initCommandLineArguments(G_APPLICATION(prog->app), &prog->args, argc, argv);
	int rc = g_application_run(G_APPLICATION(prog->app), argc, argv);
	g_object_unref(prog->app);
#ifndef CONSOLE
	freeWindow(prog->win);
#endif
	freeArguments(&prog->args);
	free(prog);
	return rc;
}
