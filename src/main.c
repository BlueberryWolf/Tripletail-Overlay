#include "raylib.h"
#include "raymath.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio.h"
#include "network.h"
#include "platform.h"
#include "ringbuffer.h"
#include "state.h"
#include "ui.h"

unsigned char *my_stbi_load_from_memory(unsigned char const *buffer, int len, int *x, int *y, int *channels_in_file,
                                        int desired_channels);
void my_stbi_image_free(void *retval_from_stbi_load);

// global junk
AppState g_state = { 0 };
VisState g_vis = { 0 };
Texture2D g_cover_art = { 0 };
Texture2D g_tail_icon = { 0 };
Font g_font_reg = { 0 };
Font g_font_med = { 0 };
RingBuffer *g_pcm_rb = NULL;
kiss_fft_cfg g_fft_cfg = NULL;
Platform *g_platform = NULL;

static uint8_t *g_pending_art_data = NULL;
static size_t g_pending_art_size = 0;
static pthread_mutex_t g_art_mutex = PTHREAD_MUTEX_INITIALIZER;

// fetching pixels in the background, yay!
static void *DownloadCoverArtThread(void *arg) {
    char *url = (char *)arg;
    size_t size = 0;
    uint8_t *data = DownloadCoverArt(url, &size);
    free(url);
    if (data) {
        pthread_mutex_lock(&g_art_mutex);
        if (g_pending_art_data) free(g_pending_art_data);
        g_pending_art_data = data;
        g_pending_art_size = size;
        pthread_mutex_unlock(&g_art_mutex);
    }
    return NULL;
}

static void LoadPendingCoverArt(void) {
    pthread_mutex_lock(&g_art_mutex);
    if (g_pending_art_data) {
        int w, h, c;
        unsigned char *pixels = my_stbi_load_from_memory(g_pending_art_data, (int)g_pending_art_size, &w, &h, &c, 4);
        if (pixels) {
            Image img = {
                .data = pixels, .width = w, .height = h, .mipmaps = 1, .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
            };
            if (g_cover_art.id != 0) UnloadTexture(g_cover_art);
            g_cover_art = LoadTextureFromImage(img);
            GenTextureMipmaps(&g_cover_art);
            SetTextureFilter(g_cover_art, TEXTURE_FILTER_BILINEAR);
            my_stbi_image_free(pixels);
        }
        free(g_pending_art_data);
        g_pending_art_data = NULL;
    }
    pthread_mutex_unlock(&g_art_mutex);
}

int main(void) {
    g_platform = CreatePlatform();
    if (!PlatformEnsureSingleInstance(g_platform)) return 0;

    // window settings
    SetConfigFlags(FLAG_WINDOW_TRANSPARENT | FLAG_WINDOW_TOPMOST | FLAG_WINDOW_UNDECORATED | FLAG_VSYNC_HINT);
    SetTraceLogLevel(LOG_NONE);
    InitWindow(500, 150, "Tripletail Overlay");
    
    // sync to monitor refresh rate
    int monitor = GetCurrentMonitor();
    SetTargetFPS(GetMonitorRefreshRate(monitor));

    // stick it to the top right
    SetWindowPosition(GetMonitorWidth(monitor) - 500, 0);

    // audio setup
    InitAudioDevice();
    SetAudioStreamBufferSizeDefault(2048);
    AudioStream stream = LoadAudioStream(SAMPLE_RATE, 16, CHANNELS);
    SetAudioStreamCallback(stream, AppAudioCallback);
    PlayAudioStream(stream);

    // warm up the engines
    InitNetwork();
    InitAudioSystem();
    InitUI();
#if defined(_WIN32) || defined(__APPLE__)
    PlatformSetWindowOverlay(g_platform, GetWindowHandle());
#else
    SetWindowState(FLAG_WINDOW_MOUSE_PASSTHROUGH);
#endif

    // buffers for the audio juice
    RingBuffer *net_rb = rb_create(128 * 1024);
    g_pcm_rb = rb_create(SAMPLE_RATE * CHANNELS * sizeof(int16_t));
    StartAudioStream(net_rb);

    // decoder
    pthread_t decode_tid;
    pthread_create(&decode_tid, NULL, DecodeThread, net_rb);

    // volume defaults
    g_state.volume = 0.15f;
    FILE *vfile = fopen("volume.txt", "r");
    if (vfile) {
        if (fscanf(vfile, "%f", &g_state.volume) != 1) g_state.volume = 0.15f;
        fclose(vfile);
    }
    SetAudioStreamVolume(stream, g_state.volume);
    strcpy(g_state.title, "Connecting...");

    g_state.last_hover = false;
    double last_pcm_time = GetTime();

    while (!WindowShouldClose()) {
#ifndef _WIN32
        // give the window a moment to breathe on wayland/macos
        static bool overlay_set = false;
        if (!overlay_set) {
            PlatformSetWindowOverlay(g_platform, GetWindowHandle());
            overlay_set = true;
        }
#endif

        // where is the mouse even
        float mx, my;
        PlatformGetMousePos(g_platform, GetWindowHandle(), &mx, &my);
        g_state.hovering = CheckCollisionPointCircle((Vector2) { mx, my }, (Vector2) { 460, 50 }, 35);

        // toggle click-through
        if (g_state.hovering != g_state.last_hover) {
            if (g_state.hovering)
                ClearWindowState(FLAG_WINDOW_MOUSE_PASSTHROUGH);
            else
                SetWindowState(FLAG_WINDOW_MOUSE_PASSTHROUGH);
            g_state.last_hover = g_state.hovering;
        }

        if (g_state.hovering) {
            // mode switching
            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                g_state.show_mode = (g_state.show_mode + 1) % 3;
                g_state.status_timer = 2.0f;
                if (g_state.show_mode == 0)
                    strcpy(g_state.status_text, "mode: auto");
                else if (g_state.show_mode == 1)
                    strcpy(g_state.status_text, "mode: pinned");
                else
                    strcpy(g_state.status_text, "mode: locked");
            }
            // scroll for volume
            float wheel = GetMouseWheelMove();
            if (wheel != 0) {
#ifdef __APPLE__
                // macos trackpads are crazy sensitive
                g_state.volume = Clamp(g_state.volume + wheel * 0.005f, 0, 1);
#else
                g_state.volume = Clamp(g_state.volume + wheel * 0.015f, 0, 1);
#endif
                SetAudioStreamVolume(stream, g_state.volume);
                FILE *vfs = fopen("volume.txt", "w");
                if (vfs) {
                    fprintf(vfs, "%f", g_state.volume);
                    fclose(vfs);
                }
            }
        }

        // track checking
        TrackMetadata meta;
        if (GetLatestMetadata(&meta)) {
            if (strcmp(meta.title, g_state.title) != 0 && strlen(meta.title) > 0) {
                strcpy(g_state.title, meta.title);
                strcpy(g_state.artist, meta.artist);
                g_state.popup_timer = 10.0f;
                g_state.elapsed = (float)meta.elapsed;
                g_state.progress_lerp = 0.0f;
                if (strcmp(meta.art_url, g_state.art_url) != 0 && strlen(meta.art_url) > 0) {
                    strcpy(g_state.art_url, meta.art_url);
                    pthread_t tid;
                    pthread_create(&tid, NULL, DownloadCoverArtThread, strdup(meta.art_url));
                    pthread_detach(tid);
                }
            } else if (fabs(g_state.elapsed - (float)meta.elapsed) > 2.0f) {
                g_state.elapsed = (float)meta.elapsed;
            }
            g_state.duration = meta.duration;
        }

        if (rb_available(g_pcm_rb) > 0) {
            last_pcm_time = GetTime();
        } else if (GetTime() - last_pcm_time > 10.0) {
            last_pcm_time = GetTime();
        }

        LoadPendingCoverArt();

        // trim the fat
        static float ram_timer = 0;
        ram_timer += GetFrameTime();
        if (ram_timer > 5.0f) {
            PlatformOptimizeMemory(g_platform);
            ram_timer = 0;
        }

        UpdateUIState();
        DrawUI();
    }

    // clean up the mess
    UnloadTexture(g_tail_icon);
    if (g_cover_art.id != 0) UnloadTexture(g_cover_art);
    UnloadFont(g_font_reg);
    UnloadFont(g_font_med);
    CleanupAudioSystem();
    CleanupNetwork();
    DestroyPlatform(g_platform);
    CloseWindow();
    return 0;
}
