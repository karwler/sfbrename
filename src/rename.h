#ifndef RENAME_H
#define RENAME_H

#include "utils.h"

typedef enum MessageBehavior {
	MSGBEHAVIOR_ASK,
	MSGBEHAVIOR_ABORT,
	MSGBEHAVIOR_CONTINUE
} MessageBehavior;

typedef struct Process {
#ifndef CONSOLE
	GtkTreeModel* model;
	GtkTreeIter it;
#endif
	size_t id;
	size_t total;
	GRegex* regExtension;
	GRegex* regRename;
	const char* extensionName;
	const char* extensionReplace;
	const char* rename;
	const char* replace;
	const char* addInsert;
	const char* addPrefix;
	const char* addSuffix;
	const char* numberPadStr;
	const char* numberPrefix;
	const char* numberSuffix;
	const char* dateFormat;
	const char* destination;
	size_t nameLen;
	size_t dstdirLen;
	int64 numberStart;
	int64 numberStep;
	MessageBehavior messageBehavior;
	RenameMode extensionMode;
	RenameMode renameMode;
	DateMode dateMode;
	DestinationMode destinationMode;
	ushort extensionNameLen;
	ushort extensionReplaceLen;
	short extensionElements;
	ushort renameLen;
	ushort replaceLen;
	short removeFrom;
	short removeTo;
	ushort removeFirst;
	ushort removeLast;
	ushort addInsertLen;
	short addAt;
	ushort addPrefixLen;
	ushort addSuffixLen;
	short numberLocation;
	ushort numberPadding;
	ushort numberPadStrLen;
	ushort numberPrefixLen;
	ushort numberSuffixLen;
	ushort dateFormatLen;
	short dateLocation;
	ushort destinationLen;
	bool extensionCi;
	bool extensionRegex;
	bool replaceCi;
	bool replaceRegex;
	bool number;
	uint8 numberBase;
	bool forward;
	int8 step;
	char name[FILENAME_MAX];
	char extension[FILENAME_MAX];
	char original[PATH_MAX];
	char dstdir[PATH_MAX];
} Process;

#ifndef CONSOLE
void setWidgetsSensitive(Window* win, bool sensitive);
gboolean updateProgressBar(Window* win);
void windowRename(Window* win);
void windowPreview(Window* win);
#endif
void consoleRename(Window* win);
void consolePreview(Window* win);
#endif
