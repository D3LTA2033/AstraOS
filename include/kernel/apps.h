/* AstraOS - Built-in GUI Applications */

#ifndef _ASTRA_KERNEL_APPS_H
#define _ASTRA_KERNEL_APPS_H

/* Create an Astracor terminal window */
int app_terminal_create(int x, int y, int w, int h);

/* Create a code editor window */
int app_editor_create(int x, int y, int w, int h, const char *filename);

/* Create a file manager window */
int app_filemgr_create(int x, int y, int w, int h);

#endif /* _ASTRA_KERNEL_APPS_H */
