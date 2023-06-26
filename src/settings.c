#ifndef CONSOLE
#include "settings.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <zlib.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define RSC_PATH "share/sfbrename/"
#define RSC_NAME_RESERVE 32
#define SDIRC_NAME "/sfbrename/"
#define SFILE_NAME "settings.ini"
#define KEYWORD_SIZE "size"
#define KEYWORD_MAXIMIZED "maximized"
#define KEYWORD_PREVIEW "preview"
#define KEYWORD_DETAILS "details"
#define KEYWORD_TEMPLATES "templates"
#define CFG_NAME_RESERVE 16
#define INFLATED_SIZE 40000
#define INFLATE_INCREMENT 10000

#define skipSpaces(str) for (; *str == ' ' || *str == '\t' || *str == '\v' || *str == '\r'; ++str)
#define readBool(str) (!strncasecmp(str, "true", 4) || !strncasecmp(str, "on", 2) || *str == '1')

static char* readGzip(const char* path, size_t* olen) {
	gzFile file = gzopen(path, "rb");
	if (!file)
		return NULL;

	size_t len = 0;
	size_t siz = INFLATED_SIZE;
	char* str = malloc(siz * sizeof(char));
	for (;;) {
		len += gzread(file, str + len, (uint)(siz - len) * sizeof(char));
		if (len < siz)
			break;
		siz += INFLATE_INCREMENT;
		str = realloc(str, siz);
	}
	gzclose(file);
	if (olen)
		*olen = len;
	return str;
}

static char* readFile(const char* path, size_t* olen) {
	int fd = open(path, O_RDONLY);
	if (fd == -1)
		return NULL;

	off_t len = lseek(fd, 0, SEEK_END);
	if (len == -1 || lseek(fd, 0, SEEK_SET) == -1) {
		close(fd);
		return NULL;
	}

	char* str = malloc((len + 1) * sizeof(char));
	ssize_t rlen = read(fd, str, len * sizeof(char));
	close(fd);
	if (rlen == -1) {
		free(str);
		return NULL;
	}
	if (rlen < len) {
		len = rlen;
		str = realloc(str, (rlen + 1) * sizeof(char));
	}
	str[len] = '\0';
	if (olen)
		*olen = len;
	return str;
}

char* loadTextAsset(Settings* set, const char* name, size_t* olen) {
	size_t nlen = strlen(name);
	memcpy(set->rscPath + set->rlen, name, (nlen + 1) * sizeof(char));
	char* text = readGzip(set->rscPath, olen);
	if (!text) {
		set->rscPath[set->rlen + nlen - 3] = '\0';
		text = readFile(set->rscPath, olen);
		if (!text)
			g_printerr("Failed to read %s\n", name);
	}
	return text;
}

GtkBuilder* loadUi(Settings* set, const char* name) {
	size_t glen;
	char* gui = loadTextAsset(set, name, &glen);
	if (!gui)
		return NULL;

	GtkBuilder* builder = gtk_builder_new();
	GError* error = NULL;
	bool ok = gtk_builder_add_from_string(builder, gui, glen, &error);
	free(gui);
	if (!ok) {
		g_printerr("Failed to load %s: %s\n", name, error->message);
		g_clear_error(&error);
		g_object_unref(builder);
		return NULL;
	}
	return builder;
}

static void readRecentTemplates(Settings* set, const char* pos, const char* end) {
	for (uint i = 0; i < MAX_RECENT_TEMPLATES && pos != end;) {
		size_t skips = 0;
		const char* sep;
		for (sep = pos; sep != end && *sep != ';'; ++sep)
			if (*sep == '\\' && sep + 1 != end) {
				++sep;
				++skips;
			}

		size_t len = sep - pos - skips;
		if (len) {
			set->templates[i] = malloc((len + 1) * sizeof(char));
			char* dst = set->templates[i];
			const char* begin;
			for (begin = pos; pos != sep; ++pos) {
				if (*pos == '\\' && sep + 1 != end) {
					size_t slen = pos - begin;
					memcpy(dst, begin, slen * sizeof(char));
					dst += slen;
					begin = ++pos;
				}
			}
			size_t slen = sep - begin;
			memcpy(dst, begin, slen * sizeof(char));
			dst[slen] = '\0';
			++i;
		}
		pos = sep + (sep != end);
	}
}

static void writeRecentTemplates(FILE* fd, const Settings* set) {
	fputs(KEYWORD_TEMPLATES, fd);
	fputc('=', fd);
	for (uint i = 0; i < MAX_RECENT_TEMPLATES && set->templates[i]; ++i) {
		for (const char* src = set->templates[i]; *src;) {
			size_t l = strcspn(src, ";\\");
			fwrite(src, sizeof(char), l, fd);
			if (!src[l])
				break;
			fputc('\\', fd);
			fputc(src[l], fd);
			src += l + 1;
		}
		fputc(';', fd);
	}
	fputc('\n', fd);
}

void loadSettings(Settings* set) {
	set->rscPath = malloc(PATH_MAX * sizeof(char));
	set->autoPreview = true;
	set->showDetails = true;
#ifdef _WIN32
	wchar_t* wpath = malloc(PATH_MAX * sizeof(wchar_t));
	set->rlen = GetModuleFileNameW(NULL, wpath, PATH_MAX) + 1;
	if (set->rlen > 1 && set->rlen <= PATH_MAX)
		set->rlen = WideCharToMultiByte(CP_UTF8, 0, wpath, set->rlen, set->rscPath, PATH_MAX, NULL, NULL);
	free(wpath);
#else
	set->rlen = readlink("/proc/self/exe", set->rscPath, PATH_MAX);
#endif
	if (set->rlen > 1 && set->rlen <= PATH_MAX) {
#ifdef _WIN32
		unbackslashify(set->rscPath);
#endif
		char* pos = memrchr(set->rscPath, '/', --set->rlen);
		if (pos)
			pos = memrchr(set->rscPath, '/', pos - set->rscPath - (pos != set->rscPath));
		set->rlen = pos - set->rscPath + 1;
	} else {
		strcpy(set->rscPath, "../");
		set->rlen = strlen(set->rscPath);
	}
	size_t rslen = strlen(RSC_PATH);
	set->rscPath = realloc(set->rscPath, set->rlen + rslen + RSC_NAME_RESERVE);
	memcpy(set->rscPath + set->rlen, RSC_PATH, rslen);
	set->rlen += rslen;

	const char* base = g_get_user_config_dir();
	size_t blen = strlen(base), slen = strlen(SDIRC_NAME);
	set->cfgPath = malloc((blen + slen + CFG_NAME_RESERVE) * sizeof(char));
	memcpy(set->cfgPath, base, blen * sizeof(char));
	strcpy(set->cfgPath + blen, SDIRC_NAME);
	set->clen = blen + slen;

	strcpy(set->cfgPath + set->clen, SFILE_NAME);
	char* text = readFile(set->cfgPath, NULL);
	if (text) {
		for (const char* pos = text; *pos;) {
			skipSpaces(pos);
			size_t s = strcspn(pos, "=\n");
			if (pos[s] != '=') {
				pos += s + 1;
				continue;
			}

			const char* val = pos + s + 1;
			skipSpaces(val);
			const char* end = strchr(val, '\n');
			if (!end)
				end = val + strlen(val);

			if (!strncasecmp(pos, KEYWORD_SIZE, strlen(KEYWORD_SIZE))) {
				char* next;
				set->width = strtoull(val, &next, 0);
				set->height = strtoull(next, NULL, 0);
			} else if (!strncasecmp(pos, KEYWORD_MAXIMIZED, strlen(KEYWORD_MAXIMIZED)))
				set->maximized = readBool(val);
			else if (!strncasecmp(pos, KEYWORD_PREVIEW, strlen(KEYWORD_PREVIEW)))
				set->autoPreview = readBool(val);
			else if (!strncasecmp(pos, KEYWORD_DETAILS, strlen(KEYWORD_DETAILS)))
				set->showDetails = readBool(val);
			else if (!strncasecmp(pos, KEYWORD_TEMPLATES, strlen(KEYWORD_TEMPLATES)))
				readRecentTemplates(set, val, end);
			pos = end + 1;
		}
		free(text);
	}
}

static void writeIniBool(FILE* fd, char* str, const char* key, bool on) {
	fputs(key, fd);
	fputc('=', fd);
	fputs(on ? "true\n" : "false\n", fd);
}

void saveSettings(Settings* set) {
	set->cfgPath[set->clen] = '\0';
	mkdirCommon(set->cfgPath);
	strcpy(set->cfgPath + set->clen, SFILE_NAME);

	FILE* fd = fopen(set->cfgPath, "w");
	if (fd) {
		char str[64];
		int len = snprintf(str, sizeof(str) / sizeof(char), KEYWORD_SIZE "=%u %u\n", set->width, set->height);
		fwrite(str, sizeof(char), len, fd);
		writeIniBool(fd, str, KEYWORD_MAXIMIZED, set->maximized);
		writeIniBool(fd, str, KEYWORD_PREVIEW, set->autoPreview);
		writeIniBool(fd, str, KEYWORD_DETAILS, set->showDetails);
		writeRecentTemplates(fd, set);
		fclose(fd);
	} else
		g_printerr("Failed to write settings file '%s': %s", set->cfgPath, strerror(errno));
}

void freeSettings(Settings* set) {
	free(set->rscPath);
	free(set->cfgPath);
	for (uint i = 0; i < MAX_RECENT_TEMPLATES && set->templates[i]; ++i)
		free(set->templates[i]);
}
#endif
