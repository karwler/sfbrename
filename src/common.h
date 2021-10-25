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

#endif
