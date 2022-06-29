#ifndef LOGIN_H
#define LOGIN_H

#ifndef CONSOLE
#include "common.h"

typedef struct LoginDialog {
	GtkDialog* dialog;
	GtkEntry* etUser;
	GtkEntry* etPassword;
	GtkEntry* etDomain;
} LoginDialog;

GtkBuilder* loadUiFile(const char* relPath, char* path, size_t* pplen);
void setWindowIcon(GtkWindow* window, char* path, size_t plen);
GtkResponseType runLoginDialog(const char* name, LoginDialog* ld);

#endif
#endif
