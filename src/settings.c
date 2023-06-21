#ifndef CONSOLE
#include "settings.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <zlib.h>
#ifdef _WIN32
#include <windows.h>
#endif

#define RSC_PATH "share/sfbrename/"
#define RSC_NAME_RESERVE 16
#define SDIRC_NAME "/sfbrename"
#define SFILE_NAME "/settings.dat"
#define INFLATED_SIZE 40000
#define INFLATE_INCREMENT 10000

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
	FILE* file = fopen(path, "rb");
	if (!file || fseek(file, 0, SEEK_END))
		return NULL;
	size_t len = ftell(file);
	if (len == SIZE_MAX || fseek(file, 0, SEEK_SET))
		return NULL;

	char* str = malloc(len * sizeof(char));
	size_t rlen = fread(str, sizeof(char), len, file);
	if (!rlen) {
		free(str);
		return NULL;
	}
	if (rlen < len) {
		len = rlen;
		str = realloc(str, rlen * sizeof(char));
	}
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

static char* getSettingsDir(size_t* len) {
	const char* base = g_get_user_config_dir();
	size_t blen = strlen(base), dlen = strlen(SDIRC_NAME);
	char* path = malloc((blen + dlen + strlen(SFILE_NAME) + 1) * sizeof(char));
	memcpy(path, base, blen * sizeof(char));
	strcpy(path + blen, SDIRC_NAME);
	*len = blen + dlen;
	return path;
}

void loadSettings(Settings* set) {
	*set = (Settings){
		.rscPath = malloc(PATH_MAX * sizeof(char)),
		.autoPreview = true,
		.showDetails = true
	};
#ifdef _WIN32
	wchar* wpath = malloc(PATH_MAX * sizeof(wchar));
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

	size_t dlen;
	char* path = getSettingsDir(&dlen);
	strcpy(path + dlen, SFILE_NAME);

	FILE* ifh = fopen(path, "rb");
	if (ifh) {
		fread(&set->width, sizeof(int), 1, ifh);
		fread(&set->height, sizeof(int), 1, ifh);

		uint8 buf[3];
		switch (fread(buf, sizeof(*buf), sizeof(buf) / sizeof(*buf), ifh)) {
		case 3:
			set->maximized = buf[2];
		case 2:
			set->showDetails = buf[1];
		case 1:
			set->autoPreview = buf[0];
		}
		fclose(ifh);
	}
	free(path);
}

void saveSettings(const Settings* set) {
	size_t dlen;
	char* path = getSettingsDir(&dlen);
#ifdef _WIN32
	mkdir(path);
#else
	mkdir(path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
#endif
	strcpy(path + dlen, SFILE_NAME);

	FILE* ofh = fopen(path, "wb");
	if (ofh) {
		fwrite(&set->width, sizeof(int), 1, ofh);
		fwrite(&set->height, sizeof(int), 1, ofh);

		uint8 buf[3] = { set->autoPreview, set->showDetails, set->maximized };
		fwrite(buf, sizeof(*buf), sizeof(buf) / sizeof(*buf), ofh);
		fclose(ofh);
	} else
		g_printerr("Failed to write settings file '%s'", path);
	free(path);
}
#endif
