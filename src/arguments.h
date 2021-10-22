#ifndef ARGUMENTS_H
#define ARGUMENTS_H

#include "common.h"

typedef struct Window Window;

typedef struct Arguments {
	GFile** files;
	size_t nFiles;
	char* extensionModeStr;
	char* extensionName;
	char* extensionReplace;
	char* renameModeStr;
	char* rename;
	char* replace;
	char* addInsert;
	char* addPrefix;
	char* addSuffix;
	char* numberPadStr;
	char* numberPrefix;
	char* numberSuffix;
	char* destinationModeStr;
	char* destination;
	int64 extensionElements;
	int64 removeFrom;
	int64 removeTo;
	int64 removeFirst;
	int64 removeLast;
	int64 addAt;
	int64 numberLocation;
	int64 numberStart;
	int64 numberStep;
	int64 numberBase;
	int64 numberPadding;
	gboolean extensionCi;
	gboolean extensionRegex;
	gboolean replaceCi;
	gboolean replaceRegex;
	gboolean backwards;
	gboolean noAutoPreview;
#ifndef CONSOLE
	gboolean noGui;
#endif
	gboolean dry;

	RenameMode extensionMode;
	RenameMode renameMode;
	DestinationMode destinationMode;
	bool number;
} Arguments;

void processArgumentOptions(Arguments* arg);
void initCommandLineArguments(GApplication* app, Arguments* arg, int argc, char** argv);

#endif
