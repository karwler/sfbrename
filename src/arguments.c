#include "arguments.h"
#include "main.h"
#include <ctype.h>
#include <sys/stat.h>

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

static RenameMode parseRenameMode(const gchar* mode, char** name, char** replace) {
	checkArgName(name);
	checkArgName(replace);
	if (*replace)
		return RENAME_REPLACE;
	if (*name)
		return RENAME_RENAME;

	if (!mode)
		return RENAME_KEEP;
	llong id = strtoll(mode, NULL, 0);
	if (id < RENAME_KEEP || id > RENAME_REVERSE)
		id = RENAME_KEEP;

	if (id == RENAME_RENAME || !strcasecmp(mode, "n") || !strcasecmp(mode, "rename"))
		return RENAME_RENAME;
	if (id == RENAME_REPLACE || !strcasecmp(mode, "r") || !strcasecmp(mode, "replace"))
		return RENAME_REPLACE;
	if (id == RENAME_LOWER_CASE || !strcasecmp(mode, "l") || !strcasecmp(mode, "lower"))
		return RENAME_LOWER_CASE;
	if (id == RENAME_UPPER_CASE || !strcasecmp(mode, "u") || !strcasecmp(mode, "upper"))
		return RENAME_UPPER_CASE;
	if (id == RENAME_REVERSE || !strcasecmp(mode, "v") || !strcasecmp(mode, "reverse"))
		return RENAME_REVERSE;
	return RENAME_KEEP;
}

static DestinationMode parseDestinationMode(const gchar* mode) {
	if (!mode)
		return DESTINATION_IN_PLACE;
	llong id = strtoll(mode, NULL, 0);
	if (id < DESTINATION_IN_PLACE || id > DESTINATION_LINK)
		id = -1;

	if (id == DESTINATION_IN_PLACE || !strcasecmp(mode, "p") || !strcasecmp(mode, "in-place"))
		return DESTINATION_IN_PLACE;
	if (id == DESTINATION_MOVE || !strcasecmp(mode, "m") || !strcasecmp(mode, "move"))
		return DESTINATION_MOVE;
	if (id == DESTINATION_COPY || !strcasecmp(mode, "c") || !strcasecmp(mode, "copy"))
		return DESTINATION_COPY;
	if (id == DESTINATION_LINK || !strcasecmp(mode, "l") || !strcasecmp(mode, "link"))
		return DESTINATION_LINK;
	return DESTINATION_IN_PLACE;
}

void processArguments(Window* win) {
	size_t plen = 0;
	char path[PATH_MAX];
	if (getcwd(path, PATH_MAX)) {
		plen = strlen(path);
		if (path[plen - 1] != '/')
			path[plen++] = '/';
	} else
		g_printerr("Current working directory path too long\n");

#ifdef __MINGW32__
	char buf[PATH_MAX];
#endif
	Arguments* args = win->args;
	if (!args->noGui && args->files) {
		for (int i = 0; i < args->nFiles; ++i) {
			const char* file = g_file_peek_path(args->files[i]);
			if (file) {
#ifdef __MINGW32__
				size_t fsiz = strlen(file) + 1;
				if (fsiz >= PATH_MAX) {
					g_printerr("Filepath '%s' is too long\n", file);
					continue;
				}
				memcpy(buf, file, fsiz * sizeof(char));
				unbackslashify(buf);
				addFile(win, buf);
#else
				addFile(win, file);
#endif
			}
		}
		args->files = NULL;
	}

	args->gotExtensionMode = parseRenameMode(args->extensionMode, &args->extensionName, &args->extensionReplace);
	args->extensionElements = nclamp(args->extensionElements, -1, FILENAME_MAX - 1);
	args->gotRenameMode = parseRenameMode(args->renameMode, &args->rename, &args->replace);

	args->removeFrom = nclamp(args->removeFrom, 0, FILENAME_MAX - 1);
	args->removeTo = nclamp(args->removeTo, 0, FILENAME_MAX - 1);
	args->removeFirst = nclamp(args->removeFirst, 0, FILENAME_MAX - 1);
	args->removeLast = nclamp(args->removeLast, 0, FILENAME_MAX - 1);

	args->addAt = nclamp(args->addAt, -FILENAME_MAX + 1, FILENAME_MAX - 1);
	checkArgName(&args->addInsert);
	checkArgName(&args->addPrefix);
	checkArgName(&args->addSuffix);

	args->numberLocation = nclamp(args->numberLocation, -FILENAME_MAX + 1, FILENAME_MAX);
	args->numberStart = nclamp(args->numberStart, INT_MIN, INT_MAX);
	args->numberStep = nclamp(args->numberStep, INT_MIN, INT_MAX);
	args->numberBase = nclamp(args->numberBase, 2, 64);
	args->numberPadding = nclamp(args->numberPadding, 1, MAX_DIGITS);
	checkArgName(&args->numberPadStr);
	checkArgName(&args->numberPrefix);
	checkArgName(&args->numberSuffix);

	args->gotDestinationMode = parseDestinationMode(args->destinationMode);
	checkArgName(&args->destination);
}

void initCommandLineArguments(GtkApplication* app, Arguments* args, int argc, char** argv) {
	memset(args, 0, sizeof(Arguments));
	args->extensionElements = -1;
	args->numberLocation = -1;
	args->numberStep = 1;
	args->numberBase = 10;
	args->numberPadding = 1;

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
		{ "no-gui", 'g', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &args->noGui, "\n\tDon't open a window and only process the files.\n", "TEST" },
		{ "no-auto-preview", 'a', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &args->noAutoPreview, "\n\tDisable auto preview. When combined with --no-gui it'll disable verbose output.\n", NULL },
		{ "dry", 'y', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &args->dry, "\n\tWhen combined with --no-gui the new filenames will be shown without renaming any files.\n", NULL },
		{ "backwards", 'b', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &args->backwards, "\n\tRename the files in backwards order. Useful for when filenames might overlap during the process.\n", NULL },
		{ "add-insert", 'i', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &args->addInsert, "\n\tInsert a string at the location specified by --add-at.\n", "STRING" },
		{ "add-at", 'k', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &args->addAt, "\n\tInsert the string set by --add-insert at this index. A negative index can be used to set a location relative to a filename's length.\n", "INDEX" },
		{ "add-prefix", 'p', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &args->addPrefix, "\n\tPrefix filenames with this string.\n", "STRING" },
		{ "add-suffix", 's', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &args->addSuffix, "\n\tSuffix filenames with this string.\n", "STRING" },
		{ "destination-mode", 'D', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &args->destinationMode, "\n\tSet whether to rename the files in place, move them, copy them or create symlinks to them. This option can be set with \"in-place\", \"move\", \"copy\", \"link\", their first letters or indices 0 - 3.\n", "MODE" },
		{ "destination", 'd', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &args->destination, "\n\tSet the destination directory when --destination-mode isn't set to \"in place\".\n", "DIRECTORY" },
		{ "extension-mode", 'M', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &args->extensionMode, extMsg, "MODE" },
		{ "extension-name", 'N', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &args->extensionName, "\n\tReplace the extension of a filename with this string. If --extension-mode is set to \"replace\" this string will be replaced by the string set with --extension-replace. Implies \"--extension-mode rename\" if --extension-mode isn't set to \"replace\".\n", "STRING" },
		{ "extension-replace", 'R', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &args->extensionReplace, "\n\tReplace the string set by --extension-name with this string. Implies \"--extension-mode replace\".\n", "STRING" },
		{ "extension-case", 'I', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &args->extensionCi, "\n\tDo a case insensitive search when --extension-mode is set to \"replace\".\n", NULL },
		{ "extension-regex", 'X', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &args->extensionRegex, "\n\tUse the string set by --extension-name as a regular expression when --extension-mode is set to \"replace\".\n", NULL },
		{ "extension-elements", 'E', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &args->extensionElements, "\n\tThe number of dots to consider part of a filename an extension. A negative value will use all dots, excluding an initial dot. Default value is -1.\n", "NUMBER" },
		{ "number-location", 'L', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &args->numberLocation, "\n\tAn index where to insert a number into a filename. A negative index can be used to set a location relative to a filename's length.\n", "INDEX" },
		{ "number-start", 'S', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &args->numberStart, "\n\tA starting number for the numbering. Default value is 0.\n", "START" },
		{ "number-step", 'T', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &args->numberStep, "\n\tThe increment for the numbering. Default value is 1.\n", "INCREMENT" },
		{ "number-base", 'B', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &args->numberBase, "\n\tThe base for which numerical system to use. Can be between 2 and 64 and is 10 by default.\n", "BASE" },
		{ "number-padding", 'D', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &args->numberPadding, "\n\tA padding number for at least how many digits to print. Said digits are set by --number-padstr. Default value is 1 i.e. no padding.\n", "NUMBER" },
		{ "number-padstr", 'C', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &args->numberPadStr, "\n\tWhat padding string to use for when --number-padding is set. Default value is \"0\".\n", "STRING" },
		{ "number-prefix", 'P', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &args->numberPrefix, "\n\tPrefix the number with this string.\n", "STRING" },
		{ "number-suffix", 'S', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &args->numberSuffix, "\n\tSuffix the number with this string.\n", "STRING" },
		{ "remove-from", 'o', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &args->removeFrom, "\n\tStart removing characters starting from this index. This number must be lower than the one set by --remove-to.\n", "INDEX" },
		{ "remove-to", 't', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &args->removeTo, "\n\tRemove characters from the index set by --remove-from until this index. This number must be greater than the one set by --remove-from.\n", "INDEX" },
		{ "remove-first", 'f', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &args->removeFirst, "\n\tA number of characters to remove from the beginning of a filename.\n", "LENGTH" },
		{ "remove-last", 'l', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &args->removeLast, "\n\tA number of characters to remove from the end of a filename.\n", "LENGTH" },
		{ "rename-mode", 'm', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &args->renameMode, filMsg, "MODE" },
		{ "rename-name", 'n', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &args->rename, "\n\tReplace a filename with this string. If --rename-mode is set to \"replace\" this string will be replaced by the string set with --rename-replace. Implies \"--rename-mode rename\" if --rename-mode isn't set to \"replace\".\n", "STRING" },
		{ "rename-replace", 'r', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &args->replace, "\n\tReplace the string set by --rename-name with this string. Implies \"--rename-mode replace\".\n", "STRING" },
		{ "rename-case", 'i', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &args->replaceCi, "\n\tDo a case insensitive search when --rename-mode is set to \"replace\".\n", NULL },
		{ "rename-regex", 'x', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &args->replaceRegex, "\n\tUse the string set by --rename-name as a regular expression when --rename-mode is set to \"replace\".\n", NULL },
		{ NULL, '\0', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, NULL, NULL }
	};
	const int nid = 15;
	for (int i = 1; i < argc; ++i)
		for (int j = 0; j < 7; ++j)
			if ((argv[i][0] == '-' && argv[i][1] == params[j + nid].short_name && !argv[i][2]) || (!strncmp(argv[i], "--", 2) && !strcmp(argv[i] + 2, params[j + nid].long_name))) {
				args->number = true;
				goto loopEnd;
			}
loopEnd:
	g_application_add_main_option_entries(G_APPLICATION(app), params);
	g_application_set_option_context_parameter_string(G_APPLICATION(app), "[FILE\xE2\x80\xA6]");
	g_application_set_option_context_summary(G_APPLICATION(app), "Simple Fucking Bulk Rename: A small tool for batch renaming files.");
}
