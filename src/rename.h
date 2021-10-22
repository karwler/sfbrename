#ifndef RENAME_H
#define RENAME_H

#include "common.h"
#include <regex.h>

#ifndef CONSOLE
typedef enum ThreadCode {
	THREAD_RUN,
	THREAD_ABORT,
	THREAD_DISCARD
} ThreadCode;
#endif

typedef struct Window Window;

typedef struct Process {
#ifndef CONSOLE
	GThread* thread;
	GMutex mutex;
	GtkTreeModel* model;
	GtkTreeIter it;
#endif
	size_t id;
	size_t total;
	regex_t regExtension;
	regex_t regRename;
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
	const char* destination;
	size_t nameLen;
	size_t dstdirLen;
#ifndef CONSOLE
	ThreadCode threadCode;
#endif
	RenameMode extensionMode;
	RenameMode renameMode;
	int64 numberStart;
	int64 numberStep;
	DestinationMode destinationMode;
	ushort extensionNameLen;
	ushort extensionReplaceLen;
	short extensionElements;
	ushort renameLen;
	ushort replaceLen;
	ushort removeFrom;
	ushort removeTo;
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
	ushort destinationLen;
	bool extensionCi;
	bool extensionRegex;
	bool replaceCi;
	bool replaceRegex;
	bool number;
	uint8 numberBase;
	bool forward;
	int8 step;
	char name[PATH_MAX];
	char extension[FILENAME_MAX];
	char dstdir[PATH_MAX];
} Process;

#ifndef CONSOLE
void joinThread(Process* prc);
void windowRename(Window* win);
void windowPreview(Window* win);
#endif
void consoleRename(Window* win);
void consolePreview(Window* win);
#endif
