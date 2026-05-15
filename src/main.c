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

// snap helpers

typedef struct {
    int w, h, x, y;
} WindowLayout;

static WindowLayout GetSnapLayout(SnapPos snap, int monW, int monH) {
    WindowLayout L = { 0 };
    switch (snap) {
    case SNAP_TOP_RIGHT:
        L.w = 500;
        L.h = 150;
        L.x = monW - 500;
        L.y = 0;
        break;
    case SNAP_TOP_LEFT:
        L.w = 500;
        L.h = 150;
        L.x = 0;
        L.y = 0;
        break;
    case SNAP_TOP_CENTER:
        L.w = 500;
        L.h = 150;
        L.x = (monW - 500) / 2;
        L.y = 0;
        break;
    case SNAP_RIGHT_TOP:
        L.w = 150;
        L.h = 500;
        L.x = monW - 150;
        L.y = 0;
        break;
    case SNAP_RIGHT_CENTER:
        L.w = 150;
        L.h = 500;
        L.x = monW - 150;
        L.y = (monH - 500) / 2;
        break;
    case SNAP_RIGHT_BOTTOM:
        L.w = 150;
        L.h = 500;
        L.x = monW - 150;
        L.y = monH - 500;
        break;
    case SNAP_LEFT_TOP:
        L.w = 150;
        L.h = 500;
        L.x = 0;
        L.y = 0;
        break;
    case SNAP_LEFT_CENTER:
        L.w = 150;
        L.h = 500;
        L.x = 0;
        L.y = (monH - 500) / 2;
        break;
    case SNAP_LEFT_BOTTOM:
        L.w = 150;
        L.h = 500;
        L.x = 0;
        L.y = monH - 500;
        break;
    case SNAP_BOTTOM_RIGHT:
        L.w = 500;
        L.h = 150;
        L.x = monW - 500;
        L.y = monH - 150;
        break;
    case SNAP_BOTTOM_LEFT:
        L.w = 500;
        L.h = 150;
        L.x = 0;
        L.y = monH - 150;
        break;
    case SNAP_BOTTOM_CENTER:
        L.w = 500;
        L.h = 150;
        L.x = (monW - 500) / 2;
        L.y = monH - 150;
        break;
    default:
        L.w = 500;
        L.h = 150;
        L.x = monW - 500;
        L.y = 0;
        break;
    }
    return L;
}

static void ApplySnap(SnapPos snap, int monitor) {
    int mon = (monitor < 0) ? GetCurrentMonitor() : monitor;
    int mw = GetMonitorWidth(mon), mh = GetMonitorHeight(mon);
    Vector2 mpos = GetMonitorPosition(mon);
    WindowLayout L = GetSnapLayout(snap, mw, mh);
    SetWindowSize(L.w, L.h);
    SetWindowPosition((int)mpos.x + L.x, (int)mpos.y + L.y);
    g_state.monitor_idx = mon;
}

static void SaveSnap(SnapPos snap, int monitor) {
    FILE *pfs = fopen("position.txt", "w");
    if (pfs) {
        fprintf(pfs, "%d %d", (int)snap, monitor);
        fclose(pfs);
    }
}

static const char *SnapName(SnapPos snap) {
    switch (snap) {
    case SNAP_TOP_RIGHT:
        return "top-right";
    case SNAP_TOP_LEFT:
        return "top-left";
    case SNAP_TOP_CENTER:
        return "top-center";
    case SNAP_RIGHT_TOP:
        return "right-top";
    case SNAP_RIGHT_CENTER:
        return "right-center";
    case SNAP_RIGHT_BOTTOM:
        return "right-bottom";
    case SNAP_LEFT_TOP:
        return "left-top";
    case SNAP_LEFT_CENTER:
        return "left-center";
    case SNAP_LEFT_BOTTOM:
        return "left-bottom";
    case SNAP_BOTTOM_RIGHT:
        return "bottom-right";
    case SNAP_BOTTOM_LEFT:
        return "bottom-left";
    case SNAP_BOTTOM_CENTER:
        return "bottom-center";
    default:
        return "top-right";
    }
}

// returns the window-space position of the tail icon for a snap position
static Vector2 TailWindowPos(SnapPos snap) {
    switch (snap) {
    case SNAP_LEFT_TOP:
    case SNAP_LEFT_CENTER:
    case SNAP_LEFT_BOTTOM:
        return (Vector2) { 50.0f, 40.0f };
    case SNAP_RIGHT_TOP:
    case SNAP_RIGHT_CENTER:
    case SNAP_RIGHT_BOTTOM:
        return (Vector2) { 100.0f, 460.0f };
    default: // top and bottom snaps
        return (Vector2) { 460.0f, 50.0f };
    }
}

// find nearest snap zone based on screen-space cursor position
static SnapPos SnapFromScreenPos(float gx, float gy, int monW, int monH) {
    SnapPos best = SNAP_TOP_RIGHT;
    float bestScore = 1e30f;

    float dL = gx, dR = monW - gx, dT = gy, dB = monH - gy;
    float minD = dL;
    if (dR < minD) minD = dR;
    if (dT < minD) minD = dT;
    if (dB < minD) minD = dB;

    for (int i = 0; i < SNAP_COUNT; i++) {
        WindowLayout L = GetSnapLayout((SnapPos)i, monW, monH);
        float cx = L.x + L.w / 2.0f;
        float cy = L.y + L.h / 2.0f;
        float dx = gx - cx, dy = gy - cy;
        float score = dx * dx + dy * dy;

        // bias toward the wall the mouse is hugging (:3)
        if (i >= 3 && i <= 5 && dR == minD)
            score *= 0.5f; // right
        else if (i >= 6 && i <= 8 && dL == minD)
            score *= 0.5f; // left
        else if (i >= 9 && i <= 11 && dB == minD)
            score *= 0.5f; // bottom
        else if (i >= 0 && i <= 2 && dT == minD)
            score *= 0.5f; // top

        if (score < bestScore) {
            bestScore = score;
            best = (SnapPos)i;
        }
    }
    return best;
}

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
#ifdef __linux__
    setenv("GLFW_PLATFORM", "x11", 0);
#endif
    g_platform = CreatePlatform();
    if (!PlatformEnsureSingleInstance(g_platform)) return 0;

    // window settings
    SetConfigFlags(FLAG_WINDOW_TRANSPARENT | FLAG_WINDOW_TOPMOST | FLAG_WINDOW_UNDECORATED | FLAG_VSYNC_HINT);
    SetTraceLogLevel(LOG_NONE);
    InitWindow(500, 150, "Tripletail Overlay");

    // sync to monitor refresh rate
    int monitor = GetCurrentMonitor();
    SetTargetFPS(GetMonitorRefreshRate(monitor));

    // load snap position, stick it to the top right
    g_state.snap_pos = SNAP_TOP_RIGHT;
    g_state.monitor_idx = 0;
    FILE *pfile = fopen("position.txt", "r");
    if (pfile) {
        int sp = 0, mi = 0;
        if (fscanf(pfile, "%d %d", &sp, &mi) >= 1) {
            if (sp >= 0 && sp < SNAP_COUNT) g_state.snap_pos = (SnapPos)sp;
            if (mi >= 0 && mi < GetMonitorCount()) g_state.monitor_idx = mi;
        }
        fclose(pfile);
    }
    ApplySnap(g_state.snap_pos, g_state.monitor_idx);

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
    g_state.drag_snap = -1;

    // drag state
    static int dragging = 0;
    static int drag_committed = 0; // only snap if moved enough
    static float drag_origin_gx = 0, drag_origin_gy = 0;
    static SnapPos drag_preview = SNAP_TOP_RIGHT;

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

#ifdef __linux__
        Vector2 scale = GetWindowScaleDPI();
        mx /= scale.x;
        my /= scale.y;
#endif

        // screen-space mouse position (for drag snapping)
        float gx, gy;
        PlatformGetGlobalMousePos(g_platform, &gx, &gy);

        // hover
        Vector2 tailPos = TailWindowPos(g_state.snap_pos);
        g_state.hovering = CheckCollisionPointCircle((Vector2) { mx, my }, tailPos, 35);

        // left-click drag on the tail :3
        if (g_state.hovering && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !dragging) {
            dragging = 1;
            drag_committed = 0;
            drag_origin_gx = gx;
            drag_origin_gy = gy;
            // disable passthrough while dragging to keep getting events
            ClearWindowState(FLAG_WINDOW_MOUSE_PASSTHROUGH);
        }

        if (dragging) {
            // only snap if mouse has moved enough
            float dist

                = sqrtf((gx - drag_origin_gx) * (gx - drag_origin_gx) + (gy - drag_origin_gy) * (gy - drag_origin_gy));
            if (!drag_committed && dist > 15.0f) drag_committed = 1;

            if (drag_committed) {
                // find monitor
                int monCount = GetMonitorCount();
                int targetMon = 0;
                for (int i = 0; i < monCount; i++) {
                    Vector2 mp = GetMonitorPosition(i);
                    int mw = GetMonitorWidth(i), mh = GetMonitorHeight(i);
                    if (gx >= mp.x && gx < mp.x + mw && gy >= mp.y && gy < mp.y + mh) {
                        targetMon = i;
                        break;
                    }
                }

                int mw = GetMonitorWidth(targetMon), mh = GetMonitorHeight(targetMon);
                Vector2 mp = GetMonitorPosition(targetMon);

                // where do I go
                drag_preview = SnapFromScreenPos(gx - mp.x, gy - mp.y, mw, mh);
                g_state.drag_snap = drag_preview;

                if (drag_preview != g_state.snap_pos || targetMon != g_state.monitor_idx) {
                    g_state.snap_pos = drag_preview;
                    ApplySnap(g_state.snap_pos, targetMon);
                }
            }

            if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                dragging = 0;
                g_state.drag_snap = -1;
                if (drag_committed) {
                    SaveSnap(g_state.snap_pos, g_state.monitor_idx);
                    g_state.status_timer = 2.0f;
                    snprintf(g_state.status_text, sizeof(g_state.status_text), "%s", SnapName(g_state.snap_pos));
                }
                g_state.last_hover = -1;
            }
        }

        // toggle click-through
        if (!dragging) {
            if (g_state.hovering != g_state.last_hover) {
                if (g_state.hovering)
                    ClearWindowState(FLAG_WINDOW_MOUSE_PASSTHROUGH);
                else
                    SetWindowState(FLAG_WINDOW_MOUSE_PASSTHROUGH);
                g_state.last_hover = g_state.hovering;
            }
        }

        if (g_state.hovering && !dragging) {
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

        // if no audio for 10s, kick the stream
        static double last_reconnect_attempt = 0;
        double now = GetTime();
        if (now - GetLastAudioTime() > 10.0 && now - last_reconnect_attempt > 15.0) {
            RequestStreamReconnect();
            last_reconnect_attempt = now;
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
