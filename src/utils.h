#ifndef UTILS_H
#define UTILS_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef CONSOLE
#include <gio/gio.h>
#include <glib.h>
#else
#include <gtk/gtk.h>
#endif
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_DIGITS_I64B 64
#define DIGIT2CHAR_UPPER "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-"
#define DIGIT2CHAR_LOWER "0123456789abcdefghijklmnopqrstuvwxyz"
#define DIGIT2CHAR_BASE64URL "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"
#define DEFAULT_DATE_FORMAT "%F"

#define pickDigitChars(base, upper) ((base) < 64 ? (upper) || (base) > 36 ? DIGIT2CHAR_UPPER : DIGIT2CHAR_LOWER : DIGIT2CHAR_BASE64URL)
#define noop(x) (x)

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned uint;
typedef unsigned long ulong;
typedef long long llong;
typedef unsigned long long ullong;

typedef struct Arguments Arguments;
typedef struct Process Process;
typedef struct Settings Settings;
typedef struct Window Window;

typedef enum RenameMode {
	RENAME_KEEP,
	RENAME_RENAME,
	RENAME_REPLACE,
	RENAME_LOWER_CASE,
	RENAME_UPPER_CASE,
	RENAME_REVERSE
} RenameMode;

typedef enum DestinationMode {
	DESTINATION_IN_PLACE,
	DESTINATION_MOVE,
	DESTINATION_COPY,
	DESTINATION_LINK
} DestinationMode;

typedef enum DateMode {
	DATE_NONE,
	DATE_CREATE,
	DATE_MODIFY,
	DATE_ACCESS,
	DATE_CHANGE
} DateMode;

typedef enum ResponseType {
	RESPONSE_WAIT,
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

#ifndef CONSOLE
typedef enum ThreadCode {
	THREAD_NONE,
	THREAD_RENAME,
	THREAD_POPULATE,
	THREAD_ABORT
} ThreadCode;

typedef struct AsyncMessage {
	GtkWindow* parent;
	char* text;
	GMutex mutex;
	GCond cond;
	ResponseType response;
	MessageType message;
	ButtonsType buttons;
} AsyncMessage;

typedef struct FileInfo {
	char* size;
	char* user;
	char* group;
	char create[20];
	char modify[20];
	char access[20];
	char change[20];
#ifdef _WIN32
	char perms[2];
#else
	char perms[11];
#endif
} FileInfo;

void runThread(Window* win, ThreadCode code, GThreadFunc proc, GSourceFunc fin, void* data);
void finishThread(Window* win);
void setFileInfo(const char* file, FileInfo* info);
void freeFileInfo(FileInfo* info);
GtkTreeRowReference** getTreeViewSelectedRowRefs(GtkTreeView* view, GtkTreeModel** model, uint* cnt);
void sortTreeViewColumn(GtkTreeView* treeView, GtkListStore* listStore, int colId, bool ascending);
char* showInputText(GtkWindow* parent, const char* title, const char* label, const char* text);
#endif
ResponseType showMessageV(Window* win, MessageType type, ButtonsType buttons, const char* format, va_list args);
ResponseType showMessage(Window* win, MessageType type, ButtonsType buttons, const char* format, ...);
char* sanitizeNumber(const char* text, uint8_t base);
llong strToLlong(const char* str, uint8_t base);
size_t llongToRevStr(char* buf, llong num, uint8_t base, const char* digits);
size_t llongToStr(char* buf, llong num, uint8_t base, bool upper);
char* newStrncat(uint n, ...);

#ifdef _WIN32
void* memrchr(const void* s, int c, size_t n);
wchar_t* stow(const char* src);
char* wtos(const wchar_t* src);
void unbackslashify(char* path);

#define mkdirCommon(path) mkdir(path)
#else
#define mkdirCommon(path) mkdir(path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)
#endif

#endif
