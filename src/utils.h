#ifndef COMMON_H
#define COMMON_H

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
#define DEFAULT_DATE_FORMAT "%F"

typedef wchar_t wchar;
typedef unsigned short ushort;
typedef unsigned uint;
typedef unsigned long ulong;
typedef long long llong;
typedef unsigned long long ullong;
typedef int8_t int8;
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef int64_t int64;
typedef uint64_t uint64;

typedef struct Arguments Arguments;
typedef struct Process Process;
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
#ifndef _WIN32
	DATE_CREATE,
#endif
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
#ifndef _WIN32
	const struct passwd* pwd;
	const struct group* grp;
#endif
	char* size;
#ifndef _WIN32
	char create[20];
#endif
	char modify[20];
	char access[20];
	char change[20];
	char perms[11];
} FileInfo;

void runThread(Window* win, ThreadCode code, GThreadFunc proc, GSourceFunc fin, void* data);
void finishThread(Window* win);
void setFileInfo(const char* file, FileInfo* info);
#endif
ResponseType showMessageV(Window* win, MessageType type, ButtonsType buttons, const char* format, va_list args);
ResponseType showMessage(Window* win, MessageType type, ButtonsType buttons, const char* format, ...);
#ifdef _WIN32
void* memrchr(const void* s, int c, size_t n);
void unbackslashify(char* path);
#endif

#endif
