#ifndef WINDOW_H
#define WINDOW_H

#ifndef CONSOLE
#include "settings.h"

typedef enum FileColumn {
	FCOL_INVALID = -1,
	FCOL_OLD_NAME,
	FCOL_NEW_NAME,
	FCOL_DIRECTORY,
	FCOL_SIZE,
	FCOL_CREATE,
	FCOL_MODIFY,
	FCOL_ACCESS,
	FCOL_CHANGE,
	FCOL_PERMISSIONS,
	FCOL_USER,
	FCOL_GROUP
} FileColumn;

typedef enum Sorting {
	SORT_NONE,
	SORT_ASC,
	SORT_DSC
} Sorting;

typedef struct Window {
	Process* proc;
	const Arguments* args;
	Settings sets;
	GThread* thread;
	GtkTreeIter lastFile;
	GtkTreeIter* lastFilePtr;

	GtkApplicationWindow* window;
	GtkButton* btAddFiles;
	GtkButton* btAddFolders;
	GtkButton* btOptions;

	GtkTreeView* tblFiles;
	GtkListStore* lsFiles;
	GtkTreeViewColumn* tblFilesName;
	GtkTreeViewColumn* tblFilesDirectory;
	GtkTreeViewColumn* tblFilesSize;
	GtkTreeViewColumn* tblFilesCreate;
	GtkTreeViewColumn* tblFilesModify;
	GtkTreeViewColumn* tblFilesAccess;
	GtkTreeViewColumn* tblFilesChange;
	GtkTreeViewColumn* tblFilesPermissions;
	GtkTreeViewColumn* tblFilesUser;
	GtkTreeViewColumn* tblFilesGroup;

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
	GtkCheckButton* cbNumberUpper;
	GtkSpinButton* sbNumberStart;
	GtkSpinButton* sbNumberStep;
	GtkSpinButton* sbNumberPadding;
	GtkEntry* etNumberPadding;
	GtkEntry* etNumberPrefix;
	GtkEntry* etNumberSuffix;

	GtkComboBoxText* cmbDateMode;
	GtkEntry* etDateFormat;
	GtkSpinButton* sbDateLocation;

	GtkComboBoxText* cmbDestinationMode;
	GtkEntry* etDestination;
	GtkButton* btDestination;
	GtkButton* btPreview;
	GtkButton* btRename;
	GtkCheckButton* cbDestinationForward;
	GtkProgressBar* pbRename;

	_Atomic ThreadCode threadCode;
	Sorting nameSort;
	Sorting directorySort;
	bool dryAuto;
} Window;

void setWidgetsSensitive(Window* win, bool sensitive);
G_MODULE_EXPORT void clickAddFiles(GtkButton* button, Window* win);
G_MODULE_EXPORT void clickAddFolders(GtkButton* button, Window* win);
void activateClear(GtkMenuItem* item, Window* win);
void autoPreview(Window* win);
Window* openWindow(GtkApplication* app, const Arguments* arg, Process* prc, GFile** files, size_t nFiles);
void freeWindow(Window* win);

#endif

#endif
