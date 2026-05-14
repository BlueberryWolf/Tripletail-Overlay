#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>

void GetGlobalMousePos(void *windowHandle, float *x, float *y);
void OptimizeMemory(void);
void SetWindowOverlay(void *windowHandle);
bool EnsureSingleInstance(void);

#endif
