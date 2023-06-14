#include "arguments.h"
#include "main.h"
#include "rename.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <shlwapi.h>
#else
#include <sys/sendfile.h>
#endif

#define MAX_DIGITS_I32D 10
#define CONTINUE_TEXT "\nContinue?"
#ifndef _WIN32
#define PERMISSION_MASK (S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXO | S_IRWXO)
#endif

#ifndef CONSOLE
typedef struct TableUpdate {
	Window* win;
	GtkTreeIter iter;
	char name[FILENAME_MAX];
} TableUpdate;
#endif

#ifdef _WIN32
static wchar* stow(const char* src) {
	wchar* dst;
	int len = MultiByteToWideChar(CP_UTF8, 0, src, -1, NULL, 0);
	if (len > 1) {
		dst = malloc(len * sizeof(wchar));
		MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, len);
	} else {
		dst = malloc(sizeof(wchar));
		dst[0] = '\0';
	}
	return dst;
}

static int createSymlink(const char* path, const char* target) {
	wchar* wpath = stow(path);
	wchar* wtarget = stow(target);
	DWORD attr = GetFileAttributesW(wtarget);
	DWORD flags = attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0;
	int rc = !CreateSymbolicLinkW(wpath, wtarget, flags);
	free(wpath);
	free(wtarget);
	return rc;
}
#endif

static char* joinPath(const char* dir, size_t dlen, const char* file, size_t flen) {
	char* path = malloc((dlen + flen + 2) * sizeof(char));
	memcpy(path, dir, dlen * sizeof(char));
	path[dlen] = '/';
	memcpy(path + dlen + 1, file, (flen + 1) * sizeof(char));
	return path;
}

static int copyFile(const char* src, const char* dst) {
	struct stat ps;
#ifdef _WIN32
	if (stat(src, &ps))
#else
	if (lstat(src, &ps))
#endif
		return -1;

	int rc;
	switch (ps.st_mode & S_IFMT) {
	case S_IFDIR: {
#ifdef _WIN32
		mkdir(dst);
#else
		mkdir(dst, ps.st_mode & PERMISSION_MASK);
#endif
		DIR* dir = opendir(src);
		if (!dir)
			return -1;

		rc = 0;
		size_t slen = strlen(src);
		size_t dlen = strlen(dst);
		for (struct dirent* entry = readdir(dir); entry; entry = readdir(dir))
			if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
				size_t nlen = strlen(entry->d_name);
				char* from = joinPath(src, slen, entry->d_name, nlen);
				char* to = joinPath(dst, dlen, entry->d_name, nlen);
				rc |= copyFile(from, to);
				free(from);
				free(to);
			}
		closedir(dir);
		break; }
	case S_IFREG: {
#ifdef _WIN32
		wchar* wsrc = stow(src);
		wchar* wdst = stow(dst);
		rc = !CopyFileW(wsrc, wdst, false);
		free(wsrc);
		free(wdst);
#else
		int in = open(src, O_RDONLY);
		if (in == -1)
			return -1;

		int out = creat(dst, ps.st_mode & PERMISSION_MASK);
		if (out == -1) {
			close(in);
			return -1;
		}
		rc = sendfile(out, in, NULL, ps.st_size) != -1 ? 0 : errno;
		close(in);
		close(out);
#endif
		break; }
#ifndef _WIN32
	case S_IFLNK: {
		rc = -1;
		char* path = malloc((ps.st_size + 1) * sizeof(char));
		ssize_t len = readlink(src, path, ps.st_size);
		if (len != -1) {
			path[len] = '\0';
			rc = symlink(path, dst);
		}
		free(path);
		break; }
#endif
	default:
		rc = -1;
	}
	return rc;
}

static ResponseType continueError(Process* prc, Window* win, const char* format, ...) {
	va_list args;
	va_start(args, format);
	ResponseType rc = RESPONSE_NONE;
	if (prc->messageBehavior == MSGBEHAVIOR_ASK && (prc->forward ? prc->id < prc->total - 1 : prc->id)) {
		size_t flen = strlen(format);
		char* fmt = malloc((flen + sizeof(CONTINUE_TEXT)) * sizeof(char));
		memcpy(fmt, format, flen * sizeof(char));
		strcpy(fmt + flen, CONTINUE_TEXT);
		rc = showMessageV(win, MESSAGE_ERROR, BUTTONS_YES_NO, fmt, args);
		free(fmt);
	} else {
		switch (prc->messageBehavior) {
		case MSGBEHAVIOR_ASK:
			rc = showMessageV(win, MESSAGE_ERROR, BUTTONS_OK, format, args);
			break;
		case MSGBEHAVIOR_ABORT:
			if (!win)
				showMessageV(win, MESSAGE_ERROR, BUTTONS_OK, format, args);
			rc = RESPONSE_NO;
			break;
		case MSGBEHAVIOR_CONTINUE:
			if (!win)
				showMessageV(win, MESSAGE_ERROR, BUTTONS_OK, format, args);
			rc = RESPONSE_YES;
		}
	}
	va_end(args);
	return rc;
}

static size_t replaceRegex(const GRegex* reg, char* name, size_t nameLen, const char* new) {
	char* str = g_regex_replace(reg, name, nameLen, 0, new, G_REGEX_MATCH_NEWLINE_ANYCRLF, NULL);
	if (!str)
		return nameLen;

	size_t slen = strlen(str);
	if (slen < FILENAME_MAX)
		memcpy(name, str, (slen + 1) * sizeof(char));
	g_free(str);
	return slen;
}

static size_t replaceStrings(char* name, const char* old, ushort olen, const char* new, ushort nlen, bool ci) {
#ifdef _WIN32
	char* (*scmp)(const char*, const char*) = ci ? StrStrIA : strstr;
#else
	char* (*scmp)(const char*, const char*) = ci ? strcasestr : strstr;
#endif
	char buf[FILENAME_MAX];
	size_t blen = 0;
	char* last = name;
	for (char* it; (it = scmp(last, old));) {
		size_t diff = it - last;
		if (blen + diff + nlen >= FILENAME_MAX)
			return SIZE_MAX;

		memcpy(buf + blen, last, diff * sizeof(char));
		memcpy(buf + blen + diff, new, nlen * sizeof(char));
		blen += diff + nlen;
		last = it + olen;
	}
	size_t rest = strlen(last);
	size_t tlen = blen + rest;
	if (blen + rest >= FILENAME_MAX)
		return SIZE_MAX;

	memcpy(buf + blen, last, (rest + 1) * sizeof(char));
	memcpy(name, buf, (tlen + 1) * sizeof(char));
	return tlen;
}

static size_t moveGName(char* dst, char* src) {
	size_t len = strlen(src);
	if (len < FILENAME_MAX)
		memcpy(dst, src, (len + 1) * sizeof(char));
	g_free(src);
	return len;
}

static ResponseType nameRename(Process* prc, Window* win) {
	switch (prc->renameMode) {
	case RENAME_KEEP:
		return RESPONSE_NONE;
	case RENAME_RENAME:
		prc->nameLen = prc->renameLen;
		if (prc->nameLen < FILENAME_MAX)
			memcpy(prc->name, prc->rename, (prc->nameLen + 1) * sizeof(char));
		break;
	case RENAME_REPLACE:
		if (prc->replaceRegex)
			prc->nameLen = replaceRegex(prc->regRename, prc->name, prc->nameLen, prc->replace);
		else if (prc->renameLen)
			prc->nameLen = replaceStrings(prc->name, prc->rename, prc->renameLen, prc->replace, prc->replaceLen, prc->replaceCi);
		break;
	case RENAME_LOWER_CASE:
		prc->nameLen = moveGName(prc->name, g_utf8_strdown(prc->name, prc->nameLen));
		break;
	case RENAME_UPPER_CASE:
		prc->nameLen = moveGName(prc->name, g_utf8_strup(prc->name, prc->nameLen));
		break;
	case RENAME_REVERSE:
		prc->nameLen = moveGName(prc->name, g_utf8_strreverse(prc->name, prc->nameLen));
	}

	if (prc->nameLen >= FILENAME_MAX)
		return continueError(prc, win, "Filename became too long during rename.");
	return RESPONSE_NONE;
}

static char* getUtf8Offset(char* str, size_t len, ssize_t id, size_t ulen) {
	if (id >= 0)
		return (size_t)id <= ulen ? g_utf8_offset_to_pointer(str, id) : str + len;
	return (size_t)-id <= ulen ? g_utf8_offset_to_pointer(str, ulen + id + 1) : str;
}

static void nameRemove(Process* prc) {
	size_t ulen = g_utf8_strlen(prc->name, prc->nameLen);
	if (prc->removeFrom != prc->removeTo) {
		char* pfr = getUtf8Offset(prc->name, prc->nameLen, prc->removeFrom, ulen);
		char* pto = getUtf8Offset(prc->name, prc->nameLen, prc->removeTo, ulen);
		ssize_t diff = pto - pfr;
		size_t dlen = ABS(diff);
		if (dlen < ulen) {
			if (diff < 0) {
				char* tmp = pfr;
				pfr = pto;
				pto = tmp;
			}
			memmove(pfr, pto, (size_t)(prc->name + prc->nameLen - pto + 1) * sizeof(char));
			prc->nameLen -= dlen;
			ulen -= g_utf8_strlen(pfr, dlen);
		} else {
			prc->nameLen = ulen = 0;
			prc->name[0] = '\0';
		}
	}

	if (prc->removeFirst) {
		if (prc->removeFirst < ulen) {
			char* src = g_utf8_offset_to_pointer(prc->name, prc->removeFirst);
			prc->nameLen = prc->name + prc->nameLen - src;
			ulen -= prc->removeFirst;
			memmove(prc->name, src, (prc->nameLen + 1) * sizeof(char));
		} else {
			prc->nameLen = ulen = 0;
			prc->name[0] = '\0';
		}
	}

	if (prc->removeLast) {
		if (prc->removeLast < ulen) {
			char* src = g_utf8_offset_to_pointer(prc->name, ulen - prc->removeLast);
			prc->nameLen = src - prc->name;
			*src = '\0';
		} else {
			prc->nameLen = 0;
			prc->name[0] = '\0';
		}
	}
}

static ResponseType nameAdd(Process* prc, Window* win) {
	if (prc->addInsertLen) {
		if (prc->nameLen + prc->addInsertLen >= FILENAME_MAX)
			return continueError(prc, win, "Filename '%s' became too long during add.", prc->name);

		char* pos = getUtf8Offset(prc->name, prc->nameLen, prc->addAt, g_utf8_strlen(prc->name, prc->nameLen));
		memmove(pos + prc->addInsertLen, pos, (size_t)(prc->name + prc->nameLen - pos + 1) * sizeof(char));
		memcpy(pos, prc->addInsert, prc->addInsertLen * sizeof(char));
		prc->nameLen += prc->addInsertLen;
	}

	if (prc->addPrefixLen) {
		if (prc->nameLen + prc->addPrefixLen >= FILENAME_MAX)
			return continueError(prc, win, "Filename '%s' became too long during add.", prc->name);

		memmove(prc->name + prc->addPrefixLen, prc->name, (prc->nameLen + 1) * sizeof(char));
		memcpy(prc->name, prc->addPrefix, prc->addPrefixLen * sizeof(char));
		prc->nameLen += prc->addPrefixLen;
	}

	if (prc->addSuffixLen) {
		if (prc->nameLen + prc->addSuffixLen >= FILENAME_MAX)
			return continueError(prc, win, "Filename '%s' became too long during add.", prc->name);

		memcpy(prc->name + prc->nameLen, prc->addSuffix, (prc->addSuffixLen + 1) * sizeof(char));
		prc->nameLen += prc->addSuffixLen;
	}
	return RESPONSE_NONE;
}

static ResponseType nameNumber(Process* prc, Window* win) {
	int64 val = (int64)prc->id * prc->numberStep + prc->numberStart;
	bool negative = val < 0;
	const char* digits = prc->numberBase < 64 ? "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz+" : "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	char buf[MAX_DIGITS_I64B];
	size_t blen = 0;
	if (val) {
		for (uint64 num = !negative ? val : -val; num; num /= prc->numberBase)
			buf[blen++] = digits[num % prc->numberBase];
	} else
		buf[blen++] = '0';

	size_t padLeft = blen < prc->numberPadding && prc->numberPadStrLen ? (prc->numberPadding - blen) * prc->numberPadStrLen : 0;
	size_t pbslen = prc->numberPrefixLen + negative + padLeft + blen + prc->numberSuffixLen;
	if (prc->nameLen + pbslen >= FILENAME_MAX)
		return continueError(prc, win, "Filename '%s' became too long while adding number.", prc->name);

	char* pos = getUtf8Offset(prc->name, prc->nameLen, prc->numberLocation, g_utf8_strlen(prc->name, prc->nameLen));
	memmove(pos + pbslen, pos, (size_t)(prc->name + prc->nameLen - pos + 1) * sizeof(char));
	pos = (char*)memcpy(pos, prc->numberPrefix, prc->numberPrefixLen * sizeof(char)) + prc->numberPrefixLen;
	if (negative)
		*pos++ = '-';
	for (size_t i = 0; i < padLeft; i += prc->numberPadStrLen)
		pos = (char*)memcpy(pos, prc->numberPadStr, prc->numberPadStrLen * sizeof(char)) + prc->numberPadStrLen;
	while (blen)
		*pos++ = buf[--blen];
	memcpy(pos, prc->numberSuffix, prc->numberSuffixLen * sizeof(char));
	prc->nameLen += pbslen;
	return RESPONSE_NONE;
}

static ResponseType nameDate(Process* prc, Window* win) {
	if (prc->dateMode == DATE_NONE)
		return RESPONSE_NONE;

	struct stat ps;
#ifdef _WIN32
	int rc = stat(prc->original, &ps);
#else
	int rc = prc->dateLinks ? stat(prc->original, &ps) : lstat(prc->original, &ps);
#endif
	if (rc)
		return continueError(prc, win, "Failed to retrieve file info: %s", strerror(errno));

	GDateTime* date;
	switch (prc->dateMode) {
#ifdef _WIN32
	case DATE_MODIFY:
		date = g_date_time_new_from_unix_local(ps.st_mtime);
		break;
	case DATE_ACCESS:
		date = g_date_time_new_from_unix_local(ps.st_atime);
		break;
	case DATE_CHANGE:
		date = g_date_time_new_from_unix_local(ps.st_ctime);
#else
	case DATE_MODIFY:
		date = g_date_time_new_from_unix_local(ps.st_mtim.tv_sec);
		break;
	case DATE_ACCESS:
		date = g_date_time_new_from_unix_local(ps.st_atim.tv_sec);
		break;
	case DATE_CHANGE:
		date = g_date_time_new_from_unix_local(ps.st_ctim.tv_sec);
#endif
	}
	char* dstr = g_date_time_format(date, prc->dateFormat);
	g_date_time_unref(date);
	if (!dstr)
		return continueError(prc, win, "Failed to format date for file '%s'.", prc->name);
	size_t dlen = strlen(dstr);
	if (prc->nameLen + dlen >= FILENAME_MAX) {
		g_free(dstr);
		return continueError(prc, win, "Filename '%s' became too long while adding date.", prc->name);
	}

	char* pos = getUtf8Offset(prc->name, prc->nameLen, prc->dateLocation, g_utf8_strlen(prc->name, prc->nameLen));
	memmove(pos + dlen, pos, (size_t)(prc->name + prc->nameLen - pos + 1) * sizeof(char));
	memcpy(pos, dstr, dlen);
	prc->nameLen += dlen;
	g_free(dstr);
	return RESPONSE_NONE;
}

static size_t processExtension(Process* prc, const char* str, size_t slen) {
	if (prc->extensionElements < 0) {
		bool offs = str[0] == '.';
		char* pos = memchr(str + offs, '.', slen - offs);
		prc->nameLen = pos ? (size_t)(pos - str) : slen;
	} else {
		prc->nameLen = slen;
		for (int i = 0; i < prc->extensionElements; ++i) {
			char* pos = memrchr(str, '.', prc->nameLen);
			if (!pos)
				break;
			prc->nameLen = pos - str;
		}
	}
	size_t elen = slen - prc->nameLen;
	memcpy(prc->name, str, prc->nameLen * sizeof(char));
	prc->name[prc->nameLen] = '\0';
	if (!elen) {
		prc->extension[0] = '\0';
		return 0;
	}

	switch (prc->extensionMode) {
	case RENAME_KEEP:
		memcpy(prc->extension, str + prc->nameLen, (elen + 1) * sizeof(char));
		break;
	case RENAME_RENAME:
		elen = prc->extensionNameLen;
		if (elen < FILENAME_MAX)
			memcpy(prc->extension, prc->extensionName, (elen + 1) * sizeof(char));
		break;
	case RENAME_REPLACE:
		memcpy(prc->extension, str + prc->nameLen, (elen + 1) * sizeof(char));
		if (prc->extensionRegex)
			return replaceRegex(prc->regExtension, prc->extension, elen, prc->extensionReplace);
		if (prc->extensionNameLen)
			return replaceStrings(prc->extension, prc->extensionName, prc->extensionNameLen, prc->extensionReplace, prc->extensionReplaceLen, prc->extensionCi);
		break;
	case RENAME_LOWER_CASE:
		prc->extension[0] = str[prc->nameLen];
		return moveGName(prc->extension + 1, g_utf8_strdown(str + prc->nameLen + 1, elen - 1));
	case RENAME_UPPER_CASE:
		prc->extension[0] = str[prc->nameLen];
		return moveGName(prc->extension + 1, g_utf8_strup(str + prc->nameLen + 1, elen - 1));
	case RENAME_REVERSE:
		prc->extension[0] = str[prc->nameLen];
		return moveGName(prc->extension + 1, g_utf8_strreverse(str + prc->nameLen + 1, elen - 1));
	}
	return elen;
}

static ResponseType processName(Process* prc, const char* oldn, size_t olen, Window* win) {
	size_t elen = processExtension(prc, oldn, olen);
	if (elen >= FILENAME_MAX)
		return continueError(prc, win, "Extension became too long.");

	ResponseType rc = nameRename(prc, win);
	if (rc != RESPONSE_NONE)
		return rc;
	nameRemove(prc);
	rc = nameAdd(prc, win);
	if (rc != RESPONSE_NONE)
		return rc;
	if (prc->number) {
		rc = nameNumber(prc, win);
		if (rc != RESPONSE_NONE)
			return rc;
	}
	rc = nameDate(prc, win);
	if (rc != RESPONSE_NONE)
		return rc;

	if (prc->nameLen + elen >= FILENAME_MAX)
		return continueError(prc, win, "Filename '%s' became too long while reapplying extension.", prc->name);
	memcpy(prc->name + prc->nameLen, prc->extension, (elen + 1) * sizeof(char));
	prc->nameLen += elen;
	return rc;
}

static bool initRegex(bool* inUse, GRegex** reg, const char* expr, ushort exlen, bool use, bool ci, Window* win) {
	*inUse = use && exlen;
	if (*inUse) {
		GError* err = NULL;
		*reg = g_regex_new(expr, (ci ? G_REGEX_CASELESS : 0) | G_REGEX_MULTILINE | G_REGEX_DOTALL | G_REGEX_OPTIMIZE | G_REGEX_NEWLINE_ANYCRLF, G_REGEX_MATCH_NEWLINE_ANYCRLF, &err);
		if (!*reg) {
			if (!(win && win->dryAuto))
				showMessage(win, MESSAGE_ERROR, BUTTONS_OK, "Invalid regular expression: '%s'\n", err->message);
			g_clear_error(&err);
			return *inUse = false;
		}
	} else
		*reg = NULL;
	return true;
}

static void freeRegexes(Process* prc) {
	if (prc->extensionRegex)
		 g_regex_unref(prc->regExtension);
	if (prc->replaceRegex)
		 g_regex_unref(prc->regRename);
}

static bool initRename(Process* prc, Arguments* arg, Window* win) {
	prc->messageBehavior = arg->msgAbort ? MSGBEHAVIOR_ABORT : arg->msgContinue ? MSGBEHAVIOR_CONTINUE : MSGBEHAVIOR_ASK;

	if (!g_utf8_validate(prc->extensionName, prc->extensionNameLen, NULL)) {
		showMessage(win, MESSAGE_ERROR, BUTTONS_OK, "Invalid UTF-8 extension name");
		return false;
	}
	if (!g_utf8_validate(prc->extensionReplace, prc->extensionReplaceLen, NULL)) {
		showMessage(win, MESSAGE_ERROR, BUTTONS_OK, "Invalid UTF-8 extension replace");
		return false;
	}
	if (!g_utf8_validate(prc->rename, prc->renameLen, NULL)) {
		showMessage(win, MESSAGE_ERROR, BUTTONS_OK, "Invalid UTF-8 rename name");
		return false;
	}
	if (!g_utf8_validate(prc->replace, prc->replaceLen, NULL)) {
		showMessage(win, MESSAGE_ERROR, BUTTONS_OK, "Invalid UTF-8 rename replace");
		return false;
	}
	if (!g_utf8_validate(prc->addInsert, prc->addInsertLen, NULL)) {
		showMessage(win, MESSAGE_ERROR, BUTTONS_OK, "Invalid UTF-8 add insert string");
		return false;
	}
	if (!g_utf8_validate(prc->addPrefix, prc->addPrefixLen, NULL)) {
		showMessage(win, MESSAGE_ERROR, BUTTONS_OK, "Invalid UTF-8 add prefix");
		return false;
	}
	if (!g_utf8_validate(prc->addSuffix, prc->addSuffixLen, NULL)) {
		showMessage(win, MESSAGE_ERROR, BUTTONS_OK, "Invalid UTF-8 add suffix");
		return false;
	}
	if (!g_utf8_validate(prc->numberPadStr, prc->numberPadStrLen, NULL)) {
		showMessage(win, MESSAGE_ERROR, BUTTONS_OK, "Invalid UTF-8 number pad string");
		return false;
	}
	if (!g_utf8_validate(prc->numberPrefix, prc->numberPrefixLen, NULL)) {
		showMessage(win, MESSAGE_ERROR, BUTTONS_OK, "Invalid UTF-8 number prefix");
		return false;
	}
	if (!g_utf8_validate(prc->numberSuffix, prc->numberSuffixLen, NULL)) {
		showMessage(win, MESSAGE_ERROR, BUTTONS_OK, "Invalid UTF-8 number suffix");
		return false;
	}
	if (!g_utf8_validate(prc->dateFormat, prc->dateFormatLen, NULL)) {
		showMessage(win, MESSAGE_ERROR, BUTTONS_OK, "Invalid UTF-8 date format");
		return false;
		}
	if (!g_utf8_validate(prc->destination, prc->destinationLen, NULL)) {
		showMessage(win, MESSAGE_ERROR, BUTTONS_OK, "Invalid UTF-8 destination path");
		return false;
	}

	bool okExt = initRegex(&prc->extensionRegex, &prc->regExtension, prc->extensionName, prc->extensionNameLen, prc->extensionRegex, prc->extensionCi, win);
	bool okName = initRegex(&prc->replaceRegex, &prc->regRename, prc->rename, prc->renameLen, prc->replaceRegex, prc->replaceCi, win);
	if (!(okExt && okName)) {
		freeRegexes(prc);
		return false;
	}
	return true;
}

#ifndef CONSOLE
static bool initWindowRename(Window* win) {
	Process* prc = win->proc;
	prc->model = gtk_tree_view_get_model(win->tblFiles);
	prc->extensionName = gtk_entry_get_text(win->etExtension);
	prc->extensionNameLen = strlen(prc->extensionName);
	prc->extensionReplace = gtk_entry_get_text(win->etExtensionReplace);
	prc->extensionReplaceLen = strlen(prc->extensionReplace);
	prc->rename = gtk_entry_get_text(win->etRename);
	prc->renameLen = strlen(prc->rename);
	prc->replace = gtk_entry_get_text(win->etReplace);
	prc->replaceLen = strlen(prc->replace);
	prc->addInsert = gtk_entry_get_text(win->etAddInsert);
	prc->addInsertLen = strlen(prc->addInsert);
	prc->addPrefix = gtk_entry_get_text(win->etAddPrefix);
	prc->addPrefixLen = strlen(prc->addPrefix);
	prc->addSuffix = gtk_entry_get_text(win->etAddSuffix);
	prc->addSuffixLen = strlen(prc->addSuffix);
	prc->numberPadStr = gtk_entry_get_text(win->etNumberPadding);
	prc->numberPadStrLen = strlen(prc->numberPadStr);
	prc->numberPrefix = gtk_entry_get_text(win->etNumberPrefix);
	prc->numberPrefixLen = strlen(prc->numberPrefix);
	prc->numberSuffix = gtk_entry_get_text(win->etNumberSuffix);
	prc->numberSuffixLen = strlen(prc->numberSuffix);
	prc->dateFormat = gtk_entry_get_text(win->etDateFormat);
	prc->dateFormatLen = strlen(prc->dateFormat);
	prc->destination = gtk_entry_get_text(win->etDestination);
	prc->destinationLen = strlen(prc->destination);
	prc->numberStart = gtk_spin_button_get_value_as_int(win->sbNumberStart);
	prc->numberStep = gtk_spin_button_get_value_as_int(win->sbNumberStep);
	prc->extensionElements = gtk_spin_button_get_value_as_int(win->sbExtensionElements);
	prc->extensionMode = gtk_combo_box_get_active(GTK_COMBO_BOX(win->cmbExtensionMode));
	prc->renameMode = gtk_combo_box_get_active(GTK_COMBO_BOX(win->cmbRenameMode));
	prc->dateMode = gtk_combo_box_get_active(GTK_COMBO_BOX(win->cmbDateMode));
	prc->destinationMode = gtk_combo_box_get_active(GTK_COMBO_BOX(win->cmbDestinationMode));
	prc->removeFrom = gtk_spin_button_get_value_as_int(win->sbRemoveFrom);
	prc->removeTo = gtk_spin_button_get_value_as_int(win->sbRemoveTo);
	prc->removeFirst = gtk_spin_button_get_value_as_int(win->sbRemoveFirst);
	prc->removeLast = gtk_spin_button_get_value_as_int(win->sbRemoveLast);
	prc->addAt = gtk_spin_button_get_value_as_int(win->sbAddAt);
	prc->numberLocation = gtk_spin_button_get_value_as_int(win->sbNumberLocation);
	prc->numberPadding = gtk_spin_button_get_value_as_int(win->sbNumberPadding);
	prc->dateLocation = gtk_spin_button_get_value_as_int(win->sbDateLocation);
	prc->extensionCi = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->cbExtensionCi));
	prc->extensionRegex = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->cbExtensionRegex));
	prc->replaceCi = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->cbReplaceCi));
	prc->replaceRegex = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->cbReplaceRegex));
	prc->number = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->cbNumber));
	prc->numberBase = gtk_spin_button_get_value_as_int(win->sbNumberBase);
#ifndef _WIN32
	prc->dateLinks = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->cbDateLinks));
#endif
	prc->forward = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->cbDestinationForward));
	prc->total = gtk_tree_model_iter_n_children(prc->model, NULL);
	prc->id = prc->forward ? 0 : prc->total - 1;
	prc->step = prc->forward ? 1 : -1;
	if (prc->forward) {
		if (!gtk_tree_model_get_iter_first(prc->model, &prc->it))
			return false;
	} else if (!gtk_tree_model_iter_nth_child(prc->model, &prc->it, NULL, (int)prc->id))
		return false;
	if (!prc->destinationLen) {
		prc->destinationMode = DESTINATION_IN_PLACE;
		gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbDestinationMode), DESTINATION_IN_PLACE);
	}
	return initRename(prc, win->args, win);
}
#endif

static bool initConsoleRename(Process* prc, Arguments* arg) {
	if (!arg->files)
		return false;
	prc->extensionName = arg->extensionName ? arg->extensionName : "";
	prc->extensionReplace = arg->extensionReplace ? arg->extensionReplace : "";
	prc->rename = arg->rename ? arg->rename : "";
	prc->replace = arg->replace ? arg->replace : "";
	prc->addInsert = arg->addInsert ? arg->addInsert : "";
	prc->addPrefix = arg->addPrefix ? arg->addPrefix : "";
	prc->addSuffix = arg->addSuffix ? arg->addSuffix : "";
	prc->numberPadStr = arg->numberPadStr ? arg->numberPadStr : "";
	prc->numberPrefix = arg->numberPrefix ? arg->numberPrefix : "";
	prc->numberSuffix = arg->numberSuffix ? arg->numberSuffix : "";
	prc->dateFormat = arg->dateFormat ? arg->dateFormat : DEFAULT_DATE_FORMAT;
	prc->destination = arg->destination ? arg->destination : "";
	prc->forward = !arg->backwards;
	prc->total = arg->nFiles;
	prc->id = prc->forward ? 0 : prc->total - 1;
	prc->step = prc->forward ? 1 : -1;
	prc->numberStart = arg->numberStart;
	prc->numberStep = arg->numberStep;
	prc->extensionMode = arg->extensionMode;
	prc->renameMode = arg->renameMode;
	prc->dateMode = arg->dateMode;
	prc->destinationMode = arg->destinationMode;
	prc->extensionNameLen = strlen(prc->extensionName);
	prc->extensionReplaceLen = strlen(prc->extensionReplace);
	prc->extensionElements = arg->extensionElements;
	prc->renameLen = strlen(prc->rename);
	prc->replaceLen = strlen(prc->replace);
	prc->removeFrom = arg->removeFrom;
	prc->removeTo = arg->removeTo;
	prc->removeFirst = arg->removeFirst;
	prc->removeLast = arg->removeLast;
	prc->addInsertLen = strlen(prc->addInsert);
	prc->addAt = arg->addAt;
	prc->addPrefixLen = strlen(prc->addPrefix);
	prc->addSuffixLen = strlen(prc->addSuffix);
	prc->numberLocation = arg->numberLocation;
	prc->numberPadding = arg->numberPadding;
	prc->numberPadStrLen = strlen(prc->numberPadStr);
	prc->numberPrefixLen = strlen(prc->numberPrefix);
	prc->numberSuffixLen = strlen(prc->numberSuffix);
	prc->dateFormatLen = strlen(prc->dateFormat);
	prc->dateLocation = arg->dateLocation;
	prc->destinationLen = strlen(prc->destination);
	prc->extensionCi = arg->extensionCi;
	prc->extensionRegex = arg->extensionRegex;
	prc->replaceCi = arg->replaceCi;
	prc->replaceRegex = arg->replaceRegex;
	prc->number = arg->number;
	prc->numberBase = arg->numberBase;
#ifndef _WIN32
	prc->dateLinks = arg->dateLinks;
#endif
	if (!arg->destination)
		prc->destinationMode = DESTINATION_IN_PLACE;
	return initRename(prc, arg, NULL);
}

#ifndef CONSOLE
static void setProgressBar(GtkProgressBar* bar, size_t pos, size_t total, bool fwd) {
	if (!fwd)
		pos = total - pos - 1;
	char text[MAX_DIGITS_I32D * 2 + 2];
	sprintf(text, "%zu/%zu", pos, total);
	gtk_progress_bar_set_fraction(bar, (double)pos / (double)total);
	gtk_progress_bar_set_text(bar, text);
}

static void setWidgetsSensitive(Window* win, bool sensitive) {
	Process* prc = win->proc;
	gtk_widget_set_sensitive(GTK_WIDGET(win->btAddFiles), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->btAddFolders), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->btOptions), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->tblFiles), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->cmbExtensionMode), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->etExtension), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->etExtensionReplace), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->cbExtensionCi), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->cbExtensionRegex), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->sbExtensionElements), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->cmbRenameMode), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->etRename), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->etReplace), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->cbReplaceCi), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->cbReplaceRegex), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->sbRemoveFrom), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->sbRemoveTo), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->sbRemoveFirst), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->sbRemoveLast), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->etAddInsert), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->sbAddAt), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->etAddPrefix), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->etAddSuffix), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->cbNumber), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->sbNumberLocation), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->sbNumberBase), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->sbNumberStart), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->sbNumberStep), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->sbNumberPadding), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->etNumberPadding), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->etNumberPrefix), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->etNumberSuffix), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->cmbDateMode), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->etDateFormat), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->sbDateLocation), sensitive);
#ifndef _WIN32
	gtk_widget_set_sensitive(GTK_WIDGET(win->cbDateLinks), sensitive);
#endif
	gtk_widget_set_sensitive(GTK_WIDGET(win->cmbDestinationMode), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->etDestination), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->btDestination), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->btPreview), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(win->cbDestinationForward), sensitive);
	if (sensitive)
		gtk_button_set_label(win->btRename, "Rename");
	else {
		gtk_button_set_label(win->btRename, "Abort");
		setProgressBar(win->pbRename, 0, prc->total, true);
	}
}

static void setOriginalNameWindow(Process* prc, char** name, size_t* nameLen, char** dirc, size_t* dircLen) {
	gtk_tree_model_get(prc->model, &prc->it, FCOL_OLD_NAME, name, FCOL_DIRECTORY, dirc, FCOL_INVALID);
	*nameLen = strlen(*name);
	*dircLen = strlen(*dirc);
	memcpy(prc->original, *dirc, *dircLen * sizeof(char));
	memcpy(prc->original + *dircLen, *name, (*nameLen + 1) * sizeof(char));
}
#endif

static void setOriginalNameConsole(Process* prc, Arguments* arg, size_t* plen) {
	const char* path = g_file_peek_path(arg->files[prc->id]);
	*plen = strlen(path);
	memcpy(prc->original, path, (*plen + 1) * sizeof(char));
#ifdef _WIN32
	unbackslashify(prc->original);
#endif
}

static bool initDestination(Process* prc, Window* win) {
	if (prc->destinationMode == DESTINATION_IN_PLACE) {
		prc->dstdirLen = 0;
		return true;
	}

	struct stat ps;
	if (stat(prc->destination, &ps) || !S_ISDIR(ps.st_mode)) {
		showMessage(win, MESSAGE_ERROR, BUTTONS_OK, "Destination '%s' is not a valid directory", prc->destination);
		freeRegexes(prc);
		return false;
	}

	bool extend = prc->destination[prc->destinationLen - 1] != '/';
	prc->dstdirLen = prc->destinationLen + extend;
	if (prc->dstdirLen >= PATH_MAX) {
		showMessage(win, MESSAGE_ERROR, BUTTONS_OK, "Directory '%s' is too long", prc->destination);
		freeRegexes(prc);
		return false;
	}

	memcpy(prc->dstdir, prc->destination, (prc->destinationLen + 1) * sizeof(char));
	if (extend)
		strcpy(prc->dstdir + prc->destinationLen, "/");
	return true;
}

static ResponseType processFile(Process* prc, const char* oldn, size_t olen, Window* win) {
	if (prc->dstdirLen + prc->nameLen >= PATH_MAX)
		return continueError(prc, win, "Path '%s%s' is too long.", prc->dstdir, prc->name);

	memcpy(prc->dstdir + prc->dstdirLen, prc->name, (prc->nameLen + 1) * sizeof(char));
#ifdef _WIN32
	int rc = (int (*const[4])(const char*, const char*)){ rename, rename, copyFile, createSymlink }[prc->destinationMode](prc->original, prc->dstdir);
#else
	int rc = (int (*const[4])(const char*, const char*)){ rename, rename, copyFile, symlink }[prc->destinationMode](prc->original, prc->dstdir);
#endif
	return rc ? continueError(prc, win, "Failed to rename '%s' to '%s':\n%s", prc->original, prc->dstdir, strerror(errno)) : RESPONSE_NONE;
}

#ifndef CONSOLE
static gboolean updateProgressBar(Window* win) {
	Process* prc = win->proc;
	setProgressBar(win->pbRename, prc->id, prc->total, prc->forward);
	return G_SOURCE_REMOVE;
}

static gboolean updateTableNames(TableUpdate* tu) {
	gtk_list_store_set(tu->win->lsFiles, &tu->iter, FCOL_OLD_NAME, tu->name, FCOL_NEW_NAME, tu->name, FCOL_INVALID);
	free(tu);
	return G_SOURCE_REMOVE;
}

void joinThread(Process* prc) {
	if (prc->thread) {
		g_thread_join(prc->thread);
		prc->thread = NULL;
	}
	freeRegexes(prc);
}

static gboolean finishWindowRenameProc(Window* win) {
	joinThread(win->proc);
	setWidgetsSensitive(win, true);
	autoPreview(win);
	return G_SOURCE_REMOVE;
}

static void* windowRenameProc(void* userData) {
	Window* win = userData;
	Process* prc = win->proc;
	ResponseType rc;
	ThreadCode tc;
	bool inThread = prc->threadCode & THREAD_SAME;
	char* oldName;
	char* oldDirc;
	size_t oldNameLen, oldDircLen;
	do {
		setOriginalNameWindow(prc, &oldName, &oldNameLen, &oldDirc, &oldDircLen);
		rc = processName(prc, oldName, oldNameLen, win);
		if (rc == RESPONSE_NONE) {
			if (prc->destinationMode == DESTINATION_IN_PLACE) {
				prc->dstdirLen = oldDircLen;
				memcpy(prc->dstdir, oldDirc, (oldDircLen + 1) * sizeof(char));
			}

			rc = processFile(prc, oldName, oldNameLen, win);
			if (rc == RESPONSE_NONE) {
				if (inThread) {
					TableUpdate* tu = malloc(sizeof(TableUpdate));
					tu->win = win;
					tu->iter = prc->it;
					memcpy(tu->name, prc->name + prc->dstdirLen, (prc->nameLen + 1) * sizeof(char));
					g_idle_add(G_SOURCE_FUNC(updateTableNames), tu);
				} else
					gtk_list_store_set(win->lsFiles, &prc->it, FCOL_OLD_NAME, prc->name, FCOL_NEW_NAME, prc->name, FCOL_INVALID);
			}
		}
		g_free(oldName);
		g_free(oldDirc);
		prc->id += prc->step;
		if (inThread)
			g_idle_add(G_SOURCE_FUNC(updateProgressBar), win);
		else
			setProgressBar(win->pbRename, prc->id, prc->total, prc->forward);

		tc = prc->threadCode & ~THREAD_SAME;
	} while (tc == THREAD_RUN && (rc == RESPONSE_NONE || rc == RESPONSE_YES) && (prc->forward ? gtk_tree_model_iter_next(prc->model, &prc->it) : gtk_tree_model_iter_previous(prc->model, &prc->it)));
	if (tc != THREAD_DISCARD)
		g_idle_add(G_SOURCE_FUNC(finishWindowRenameProc), win);
	return NULL;
}

void windowRename(Window* win) {
	Process* prc = win->proc;
	if (!initWindowRename(win))
		return;
	if (!initDestination(prc, win))
		return;

	prc->threadCode = THREAD_RUN;
	setWidgetsSensitive(win, false);
	if (win->singleThread) {
		prc->threadCode |= THREAD_SAME;
		windowRenameProc(win);
		return;
	}

	GError* err = NULL;
	prc->thread = g_thread_try_new(NULL, windowRenameProc, win, &err);
	if (!prc->thread) {
		ResponseType rc = showMessage(win, MESSAGE_ERROR, BUTTONS_YES_NO, "Failed to create thread: %s\nRun on main thread?", err->message);
		g_clear_error(&err);
		if (rc == RESPONSE_YES)
			windowRenameProc(win);
		else
			finishWindowRenameProc(win);
	}
}

void windowPreview(Window* win) {
	Process* prc = win->proc;
	if (!initWindowRename(win))
		return;

	ResponseType rc;
	char* oldName;
	char* oldDirc;
	size_t oldNameLen, oldDircLen;
	do {
		setOriginalNameWindow(prc, &oldName, &oldNameLen, &oldDirc, &oldDircLen);
		rc = processName(prc, oldName, oldNameLen, win);
		if (rc == RESPONSE_NONE)
			gtk_list_store_set(win->lsFiles, &prc->it, FCOL_NEW_NAME, prc->name, FCOL_INVALID);
		g_free(oldName);
		g_free(oldDirc);
		prc->id += prc->step;
	} while ((rc == RESPONSE_NONE || rc == RESPONSE_YES) && (prc->forward ? gtk_tree_model_iter_next(prc->model, &prc->it) : gtk_tree_model_iter_previous(prc->model, &prc->it)));
	freeRegexes(prc);
}
#endif

void consoleRename(Window* win) {
	Process* prc = win->proc;
	Arguments* arg = win->args;
	if (!initConsoleRename(prc, arg))
		return;
	if (!initDestination(prc, NULL))
		return;

	ResponseType rc;
	size_t plen;
	do {
		setOriginalNameConsole(prc, arg, &plen);
		const char* oldn = memrchr(prc->original, '/', plen * sizeof(char));
		if (oldn) {
			++oldn;
			if (prc->destinationMode == DESTINATION_IN_PLACE) {
				prc->dstdirLen = oldn - prc->original;
				memcpy(prc->dstdir, prc->original, prc->dstdirLen * sizeof(char));
				prc->dstdir[prc->dstdirLen] = '\0';
			}
		} else {
			if (prc->destinationMode == DESTINATION_IN_PLACE) {
				prc->dstdirLen = 0;
				prc->dstdir[0] = '\0';
			}
			oldn = prc->original;
		}
		size_t olen = prc->original + plen - oldn;

		rc = processName(prc, oldn, olen, NULL);
		if (rc == RESPONSE_NONE) {
			rc = processFile(prc, oldn, olen, NULL);
			if (rc == RESPONSE_NONE && !arg->noAutoPreview) {
				if (prc->destinationMode == DESTINATION_IN_PLACE)
					g_print("'%s' -> '%s'\n", oldn, prc->name);
				else
					g_print("'%s' -> '%s%s'\n", prc->original, prc->dstdir, prc->name);
			}
		}
		prc->id += prc->step;
	} while ((rc == RESPONSE_NONE || rc == RESPONSE_YES) && prc->id < arg->nFiles);
	freeRegexes(prc);
}

void consolePreview(Window* win) {
	Process* prc = win->proc;
	Arguments* arg = win->args;
	if (!initConsoleRename(prc, arg))
		return;

	ResponseType rc;
	size_t olen;
	do {
		setOriginalNameConsole(prc, arg, &olen);
		const char* oldn = memrchr(prc->original, '/', olen * sizeof(char));
		if (oldn)
			olen = prc->original + olen - ++oldn;
		else
			oldn = prc->original;

		rc = processName(prc, oldn, olen, NULL);
		if (rc == RESPONSE_NONE)
			g_print("'%s' -> '%s'\n", oldn, prc->name);
		prc->id += prc->step;
	} while ((rc == RESPONSE_NONE || rc == RESPONSE_YES) && prc->id < arg->nFiles);
	freeRegexes(prc);
}
