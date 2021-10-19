#include "vlk_window.h"

#include <stdio.h>

//#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>

static Display *dpy;
static int screen;
static Window win;
static GC gc;
static unsigned int width = 1280;
static unsigned int height = 720;

Display *window_get_dpy() {
	return dpy;
}

Window window_get_window() {
	return win;
}

void window_create() {
    unsigned long black, white;
    dpy = XOpenDisplay((char*)0);
    screen = DefaultScreen(dpy);
    black = BlackPixel(dpy, screen);
    white = WhitePixel(dpy, screen);

	// Get Display Width and Height
	XWindowAttributes ra;
	XGetWindowAttributes(dpy, DefaultRootWindow(dpy), &ra);\

    win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy),
				ra.width / 2, ra.height / 2,
				width, height,
				5, white, black);

    XSetStandardProperties(dpy, win, "vlkTest", "vlkTest", None, NULL, 0, NULL);
    XSelectInput(dpy, win, ExposureMask | ButtonPressMask | KeyPressMask);
    gc = XCreateGC(dpy, win, 0, 0);
    XSetBackground(dpy, gc, white);
    XSetForeground(dpy, gc, black);

    XClearWindow(dpy, win);
    XMapRaised(dpy, win);
}

char window_run() {
	Atom wmDeleteMessage = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(dpy, win, &wmDeleteMessage, 1);

    XEvent event;		/* the XEvent declaration !!! */
	KeySym key;		/* a dealie-bob to handle KeyPress Events */	
	char text[255];		/* a char buffer for KeyPress Events */

	while(1) {
		XNextEvent(dpy, &event);
	
		if (event.type==Expose && event.xexpose.count==0) {
			// Redrall
		}
		if (event.type==KeyPress&&
		    XLookupString(&event.xkey,text,255,&key,0)==1) {
			if (text[0]=='q') {
				break;
			}
			printf("You pressed the %c key!\n",text[0]);
		}
		if (event.type==ButtonPress) {
			printf("You pressed a button at (%i,%i)\n",
				event.xbutton.x,event.xbutton.y);
		}
		if (event.type == ClientMessage && event.xclient.data.l[0] == wmDeleteMessage) {
			break;
		}
	}
	return 0;
}

void window_destroy() {
   XFreeGC(dpy, gc);
   XDestroyWindow(dpy, win);
   XCloseDisplay(dpy);
}