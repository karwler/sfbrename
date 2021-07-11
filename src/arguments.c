#include "arguments.h"
#include "main.h"
#include <ctype.h>
#include <sys/stat.h>

static void checkArgName(gchar** name) {
	if (*name) {
		size_t len = strlen(*name);
		if (len >= FILENAME_MAX) {
			g_printerr("name '%s' is too long\n", *name);
			g_free(*name);
			*name = NULL;
		}
	}
}

static RenameMode parseRenameMode(const gchar* mode) {
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
		g_printerr("current working directory path is too long\n");

	Arguments* args = win->args;
	if (!args->noGui && args->files) {
		for (int i = 0; i < args->nFiles; ++i) {
			const char* file = g_file_peek_path(args->files[i]);
			if (file) {
				if (file[0] == '/')
					addFile(win, file);
				else {
					size_t len = strlen(file);
					if (plen + len < PATH_MAX) {
						memcpy(path + plen, file, (len + 1) * sizeof(char));
						addFile(win, path);
					} else
						g_printerr("path for '%s' is too long\n", file);
				}
			}
		}
		args->files = NULL;
	}

	args->gotExtensionMode = parseRenameMode(args->extensionMode);
	args->extensionElements = nclamp(args->extensionElements, -1, FILENAME_MAX - 1);
	checkArgName(&args->extensionName);
	checkArgName(&args->extensionReplace);

	args->gotRenameMode = parseRenameMode(args->renameMode);
	checkArgName(&args->rename);
	checkArgName(&args->replace);

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

	GOptionEntry params[] = {
		{ "add-insert", 'i', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &args->addInsert, "", NULL, },
		{ "add-at", 'k', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &args->addAt, "", NULL, },
		{ "add-prefix", 'p', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &args->addPrefix, "", NULL, },
		{ "add-suffix", 's', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &args->addSuffix, "", NULL, },
		{ "destination-mode", 'D', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &args->destinationMode, "", NULL, },
		{ "destination", 'd', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &args->destination, "", NULL, },
		{ "extension-mode", 'M', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &args->extensionMode, "", NULL, },
		{ "extension-name", 'N', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &args->extensionName, "", NULL, },
		{ "extension-replace", 'R', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &args->extensionReplace, "", NULL, },
		{ "extension-case", 'I', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &args->extensionCi, "", NULL, },
		{ "extension-regex", 'X', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &args->extensionRegex, "", NULL, },
		{ "extension-elements", 'E', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &args->extensionElements, "", NULL, },
		{ "backwards", 'b', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &args->backwards, "", NULL, },
		{ "no-auto-preview", 'a', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &args->noAutoPreview, "", NULL, },
		{ "no-gui", 'g', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &args->noGui, "", NULL, },
		{ "number-location", 'L', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &args->numberLocation, "", NULL, },
		{ "number-start", 'S', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &args->numberStart, "", NULL, },
		{ "number-step", 'T', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &args->numberStep, "", NULL, },
		{ "number-base", 'B', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &args->numberBase, "", NULL, },
		{ "number-padding", 'D', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &args->numberPadding, "", NULL, },
		{ "number-padstr", 'C', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &args->numberPadStr, "", NULL, },
		{ "number-prefix", 'P', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &args->numberPrefix, "", NULL, },
		{ "number-suffix", 'S', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &args->numberSuffix, "", NULL, },
		{ "dry", 'y', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &args->dry, "", NULL, },
		{ "remove-from", 'o', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &args->removeFrom, "", NULL, },
		{ "remove-to", 't', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &args->removeTo, "", NULL, },
		{ "remove-first", 'f', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &args->removeFirst, "", NULL, },
		{ "remove-last", 'l', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT64, &args->removeLast, "", NULL, },
		{ "rename-mode", 'm', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &args->renameMode, "", NULL, },
		{ "rename-name", 'n', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &args->rename, "", NULL, },
		{ "rename-replace", 'r', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &args->replace, "", NULL, },
		{ "rename-case", 'i', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &args->replaceCi, "", NULL, },
		{ "rename-regex", 'x', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &args->replaceRegex, "", NULL, },
		{ NULL, '\0', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, NULL, NULL }
	};
	const int nid = 15;
	for (int i = 1; i < argc; ++i)
		for (int j = 0; j < 7; ++j) {
			if ((argv[i][0] == '-' && argv[i][1] == params[j + nid].short_name && !argv[i][2]) || (!strncmp(argv[i], "--", 2) && !strcmp(argv[i] + 2, params[j + nid].long_name))) {
				args->number = true;
				goto loopEnd;
			}
		}
loopEnd:
	g_application_add_main_option_entries(G_APPLICATION(app), params);
}
