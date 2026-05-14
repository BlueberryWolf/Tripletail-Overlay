#include "platform.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
// force rename windows functions that clash with raylib types/functions
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
#endif

#include <raylib.h>

#ifdef _WIN32
void GetGlobalMousePos(void *windowHandle, float *x, float *y) {
    POINT p;
    GetCursorPos(&p);
    if (x) *x = (float)p.x;
    if (y) *y = (float)p.y;
}

void OptimizeMemory(void) { SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1); }

void SetWindowOverlay(void *windowHandle) {
    HWND hwnd = (HWND)windowHandle;
    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, style | WS_EX_TOOLWINDOW | WS_EX_TOPMOST);

    // windows api garbage to hide from taskbar and stay on top
    typedef struct {
        void *lpVtbl;
    } ITaskbarList;
    typedef struct {
        HRESULT(STDMETHODCALLTYPE *QueryInterface)(void *, const GUID *, void **);
        ULONG(STDMETHODCALLTYPE *AddRef)(void *);
        ULONG(STDMETHODCALLTYPE *Release)(void *);
        HRESULT(STDMETHODCALLTYPE *HrInit)(void *);
        HRESULT(STDMETHODCALLTYPE *AddTab)(void *, HWND);
        HRESULT(STDMETHODCALLTYPE *DeleteTab)(void *, HWND);
        HRESULT(STDMETHODCALLTYPE *ActivateTab)(void *, HWND);
        HRESULT(STDMETHODCALLTYPE *SetActiveAlt)(void *, HWND);
    } ITaskbarListVtbl;

    const GUID CLSID_TaskbarList = { 0x56a868b1, 0x0ad4, 0x11d0, { 0xb9, 0xa9, 0x00, 0xa0, 0xc9, 0x22, 0x31, 0x96 } };
    const GUID IID_ITaskbarList = { 0x56a868b1, 0x0ad4, 0x11d0, { 0xb9, 0xa9, 0x00, 0xa0, 0xc9, 0x22, 0x31, 0x96 } };

    ITaskbarList *ptl = NULL;
    CoInitialize(NULL);
    if (SUCCEEDED(CoCreateInstance(&CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, &IID_ITaskbarList, (void **)&ptl))) {
        ITaskbarListVtbl *vtbl = (ITaskbarListVtbl *)ptl->lpVtbl;
        vtbl->HrInit(ptl);
        vtbl->DeleteTab(ptl, hwnd);
        vtbl->Release(ptl);
    }

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

void GetGlobalMousePos(void *windowHandle, float *x, float *y) {
    // we need global pos even when passing through
    Display *display = XOpenDisplay(NULL);
    if (!display) return;

    Window root = DefaultRootWindow(display);
    Window child;
    int rootX, rootY, winX, winY;
    unsigned int mask;
    if (XQueryPointer(display, root, &root, &child, &rootX, &rootY, &winX, &winY, &mask)) {
        if (x) *x = (float)rootX;
        if (y) *y = (float)rootY;
    }
    XCloseDisplay(display);
}

void OptimizeMemory(void) { malloc_trim(0); }

static int X11ErrorHandler(Display *display, XErrorEvent *error) {
    (void)display;
    (void)error;
    return 0;
}

void SetWindowOverlay(void *windowHandle) {
    Display *display = XOpenDisplay(NULL);
    if (!display) return;

    // suppress X11 errors because I think Wayland/XWayland is picky about timing
    int (*oldHandler)(Display *, XErrorEvent *) = XSetErrorHandler(X11ErrorHandler);

    Window win = (Window)windowHandle;
    Atom stateAbove = XInternAtom(display, "_NET_WM_STATE_ABOVE", False);
    Atom stateSticky = XInternAtom(display, "_NET_WM_STATE_STICKY", False);
    Atom stateSkipTaskbar = XInternAtom(display, "_NET_WM_STATE_SKIP_TASKBAR", False);
    Atom wmState = XInternAtom(display, "_NET_WM_STATE", False);

    Atom atoms[] = { stateAbove, stateSticky, stateSkipTaskbar };
    XChangeProperty(display, win, wmState, XA_ATOM, 32, PropModeReplace, (unsigned char *)atoms, 3);

    XFlush(display);
    XSetErrorHandler(oldHandler);
    XCloseDisplay(display);
}

bool EnsureSingleInstance(void) {
    int fd = open("/tmp/tripletail-overlay.lock", O_CREAT | O_RDWR, 0666);
    if (fd < 0) return true; // well shit ig we tried lol

    if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
        close(fd);
        return false;
    }
    return true;
}
#endif
