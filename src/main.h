#ifndef UTILS_H
#define UTILS_H

#include "common.h"

typedef enum FileColumn {
	FCOL_INVALID = -1,
	FCOL_OLD_NAME,
	FCOL_NEW_NAME,
	FCOL_DIRECTORY
} FileColumn;

#ifndef CONSOLE
typedef enum Sorting {
	SORT_NONE,
	SORT_ASC,
	SORT_DSC
} Sorting;
#endif

typedef enum ResponseType {
	RESPONSE_NONE = -1,
	RESPONSE_REJECT = -2,
	RESPONSE_ACCEPT = -3,
	RESPONSE_DELETE_EVENT = -4,
	RESPONSE_OK = -5,
	RESPONSE_CANCEL = -6,
	RESPONSE_CLOSE = -7,
	RESPONSE_YES = -8,
	RESPONSE_NO = -9,
	RESPONSE_APPLY = -10,
	RESPONSE_HELP = -11
} ResponseType;

typedef enum MessageType {
	MESSAGE_INFO,
	MESSAGE_WARNING,
	MESSAGE_QUESTION,
	MESSAGE_ERROR,
	MESSAGE_OTHER
} MessageType;

typedef enum ButtonsType {
	BUTTONS_NONE,
	BUTTONS_OK,
	BUTTONS_CLOSE,
	BUTTONS_CANCEL,
	BUTTONS_YES_NO,
	BUTTONS_OK_CANCEL
} ButtonsType;

typedef struct Arguments Arguments;
typedef struct Process Process;

typedef struct Window {
	Process* proc;
#ifdef CONSOLE
	GApplication* app;
#else
	GtkApplication* app;
#endif
	Arguments* args;

#ifndef CONSOLE
	char* rscPath;
	size_t rlen;

	GtkApplicationWindow* window;
	GtkButton* btAddFiles;
	GtkButton* btAddFolders;
	GtkButton* btOptions;

	GtkTreeView* tblFiles;
	GtkListStore* lsFiles;
	GtkTreeViewColumn* tblFilesName;
	GtkTreeViewColumn* tblFilesDirectory;

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

	GtkComboBoxText* cmbDateMode;
	GtkEntry* etDateFormat;
	GtkSpinButton* sbDateLocation;
#ifndef _WIN32
	GtkCheckButton* cbDateLinks;
#endif

	GtkComboBoxText* cmbDestinationMode;
	GtkEntry* etDestination;
	GtkButton* btDestination;
	GtkButton* btPreview;
	GtkButton* btRename;
	GtkCheckButton* cbDestinationForward;
	GtkProgressBar* pbRename;

	Sorting nameSort;
	Sorting directorySort;
	bool autoPreview;
	bool singleThread;
#endif
	bool dryAuto;
} Window;

#ifdef _WIN32
void* memrchr(const void* s, int c, size_t n);
void unbackslashify(char* path);
#endif
#ifndef CONSOLE
void autoPreview(Window* win);
#endif
ResponseType showMessageV(Window* win, MessageType type, ButtonsType buttons, const char* format, va_list args);
ResponseType showMessage(Window* win, MessageType type, ButtonsType buttons, const char* format, ...);

#endif
