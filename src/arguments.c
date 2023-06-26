#include "arguments.h"

#ifdef _WIN32
#define INVALID_FNCHARS "\"*/:<>?\\|\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F"
#else
#define INVALID_FNCHARS "/"
#endif

char* validateFilename(const char* name) {
	size_t len = strcspn(name, INVALID_FNCHARS);
	if (!name[len])
		return NULL;

	const char* pos = name;
	char* str = g_malloc(strlen(name) * sizeof(char));
	char* out = str;
	do {
		memcpy(out, pos, len);
		out += len;
		pos += len;
		pos += strspn(pos, INVALID_FNCHARS);
		len = strcspn(pos, INVALID_FNCHARS);
	} while (pos[len]);
	memcpy(out, pos, (len + 1) * sizeof(char));
	return str;
}

static void checkArgName(char** name, bool validate) {
	if (*name) {
		size_t len = strlen(*name);
		if (len >= FILENAME_MAX) {
			g_printerr("Filename '%s' is too long\n", *name);
			g_free(*name);
			*name = NULL;
		} else if (validate) {
			char* str = validateFilename(*name);
			if (str) {
				g_free(*name);
				*name = str;
			}
		}
	}
}

static int64_t getArgInt(char** text, uint8_t base, int64_t def) {
	if (!*text)
		return def;

	char* alt = sanitizeNumber(*text, base);
	if (alt) {
		g_printerr("Number sanitized from '%s' to '%s'\n", *text, alt);
		*text = alt;
	}
	return MAX(strToLlong(*text, base), -INT64_MAX);
}

static RenameMode parseRenameMode(gchar* mode, char** name, char** replace) {
	checkArgName(name, true);
	checkArgName(replace, true);

	RenameMode rm = RENAME_KEEP;
	if (*replace)
		rm = RENAME_REPLACE;
	else if (*name)
		rm = RENAME_RENAME;
	else if (mode) {
		llong id = strtoll(mode, NULL, 0);
		if (id == RENAME_RENAME || !strcasecmp(mode, "n") || !strcasecmp(mode, "rename"))
			rm = RENAME_RENAME;
		else if (id == RENAME_REPLACE || !strcasecmp(mode, "r") || !strcasecmp(mode, "replace"))
			rm = RENAME_REPLACE;
		else if (id == RENAME_LOWER_CASE || !strcasecmp(mode, "l") || !strcasecmp(mode, "lower"))
			rm = RENAME_LOWER_CASE;
		else if (id == RENAME_UPPER_CASE || !strcasecmp(mode, "u") || !strcasecmp(mode, "upper"))
			rm = RENAME_UPPER_CASE;
		else if (id == RENAME_REVERSE || !strcasecmp(mode, "v") || !strcasecmp(mode, "reverse"))
			rm = RENAME_REVERSE;
	}
	g_free(mode);
	return rm;
}

static DateMode parseDateMode(gchar* mode) {
	if (!mode)
		return DATE_NONE;

	DateMode dm = DATE_NONE;
	llong id = strtoll(mode, NULL, 0);
	if (id == DATE_MODIFY || !strcasecmp(mode, "m") || !strcasecmp(mode, "modify"))
		dm = DATE_MODIFY;
	else if (id == DATE_ACCESS || !strcasecmp(mode, "a") || !strcasecmp(mode, "access"))
		dm = DATE_ACCESS;
	else if (id == DATE_CHANGE || !strcasecmp(mode, "c") || !strcasecmp(mode, "change"))
		dm = DATE_CHANGE;
	g_free(mode);
	return dm;
}

static DestinationMode parseDestinationMode(gchar* mode) {
	if (!mode)
		return DESTINATION_IN_PLACE;

	DestinationMode dm = DESTINATION_IN_PLACE;
	llong id = strtoll(mode, NULL, 0);
	if (id == DESTINATION_MOVE || !strcasecmp(mode, "m") || !strcasecmp(mode, "move"))
		dm = DESTINATION_MOVE;
	else if (id == DESTINATION_COPY || !strcasecmp(mode, "c") || !strcasecmp(mode, "copy"))
		dm = DESTINATION_COPY;
	else if (id == DESTINATION_LINK || !strcasecmp(mode, "l") || !strcasecmp(mode, "link"))
		dm = DESTINATION_LINK;
	g_free(mode);
	return dm;
}

void processArgumentOptions(Arguments* arg) {
	arg->extensionMode = parseRenameMode(arg->extensionModeStr, &arg->extensionName, &arg->extensionReplace);
	arg->extensionElements = CLAMP(arg->extensionElements, -1, FILENAME_MAX - 1);
	arg->renameMode = parseRenameMode(arg->renameModeStr, &arg->rename, &arg->replace);

	arg->removeFrom = CLAMP(arg->removeFrom, -FILENAME_MAX + 1, FILENAME_MAX - 1);
	arg->removeTo = CLAMP(arg->removeTo, -FILENAME_MAX + 1, FILENAME_MAX - 1);
	arg->removeFirst = CLAMP(arg->removeFirst, 0, FILENAME_MAX - 1);
	arg->removeLast = CLAMP(arg->removeLast, 0, FILENAME_MAX - 1);

	arg->addAt = CLAMP(arg->addAt, -FILENAME_MAX + 1, FILENAME_MAX - 1);
	checkArgName(&arg->addInsert, true);
	checkArgName(&arg->addPrefix, true);
	checkArgName(&arg->addSuffix, true);

	arg->numberLocation = CLAMP(arg->numberLocation, -FILENAME_MAX + 1, FILENAME_MAX);
	arg->numberBase = CLAMP(arg->numberBase, 2, 64);
	arg->numberStart = getArgInt(&arg->numberStartStr, arg->numberBase, 0);
	arg->numberStep = getArgInt(&arg->numberStepStr, arg->numberBase, 1);
	arg->numberPadding = CLAMP(arg->numberPadding, 1, MAX_DIGITS_I64B);
	checkArgName(&arg->numberPadStr, true);
	if (!arg->numberPadStr) {
		arg->numberPadStr = g_malloc(2 * sizeof(char));
		strcpy(arg->numberPadStr, "0");
	}
	checkArgName(&arg->numberPrefix, true);
	checkArgName(&arg->numberSuffix, true);

	arg->dateMode = parseDateMode(arg->dateModeStr);
	arg->dateLocation = CLAMP(arg->dateLocation, -FILENAME_MAX + 1, FILENAME_MAX);

	arg->destinationMode = parseDestinationMode(arg->destinationModeStr);
	checkArgName(&arg->destination, false);
}

void initCommandLineArguments(GApplication* app, Arguments* arg, int argc, char** argv) {
	arg->extensionElements = -1;
	arg->numberLocation = -1;
	arg->numberBase = 10;
	arg->numberPadding = 1;
	arg->dateLocation = -1;

	const char* extMsg = "\n\tSet how to change a filename's extension.\n\n"
"1. Replace the extension with the string set by --extension-name.\n   This option is set with \"rename\", \"n\" or \"1\".\n"
"2. Replace all occurrences of a string set by --extension-name with the string set by --extension-replace.\n   Use --extension-case to make the search case insensitive and --extension-regex to use --extension-name as a regular expression.\n   This option is set with \"replace\", \"r\" or \"2\".\n"
"3. Transform an extension to lower case.\n   This option is set with \"lower\", \"l\" or \"3\".\n"
"4. Transform an extension to upper case.\n   This option is set with \"upper\", \"u\" or \"4\".\n"
"5. Reverse an extension.\n   This option is set with \"reverse\", \"v\" or \"5\".\n";
	const char* filMsg = "\n\tSet how to change the whole filename.\n\n"
"1. Replace the filename with the string set by --rename-name.\n   This option is set with \"rename\", \"n\" or \"1\".\n"
"2. Replace all occurrences of a string set by --rename-name with the string set by --rename-replace.\n   Use --rename-case to make the search case insensitive and --rename-regex to use --rename-name as a regular expression.\n   This option is set with \"replace\", \"r\" or \"2\".\n"
"3. Transform the filename to lower case.\n   This option is set with \"lower\", \"l\" or \"3\".\n"
"4. Transform the filename to upper case.\n   This option is set with \"upper\", \"u\" or \"4\".\n"
"5. Reverse the filename.\n   This option is set with \"reverse\", \"v\" or \"5\".\n";

	GOptionEntry params[] = {
#ifdef CONSOLE
		{ "no-gui", 'g', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &arg->noGui, "\n\tThis option is ignored. It exists for compatibility reasons.\n", NULL },
#else
		{ "no-gui", 'g', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &arg->noGui, "\n\tDon't open a window and only process the files.\n", NULL },
#endif
		{ "dry", 'y', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &arg->dry, "\n\tWhen combined with --no-gui the new filenames will be shown without renaming any files.\n", NULL },
		{ "verbose", 'v', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &arg->verbose, "\n\tEnable verbose output.\n", NULL },
		{ "continue", 'z', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &arg->msgContinue, "\n\tContinue the renaming process even if an error occurs.\n\tIf this option or --abort aren't set, the user will be asked whether to continue.\n", NULL },
		{ "abort", 'Z', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &arg->msgAbort, "\n\tAbort the renaming process when an error occurs.\n\tIf this option or --continue aren't set, the user will be asked whether to continue.\n", NULL },
		{ "backwards", 'b', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &arg->backwards, "\n\tRename the files in backwards order.\n\tUseful for when filenames might overlap during the process.\n", NULL },
		{ "add-insert", 'j', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &arg->addInsert, "\n\tInsert a string at the location specified by --add-at.\n", "STRING" },
		{ "add-at", 'k', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &arg->addAt, "\n\tInsert the string set by --add-insert at this index.\n\tA negative index can be used to set a location relative to a filename's length.\n\tDefault value is 0.\n", "INDEX" },
		{ "add-prefix", 'p', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &arg->addPrefix, "\n\tPrefix filenames with this string.\n", "STRING" },
		{ "add-suffix", 's', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &arg->addSuffix, "\n\tSuffix filenames with this string.\n", "STRING" },
		{ "date-mode", 'e', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &arg->dateModeStr, "\n\tSet whether to insert the file's modification, access or status change date.\n\tThis option can be set with \"modify\", \"access\", \"change\", their first letters or indices 0 - 3.\n\tDefault value is 0.\n", "MODE" },
		{ "date-format", 'F', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &arg->dateFormat, "\n\tHow the date will be formatted.\n\tDefault value is " DEFAULT_DATE_FORMAT "\".\n", "STRING" },
		{ "date-location", 'O', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &arg->dateLocation, "\n\tAn index where to insert a date into a filename.\n\tA negative index can be used to set a location relative to a filename's length.\n\tDefault value is -1.\n", "INDEX" },
		{ "destination-mode", 'D', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &arg->destinationModeStr, "\n\tSet whether to rename the files in place, move them, copy them or create symlinks to them.\n\tThis option can be set with \"in-place\", \"move\", \"copy\", \"link\", their first letters or indices 0 - 3.\n\tDefault value is 0.\n", "MODE" },
		{ "destination", 'd', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &arg->destination, "\n\tSet the destination directory when --destination-mode isn't set to \"in place\".\n", "DIRECTORY" },
		{ "extension-mode", 'M', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &arg->extensionModeStr, extMsg, "MODE" },
		{ "extension-name", 'N', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &arg->extensionName, "\n\tReplace the extension of a filename with this string.\n\tIf --extension-mode is set to \"replace\" this string will be replaced by the string set with --extension-replace.\n\tImplies \"--extension-mode rename\" if --extension-mode isn't set to \"replace\".\n", "STRING" },
		{ "extension-replace", 'R', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &arg->extensionReplace, "\n\tReplace the string set by --extension-name with this string.\n\tImplies \"--extension-mode replace\".\n", "STRING" },
		{ "extension-case", 'I', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &arg->extensionCi, "\n\tDo a case insensitive search when --extension-mode is set to \"replace\".\n", NULL },
		{ "extension-regex", 'X', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &arg->extensionRegex, "\n\tUse the string set by --extension-name as a regular expression when --extension-mode is set to \"replace\".\n", NULL },
		{ "extension-elements", 'E', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &arg->extensionElements, "\n\tThe number of dots to consider part of a filename an extension.\n\tA positive value counts from the back while a negative value counts from the front.\n\tA number of 0 will use all dots, excluding an initial dot.\n\tDefault value is 0.\n", "NUMBER" },
		{ "number-location", 'K', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &arg->numberLocation, "\n\tAn index where to insert a number into a filename.\n\tA negative index can be used to set a location relative to a filename's length.\n\tDefault value is -1.\n", "INDEX" },
		{ "number-start", 'L', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &arg->numberStartStr, "\n\tA starting number for the numbering.\n\tMust be in the numerical system set by --number-base.\n\tDefault value is 0.\n", "START" },
		{ "number-step", 'T', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &arg->numberStepStr, "\n\tThe increment for the numbering.\n\tMust be in the numerical system set by --number-base.\n\tDefault value is 1.\n", "INCREMENT" },
		{ "number-base", 'B', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &arg->numberBase, "\n\tThe base for which numerical system to use.\n\tCan be between 2 and 64 and is 10 by default.\n", "BASE" },
		{ "number-lower", 'u', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &arg->numberLower, "\n\tUse small letters for numbers of a radix above 10 and below 37.\n", NULL },
		{ "number-padding", 'G', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &arg->numberPadding, "\n\tA padding number for at least how many digits to print.\n\tSaid digits are set by --number-padstr.\n\tDefault value is 1 i.e. no padding.\n", "NUMBER" },
		{ "number-padstr", 'C', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &arg->numberPadStr, "\n\tWhat padding string to use for when --number-padding is set.\n\tDefault value is \"0\".\n", "STRING" },
		{ "number-prefix", 'P', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &arg->numberPrefix, "\n\tPrefix the number with this string.\n", "STRING" },
		{ "number-suffix", 'S', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &arg->numberSuffix, "\n\tSuffix the number with this string.\n", "STRING" },
		{ "remove-from", 'o', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &arg->removeFrom, "\n\tStart removing characters starting from this index.\n\tA negative index can be used to set a location relative to a filename's length.\n\tIf this index ends up being greater than the one set by --remove-to, the two values will be switched.\n", "INDEX" },
		{ "remove-to", 't', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &arg->removeTo, "\n\tRemove characters from the index set by --remove-from until this index.\n\tA negative index can be used to set a location relative to a filename's length.\n\tIf this index ends up being less than the one set by --remove-from, the two values will be switched.\n", "INDEX" },
		{ "remove-first", 'f', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &arg->removeFirst, "\n\tA number of characters to remove from the beginning of a filename.\n", "LENGTH" },
		{ "remove-last", 'l', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &arg->removeLast, "\n\tA number of characters to remove from the end of a filename.\n", "LENGTH" },
		{ "rename-mode", 'm', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &arg->renameModeStr, filMsg, "MODE" },
		{ "rename-name", 'n', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &arg->rename, "\n\tReplace a filename with this string.\n\tIf --rename-mode is set to \"replace\" this string will be replaced by the string set with --rename-replace.\n\tImplies \"--rename-mode rename\" if --rename-mode isn't set to \"replace\".\n", "STRING" },
		{ "rename-replace", 'r', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &arg->replace, "\n\tReplace the string set by --rename-name with this string.\n\tImplies \"--rename-mode replace\".\n", "STRING" },
		{ "rename-case", 'i', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &arg->replaceCi, "\n\tDo a case insensitive search when --rename-mode is set to \"replace\".\n", NULL },
		{ "rename-regex", 'x', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &arg->replaceRegex, "\n\tUse the string set by --rename-name as a regular expression when --rename-mode is set to \"replace\".\n", NULL },
		{ NULL, '\0', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, NULL, NULL }
	};
	const int nid = 18;
	for (int i = 1; i < argc; ++i)
		for (int j = 0; j < 8; ++j)
			if ((argv[i][0] == '-' && argv[i][1] == params[j + nid].short_name && !argv[i][2]) || (!strncmp(argv[i], "--", 2) && !strcmp(argv[i] + 2, params[j + nid].long_name))) {
				arg->number = true;
				goto loopEnd;
			}
loopEnd:
	g_application_add_main_option_entries(app, params);
	g_application_set_option_context_parameter_string(app, "[FILE\xE2\x80\xA6]");
	g_application_set_option_context_summary(app, "Simple Fucking Bulk Rename: A small tool for batch renaming files.");
}

void freeArguments(Arguments* arg) {
	g_free(arg->extensionName);
	g_free(arg->extensionReplace);
	g_free(arg->rename);
	g_free(arg->replace);
	g_free(arg->addInsert);
	g_free(arg->addPrefix);
	g_free(arg->addSuffix);
	g_free(arg->numberStartStr);
	g_free(arg->numberStepStr);
	g_free(arg->numberPadStr);
	g_free(arg->numberPrefix);
	g_free(arg->numberSuffix);
	g_free(arg->dateFormat);
	g_free(arg->destination);
}
