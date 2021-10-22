#include "arguments.h"

static void checkArgName(gchar** name) {
	if (*name) {
		size_t len = strlen(*name);
		if (len >= FILENAME_MAX) {
			g_printerr("Filename '%s' is too long\n", *name);
			g_free(*name);
			*name = NULL;
		}
	}
}

static RenameMode parseRenameMode(gchar* mode, char** name, char** replace) {
	checkArgName(name);
	checkArgName(replace);

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

static DestinationMode parseDestinationMode(gchar* mode) {
	if (!mode)
		return DESTINATION_IN_PLACE;

	DestinationMode dm = DESTINATION_IN_PLACE;
	llong id = strtoll(mode, NULL, 0);
	if (id == DESTINATION_MOVE || !strcasecmp(mode, "m") || !strcasecmp(mode, "move"))
		dm = DESTINATION_MOVE;
	if (id == DESTINATION_COPY || !strcasecmp(mode, "c") || !strcasecmp(mode, "copy"))
		dm = DESTINATION_COPY;
	if (id == DESTINATION_LINK || !strcasecmp(mode, "l") || !strcasecmp(mode, "link"))
		dm = DESTINATION_LINK;
	g_free(mode);
	return dm;
}

void processArgumentOptions(Arguments* arg) {
	arg->extensionMode = parseRenameMode(arg->extensionModeStr, &arg->extensionName, &arg->extensionReplace);
	arg->extensionElements = CLAMP(arg->extensionElements, -1, FILENAME_MAX - 1);
	arg->renameMode = parseRenameMode(arg->renameModeStr, &arg->rename, &arg->replace);

	arg->removeFrom = CLAMP(arg->removeFrom, 0, FILENAME_MAX - 1);
	arg->removeTo = CLAMP(arg->removeTo, 0, FILENAME_MAX - 1);
	arg->removeFirst = CLAMP(arg->removeFirst, 0, FILENAME_MAX - 1);
	arg->removeLast = CLAMP(arg->removeLast, 0, FILENAME_MAX - 1);

	arg->addAt = CLAMP(arg->addAt, -FILENAME_MAX + 1, FILENAME_MAX - 1);
	checkArgName(&arg->addInsert);
	checkArgName(&arg->addPrefix);
	checkArgName(&arg->addSuffix);

	arg->numberLocation = CLAMP(arg->numberLocation, -FILENAME_MAX + 1, FILENAME_MAX);
	arg->numberStart = MAX(arg->numberStart, -INT64_MAX);
	arg->numberStep = MAX(arg->numberStep, -INT64_MAX);
	arg->numberBase = CLAMP(arg->numberBase, 2, 64);
	arg->numberPadding = CLAMP(arg->numberPadding, 1, MAX_DIGITS_I64B);
	checkArgName(&arg->numberPadStr);
	if (!arg->numberPadStr) {
		arg->numberPadStr = g_malloc(2 * sizeof(char));
		strcpy(arg->numberPadStr, "0");
	}
	checkArgName(&arg->numberPrefix);
	checkArgName(&arg->numberSuffix);

	arg->destinationMode = parseDestinationMode(arg->destinationModeStr);
	checkArgName(&arg->destination);
}

void initCommandLineArguments(GApplication* app, Arguments* arg, int argc, char** argv) {
	memset(arg, 0, sizeof(Arguments));
	arg->extensionElements = -1;
	arg->numberLocation = -1;
	arg->numberStep = 1;
	arg->numberBase = 10;
	arg->numberPadding = 1;

	const char* extMsg = "\n\tSet how to change a filename's extension.\n"
"1. Replace the extension with the string set by --extension-name. This option is set with \"rename\", \"n\" or \"1\".\n"
"2. Replace all occurrences of a string set by --extension-name with the string set by --extension-replace. Use --extension-case to make the search case insensitive and --extension-regex to use --extension-name as a regular expression. This option is set with \"replace\", \"r\" or \"2\".\n"
"3. Transform an extension to lower case. This option is set with \"lower\", \"l\" or \"3\".\n"
"4. Transform an extension to upper case. This option is set with \"upper\", \"u\" or \"4\".\n"
"5. Reverse an extension. This option is set with \"reverse\", \"v\" or \"5\".\n";
	const char* filMsg = "\n\tSet how to change the whole filename.\n"
"1. Replace the filename with the string set by --rename-name. This option is set with \"rename\", \"n\" or \"1\".\n"
"2. Replace all occurrences of a string set by --rename-name with the string set by --rename-replace. Use --rename-case to make the search case insensitive and --rename-regex to use --rename-name as a regular expression. This option is set with \"replace\", \"r\" or \"2\".\n"
"3. Transform the filename to lower case. This option is set with \"lower\", \"l\" or \"3\".\n"
"4. Transform the filename to upper case. This option is set with \"upper\", \"u\" or \"4\".\n"
"5. Reverse the filename. This option is set with \"reverse\", \"v\" or \"5\".\n";

	GOptionEntry params[] = {
#ifndef CONSOLE
		{ "no-gui", 'g', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &arg->noGui, "\n\tDon't open a window and only process the files.\n", "TEST" },
#endif
		{ "no-auto-preview", 'a', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &arg->noAutoPreview, "\n\tDisable auto preview. When combined with --no-gui it'll disable verbose output.\n", NULL },
		{ "dry", 'y', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &arg->dry, "\n\tWhen combined with --no-gui the new filenames will be shown without renaming any files.\n", NULL },
		{ "backwards", 'b', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &arg->backwards, "\n\tRename the files in backwards order. Useful for when filenames might overlap during the process.\n", NULL },
		{ "add-insert", 'i', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &arg->addInsert, "\n\tInsert a string at the location specified by --add-at.\n", "STRING" },
		{ "add-at", 'k', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &arg->addAt, "\n\tInsert the string set by --add-insert at this index. A negative index can be used to set a location relative to a filename's length.\n", "INDEX" },
		{ "add-prefix", 'p', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &arg->addPrefix, "\n\tPrefix filenames with this string.\n", "STRING" },
		{ "add-suffix", 's', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &arg->addSuffix, "\n\tSuffix filenames with this string.\n", "STRING" },
		{ "destination-mode", 'D', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &arg->destinationModeStr, "\n\tSet whether to rename the files in place, move them, copy them or create symlinks to them. This option can be set with \"in-place\", \"move\", \"copy\", \"link\", their first letters or indices 0 - 3.\n", "MODE" },
		{ "destination", 'd', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &arg->destination, "\n\tSet the destination directory when --destination-mode isn't set to \"in place\".\n", "DIRECTORY" },
		{ "extension-mode", 'M', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &arg->extensionModeStr, extMsg, "MODE" },
		{ "extension-name", 'N', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &arg->extensionName, "\n\tReplace the extension of a filename with this string. If --extension-mode is set to \"replace\" this string will be replaced by the string set with --extension-replace. Implies \"--extension-mode rename\" if --extension-mode isn't set to \"replace\".\n", "STRING" },
		{ "extension-replace", 'R', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &arg->extensionReplace, "\n\tReplace the string set by --extension-name with this string. Implies \"--extension-mode replace\".\n", "STRING" },
		{ "extension-case", 'I', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &arg->extensionCi, "\n\tDo a case insensitive search when --extension-mode is set to \"replace\".\n", NULL },
		{ "extension-regex", 'X', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &arg->extensionRegex, "\n\tUse the string set by --extension-name as a regular expression when --extension-mode is set to \"replace\".\n", NULL },
		{ "extension-elements", 'E', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &arg->extensionElements, "\n\tThe number of dots to consider part of a filename an extension. A negative value will use all dots, excluding an initial dot. Default value is -1.\n", "NUMBER" },
		{ "number-location", 'L', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &arg->numberLocation, "\n\tAn index where to insert a number into a filename. A negative index can be used to set a location relative to a filename's length.\n", "INDEX" },
		{ "number-start", 'S', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &arg->numberStart, "\n\tA starting number for the numbering. Default value is 0.\n", "START" },
		{ "number-step", 'T', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &arg->numberStep, "\n\tThe increment for the numbering. Default value is 1.\n", "INCREMENT" },
		{ "number-base", 'B', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &arg->numberBase, "\n\tThe base for which numerical system to use. Can be between 2 and 64 and is 10 by default.\n", "BASE" },
		{ "number-padding", 'D', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &arg->numberPadding, "\n\tA padding number for at least how many digits to print. Said digits are set by --number-padstr. Default value is 1 i.e. no padding.\n", "NUMBER" },
		{ "number-padstr", 'C', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &arg->numberPadStr, "\n\tWhat padding string to use for when --number-padding is set. Default value is \"0\".\n", "STRING" },
		{ "number-prefix", 'P', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &arg->numberPrefix, "\n\tPrefix the number with this string.\n", "STRING" },
		{ "number-suffix", 'S', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &arg->numberSuffix, "\n\tSuffix the number with this string.\n", "STRING" },
		{ "remove-from", 'o', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &arg->removeFrom, "\n\tStart removing characters starting from this index. This number must be lower than the one set by --remove-to.\n", "INDEX" },
		{ "remove-to", 't', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &arg->removeTo, "\n\tRemove characters from the index set by --remove-from until this index. This number must be greater than the one set by --remove-from.\n", "INDEX" },
		{ "remove-first", 'f', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &arg->removeFirst, "\n\tA number of characters to remove from the beginning of a filename.\n", "LENGTH" },
		{ "remove-last", 'l', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &arg->removeLast, "\n\tA number of characters to remove from the end of a filename.\n", "LENGTH" },
		{ "rename-mode", 'm', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &arg->renameModeStr, filMsg, "MODE" },
		{ "rename-name", 'n', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &arg->rename, "\n\tReplace a filename with this string. If --rename-mode is set to \"replace\" this string will be replaced by the string set with --rename-replace. Implies \"--rename-mode rename\" if --rename-mode isn't set to \"replace\".\n", "STRING" },
		{ "rename-replace", 'r', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &arg->replace, "\n\tReplace the string set by --rename-name with this string. Implies \"--rename-mode replace\".\n", "STRING" },
		{ "rename-case", 'i', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &arg->replaceCi, "\n\tDo a case insensitive search when --rename-mode is set to \"replace\".\n", NULL },
		{ "rename-regex", 'x', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &arg->replaceRegex, "\n\tUse the string set by --rename-name as a regular expression when --rename-mode is set to \"replace\".\n", NULL },
		{ NULL, '\0', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, NULL, NULL }
	};
	const int nid = 15;
	for (int i = 1; i < argc; ++i)
		for (int j = 0; j < 7; ++j)
			if ((argv[i][0] == '-' && argv[i][1] == params[j + nid].short_name && !argv[i][2]) || (!strncmp(argv[i], "--", 2) && !strcmp(argv[i] + 2, params[j + nid].long_name))) {
				arg->number = true;
				goto loopEnd;
			}
loopEnd:
	g_application_add_main_option_entries(app, params);
	g_application_set_option_context_parameter_string(app, "[FILE\xE2\x80\xA6]");
	g_application_set_option_context_summary(app, "Simple Fucking Bulk Rename: A small tool for batch renaming files.");
}
