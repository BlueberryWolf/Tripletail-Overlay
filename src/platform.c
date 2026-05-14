#include "platform.h"
#include <raylib.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define Rectangle _WinRectangle
#define CloseWindow _WinCloseWindow
#define ShowCursor _WinShowCursor
#define LoadImage _WinLoadImage
#define DrawText _WinDrawText
#define DrawTextEx _WinDrawTextEx
#define PlaySound _WinPlaySound
#include <objbase.h>
#include <psapi.h>
#include <windows.h>
#undef Rectangle
#undef CloseWindow
#undef ShowCursor
#undef LoadImage
#undef DrawText
#undef DrawTextEx
#undef PlaySound

void InitPlatform(void) {
    // nothing special needed here for now
}

void GetGlobalMousePos(void *windowHandle, float *x, float *y) {
    POINT p;
    GetCursorPos(&p);
    if (windowHandle) ScreenToClient((HWND)windowHandle, &p);
    if (x) *x = (float)p.x;
    if (y) *y = (float)p.y;
}

void OptimizeMemory(void) {
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
}

void SetWindowOverlay(void *windowHandle) {
    if (!windowHandle) return;
    HWND hwnd = (HWND)windowHandle;

    // WS_EX_TOOLWINDOW hides it from the taskbar without needing complex COM
    // WS_EX_TOPMOST ensures it stays on top
    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, style | WS_EX_TOOLWINDOW | WS_EX_TOPMOST);

    // trigger the changes
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

bool EnsureSingleInstance(void) {
    static HANDLE hMutex = NULL;
    hMutex = CreateMutexA(NULL, TRUE, "TripletailOverlayMutex_v1");
    if (hMutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) CloseHandle(hMutex);
        return false;
    }
    return true;
}

#else
#define Font _XFont
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#undef Font
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <sys/file.h>
#include <unistd.h>

static int X11ErrorHandler(Display *display, XErrorEvent *error) {
    (void)display;
    (void)error;
    return 0;
}

void InitPlatform(void) { XSetErrorHandler(X11ErrorHandler); }

void GetGlobalMousePos(void *windowHandle, float *x, float *y) {
    Display *display = XOpenDisplay(NULL);
    if (!display) return;
    Window root = DefaultRootWindow(display), child;
    int rx, ry, wx, wy;
    unsigned int mask;
    if (XQueryPointer(display, root, &root, &child, &rx, &ry, &wx, &wy, &mask)) {
        int dx, dy;
        Window junk;
        if (windowHandle && XTranslateCoordinates(display, root, (Window)windowHandle, rx, ry, &dx, &dy, &junk)) {
            if (x) *x = (float)dx;
            if (y) *y = (float)dy;
        } else {
            if (x) *x = (float)rx;
            if (y) *y = (float)ry;
        }
    }
    XCloseDisplay(display);
}

void OptimizeMemory(void) { malloc_trim(0); }

void SetWindowOverlay(void *windowHandle) {
    if (!windowHandle) return;
    Display *display = XOpenDisplay(NULL);
    if (!display) return;

    Window win = (Window)windowHandle;
    Atom stateAbove = XInternAtom(display, "_NET_WM_STATE_ABOVE", False);
    Atom stateSticky = XInternAtom(display, "_NET_WM_STATE_STICKY", False);
    Atom stateSkipTaskbar = XInternAtom(display, "_NET_WM_STATE_SKIP_TASKBAR", False);
    Atom wmState = XInternAtom(display, "_NET_WM_STATE", False);

    Atom atoms[] = { stateAbove, stateSticky, stateSkipTaskbar };
    XChangeProperty(display, win, wmState, XA_ATOM, 32, PropModeReplace, (unsigned char *)atoms, 3);
    XFlush(display);
    XCloseDisplay(display);
}

bool EnsureSingleInstance(void) {
    int fd = open("/tmp/tripletail-overlay.lock", O_CREAT | O_RDWR, 0666);
    if (fd < 0) return true;
    if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
        close(fd);
        return false;
    }
    return true;
}
#endif
