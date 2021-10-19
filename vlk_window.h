#ifndef VLK_WINDOW_H
#define VLK_WINDOW_H

#include <X11/Xlib.h>

Display *window_get_dpy();
Window window_get_window();

void window_create();
char window_run();
void window_destroy();

#endif // VLK_WINDOW_H