#include "main.h"

/* Draw all interface elements */
void guiDrawAll(void) {
	foldersDraw();
	gridDraw();
	pthread_mutex_lock(&globals.channelsMutex);
	clipsDraw(0);
	clipsDraw(1);
	clipsDraw(2);
	pthread_mutex_unlock(&globals.channelsMutex);	
}

/* For glPrint() */
void fontInit(void) {
	XFontStruct		*font;
	uint16_t		first, last, total, i;

	font = XLoadQueryFont(globals.dpy, "-*-*-medium-r-*-sans-12-*-*-*-*-*-*-*");
	first = font->min_char_or_byte2;
	last = font->max_char_or_byte2;
	total = last - first + 1;
	globals.fontListBase = glGenLists(total);
	glXUseXFont(font->fid, first, total, globals.fontListBase + first);

	globals.fontCharWidths = malloc(total);
	for(i = first; i < last; i++) {
		globals.fontCharWidths[i - first] = font->per_char[i].width;
	}
	
	XFreeFont(globals.dpy, font);
}

/* Returns the pixel width of a string as if glPrint()'d */
uint16_t glPrintWidth(char *s) {
	uint16_t		i, total = 0;

	for(i = 0; i < strlen(s); i++) {
		total += globals.fontCharWidths[(uint16_t)s[i]];
	}
	return(total);
}

/* Our text output function */
void glPrint(char *s) {
	glListBase(globals.fontListBase);
	glCallLists(strlen(s), GL_UNSIGNED_BYTE, s);
}

/* Window and context setup */
void guiInit(void) {
	XVisualInfo		*xvi;
	Colormap		cmap;
	XSetWindowAttributes	swa;
	XEvent			event;
	Atom			mwmHintsAtom;
	GLXContext 		ctx;
	struct mwmHints_t 	mwmHints;
	int attrs[] = {
		GLX_RGBA,
		GLX_DOUBLEBUFFER,
		GLX_RED_SIZE, 8,
		GLX_GREEN_SIZE, 8,
		GLX_BLUE_SIZE, 8,
		GLX_ALPHA_SIZE, 8,
		None
	};

	/* Open connection, create the X window and GLX context */
	globals.dpy = XOpenDisplay(NULL);
	xvi = glXChooseVisual(globals.dpy, DefaultScreen(globals.dpy), attrs);
	cmap = XCreateColormap(globals.dpy, RootWindow(globals.dpy, xvi->screen), xvi->visual, AllocNone);
	swa.colormap = cmap;
	swa.border_pixel = 0;
	swa.event_mask = StructureNotifyMask | ButtonPressMask | ButtonReleaseMask | KeyPressMask | KeyReleaseMask | ExposureMask | PointerMotionMask;
	globals.win = XCreateWindow(globals.dpy, RootWindow(globals.dpy, xvi->screen), 0, 0, 1440, 900, 0, xvi->depth, InputOutput, xvi->visual, CWBorderPixel|CWColormap|CWEventMask, &swa);
	ctx = glXCreateContext(globals.dpy, xvi, 0, GL_TRUE);
	
	XFree(xvi);

	/* Request no window decorations */
        mwmHintsAtom = XInternAtom(globals.dpy, "_MOTIF_WM_HINTS", 0);
        mwmHints.flags = MWM_HINTS_FUNCTIONS | MWM_HINTS_DECORATIONS;
        mwmHints.functions = 0;
        mwmHints.decorations = 0;
        XChangeProperty(globals.dpy, globals.win, mwmHintsAtom, mwmHintsAtom, 32, PropModeReplace, (unsigned char *)&mwmHints, 4);
	
	/* Show the window, wait for it to appear and activate the context */
	XMapWindow(globals.dpy, globals.win);
	XWindowEvent(globals.dpy, globals.win, StructureNotifyMask, &event);
	glXMakeCurrent(globals.dpy, globals.win, ctx);

	/* Force the window to be really fullscreen */
        XMoveResizeWindow(globals.dpy, globals.win, -1, -1, 1440, 900);

	/* Basic GL setup */
	glOrtho(0.0, 1440.0, 900.0, 0.0, -1.0, 1.0);
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	glXSwapBuffers(globals.dpy, globals.win);
	glClear(GL_COLOR_BUFFER_BIT);
	glXSwapBuffers(globals.dpy, globals.win);

	fontInit();
}

/* Window and context teardown */
void guiUninit(void) {
	free(globals.fontCharWidths);
	XCloseDisplay(globals.dpy);
}

/* Thread entry point - main event loop */
void gui(void *dontcare) {
	XEvent			event;
/*	struct sched_param	param; */
	
	/* Set a lowish priority */
	/* param.sched_priority = 3;
	pthread_setsched	pthread_self(), SCHED_FIFO, &param); */

	for(;;) {
		XNextEvent(globals.dpy, &event);
		switch(event.type) {
			case ButtonPress:
				handleButtonPress(&event);
				break;
			case ButtonRelease:
				handleButtonRelease(&event);
				break;
			case KeyPress:
				handleKeyPress(&event);
				break;
			case KeyRelease:
				handleKeyRelease(&event);
				break;
			case MotionNotify:
				handleMotion(&event);
				break;
			case Expose:
				handleExpose(&event);
				break;
		}
	}
}

/* Hide the mouse pointer */
void cursorHide(void) {
	Cursor			cursor;
	Pixmap			zilch;
	XColor			black;
	char			nothing[] = {0, 0, 0, 0,
				             0, 0, 0, 0,
					     0, 0, 0, 0,
				             0, 0, 0, 0};
					     
	black.red = 0;
	black.green = 0;
	black.blue = 0;

	zilch = XCreateBitmapFromData(globals.dpy, globals.win, nothing, 4, 4);

	cursor = XCreatePixmapCursor(globals.dpy, zilch, zilch, &black, &black, 0, 0);
	XDefineCursor(globals.dpy, globals.win, cursor);

	XFreePixmap(globals.dpy, zilch);
	XFreeCursor(globals.dpy, cursor);
}

/* Unhide the mouse pointer */
void cursorUnhide(void) {
	XUndefineCursor(globals.dpy, globals.win);
}
