#ifndef RENAME_H
#define RENAME_H

#include "utils.h"

#define MAX_DIGITS_I32D 10

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
	const char* numberDigits;
	const char* numberPadStr;
	const char* numberPrefix;
	const char* numberSuffix;
	const char* dateFormat;
	const char* destination;
	size_t nameLen;
	size_t dstdirLen;
	int64_t numberStart;
	int64_t numberStep;
#ifndef _WIN32
	uint statMask;
#endif
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
	uint8_t numberBase;
	bool forward;
	int8_t step;
	char name[FILENAME_MAX];
	char extension[FILENAME_MAX];
	char original[PATH_MAX];
	char dstdir[PATH_MAX];
} Process;

#ifndef CONSOLE
void setProgressBar(GtkProgressBar* bar, size_t pos, size_t total, bool fwd);
gboolean updateProgressBar(Window* win);
void windowRename(Window* win);
void windowPreview(Window* win);
#endif
void consoleRename(Process* prc, const Arguments* arg, GFile** files, size_t nFiles);
void consolePreview(Process* prc, const Arguments* arg, GFile** files, size_t nFiles);
#endif
