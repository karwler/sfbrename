#ifndef TABLE_H
#define TABLE_H

#ifndef CONSOLE
#include "utils.h"

void addFilesFromDialog(Window* win, GSList* files);
void addFilesFromArguments(Window* win, GFile** files, size_t nFiles);
void setDetailsVisible(Window* win);
G_MODULE_EXPORT void dragEndTblFiles(GtkWidget* widget, GdkDragContext* context, Window* win);

#endif

#endif
