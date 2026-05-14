#ifndef NETWORK_H
#define NETWORK_H

#include "ringbuffer.h"
#include <stddef.h>
#include <stdint.h>

void InitNetwork(void);
void CleanupNetwork(void);
void StartAudioStream(RingBuffer *rb);
void NetLog(const char *fmt, ...);

typedef struct {
    char title[256];
    char artist[256];
    char art_url[512];
    int duration;
    int elapsed;
} TrackMetadata;

int GetLatestMetadata(TrackMetadata *meta);
uint8_t *DownloadCoverArt(const char *url, size_t *out_size);

#endif
