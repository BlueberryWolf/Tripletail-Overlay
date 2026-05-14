#ifndef STATE_H
#define STATE_H

#include "kiss_fft.h"
#include "raylib.h"
#include "ringbuffer.h"
#include <pthread.h>

#define FFT_SIZE 2048
#define SAMPLE_RATE 48000
#define CHANNELS 2
#define VIS_BARS 80

typedef struct {
    char title[256];
    char artist[256];
    char art_url[512];
    int duration;
    float elapsed;
    float progress_lerp;
    float info_slide;
    float hover_alpha;
    float volume;
    float popup_timer;
    int show_mode;
    int hovering;
    int last_hover;
    float status_timer;
    char status_text[32];
} AppState;

typedef struct {
    float bins[VIS_BARS];
    float trail[VIS_BARS];
    pthread_mutex_t mutex;
} VisState;

extern AppState g_state;
extern VisState g_vis;
extern Texture2D g_cover_art;
extern Texture2D g_tail_icon;
extern Font g_font_reg;
extern Font g_font_med;
extern RingBuffer *g_pcm_rb;
extern kiss_fft_cfg g_fft_cfg;

#endif
