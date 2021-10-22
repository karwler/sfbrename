#include "arguments.h"
#include "main.h"
#include "rename.h"
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef __MINGW32__
#include <shlwapi.h>
#else
#include <sys/sendfile.h>
#endif

#define MAX_DIGITS_I32D 10
#define CONTINUE_TEXT "\nContinue?"

#ifndef CONSOLE
typedef struct TableUpdate {
	Window* win;
	GtkTreeIter iter;
	char name[FILENAME_MAX];
} TableUpdate;
#endif

static char* joinPath(const char* dir, size_t dlen, const char* file, size_t flen) {
	char* path = malloc((dlen + flen + 2) * sizeof(char));
	memcpy(path, dir, dlen * sizeof(char));
	path[dlen] = '/';
	memcpy(path + dlen + 1, file, (flen + 1) * sizeof(char));
	return path;
}

static int copyFile(const char* src, const char* dst) {
	int in = open(src, O_RDONLY);
	if (in == -1)
		return -1;

	struct stat ps;
	if (fstat(in, &ps)) {
		close(in);
		return -1;
	}
	if (S_ISDIR(ps.st_mode)) {
		close(in);
		DIR* dir = opendir(src);
		if (!dir)
			return -1;

		int rc = 0;
		size_t slen = strlen(src);
		size_t dlen = strlen(dst);
		for (struct dirent* entry = readdir(dir); entry; entry = readdir(dir)) {
			size_t nlen = strlen(entry->d_name);
			char* from = joinPath(src, slen, entry->d_name, nlen);
			char* to = joinPath(dst, dlen, entry->d_name, nlen);
#ifdef __MINGW32__
			if (!stat(from, &ps) && S_ISDIR(ps.st_mode))
				mkdir(to);
#else
			if (entry->d_type == DT_DIR)
				mkdir(to, !lstat(from, &ps) ? ps.st_mode & (S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO) : S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
#endif
			rc |= copyFile(from, to);
			free(from);
			free(to);
		}
		closedir(dir);
		return rc;
	}

#ifdef __MINGW32__
	int out = creat(dst, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
#else
	int out = creat(dst, ps.st_mode & (S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO));
#endif
	if (out == -1) {
		close(in);
		return -1;
	}
#ifdef __MINGW32__
	int rc = -1;
	long bytes = lseek(in, 0, SEEK_END);
	if (bytes != -1 && lseek(in, 0, SEEK_SET) != -1) {
		uint8* data = malloc(bytes);
		read(in, data, bytes);
		write(out, data, bytes);
		free(data);
		rc = 0;
	}
#else
	off_t bytes = 0;
	int rc = sendfile(out, in, &bytes, (size_t)ps.st_size) == -1;
#endif
	close(in);
	close(out);
	return rc;
}

#ifdef __MINGW32__
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

static int createSymlink(const char* src, const char* dst) {
	struct stat ps;
	DWORD flags = !stat(src, &ps) && S_ISDIR(ps.st_mode) ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0;
	wchar* wsrc = stow(src);
	wchar* wdst = stow(dst);
	int rc = CreateSymbolicLinkW(wdst, wsrc, flags);
	free(wsrc);
	free(wdst);
	return rc ? 0 : -1;
}
#endif

static ResponseType continueError(Process* prc, Window* win, const char* format, ...) {
	va_list args;
	va_start(args, format);
	ResponseType rc;
	if (prc->forward ? prc->id < prc->total - 1 : prc->id) {
		size_t flen = strlen(format);
		char* fmt = malloc((flen + sizeof(CONTINUE_TEXT)) * sizeof(char));
		memcpy(fmt, format, flen * sizeof(char));
		strcpy(fmt + flen, CONTINUE_TEXT);
		rc = showMessageV(win, MESSAGE_ERROR, BUTTONS_YES_NO, fmt, args);
		free(fmt);
	} else
		rc = showMessageV(win, MESSAGE_ERROR, BUTTONS_OK, format, args);
	va_end(args);
	return rc;
}

static size_t replaceRegex(char* name, regex_t* reg, const char* new, ushort nlen) {
	char buf[FILENAME_MAX];
	size_t blen = 0;
	char* last = name;
	for (regmatch_t match; !regexec(reg, last, 1, &match, 0) && match.rm_so != match.rm_eo;) {
		if (blen + (size_t)match.rm_so + nlen >= FILENAME_MAX)
			return SIZE_MAX;

		memcpy(buf + blen, last, (size_t)match.rm_so * sizeof(char));
		memcpy(buf + blen + (size_t)match.rm_so, new, nlen * sizeof(char));
		blen += (size_t)match.rm_so + nlen;
		last += match.rm_eo;
	}
	size_t rest = strlen(last);
	size_t tlen = blen + rest;
	if (tlen >= FILENAME_MAX)
		return SIZE_MAX;

	memcpy(buf + blen, last, (rest + 1) * sizeof(char));
	memcpy(name, buf, (tlen + 1) * sizeof(char));
	return tlen;
}

static size_t replaceStrings(char* name, const char* old, ushort olen, const char* new, ushort nlen, bool ci) {
#ifdef __MINGW32__
	char* (*scmp)(const char*, const char*) = ci ? StrStrIA : strstr;
#else
	char* (*scmp)(const char*, const char*) = ci ? strcasestr : strstr;
#endif
	char buf[FILENAME_MAX];
	size_t blen = 0;
	char* last = name;
	for (char* it; (it = scmp(last, old));) {
		size_t diff = (size_t)(it - last);
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

static ResponseType nameRename(Process* prc, regex_t* reg, Window* win) {
	switch (prc->renameMode) {
	case RENAME_KEEP:
		return RESPONSE_NONE;
	case RENAME_RENAME:
		prc->nameLen = prc->renameLen;
		if (prc->nameLen < FILENAME_MAX)
			memcpy(prc->name, prc->rename, (prc->nameLen + 1) * sizeof(char));
		break;
	case RENAME_REPLACE:
		if (reg)
			prc->nameLen = replaceRegex(prc->name, reg, prc->replace, prc->replaceLen);
		else if (prc->renameLen)
			prc->nameLen = replaceStrings(prc->name, prc->rename, prc->renameLen, prc->replace, prc->replaceLen, prc->replaceCi);
		break;
	case RENAME_LOWER_CASE:
		prc->nameLen = moveGName(prc->name, g_utf8_strdown(prc->name, (ssize_t)prc->nameLen));
		break;
	case RENAME_UPPER_CASE:
		prc->nameLen = moveGName(prc->name, g_utf8_strup(prc->name, (ssize_t)prc->nameLen));
		break;
	case RENAME_REVERSE:
		prc->nameLen = moveGName(prc->name, g_utf8_strreverse(prc->name, (ssize_t)prc->nameLen));
	}

	if (prc->nameLen >= FILENAME_MAX)
		return continueError(prc, win, "Filename became too long during rename.");
	return RESPONSE_NONE;
}

static void nameRemove(Process* prc) {
	size_t ulen = (size_t)g_utf8_strlen(prc->name, (ssize_t)prc->nameLen);
	if (prc->removeTo > prc->removeFrom) {
		size_t diff = prc->removeTo - prc->removeFrom;
		if (diff < ulen) {
			char* src = g_utf8_offset_to_pointer(prc->name, prc->removeTo);
			size_t cnt = (size_t)(prc->name + prc->nameLen - src);
			memmove(g_utf8_offset_to_pointer(prc->name, prc->removeFrom), src, (cnt + 1) * sizeof(char));
			prc->nameLen -= cnt;
			ulen -= diff;
		} else {
			prc->nameLen = ulen = 0;
			prc->name[0] = '\0';
		}
	}

	if (prc->removeFirst) {
		if (prc->removeFirst < ulen) {
			char* src = g_utf8_offset_to_pointer(prc->name, prc->removeFirst);
			prc->nameLen = (size_t)(prc->name + prc->nameLen - src);
			ulen -= prc->removeFirst;
			memmove(prc->name, src, (prc->nameLen + 1) * sizeof(char));
		} else {
			prc->nameLen = ulen = 0;
			prc->name[0] = '\0';
		}
	}

	if (prc->removeLast) {
		if (prc->removeLast < ulen) {
			char* src = g_utf8_offset_to_pointer(prc->name, (ssize_t)(ulen - prc->removeLast));
			prc->nameLen = (size_t)(src - prc->name);
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

		char* pos;
		size_t ulen = (size_t)g_utf8_strlen(prc->name, (ssize_t)prc->nameLen);
		if (prc->addAt >= 0)
			pos = (size_t)prc->addAt <= ulen ? g_utf8_offset_to_pointer(prc->name, prc->addAt) : prc->name + prc->nameLen;
		else
			pos = (size_t)-prc->addAt <= ulen ? g_utf8_offset_to_pointer(prc->name, (ssize_t)(ulen + (size_t)prc->addAt + 1)) : prc->name;
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
	size_t ulen = (size_t)g_utf8_strlen(prc->name, (ssize_t)prc->nameLen);
	char* pos;
	if (prc->numberLocation < 0)
		pos = (size_t)-prc->numberLocation <= ulen ? g_utf8_offset_to_pointer(prc->name, (ssize_t)(ulen + (size_t)prc->numberLocation + 1)) : prc->name;
	else
		pos = (size_t)prc->numberLocation <= ulen ? g_utf8_offset_to_pointer(prc->name, prc->numberLocation) : prc->name + prc->nameLen;

	int64 val = (int64)prc->id * prc->numberStep + prc->numberStart;
	bool negative = val < 0;
	const char* digits = prc->numberBase < 64 ? "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz+" : "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	char buf[MAX_DIGITS_I64B];
	size_t blen = 0;
	if (val) {
		for (uint64 num = (uint64)(!negative ? val : -val); num; num /= prc->numberBase)
			buf[blen++] = digits[num % prc->numberBase];
	} else
		buf[blen++] = '0';

	size_t padLeft = blen < prc->numberPadding && prc->numberPadStrLen ? (prc->numberPadding - blen) * prc->numberPadStrLen : 0;
	size_t pbslen = prc->numberPrefixLen + negative + padLeft + blen + prc->numberSuffixLen;
	if (prc->nameLen + pbslen >= FILENAME_MAX)
		return continueError(prc, win, "Filename '%s' became too long while adding number.", prc->name);

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

static size_t processExtension(Process* prc, const char* str, size_t slen) {
	size_t dot;
	if (prc->extensionElements < 0) {
		char* pos = memchr(str, '.', slen);
		if (pos == str)
			pos = memchr(str + 1, '.', slen - 1);
		dot = pos ? (size_t)(pos - str) : slen;
	} else {
		dot = slen;
		for (int i = 0; dot && i < prc->extensionElements; ++i) {
			char* pos = memrchr(str, '.', dot);
			if (!pos)
				break;
			dot = (size_t)(pos - str);
		}
	}
	size_t elen = slen - dot;
	memcpy(prc->name, str, dot * sizeof(char));
	prc->name[dot] = '\0';
	if (!elen) {
		prc->extension[0] = '\0';
		return 0;
	}

	switch (prc->extensionMode) {
	case RENAME_KEEP:
		memcpy(prc->extension, str + dot, (elen + 1) * sizeof(char));
		break;
	case RENAME_RENAME:
		elen = prc->extensionNameLen;
		if (elen < FILENAME_MAX)
			memcpy(prc->extension, prc->extensionName, (elen + 1) * sizeof(char));
		break;
	case RENAME_REPLACE:
		memcpy(prc->extension, str + dot, (elen + 1) * sizeof(char));
		if (prc->extensionRegex)
			return replaceRegex(prc->extension, &prc->regExtension, prc->extensionReplace, prc->extensionReplaceLen);
		if (prc->extensionNameLen)
			return replaceStrings(prc->extension, prc->extensionName, prc->extensionNameLen, prc->extensionReplace, prc->extensionReplaceLen, prc->extensionCi);
		break;
	case RENAME_LOWER_CASE:
		return moveGName(prc->extension, g_utf8_strdown(str + dot, (ssize_t)elen));
	case RENAME_UPPER_CASE:
		return moveGName(prc->extension, g_utf8_strup(str + dot, (ssize_t)elen));
	case RENAME_REVERSE:
		return moveGName(prc->extension, g_utf8_strreverse(str + dot, (ssize_t)elen));
	}
	return elen;
}

static ResponseType processName(Process* prc, const char* oldn, size_t olen, Window* win) {
	if (olen >= FILENAME_MAX)
		return continueError(prc, win, "Filename '%s' is too long.", oldn);
	size_t elen = processExtension(prc, oldn, olen);
	if (elen >= FILENAME_MAX)
		return continueError(prc, win, "Extension became too long.");
	prc->nameLen = olen - elen;

	ResponseType rc = nameRename(prc, prc->replaceRegex ? &prc->regRename : NULL, win);
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

	if (prc->nameLen + elen >= FILENAME_MAX)
		return continueError(prc, win, "Filename '%s' became too long while reapplying extension.", prc->name);
	memcpy(prc->name + prc->nameLen, prc->extension, (elen + 1) * sizeof(char));
	prc->nameLen += elen;
	return rc;
}

static bool initRegex(bool* inUse, regex_t* reg, const char* expr, ushort exlen, bool use, bool ci, Window* win) {
	*inUse = use && exlen;
	if (*inUse) {
		int err = regcomp(reg, expr, REG_EXTENDED | (ci ? REG_ICASE : 0));
		if (err) {
			size_t elen = regerror(err, reg, NULL, 0);
			char* estr = malloc(elen * sizeof(char));
			regerror(err, reg, estr, elen);
			showMessage(win, MESSAGE_ERROR, BUTTONS_OK, "Invalid regular expression: '%s'\n", estr);
			free(estr);
			return *inUse = false;
		}
	}
	return true;
}

static void freeRegexes(Process* prc) {
	if (prc->extensionRegex)
		regfree(&prc->regExtension);
	if (prc->replaceRegex)
		regfree(&prc->regRename);
}

static bool initRename(Process* prc, Window* win) {
	if (!g_utf8_validate_len(prc->extensionName, prc->extensionNameLen, NULL)) {
		showMessage(win, MESSAGE_ERROR, BUTTONS_OK, "Invalid UTF-8 extension name");
		return false;
	}
	if (!g_utf8_validate_len(prc->extensionReplace, prc->extensionReplaceLen, NULL)) {
		showMessage(win, MESSAGE_ERROR, BUTTONS_OK, "Invalid UTF-8 extension replace");
		return false;
	}
	if (!g_utf8_validate_len(prc->rename, prc->renameLen, NULL)) {
		showMessage(win, MESSAGE_ERROR, BUTTONS_OK, "Invalid UTF-8 rename name");
		return false;
	}
	if (!g_utf8_validate_len(prc->replace, prc->replaceLen, NULL)) {
		showMessage(win, MESSAGE_ERROR, BUTTONS_OK, "Invalid UTF-8 rename replace");
		return false;
	}
	if (!g_utf8_validate_len(prc->addInsert, prc->addInsertLen, NULL)) {
		showMessage(win, MESSAGE_ERROR, BUTTONS_OK, "Invalid UTF-8 add insert string");
		return false;
	}
	if (!g_utf8_validate_len(prc->addPrefix, prc->addPrefixLen, NULL)) {
		showMessage(win, MESSAGE_ERROR, BUTTONS_OK, "Invalid UTF-8 add prefix");
		return false;
	}
	if (!g_utf8_validate_len(prc->addSuffix, prc->addSuffixLen, NULL)) {
		showMessage(win, MESSAGE_ERROR, BUTTONS_OK, "Invalid UTF-8 add suffix");
		return false;
	}
	if (!g_utf8_validate_len(prc->numberPadStr, prc->numberPadStrLen, NULL)) {
		showMessage(win, MESSAGE_ERROR, BUTTONS_OK, "Invalid UTF-8 number pad string");
		return false;
	}
	if (!g_utf8_validate_len(prc->numberPrefix, prc->numberPrefixLen, NULL)) {
		showMessage(win, MESSAGE_ERROR, BUTTONS_OK, "Invalid UTF-8 number prefix");
		return false;
	}
	if (!g_utf8_validate_len(prc->numberSuffix, prc->numberSuffixLen, NULL)) {
		showMessage(win, MESSAGE_ERROR, BUTTONS_OK, "Invalid UTF-8 number suffix");
		return false;
	}
	if (!g_utf8_validate_len(prc->destination, prc->destinationLen, NULL)) {
		showMessage(win, MESSAGE_ERROR, BUTTONS_OK, "Invalid UTF-8 destination path");
		return false;
	}

	if (!initRegex(&prc->extensionRegex, &prc->regExtension, prc->extensionReplace, prc->extensionReplaceLen, prc->extensionRegex, prc->extensionCi, win))
		return false;
	if (!initRegex(&prc->replaceRegex, &prc->regRename, prc->replace, prc->replaceLen, prc->replaceRegex, prc->replaceCi, win)) {
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
	prc->extensionNameLen = (ushort)strlen(prc->extensionName);
	prc->extensionReplace = gtk_entry_get_text(win->etExtensionReplace);
	prc->extensionReplaceLen = (ushort)strlen(prc->extensionReplace);
	prc->rename = gtk_entry_get_text(win->etRename);
	prc->renameLen = (ushort)strlen(prc->rename);
	prc->replace = gtk_entry_get_text(win->etReplace);
	prc->replaceLen = (ushort)strlen(prc->replace);
	prc->addInsert = gtk_entry_get_text(win->etAddInsert);
	prc->addInsertLen = (ushort)strlen(prc->addInsert);
	prc->addPrefix = gtk_entry_get_text(win->etAddPrefix);
	prc->addPrefixLen = (ushort)strlen(prc->addPrefix);
	prc->addSuffix = gtk_entry_get_text(win->etAddSuffix);
	prc->addSuffixLen = (ushort)strlen(prc->addSuffix);
	prc->numberPadStr = gtk_entry_get_text(win->etNumberPadding);
	prc->numberPadStrLen = (ushort)strlen(prc->numberPadStr);
	prc->numberPrefix = gtk_entry_get_text(win->etNumberPrefix);
	prc->numberPrefixLen = (ushort)strlen(prc->numberPrefix);
	prc->numberSuffix = gtk_entry_get_text(win->etNumberSuffix);
	prc->numberSuffixLen = (ushort)strlen(prc->numberSuffix);
	prc->destination = gtk_entry_get_text(win->etDestination);
	prc->destinationLen = (ushort)strlen(prc->destination);
	prc->extensionMode = (uint)gtk_combo_box_get_active(GTK_COMBO_BOX(win->cmbExtensionMode));
	prc->renameMode = (uint)gtk_combo_box_get_active(GTK_COMBO_BOX(win->cmbRenameMode));
	prc->numberStart = gtk_spin_button_get_value_as_int(win->sbNumberStart);
	prc->numberStep = gtk_spin_button_get_value_as_int(win->sbNumberStep);
	prc->destinationMode = (uint)gtk_combo_box_get_active(GTK_COMBO_BOX(win->cmbDestinationMode));
	prc->extensionElements = (short)gtk_spin_button_get_value_as_int(win->sbExtensionElements);
	prc->removeFrom = (ushort)gtk_spin_button_get_value_as_int(win->sbRemoveFrom);
	prc->removeTo = (ushort)gtk_spin_button_get_value_as_int(win->sbRemoveTo);
	prc->removeFirst = (ushort)gtk_spin_button_get_value_as_int(win->sbRemoveFirst);
	prc->removeLast = (ushort)gtk_spin_button_get_value_as_int(win->sbRemoveLast);
	prc->addAt = (short)gtk_spin_button_get_value_as_int(win->sbAddAt);
	prc->numberLocation = (short)gtk_spin_button_get_value_as_int(win->sbNumberLocation);
	prc->numberPadding = (ushort)gtk_spin_button_get_value_as_int(win->sbNumberPadding);
	prc->extensionCi = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->cbExtensionCi));
	prc->extensionRegex = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->cbExtensionRegex));
	prc->replaceCi = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->cbReplaceCi));
	prc->replaceRegex = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->cbReplaceRegex));
	prc->number = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->cbNumber));
	prc->numberBase = (uint8)gtk_spin_button_get_value_as_int(win->sbNumberBase);
	prc->forward = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->cbDestinationForward));
	prc->total = (size_t)gtk_tree_model_iter_n_children(prc->model, NULL);
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
	return initRename(prc, win);
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
	prc->destination = arg->destination ? arg->destination : "";
	prc->forward = !arg->backwards;
	prc->id = prc->forward ? 0 : arg->nFiles - 1;
	prc->step = prc->forward ? 1 : -1;
	prc->extensionMode = arg->extensionMode;
	prc->renameMode = arg->renameMode;
	prc->numberStart = (int)arg->numberStart;
	prc->numberStep = (int)arg->numberStep;
	prc->destinationMode = arg->destinationMode;
	prc->extensionNameLen = (ushort)strlen(prc->extensionName);
	prc->extensionReplaceLen = (ushort)strlen(prc->extensionReplace);
	prc->extensionElements = (short)arg->extensionElements;
	prc->renameLen = (ushort)strlen(prc->rename);
	prc->replaceLen = (ushort)strlen(prc->replace);
	prc->removeFrom = (ushort)arg->removeFrom;
	prc->removeTo = (ushort)arg->removeTo;
	prc->removeFirst = (ushort)arg->removeFirst;
	prc->removeLast = (ushort)arg->removeLast;
	prc->addInsertLen = (ushort)strlen(prc->addInsert);
	prc->addAt = (short)arg->addAt;
	prc->addPrefixLen = (ushort)strlen(prc->addPrefix);
	prc->addSuffixLen = (ushort)strlen(prc->addSuffix);
	prc->numberLocation = (short)arg->numberLocation;
	prc->numberPadding = (ushort)arg->numberPadding;
	prc->numberPadStrLen = (ushort)strlen(prc->numberPadStr);
	prc->numberPrefixLen = (ushort)strlen(prc->numberPrefix);
	prc->numberSuffixLen = (ushort)strlen(prc->numberSuffix);
	prc->destinationLen = (ushort)strlen(prc->destination);
	prc->extensionCi = arg->extensionCi;
	prc->extensionRegex = arg->extensionRegex;
	prc->replaceCi = arg->replaceCi;
	prc->replaceRegex = arg->replaceRegex;
	prc->number = arg->number;
	prc->numberBase = (uint8)arg->numberBase;
	if (!arg->destination)
		prc->destinationMode = DESTINATION_IN_PLACE;
	return initRename(prc, NULL);
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
#endif

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
		return continueError(prc, win, "Directory '%s' and filename '%s' are too long.", prc->dstdir, prc->name);
	if (prc->dstdirLen + olen >= PATH_MAX)
		return continueError(prc, win, "Directory '%s' and filename '%s' are too long.", prc->dstdir, oldn);

	memcpy(prc->dstdir + prc->dstdirLen, oldn, (olen + 1) * sizeof(char));
	memmove(prc->name + prc->dstdirLen, prc->name, (prc->nameLen + 1) * sizeof(char));
	memcpy(prc->name, prc->dstdir, prc->dstdirLen * sizeof(char));
#ifdef __MINGW32__
	int rc = (int (*const[4])(const char*, const char*)){ rename, rename, copyFile, createSymlink }[prc->destinationMode](prc->dstdir, prc->name);
#else
	int rc = (int (*const[4])(const char*, const char*)){ rename, rename, copyFile, symlink }[prc->destinationMode](prc->dstdir, prc->name);
#endif
	return rc ? continueError(prc, win, "Failed to rename '%s' to '%s':\n%s", oldn, prc->name + prc->dstdirLen, strerror(errno)) : RESPONSE_NONE;
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
	g_mutex_clear(&prc->mutex);
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
	bool inThread = g_thread_self()->func;
	do {
		char* oldn;
		gtk_tree_model_get(prc->model, &prc->it, FCOL_OLD_NAME, &oldn, FCOL_INVALID);
		size_t olen = strlen(oldn);
		rc = processName(prc, oldn, olen, win);
		if (rc == RESPONSE_NONE) {
			if (prc->destinationMode == DESTINATION_IN_PLACE) {
				char* tmp;
				gtk_tree_model_get(prc->model, &prc->it, FCOL_DIRECTORY, &tmp, FCOL_INVALID);
				prc->dstdirLen = strlen(tmp);
				memcpy(prc->dstdir, tmp, (prc->dstdirLen + 1) * sizeof(char));
				g_free(tmp);
			}

			rc = processFile(prc, oldn, olen, win);
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
		g_free(oldn);
		prc->id += (size_t)prc->step;
		if (inThread)
			g_idle_add(G_SOURCE_FUNC(updateProgressBar), win);
		else
			setProgressBar(win->pbRename, prc->id, prc->total, prc->forward);

		g_mutex_lock(&prc->mutex);
		tc = prc->threadCode;
		g_mutex_unlock(&prc->mutex);
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
	g_mutex_init(&prc->mutex);
	setWidgetsSensitive(win, false);

	if (win->singleThread) {
		windowRenameProc(win);
		return;
	}

	GError* error;
	prc->thread = g_thread_try_new(NULL, windowRenameProc, win, &error);
	if (!prc->thread) {
		ResponseType rc = showMessage(win, MESSAGE_ERROR, BUTTONS_YES_NO, "Failed to create thread: %s\nRun on main thread?", error->message);
		g_clear_error(&error);
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
	do {
		char* oldn;
		gtk_tree_model_get(prc->model, &prc->it, FCOL_OLD_NAME, &oldn, FCOL_INVALID);
		rc = processName(prc, oldn, strlen(oldn), win);
		if (rc == RESPONSE_NONE)
			gtk_list_store_set(win->lsFiles, &prc->it, FCOL_NEW_NAME, prc->name, FCOL_INVALID);
		g_free(oldn);
		prc->id += (size_t)prc->step;
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
	do {
		const char* path = g_file_peek_path(arg->files[prc->id]);
		size_t plen = strlen(path);
		const char* sl = memrchr(path, '/', plen * sizeof(char));
		if (sl) {
			if (prc->destinationMode == DESTINATION_IN_PLACE) {
				prc->dstdirLen = (size_t)(sl - path + 1);
				memcpy(prc->dstdir, path, prc->dstdirLen * sizeof(char));
				prc->dstdir[prc->dstdirLen] = '\0';
			}
		} else {
			if (prc->destinationMode == DESTINATION_IN_PLACE) {
				prc->dstdirLen = 0;
				prc->dstdir[0] = '\0';
			}
			sl = path - 1;
		}
		size_t olen = (size_t)(path + plen - sl - 1);
		const char* oldn = sl + 1;

		rc = processName(prc, oldn, olen, NULL);
		if (rc == RESPONSE_NONE) {
			rc = processFile(prc, oldn, olen, NULL);
			if (rc == RESPONSE_NONE && !arg->noAutoPreview)
				g_print("'%s' -> '%s'\n", oldn, prc->name);
		}
		prc->id += (size_t)prc->step;
	} while ((rc == RESPONSE_NONE || rc == RESPONSE_YES) && prc->id < arg->nFiles);
	freeRegexes(prc);
}

void consolePreview(Window* win) {
	Process* prc = win->proc;
	Arguments* arg = win->args;
	if (!initConsoleRename(prc, arg))
		return;

	ResponseType rc;
	do {
		const char* path = g_file_peek_path(arg->files[prc->id]);
		size_t olen = strlen(path);
		const char* oldn = memrchr(path, '/', olen * sizeof(char));
		if (oldn)
			olen = (size_t)(path + olen - oldn - 1);
		else
			oldn = path;

		rc = processName(prc, oldn, olen, NULL);
		if (rc == RESPONSE_NONE)
			g_print("'%s' -> '%s'\n", oldn, prc->name);
		prc->id += (size_t)prc->step;
	} while ((rc == RESPONSE_NONE || rc == RESPONSE_YES) && prc->id < arg->nFiles);
	freeRegexes(prc);
}
