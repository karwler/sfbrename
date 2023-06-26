#ifndef ARGUMENTS_H
#define ARGUMENTS_H

#include "utils.h"

typedef struct Arguments {
	char* extensionModeStr;
	char* extensionName;
	char* extensionReplace;
	char* renameModeStr;
	char* rename;
	char* replace;
	char* addInsert;
	char* addPrefix;
	char* addSuffix;
	char* numberStartStr;
	char* numberStepStr;
	char* numberPadStr;
	char* numberPrefix;
	char* numberSuffix;
	char* dateModeStr;
	char* dateFormat;
	char* destinationModeStr;
	char* destination;
	int64_t extensionElements;
	int64_t removeFrom;
	int64_t removeTo;
	int64_t removeFirst;
	int64_t removeLast;
	int64_t addAt;
	int64_t numberLocation;
	int64_t numberStart;
	int64_t numberStep;
	int64_t numberBase;
	int64_t numberPadding;
	int64_t dateLocation;
	gboolean extensionCi;
	gboolean extensionRegex;
	gboolean replaceCi;
	gboolean replaceRegex;
	gboolean numberLower;
	gboolean backwards;
	gboolean noGui;
	gboolean dry;
	gboolean verbose;
	gboolean msgAbort;
	gboolean msgContinue;

	RenameMode extensionMode;
	RenameMode renameMode;
	DateMode dateMode;
	DestinationMode destinationMode;
	bool number;
} Arguments;

char* validateFilename(const char* name);
void processArgumentOptions(Arguments* arg);
void initCommandLineArguments(GApplication* app, Arguments* arg, int argc, char** argv);
void freeArguments(Arguments* arg);

#endif
