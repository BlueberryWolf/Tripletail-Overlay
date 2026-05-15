#ifndef AUDIO_H
#define AUDIO_H

#include "raylib.h"
#include "ringbuffer.h"

void InitAudioSystem(void);
void CleanupAudioSystem(void);
void AppAudioCallback(void *bufferData, unsigned int frames);
void *DecodeThread(void *lpParam);
float GetBassLevel(void);
double GetLastAudioTime(void);
void GetStreamMetadata(char *title, char *artist);

#endif
