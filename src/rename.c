#include "rename.h"
#include "main.h"
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/stat.h>

static const char emptyStr[1] = "";

static int copyFile(const char* src, const char* dst) {
	int in = open(src, O_RDONLY);
	if (in == -1)
		return -1;

	struct stat ps;
	if (fstat(in, &ps)) {
		close(in);
		return -1;
	}

	int out = creat(dst, ps.st_mode & (S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO));
	if (out == -1) {
		close(in);
		return -1;
	}

	off_t bytes = 0;
	int rc = sendfile(out, in, &bytes, ps.st_size);
	close(in);
	close(out);
	return rc;
}

static size_t replaceRegex(char* name, regex_t* reg, const char* new, ushort nlen) {
	char buf[FILENAME_MAX];
	size_t blen = 0;
	char* last = name;
	for (regmatch_t match; !regexec(reg, last, 1, &match, 0) && match.rm_so != match.rm_eo;) {
		if (blen + match.rm_so + nlen >= FILENAME_MAX)
			return -1;

		memcpy(buf + blen, last, match.rm_so * sizeof(char));
		memcpy(buf + blen + match.rm_so, new, nlen * sizeof(char));
		blen += match.rm_so + nlen;
		last += match.rm_eo;
	}
	size_t rest = strlen(last);
	size_t tlen = blen + rest;
	if (tlen >= FILENAME_MAX)
		return -1;

	memcpy(buf + blen, last, (rest + 1) * sizeof(char));
	memcpy(name, buf, (tlen + 1) * sizeof(char));
	return tlen;
}

static size_t replaceStrings(char* name, const char* old, ushort olen, const char* new, ushort nlen, bool ci) {
	char* (*scmp)(const char*, const char*) = ci ? strcasestr : strstr;
	char buf[FILENAME_MAX];
	size_t blen = 0;
	char* last = name;
	for (char* it; (it = scmp(last, old));) {
		size_t diff = it - last;
		if (blen + diff + nlen >= FILENAME_MAX)
			return -1;

		memcpy(buf + blen, last, diff * sizeof(char));
		memcpy(buf + blen + diff, new, nlen * sizeof(char));
		blen += diff + nlen;
		last = it + olen;
	}
	size_t rest = strlen(last);
	size_t tlen = blen + rest;
	if (blen + rest >= FILENAME_MAX)
		return -1;

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

static bool nameRename(Process* prc, regex_t* reg) {
	switch (prc->renameMode) {
	case RENAME_KEEP:
		return true;
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
		prc->nameLen = moveGName(prc->name, g_utf8_strdown(prc->name, prc->nameLen));
		break;
	case RENAME_UPPER_CASE:
		prc->nameLen = moveGName(prc->name, g_utf8_strup(prc->name, prc->nameLen));
		break;
	case RENAME_REVERSE:
		prc->nameLen = moveGName(prc->name, g_utf8_strreverse(prc->name, prc->nameLen));
	}

	if (prc->nameLen >= FILENAME_MAX) {
		g_printerr("new filename became too long\n");
		return false;
	}
	return true;
}

static void nameRemove(Process* prc) {
	ulong ulen = g_utf8_strlen(prc->name, prc->nameLen);
	if (prc->removeTo > prc->removeFrom) {
		uint diff = prc->removeTo - prc->removeFrom;
		if (diff < ulen) {
			char* src = g_utf8_offset_to_pointer(prc->name, prc->removeTo);
			size_t cnt = prc->name + prc->nameLen - src;
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

static bool nameAdd(Process* prc) {
	if (prc->addInsertLen) {
		if (prc->nameLen + prc->addInsertLen >= FILENAME_MAX)
			goto errorTooLong;

		char* pos;
		ulong ulen = g_utf8_strlen(prc->name, prc->nameLen);
		if (prc->numberLocation >= 0)
			pos = (uint)prc->numberLocation <= ulen ? g_utf8_offset_to_pointer(prc->name, prc->addAt) : prc->name + prc->nameLen;
		else
			pos = (uint)-prc->numberLocation <= ulen ? g_utf8_offset_to_pointer(prc->name, ulen + prc->addAt + 1) : prc->name;
		memmove(pos + prc->addInsertLen, pos, (prc->name + prc->nameLen - pos + 1) * sizeof(char));
		memcpy(pos, prc->addInsert, prc->addInsertLen * sizeof(char));
		prc->nameLen += prc->addInsertLen;
	}

	if (prc->addPrefixLen) {
		if (prc->nameLen + prc->addPrefixLen >= FILENAME_MAX)
			goto errorTooLong;

		memmove(prc->name + prc->addPrefixLen, prc->name, (prc->nameLen + 1) * sizeof(char));
		memcpy(prc->name, prc->addPrefix, prc->addPrefixLen * sizeof(char));
		prc->nameLen += prc->addPrefixLen;
	}

	if (prc->addSuffixLen) {
		if (prc->nameLen + prc->addSuffixLen >= FILENAME_MAX)
			goto errorTooLong;

		memcpy(prc->name + prc->nameLen, prc->addSuffix, (prc->addSuffixLen + 1) * sizeof(char));
		prc->nameLen += prc->addSuffixLen;
	}
	return true;

errorTooLong:
	g_printerr("filename '%s' became too long during add\n", prc->name);
	return false;
}

static bool nameNumber(Process* prc, int id) {
	ulong ulen = g_utf8_strlen(prc->name, prc->nameLen);
	char* pos;
	if (prc->numberLocation < 0)
		pos = (uint)-prc->numberLocation <= ulen ? g_utf8_offset_to_pointer(prc->name, ulen + prc->numberLocation + 1) : prc->name;
	else
		pos = (uint)prc->numberLocation <= ulen ? g_utf8_offset_to_pointer(prc->name, prc->numberLocation) : prc->name + prc->nameLen;

	int val = id * prc->numberStep + prc->numberStart;
	bool negative = val < 0;
	const char* digits = prc->numberBase < 64 ? "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz+" : "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	char buf[MAX_DIGITS + 1];
	uint blen = 0;
	if (val) {
		for (uint num = !negative ? val : -val; num; num /= prc->numberBase)
			buf[blen++] = digits[num % prc->numberBase];
	} else
		buf[blen++] = '0';

	uint padLeft = blen < prc->numberPadding && prc->numberPadStrLen ? (prc->numberPadding - blen) * prc->numberPadStrLen : 0;
	uint pbslen = prc->numberPrefixLen + negative + padLeft + blen + prc->numberSuffixLen;
	if (prc->nameLen + pbslen >= FILENAME_MAX) {
		g_printerr("filename '%s' became too long while adding number\n", prc->name);
		return false;
	}
	memmove(pos + pbslen, pos, (prc->name + prc->nameLen - pos + 1) * sizeof(char));
	pos = (char*)memcpy(pos, prc->numberPrefix, prc->numberPrefixLen * sizeof(char)) + prc->numberPrefixLen;
	if (negative)
		*pos++ = '-';
	for (uint i = 0; i < padLeft; i += prc->numberPadStrLen)
		pos = (char*)memcpy(pos, prc->numberPadStr, prc->numberPadStrLen * sizeof(char)) + prc->numberPadStrLen;
	while (blen)
		*pos++ = buf[--blen];
	memcpy(pos, prc->numberSuffix, prc->numberSuffixLen * sizeof(char));
	prc->nameLen += pbslen;
	return true;
}

static size_t processExtension(Process* prc, const char* str) {
	size_t dot;
	if (prc->extensionElements < 0) {
		char* pos = memchr(str, '.', prc->nameLen);
		if (pos == str)
			pos = memchr(str + 1, '.', prc->nameLen - 1);
		dot = pos ? (size_t)(pos - str) : prc->nameLen;
	} else {
		dot = prc->nameLen;
		for (int i = 0; dot && i < prc->extensionElements; ++i) {
			char* pos = memrchr(str, '.', dot);
			if (!pos)
				break;
			dot = pos - str;
		}
	}
	size_t elen = prc->nameLen - dot;
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
		return moveGName(prc->extension, g_utf8_strdown(str + dot, elen));
	case RENAME_UPPER_CASE:
		return moveGName(prc->extension, g_utf8_strup(str + dot, elen));
	case RENAME_REVERSE:
		return moveGName(prc->extension, g_utf8_strreverse(str + dot, elen));
	}
	return elen;
}

static bool processName(Process* prc, const char* oldn, size_t* olen) {
	prc->nameLen = *olen = strlen(oldn);
	if (prc->nameLen >= FILENAME_MAX) {
		g_printerr("filename '%s' too long\n", oldn);
		return false;
	}
	size_t elen = processExtension(prc, oldn);
	if (elen >= FILENAME_MAX) {
		g_printerr("extension too long\n");
		return false;
	}

	if (!nameRename(prc, prc->replaceRegex ? &prc->regRename : NULL))
		return false;
	nameRemove(prc);
	if (!nameAdd(prc))
		return false;
	if (prc->number)
		if (!nameNumber(prc, prc->id))
			return false;

	if (prc->nameLen + elen >= FILENAME_MAX) {
		g_printerr("filename '%s' became too long while reapplying extension\n", prc->name);
		return false;
	}
	memcpy(prc->name + prc->nameLen, prc->extension, (elen + 1) * sizeof(char));
	prc->nameLen += elen;
	return true;
}

static bool initRegex(Window* win, bool* inUse, regex_t* reg, const char* expr, ushort exlen, bool use, bool ci) {
	*inUse = use && exlen;
	if (*inUse) {
		int err = regcomp(reg, expr, REG_EXTENDED | (ci ? REG_ICASE : 0));
		if (err) {
			size_t elen = regerror(err, reg, NULL, 0);
			char* estr = malloc(elen * sizeof(char));
			regerror(err, reg, estr, elen);
			showMessageBox(win, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Invalid regular expression:\n%s", estr);
			free(estr);
			return *inUse = false;
		}
	}
	return true;
}

static bool initRename(Process* prc, Window* win) {
	if (!(g_utf8_validate_len(prc->extensionName, prc->extensionNameLen, NULL) && g_utf8_validate_len(prc->extensionReplace, prc->extensionReplaceLen, NULL)
			&& g_utf8_validate_len(prc->rename, prc->renameLen, NULL) && g_utf8_validate_len(prc->replace, prc->replaceLen, NULL) && g_utf8_validate_len(prc->addInsert, prc->addInsertLen, NULL)
			&& g_utf8_validate_len(prc->addPrefix, prc->addPrefixLen, NULL) && g_utf8_validate_len(prc->addSuffix, prc->addSuffixLen, NULL) && g_utf8_validate_len(prc->numberPadStr, prc->numberPadStrLen, NULL)
			&& g_utf8_validate_len(prc->numberPrefix, prc->numberPrefixLen, NULL) && g_utf8_validate_len(prc->numberSuffix, prc->numberSuffixLen, NULL) && g_utf8_validate_len(prc->destination, prc->destinationLen, NULL))) {
		showMessageBox(win, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Invalid UTF-8 input");
		return false;
	}

	if (!initRegex(win, &prc->extensionRegex, &prc->regExtension, prc->extensionReplace, prc->extensionReplaceLen, prc->extensionRegex, prc->extensionCi))
		return false;
	if (!initRegex(win, &prc->replaceRegex, &prc->regRename, prc->replace, prc->replaceLen, prc->replaceRegex, prc->replaceCi)) {
		if (prc->extensionRegex)
			regfree(&prc->regExtension);
		return false;
	}
	return true;
}

static void closeRename(Process* prc) {
	if (prc->extensionRegex)
		regfree(&prc->regExtension);
	if (prc->replaceRegex)
		regfree(&prc->regRename);
}

static bool initWindowRename(Window* win) {
	Process* prc = &win->proc;
	prc->model = gtk_tree_view_get_model(win->tblFiles);
	prc->extensionName = gtk_entry_get_text(win->etExtension);
	prc->extensionNameLen = strlen(prc->extension);
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
	prc->destination = gtk_entry_get_text(win->etDestination);
	prc->destinationLen = strlen(prc->destination);
	prc->extensionMode = gtk_combo_box_get_active(GTK_COMBO_BOX(win->cmbExtensionMode));
	prc->renameMode = gtk_combo_box_get_active(GTK_COMBO_BOX(win->cmbRenameMode));
	prc->numberStart = gtk_spin_button_get_value_as_int(win->sbNumberStart);
	prc->numberStep = gtk_spin_button_get_value_as_int(win->sbNumberStep);
	prc->destinationMode = gtk_combo_box_get_active(GTK_COMBO_BOX(win->cmbDestinationMode));
	prc->extensionElements = gtk_spin_button_get_value_as_int(win->sbExtensionElements);
	prc->removeFrom = gtk_spin_button_get_value_as_int(win->sbRemoveFrom);
	prc->removeTo = gtk_spin_button_get_value_as_int(win->sbRemoveTo);
	prc->removeFirst = gtk_spin_button_get_value_as_int(win->sbRemoveFirst);
	prc->removeLast = gtk_spin_button_get_value_as_int(win->sbRemoveLast);
	prc->addAt = gtk_spin_button_get_value_as_int(win->sbAddAt);
	prc->numberLocation = gtk_spin_button_get_value_as_int(win->sbNumberLocation);
	prc->numberPadding = gtk_spin_button_get_value_as_int(win->sbNumberPadding);
	prc->extensionCi = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->cbExtensionCi));
	prc->extensionRegex = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->cbExtensionRegex));
	prc->replaceCi = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->cbReplaceCi));
	prc->replaceRegex = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->cbReplaceRegex));
	prc->number = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->cbNumber));
	prc->numberBase = gtk_spin_button_get_value_as_int(win->sbNumberBase);
	prc->forward = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->cbDestinationForward));
	prc->id = prc->forward ? 0 : gtk_tree_model_iter_n_children(prc->model, NULL) - 1;
	prc->step = prc->forward ? 1 : -1;
	if (prc->forward) {
		if (!gtk_tree_model_get_iter_first(prc->model, &prc->it))
			return false;
	} else if (!gtk_tree_model_iter_nth_child(prc->model, &prc->it, NULL, prc->id))
		return false;
	if (!prc->destinationLen) {
		prc->destinationMode = DESTINATION_IN_PLACE;
		gtk_combo_box_set_active(GTK_COMBO_BOX(win->cmbDestinationMode), DESTINATION_IN_PLACE);
	}
	return initRename(prc, win);
}

static bool initConsoleRename(Process* prc, Arguments* arg) {
	if (!arg->files)
		return false;
	prc->extensionName = arg->extensionName ? arg->extensionName : emptyStr;
	prc->extensionReplace = arg->extensionReplace ? arg->extensionReplace : emptyStr;
	prc->rename = arg->rename ? arg->rename : emptyStr;
	prc->replace = arg->replace ? arg->replace : emptyStr;
	prc->addInsert = arg->addInsert ? arg->addInsert : emptyStr;
	prc->addPrefix = arg->addPrefix ? arg->addPrefix : emptyStr;
	prc->addSuffix = arg->addSuffix ? arg->addSuffix : emptyStr;
	prc->numberPadStr = arg->numberPadStr ? arg->numberPadStr : emptyStr;
	prc->numberPrefix = arg->numberPrefix ? arg->numberPrefix : emptyStr;
	prc->numberSuffix = arg->numberSuffix ? arg->numberSuffix : emptyStr;
	prc->destination = arg->destination ? arg->destination : emptyStr;
	prc->forward = !arg->backwards;
	prc->id = prc->forward ? 0 : arg->nFiles - 1;
	prc->step = prc->forward ? 1 : -1;
	prc->extensionMode = arg->gotExtensionMode;
	prc->renameMode = arg->gotRenameMode;
	prc->numberStart = arg->numberStart;
	prc->numberStep = arg->numberStep;
	prc->destinationMode = arg->gotDestinationMode;
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
	prc->destinationLen = strlen(prc->destination);
	prc->extensionCi = arg->extensionCi;
	prc->extensionRegex = arg->extensionRegex;
	prc->replaceCi = arg->replaceCi;
	prc->replaceRegex = arg->replaceRegex;
	prc->number = arg->number;
	prc->numberBase = arg->numberBase;
	if (!arg->destination)
		prc->destinationMode = DESTINATION_IN_PLACE;
	return initRename(prc, NULL);
}

static char* initDestination(Process* prc, size_t* dlen) {
	if (prc->destinationMode == DESTINATION_IN_PLACE) {
		*dlen = 0;
		return malloc(PATH_MAX * sizeof(char));
	}

	struct stat ps;
	if (stat(prc->destination, &ps) || !S_ISDIR(ps.st_mode)) {
		g_printerr("Destination '%s' is not a valid directory", prc->destination);
		closeRename(prc);
		return NULL;
	}

	bool extend = prc->destination[prc->destinationLen - 1] != '/';
	*dlen = prc->destinationLen + extend;
	if (*dlen >= PATH_MAX) {
		g_printerr("Directory '%s' is too long", prc->destination);
		closeRename(prc);
		return NULL;
	}

	char* dirc = malloc(PATH_MAX * sizeof(char));
	memcpy(dirc, prc->destination, (prc->destinationLen + 1) * sizeof(char));
	if (extend)
		memcpy(dirc + prc->destinationLen, "/", 2);
	return dirc;
}

static bool processFile(Process* prc, char* dirc, size_t dlen, const char* oldn, size_t olen) {
	if (prc->destinationMode != DESTINATION_IN_PLACE) {
		struct stat ps;
		if (stat(dirc, &ps) || !S_ISDIR(ps.st_mode)) {
			g_printerr("Destination '%s' is not a valid directory", dirc);
			return false;
		}
	}
	if (dlen + prc->nameLen >= PATH_MAX) {
		g_printerr("directory '%s' and filename '%s' are too long\n", dirc, prc->name);
		return false;
	}
	if (dlen + olen >= PATH_MAX) {
		g_printerr("directory '%s' and filename '%s' are too long\n", dirc, oldn);
		return false;
	}
	memcpy(dirc + dlen, oldn, (olen + 1) * sizeof(char));
	memmove(prc->name + dlen, prc->name, (prc->nameLen + 1) * sizeof(char));
	memcpy(prc->name, dirc, dlen * sizeof(char));
	int rc = (int (*const[4])(const char*, const char*)){ rename, rename, copyFile, symlink }[prc->destinationMode](dirc, prc->name);
	if (rc)
		g_printerr("%s\n", strerror(errno));
	return !rc;
}

void windowRename(Window* win) {
	Process* prc = &win->proc;
	if (!initWindowRename(win))
		return;

	size_t dlen;
	char* dirc = initDestination(prc, &dlen);
	if (!dirc)
		return;

	do {
		char* oldn;
		size_t olen;
		gtk_tree_model_get(prc->model, &prc->it, FCOL_OLD_NAME, &oldn, FCOL_INVALID);
		if (processName(prc, oldn, &olen)) {
			bool inPlace = !dirc;
			if (inPlace) {
				gtk_tree_model_get(prc->model, &prc->it, FCOL_DIRECTORY, &dirc, FCOL_INVALID);
				dlen = strlen(dirc);
				if (dirc[dlen - 1] != '/') {
					dirc = g_realloc(dirc, (dlen + 2) * sizeof(char));
					memcpy(dirc + dlen++, "/", 2);
				}
			}
			if (processFile(prc, dirc, dlen, oldn, olen))
				gtk_list_store_set(win->lsFiles, &prc->it, FCOL_OLD_NAME, prc->name + dlen, FCOL_NEW_NAME, prc->name + dlen, FCOL_INVALID);
			if (inPlace) {
				g_free(dirc);
				dirc = NULL;
			}
		}
		g_free(oldn);
		prc->id += prc->step;
	} while (prc->forward ? gtk_tree_model_iter_next(prc->model, &prc->it) : gtk_tree_model_iter_previous(prc->model, &prc->it));
	free(dirc);
	closeRename(prc);
}

void consoleRename(Window* win) {
	Process* prc = &win->proc;
	Arguments* arg = win->args;
	if (!initConsoleRename(prc, arg))
		return;

	size_t dlen;
	char* dirc = initDestination(prc, &dlen);
	if (!dirc)
		return;
	char* oldn = malloc(PATH_MAX * sizeof(char));

	for (; prc->forward ? prc->id < arg->nFiles : prc->id >= 0; prc->id += prc->step) {
		const char* path = g_file_peek_path(arg->files[prc->id]);
		size_t plen = strlen(path);
		const char* sl = memrchr(path, '/', plen * sizeof(char));
		if (sl) {
			if (prc->destinationMode == DESTINATION_IN_PLACE) {
				dlen = sl - path + 1;
				memcpy(dirc, path, dlen * sizeof(char));
				dirc[dlen] = '\0';
			}
		} else {
			if (prc->destinationMode == DESTINATION_IN_PLACE) {
				dlen = 0;
				dirc[0] = '\0';
			}
			sl = path - 1;
		}
		size_t olen = path + plen - sl - 1;
		memcpy(oldn, sl + 1, olen * sizeof(char));

		if (processName(prc, oldn, &olen) && !processFile(prc, dirc, dlen, oldn, olen) && !arg->noAutoPreview)
			g_print("'%s' -> '%s'\n", oldn, prc->name);
	}
	free(oldn);
	free(dirc);
	closeRename(prc);
}

void windowPreview(Window* win) {
	Process* prc = &win->proc;
	if (!initWindowRename(win))
		return;

	do {
		char* oldn;
		size_t olen;
		gtk_tree_model_get(prc->model, &prc->it, FCOL_OLD_NAME, &oldn, FCOL_INVALID);
		if (processName(prc, oldn, &olen))
			gtk_list_store_set(win->lsFiles, &prc->it, FCOL_NEW_NAME, prc->name, FCOL_INVALID);
		g_free(oldn);
		prc->id += prc->step;
	} while (prc->forward ? gtk_tree_model_iter_next(prc->model, &prc->it) : gtk_tree_model_iter_previous(prc->model, &prc->it));
	closeRename(prc);
}

void consolePreview(Window* win) {
	Process* prc = &win->proc;
	Arguments* arg = win->args;
	if (!initConsoleRename(prc, arg))
		return;

	for (; prc->forward ? prc->id < arg->nFiles : prc->id >= 0; prc->id += prc->step) {
		const char* path = g_file_peek_path(arg->files[prc->id]);
		size_t olen = strlen(path);
		const char* oldn = memrchr(path, '/', olen * sizeof(char));
		if (oldn)
			olen = path + olen - oldn - 1;
		else
			oldn = path;

		if (processName(prc, oldn, &olen))
			g_print("'%s' -> '%s'\n", oldn, prc->name);
	}
	closeRename(prc);
}
