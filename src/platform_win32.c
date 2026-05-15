#include "platform.h"
#include <raylib.h>
#include <stdlib.h>

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

struct Platform {
    HANDLE hMutex;
};

Platform *CreatePlatform(void) {
    Platform *p = (Platform *)calloc(1, sizeof(Platform));
    return p;
}

void DestroyPlatform(Platform *p) {
    if (p->hMutex) CloseHandle(p->hMutex);
    free(p);
}

void PlatformGetMousePos(Platform *p, void *windowHandle, float *x, float *y) {
    (void)p;
    POINT pt;
    GetCursorPos(&pt);
    if (windowHandle) ScreenToClient((HWND)windowHandle, &pt);
    if (x) *x = (float)pt.x;
    if (y) *y = (float)pt.y;
}

void PlatformGetGlobalMousePos(Platform *p, float *x, float *y) {
    (void)p;
    POINT pt;
    GetCursorPos(&pt);
    if (x) *x = (float)pt.x;
    if (y) *y = (float)pt.y;
}

void PlatformOptimizeMemory(Platform *p) {
    (void)p;
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
}

void PlatformSetWindowOverlay(Platform *p, void *windowHandle) {
    (void)p;
    if (!windowHandle) return;
    HWND hwnd = (HWND)windowHandle;

    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    style &= ~WS_EX_APPWINDOW;
    style |= WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE;
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, style);

    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

bool PlatformEnsureSingleInstance(Platform *p) {
    p->hMutex = CreateMutexA(NULL, TRUE, "TripletailOverlayMutex_v1");
    if (p->hMutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (p->hMutex) {
            CloseHandle(p->hMutex);
            p->hMutex = NULL;
        }
        return false;
    }
    return true;
}
