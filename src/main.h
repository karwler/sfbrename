#ifndef UTILS_H
#define UTILS_H

#define _GNU_SOURCE
#include <gtk/gtk.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <strings.h>
#include <regex.h>

typedef wchar_t wchar;
typedef unsigned short ushort;
typedef unsigned uint;
typedef unsigned long ulong;
typedef long long llong;
typedef unsigned long long ullong;
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef int64_t int64;
typedef uint64_t uint64;

typedef enum {
	FCOL_INVALID = -1,
	FCOL_OLD_NAME,
	FCOL_NEW_NAME,
	FCOL_DIRECTORY
} FileColumn;

typedef enum {
	RENAME_KEEP,
	RENAME_RENAME,
	RENAME_REPLACE,
	RENAME_LOWER_CASE,
	RENAME_UPPER_CASE,
	RENAME_REVERSE
} RenameMode;

typedef enum {
	DESTINATION_IN_PLACE,
	DESTINATION_MOVE,
	DESTINATION_COPY,
	DESTINATION_LINK
} DestinationMode;

typedef struct Arguments {
	GFile** files;
	gchar* extensionMode;
	gchar* extensionName;
	gchar* extensionReplace;
	gchar* renameMode;
	gchar* rename;
	gchar* replace;
	gchar* addInsert;
	gchar* addPrefix;
	gchar* addSuffix;
	gchar* numberPadStr;
	gchar* numberPrefix;
	gchar* numberSuffix;
	gchar* destinationMode;
	gchar* destination;
	gint64 extensionElements;
	gint64 removeFrom;
	gint64 removeTo;
	gint64 removeFirst;
	gint64 removeLast;
	gint64 addAt;
	gint64 numberLocation;
	gint64 numberStart;
	gint64 numberStep;
	gint64 numberBase;
	gint64 numberPadding;
	int nFiles;
	gboolean extensionCi;
	gboolean extensionRegex;
	gboolean replaceCi;
	gboolean replaceRegex;
	gboolean backwards;
	gboolean noAutoPreview;
	gboolean noGui;
	gboolean dry;

	RenameMode gotExtensionMode;
	RenameMode gotRenameMode;
	DestinationMode gotDestinationMode;
	bool number;
} Arguments;

typedef struct Process {
	GtkTreeModel* model;
	GtkTreeIter it;
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
	int id;
	int step;
	RenameMode extensionMode;
	RenameMode renameMode;
	int numberStart;
	int numberStep;
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
	ushort nameLen;
	bool extensionCi;
	bool extensionRegex;
	bool replaceCi;
	bool replaceRegex;
	bool number;
	uint8 numberBase;
	bool forward;
	char name[PATH_MAX];
	char extension[FILENAME_MAX];
} Process;

typedef struct Window {
	Process proc;
	GtkApplication* app;
	Arguments* args;

	GtkWindow* window;
	GtkTreeView* tblFiles;
	GtkListStore* lsFiles;

	GtkComboBoxText* cmbExtensionMode;
	GtkEntry* etExtension;
	GtkEntry* etExtensionReplace;
	GtkCheckButton* cbExtensionCi;
	GtkCheckButton* cbExtensionRegex;
	GtkSpinButton* sbExtensionElements;

	GtkComboBoxText* cmbRenameMode;
	GtkEntry* etRename;
	GtkEntry* etReplace;
	GtkCheckButton* cbReplaceCi;
	GtkCheckButton* cbReplaceRegex;

	GtkSpinButton* sbRemoveFrom;
	GtkSpinButton* sbRemoveTo;
	GtkSpinButton* sbRemoveFirst;
	GtkSpinButton* sbRemoveLast;

	GtkEntry* etAddInsert;
	GtkSpinButton* sbAddAt;
	GtkEntry* etAddPrefix;
	GtkEntry* etAddSuffix;

	GtkCheckButton* cbNumber;
	GtkSpinButton* sbNumberLocation;
	GtkSpinButton* sbNumberBase;
	GtkSpinButton* sbNumberStart;
	GtkSpinButton* sbNumberStep;
	GtkSpinButton* sbNumberPadding;
	GtkEntry* etNumberPadding;
	GtkEntry* etNumberPrefix;
	GtkEntry* etNumberSuffix;

	GtkComboBoxText* cmbDestinationMode;
	GtkEntry* etDestination;
	GtkCheckButton* cbDestinationForward;

	bool autoPreview;
} Window;

#define MAX_DIGITS 10

#define nmin(a, b) ((a) < (b) ? (a) : (b))
#define nmax(a, b) ((a) > (b) ? (a) : (b))
#define nclamp(v, lo, hi) ((v) < (lo) ? (v) : (v) > (hi) ? (hi) : (v))

#ifdef __MINGW32__
void* memrchr(const void* s, int c, size_t n);
void unbackslashify(char* path);
#endif
void addFile(Window* win, const char* file);
GtkResponseType showMessageBox(Window* win, GtkMessageType type, GtkButtonsType buttons, const char* format, ...);

#endif
