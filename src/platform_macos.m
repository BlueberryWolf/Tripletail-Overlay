#include "platform.h"
#include <raylib.h>
#include <stdlib.h>
#import <Cocoa/Cocoa.h>
#include <malloc/malloc.h>
#include <sys/file.h>
#include <unistd.h>
#include <fcntl.h>

struct Platform {
    int lockFd;
};

Platform *CreatePlatform(void) {
    Platform *p = (Platform *)calloc(1, sizeof(Platform));
    p->lockFd = -1;
    return p;
}

void DestroyPlatform(Platform *p) {
    if (p->lockFd != -1) {
        close(p->lockFd);
    }
    free(p);
}

void PlatformGetMousePos(Platform *p, void *windowHandle, float *x, float *y) {
    (void)p;
    (void)windowHandle;

    NSPoint mousePos = [NSEvent mouseLocation];

    NSScreen *mainScreen = [NSScreen mainScreen];
    float screenHeight = mainScreen.frame.size.height;
    
    Vector2 windowPos = GetWindowPosition();
    
    if (x) *x = (float)mousePos.x - windowPos.x;
    if (y) *y = (float)(screenHeight - mousePos.y) - windowPos.y;
}

void PlatformOptimizeMemory(Platform *p) {
    (void)p;
    malloc_zone_pressure_relief(NULL, 0);
}

void PlatformSetWindowOverlay(Platform *p, void *windowHandle) {
    (void)p;
    if (!windowHandle) return;
    
    NSWindow *window = (NSWindow *)windowHandle;
    
    [window setLevel:NSStatusWindowLevel];
    [window setCollectionBehavior:NSWindowCollectionBehaviorCanJoinAllSpaces | NSWindowCollectionBehaviorStationary | NSWindowCollectionBehaviorIgnoresCycle];
    [window setHasShadow:NO];
    [window setIgnoresMouseEvents:YES]; 
}

bool PlatformEnsureSingleInstance(Platform *p) {
    char lockPath[256];
    const char *tmpDir = getenv("TMPDIR");
    if (!tmpDir) tmpDir = "/tmp";
    
    snprintf(lockPath, sizeof(lockPath), "%s/tripletail-overlay.lock", tmpDir);
    p->lockFd = open(lockPath, O_CREAT | O_RDWR, 0666);
    if (p->lockFd < 0) return true;
    
    if (flock(p->lockFd, LOCK_EX | LOCK_NB) < 0) {
        close(p->lockFd);
        p->lockFd = -1;
        return false;
    }
    return true;
}
