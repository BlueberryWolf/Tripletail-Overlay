#include "ui.h"
#include "audio.h"
#include "raymath.h"
#include "state.h"
#include <stdio.h>
#include <string.h>

#include "assets/poppins_med.h"
#include "assets/poppins_reg.h"
#include "assets/tail.h"

static void FormatTime(int seconds, char *out) { sprintf(out, "%02d:%02d", seconds / 60, seconds % 60); }

static void DrawTextFixed(Font font, const char *text, int x, int y, int size, Color color) {
    Vector2 pos = { (float)x, (float)y };
    float spacing = 1.0f;
    for (int i = 0; text[i] != '\0'; i++) {
        char buffer[2] = { text[i], '\0' };
        DrawTextEx(font, buffer, pos, (float)size, spacing, color);
        if (text[i] >= '0' && text[i] <= '9')
            pos.x += size * 0.6f;
        else if (text[i] == ':')
            pos.x += size * 0.3f;
        else
            pos.x += MeasureTextEx(font, buffer, (float)size, spacing).x;
    }
}

// math for smooth things
static float SmoothLerp(float current, float target, float speed) {
    float dt = GetFrameTime();
    if (dt > 0.1f) dt = 0.1f;
    return Lerp(current, target, 1.0f - expf(-speed * 20.0f * dt));
}

void InitUI(void) {
    // load the tail from memory
    Image tailImg = LoadImageFromMemory(".png", tail_data, tail_size);
    g_tail_icon = LoadTextureFromImage(tailImg);
    UnloadImage(tailImg);
    GenTextureMipmaps(&g_tail_icon);
    SetTextureFilter(g_tail_icon, TEXTURE_FILTER_BILINEAR);

    // big fonts for big screens
    g_font_reg = LoadFontFromMemory(".ttf", poppins_reg_data, poppins_reg_size, 64, NULL, 0);
    g_font_med = LoadFontFromMemory(".ttf", poppins_med_data, poppins_med_size, 64, NULL, 0);
    SetTextureFilter(g_font_reg.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(g_font_med.texture, TEXTURE_FILTER_BILINEAR);
}

void UpdateUIState(void) {
    // ticking the clock
    if (g_state.duration > 0) {
        g_state.elapsed += GetFrameTime();
        if (g_state.elapsed > g_state.duration) g_state.elapsed = (float)g_state.duration;
        g_state.progress_lerp = SmoothLerp(g_state.progress_lerp, g_state.elapsed / (float)g_state.duration, 0.1f);
    }

    // deciding if we should show the song info or hide in shame
    int should_show = 0;
    if (g_state.show_mode == 0)
        should_show = (g_state.popup_timer > 0.0f || g_state.hovering);
    else if (g_state.show_mode == 1)
        should_show = 1;
    else if (g_state.show_mode == 2)
        should_show = 0;

    if (should_show) {
        if (g_state.popup_timer > 0.0f) g_state.popup_timer -= GetFrameTime();
        g_state.info_slide = SmoothLerp(g_state.info_slide, 1.0f, 0.05f);
        g_state.hover_alpha = SmoothLerp(g_state.hover_alpha, 1.0f, 0.1f);
    } else {
        g_state.info_slide = SmoothLerp(g_state.info_slide, 0.0f, 0.03f);
        g_state.hover_alpha = SmoothLerp(g_state.hover_alpha, 0.0f, 0.05f);
    }

    if (g_state.status_timer > 0.0f) g_state.status_timer -= GetFrameTime() * 1.5f;
}

void DrawUI(void) {
    BeginDrawing();
    ClearBackground(BLANK);

    float anchorX = 460.0f, anchorY = 50.0f, visW = 480.0f;
    float barSpacing = visW / VIS_BARS;
    Color cPink = { 236, 72, 153, 200 }, cPurple = { 139, 92, 246, 200 };

    // progress bar line at the top
    DrawRectangle(0, 0, 500, 3, (Color) { 168, 85, 247, 40 });
    int progW = (int)(500.0f * g_state.progress_lerp);
    DrawRectangleGradientH(0, 0, progW, 3, cPink,
                           (Color) { (unsigned char)Lerp(cPink.r, cPurple.r, g_state.progress_lerp),
                                     (unsigned char)Lerp(cPink.g, cPurple.g, g_state.progress_lerp),
                                     (unsigned char)Lerp(cPink.b, cPurple.b, g_state.progress_lerp), 200 });

    // visualizer bars doing visualizer things
    pthread_mutex_lock(&g_vis.mutex);
    for (int i = 0; i < VIS_BARS; i++) {
        float mag = Clamp(g_vis.bins[i], 0, 100);
        g_vis.trail[i] = Lerp(g_vis.trail[i], mag, 0.4f);

        int bx = (int)(i * barSpacing), bw = (int)((i + 1) * barSpacing) - bx - 1;
        float t = (float)i / (VIS_BARS - 1);
        unsigned char a = (unsigned char)(180 + 75 * g_state.hover_alpha);
        Color bCol = { (unsigned char)Lerp(cPink.r, cPurple.r, t), (unsigned char)Lerp(cPink.g, cPurple.g, t),
                       (unsigned char)Lerp(cPink.b, cPurple.b, t), a };

        DrawRectangle(bx, 3, bw < 1 ? 1 : bw, (int)mag, (Color) { bCol.r, bCol.g, bCol.b, (unsigned char)(a * 0.6f) });
        DrawRectangleGradientV(bx, 3, bw < 1 ? 1 : bw, (int)mag, bCol,
                               (Color) { bCol.r, bCol.g, bCol.b, (unsigned char)(a * 0.3f) });
    }
    pthread_mutex_unlock(&g_vis.mutex);

    // that sliding flyout thing
    if (g_state.info_slide > 0.01f) {
        unsigned char a8 = (unsigned char)(255 * g_state.info_slide);
        float fX = Lerp(460.0f, 10.0f, g_state.info_slide);

        // cover art with a metric ton of shadows
        if (g_cover_art.id != 0) {
            DrawRectangle(fX + 16, anchorY - 24, 72, 72, (Color) { 0, 0, 0, (unsigned char)(a8 * 0.2f) });
            DrawRectangle(fX + 14, anchorY - 26, 72, 72, (Color) { 0, 0, 0, (unsigned char)(a8 * 0.35f) });
            DrawRectangle(fX + 12, anchorY - 28, 72, 72, (Color) { 0, 0, 0, (unsigned char)(a8 * 0.5f) });
            DrawTexturePro(g_cover_art, (Rectangle) { 0, 0, (float)g_cover_art.width, (float)g_cover_art.height },
                           (Rectangle) { fX + 8, anchorY - 32, 72, 72 }, (Vector2) { 0, 0 }, 0.0f,
                           (Color) { 255, 255, 255, a8 });
        }

        BeginScissorMode((int)fX + 85, (int)anchorY - 60, 400, 150);

        // shadow colors
        Color sh1 = { 0, 0, 0, (unsigned char)(a8 * 0.3f) }, sh2 = { 0, 0, 0, (unsigned char)(a8 * 0.5f) },
              sh3 = { 0, 0, 0, (unsigned char)(a8 * 0.7f) };

        // title
        DrawTextEx(g_font_med, g_state.title, (Vector2) { fX + 93, anchorY - 30 }, 26, 0, sh1);
        DrawTextEx(g_font_med, g_state.title, (Vector2) { fX + 92, anchorY - 31 }, 26, 0, sh2);
        DrawTextEx(g_font_med, g_state.title, (Vector2) { fX + 91, anchorY - 32 }, 26, 0, sh3);
        DrawTextEx(g_font_med, g_state.title, (Vector2) { fX + 90, anchorY - 33 }, 26, 0,
                   (Color) { 255, 255, 255, a8 });

        // artist
        DrawTextEx(g_font_reg, g_state.artist, (Vector2) { fX + 93, anchorY - 1 }, 20, 0, sh1);
        DrawTextEx(g_font_reg, g_state.artist, (Vector2) { fX + 92, anchorY - 2 }, 20, 0, sh2);
        DrawTextEx(g_font_reg, g_state.artist, (Vector2) { fX + 91, anchorY - 3 }, 20, 0, sh3);
        DrawTextEx(g_font_reg, g_state.artist, (Vector2) { fX + 90, anchorY - 4 }, 20, 0,
                   (Color) { 200, 200, 200, a8 });

        char timeStr[64];
        char eStr[16], dStr[16];
        FormatTime((int)g_state.elapsed, eStr);
        FormatTime(g_state.duration, dStr);
        snprintf(timeStr, sizeof(timeStr), "%s / %s", eStr, dStr);

        // time clock
        DrawTextFixed(g_font_reg, timeStr, (int)fX + 93, (int)anchorY + 28, 16, sh1);
        DrawTextFixed(g_font_reg, timeStr, (int)fX + 92, (int)anchorY + 27, 16, sh2);
        DrawTextFixed(g_font_reg, timeStr, (int)fX + 91, (int)anchorY + 26, 16, sh3);
        DrawTextFixed(g_font_reg, timeStr, (int)fX + 90, (int)anchorY + 25, 16, (Color) { 150, 150, 150, a8 });

        if (g_state.hovering || g_state.show_mode == 1) {
            char vT[32];
            snprintf(vT, sizeof(vT), "vol: %d%%", (int)(g_state.volume * 100));
            if (g_state.show_mode == 1) strcat(vT, " [pin]");
            DrawTextEx(g_font_reg, vT, (Vector2) { fX + 93, anchorY + 50 }, 14, 0, sh1);
            DrawTextEx(g_font_reg, vT, (Vector2) { fX + 92, anchorY + 49 }, 14, 0, sh2);
            DrawTextEx(g_font_reg, vT, (Vector2) { fX + 91, anchorY + 48 }, 14, 0, sh3);
            DrawTextEx(g_font_reg, vT, (Vector2) { fX + 90, anchorY + 47 }, 14, 0,
                       (Color) { 168, 85, 247, (unsigned char)(a8 * 0.8f) });
        }
        EndScissorMode();
    }

    // floating tooltip for mode changes
    if (g_state.status_timer > 0.0f) {
        float st = g_state.status_timer;
        float alpha = st > 1.5f ? (2.0f - st) * 2.0f : (st > 0.5f ? 1.0f : st * 2.0f);
        float offset = (2.0f - st) * 8.0f;
        Vector2 tsize = MeasureTextEx(g_font_med, g_state.status_text, 16, 0);
        unsigned char a = (unsigned char)(alpha * 255);
        Color tsh1 = { 0, 0, 0, (unsigned char)(alpha * 80) }, tsh2 = { 0, 0, 0, (unsigned char)(alpha * 150) },
              tsh3 = { 0, 0, 0, (unsigned char)(alpha * 200) };

        DrawTextEx(g_font_med, g_state.status_text, (Vector2) { anchorX - tsize.x / 2 + 3, anchorY + 50 + offset + 3 },
                   16, 0, tsh1);
        DrawTextEx(g_font_med, g_state.status_text, (Vector2) { anchorX - tsize.x / 2 + 2, anchorY + 50 + offset + 2 },
                   16, 0, tsh2);
        DrawTextEx(g_font_med, g_state.status_text, (Vector2) { anchorX - tsize.x / 2 + 1, anchorY + 50 + offset + 1 },
                   16, 0, tsh3);
        DrawTextEx(g_font_med, g_state.status_text, (Vector2) { anchorX - tsize.x / 2, anchorY + 50 + offset }, 16, 0,
                   (Color) { 255, 255, 255, a });
    }

    // bouncy tail :3
    static float tail_bounce = 0.0f;
    tail_bounce = SmoothLerp(tail_bounce, GetBassLevel() * 8.0f, 0.15f);
    float tScale = (50.0f / g_tail_icon.width) * (1.2f + g_state.hover_alpha * 0.1f + tail_bounce * 0.02f);
    float dW = g_tail_icon.width * tScale, dH = g_tail_icon.height * tScale;

    DrawCircleGradient((int)anchorX, (int)anchorY, 45 + (g_state.hover_alpha * 10),
                       (Color) { 168, 85, 247, (unsigned char)(30 + 30 * g_state.hover_alpha) }, BLANK);
    DrawTexturePro(g_tail_icon, (Rectangle) { 0, 0, (float)g_tail_icon.width, (float)g_tail_icon.height },
                   (Rectangle) { anchorX, anchorY - tail_bounce, dW, dH }, (Vector2) { dW / 2.0f, dH / 2.0f },
                   tail_bounce * 0.6f, WHITE);

    EndDrawing();
}
