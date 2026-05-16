#ifndef STATE_H
#define STATE_H

typedef enum {
    SNAP_TOP_RIGHT = 0,
    SNAP_TOP_LEFT,
    SNAP_TOP_CENTER,
    SNAP_RIGHT_TOP,
    SNAP_RIGHT_CENTER,
    SNAP_RIGHT_BOTTOM,
    SNAP_LEFT_TOP,
    SNAP_LEFT_CENTER,
    SNAP_LEFT_BOTTOM,
    SNAP_BOTTOM_RIGHT,
    SNAP_BOTTOM_LEFT,
    SNAP_BOTTOM_CENTER,
    SNAP_COUNT
} SnapPos;

#include "kiss_fft.h"
#include "raylib.h"
#include "ringbuffer.h"
#include <pthread.h>

typedef struct {
    char user[32];
    char text[128];
    Color color;
    float timer;
    float alpha;
    float y_offset;
} ChatMessage;

#define MAX_VISIBLE_CHAT 3
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
    SnapPos snap_pos;
    int monitor_idx;
    int drag_snap; // -1 = not dragging, else SnapPos of hover zone
    ChatMessage chat[MAX_VISIBLE_CHAT];
    char chat_input[128];
    int chat_input_active;
    float chat_focus_alpha;
    char username[32];
    Color user_color;
    int has_set_name;
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
