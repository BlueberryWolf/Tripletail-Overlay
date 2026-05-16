#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>

typedef struct Platform Platform;

Platform *CreatePlatform(void);
void DestroyPlatform(Platform *p);

void PlatformGetMousePos(Platform *p, void *windowHandle, float *x, float *y);
void PlatformGetGlobalMousePos(Platform *p, float *x, float *y);
void PlatformOptimizeMemory(Platform *p);
void PlatformSetWindowOverlay(Platform *p, void *windowHandle);
void PlatformSetWindowFocusable(Platform *p, void *windowHandle, bool focusable);
void PlatformSetCurrentThreadPriority(Platform *p, int priority);
bool PlatformEnsureSingleInstance(Platform *p);

#endif
