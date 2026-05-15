#include "platform.h"
#include <raylib.h>
#include <stdlib.h>

#define Font _XFont
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#undef Font
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <sys/file.h>
#include <unistd.h>

struct Platform {
    Display *display;
    int lockFd;
};

static int X11ErrorHandler(Display *display, XErrorEvent *error) {
    (void)display;
    (void)error;
    return 0;
}

Platform *CreatePlatform(void) {
    Platform *p = (Platform *)calloc(1, sizeof(Platform));
    XSetErrorHandler(X11ErrorHandler);
    p->display = XOpenDisplay(NULL);
    p->lockFd = -1;
    return p;
}

void DestroyPlatform(Platform *p) {
    if (p->display) XCloseDisplay(p->display);
    if (p->lockFd != -1) close(p->lockFd);
    free(p);
}

void PlatformGetMousePos(Platform *p, void *windowHandle, float *x, float *y) {
    if (!p->display || !windowHandle) return;
    Window root = DefaultRootWindow(p->display), child;
    int rx, ry, wx, wy;
    unsigned int mask;

    if (XQueryPointer(p->display, root, &root, &child, &rx, &ry, &wx, &wy, &mask)) {
        int tx, ty;
        Window junk;
        if (XTranslateCoordinates(p->display, root, (Window)windowHandle, rx, ry, &tx, &ty, &junk)) {
            if (x) *x = (float)tx;
            if (y) *y = (float)ty;
        } else {
            if (x) *x = (float)wx;
            if (y) *y = (float)wy;
        }
    } else {
        if (x) *x = -10000.0f;
        if (y) *y = -10000.0f;
    }
}

void PlatformGetGlobalMousePos(Platform *p, float *x, float *y) {
    if (!p->display) {
        if (x) *x = 0;
        if (y) *y = 0;
        return;
    }
    Window root = DefaultRootWindow(p->display), child;
    int rx, ry, wx, wy;
    unsigned int mask;
    if (XQueryPointer(p->display, root, &root, &child, &rx, &ry, &wx, &wy, &mask)) {
        if (x) *x = (float)rx;
        if (y) *y = (float)ry;
    } else {
        if (x) *x = 0;
        if (y) *y = 0;
    }
}

void PlatformOptimizeMemory(Platform *p) {
    (void)p;
    malloc_trim(0);
}

void PlatformSetWindowOverlay(Platform *p, void *windowHandle) {
    if (!windowHandle || !p->display) return;
    Window win = (Window)windowHandle;

    Atom stateAbove = XInternAtom(p->display, "_NET_WM_STATE_ABOVE", False);
    Atom stateSticky = XInternAtom(p->display, "_NET_WM_STATE_STICKY", False);
    Atom stateSkipTaskbar = XInternAtom(p->display, "_NET_WM_STATE_SKIP_TASKBAR", False);
    Atom wmState = XInternAtom(p->display, "_NET_WM_STATE", False);

    Atom atoms[] = { stateAbove, stateSticky, stateSkipTaskbar };
    XChangeProperty(p->display, win, wmState, XA_ATOM, 32, PropModeReplace, (unsigned char *)atoms, 3);
    XFlush(p->display);
}

bool PlatformEnsureSingleInstance(Platform *p) {
    p->lockFd = open("/tmp/tripletail-overlay.lock", O_CREAT | O_RDWR, 0666);
    if (p->lockFd < 0) return true;
    if (flock(p->lockFd, LOCK_EX | LOCK_NB) < 0) {
        close(p->lockFd);
        p->lockFd = -1;
        return false;
    }
    return true;
}
