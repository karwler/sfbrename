#ifndef UTILS_H
#define UTILS_H

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

#ifndef CONSOLE
typedef enum Sorting {
	SORT_NONE,
	SORT_ASC,
	SORT_DSC
} Sorting;
#endif

typedef struct Window {
	Process* proc;
#ifdef CONSOLE
	GApplication* app;
#else
	GtkApplication* app;
#endif
	Arguments* args;

#ifndef CONSOLE
	Settings sets;
	GThread* thread;

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
#endif
	bool dryAuto;
} Window;

#ifndef CONSOLE
void autoPreview(Window* win);
#endif

#endif
