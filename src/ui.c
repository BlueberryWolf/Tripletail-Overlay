#include "ui.h"
#include "audio.h"
#include "raymath.h"
#include "rlgl.h"
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

// returns 1 if snap is a side (vertical) orientation
int SnapIsSide(SnapPos snap) {
    return snap == SNAP_LEFT_TOP || snap == SNAP_LEFT_CENTER || snap == SNAP_LEFT_BOTTOM || snap == SNAP_RIGHT_TOP
           || snap == SNAP_RIGHT_CENTER || snap == SNAP_RIGHT_BOTTOM;
}

// returns 1 if snap is on the bottom half
int SnapIsBottom(SnapPos snap) {
    return snap == SNAP_BOTTOM_LEFT || snap == SNAP_BOTTOM_CENTER || snap == SNAP_BOTTOM_RIGHT;
}

// returns 1 if snap is on the right side
int SnapIsRight(SnapPos snap) {
    return snap == SNAP_RIGHT_TOP || snap == SNAP_RIGHT_CENTER || snap == SNAP_RIGHT_BOTTOM;
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
        g_state.elapsed
            = g_state.base_elapsed + (float)(g_state.samples_played - g_state.samples_at_base) / SAMPLE_RATE;
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

    for (int i = 0; i < MAX_VISIBLE_CHAT; i++) {
        if (g_state.chat[i].timer > 0.0f) {
            g_state.chat[i].timer -= GetFrameTime();
            if (g_state.chat[i].timer < 1.0f) {
                g_state.chat[i].alpha = g_state.chat[i].timer;
                g_state.chat[i].y_offset = SmoothLerp(g_state.chat[i].y_offset, 15.0f, 0.1f);
            } else {
                g_state.chat[i].alpha = 1.0f;
                g_state.chat[i].y_offset = SmoothLerp(g_state.chat[i].y_offset, 0.0f, 0.2f);
            }
        } else {
            g_state.chat[i].alpha = 0.0f;
            g_state.chat[i].y_offset = 10.0f;
        }
    }

    if (g_state.chat_input_active) {
        g_state.chat_focus_alpha = SmoothLerp(g_state.chat_focus_alpha, 1.0f, 0.15f);
    } else {
        g_state.chat_focus_alpha = SmoothLerp(g_state.chat_focus_alpha, 0.0f, 0.2f);
    }
}

static void DrawScene(void) {
    float anchorX = 460.0f, anchorY = 50.0f, visW = 480.0f;
    float barSpacing = visW / VIS_BARS;
    Color cPink = { 236, 72, 153, 200 }, cPurple = { 139, 92, 246, 200 };
    int isBottom = SnapIsBottom(g_state.snap_pos);

    // progress bar line at the top
    if (!isBottom) {
        DrawRectangle(0, 0, 500, 3, (Color) { 168, 85, 247, 40 });
        int progW = (int)(500.0f * g_state.progress_lerp);
        DrawRectangleGradientH(0, 0, progW, 3, cPink,
                               (Color) { (unsigned char)Lerp(cPink.r, cPurple.r, g_state.progress_lerp),
                                         (unsigned char)Lerp(cPink.g, cPurple.g, g_state.progress_lerp),
                                         (unsigned char)Lerp(cPink.b, cPurple.b, g_state.progress_lerp), 200 });
    } else {
        DrawRectangle(0, 147, 500, 3, (Color) { 168, 85, 247, 40 });
        int progW = (int)(500.0f * g_state.progress_lerp);
        DrawRectangleGradientH(0, 147, progW, 3, cPink,
                               (Color) { (unsigned char)Lerp(cPink.r, cPurple.r, g_state.progress_lerp),
                                         (unsigned char)Lerp(cPink.g, cPurple.g, g_state.progress_lerp),
                                         (unsigned char)Lerp(cPink.b, cPurple.b, g_state.progress_lerp), 200 });
    }

    // visualizer bars. top snaps go downward, bottom snaps grow upward, sides are flipped so bass is at top
    int isLeft = (g_state.snap_pos == SNAP_LEFT_TOP || g_state.snap_pos == SNAP_LEFT_CENTER
                  || g_state.snap_pos == SNAP_LEFT_BOTTOM);
    pthread_mutex_lock(&g_vis.mutex);
    for (int i = 0; i < VIS_BARS; i++) {
        // left wall. reverse to put bass at top
        int bi = isLeft ? (VIS_BARS - 1 - i) : i;
        float mag = Clamp(g_vis.bins[bi], 0, 100);
        g_vis.trail[bi] = Lerp(g_vis.trail[bi], mag, 0.4f);

        int bx = (int)(i * barSpacing), bw = (int)((i + 1) * barSpacing) - bx - 1;
        float t = (float)i / (VIS_BARS - 1);
        unsigned char a = (unsigned char)(180 + 75 * g_state.hover_alpha);
        Color bCol = { (unsigned char)Lerp(cPink.r, cPurple.r, t), (unsigned char)Lerp(cPink.g, cPurple.g, t),
                       (unsigned char)Lerp(cPink.b, cPurple.b, t), a };

        if (!isBottom) {
            // normal. bars grow downward from y=3
            if (mag > 1.0f) {
                DrawRectangleGradientV(bx, 3, bw < 1 ? 1 : bw, (int)mag, bCol,
                                       (Color) { bCol.r, bCol.g, bCol.b, (unsigned char)(a * 0.3f) });
            }
        } else {
            // bottom. bars grow upward from y=147
            if (mag > 1.0f) {
                int by = 147 - (int)mag;
                DrawRectangleGradientV(bx, by, bw < 1 ? 1 : bw, (int)mag,
                                       (Color) { bCol.r, bCol.g, bCol.b, (unsigned char)(a * 0.3f) }, bCol);
            }
        }
    }
    pthread_mutex_unlock(&g_vis.mutex);

    // that sliding flyout thing
    if (g_state.info_slide > 0.01f) {
        unsigned char a8 = (unsigned char)(255 * g_state.info_slide);
        float fX = Lerp(460.0f, 10.0f, g_state.info_slide);

        // cover art with a single shadow
        if (g_cover_art.id != 0) {
            DrawRectangle(fX + 12, anchorY - 28, 76, 76, (Color) { 0, 0, 0, (unsigned char)(a8 * 0.4f) });
            DrawTexturePro(g_cover_art, (Rectangle) { 0, 0, (float)g_cover_art.width, (float)g_cover_art.height },
                           (Rectangle) { fX + 8, anchorY - 32, 72, 72 }, (Vector2) { 0, 0 }, 0.0f,
                           (Color) { 255, 255, 255, a8 });
        }

        // scissor clips text to the flyout area
        int usesScissor = !SnapIsSide(g_state.snap_pos);
        if (usesScissor) BeginScissorMode((int)fX + 85, (int)anchorY - 60, 400, 150);

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
        if (usesScissor) EndScissorMode();
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

    float chatY = SnapIsBottom(g_state.snap_pos) ? 40.0f : 80.0f;
    for (int i = 0; i < MAX_VISIBLE_CHAT; i++) {
        if (g_state.chat[i].timer > 0.0f) {
            unsigned char a = (unsigned char)(255 * g_state.chat[i].alpha);
            Color userColor = g_state.chat[i].color;
            userColor.a = a;
            Color textColor = { 255, 255, 255, a };

            float y_off = g_state.chat[i].y_offset;
            float popOffset = 0.0f;
            if (g_state.chat[i].timer > 7.7f) {
                popOffset = (g_state.chat[i].timer - 7.7f) * 20.0f;
            }

            char userPart[64];
            snprintf(userPart, sizeof(userPart), "%s: ", g_state.chat[i].user);
            Vector2 userSize = MeasureTextEx(g_font_reg, userPart, 16, 0);

            Vector2 basePos = { 10, chatY - popOffset + y_off };

            // shadow
            DrawTextEx(g_font_reg, userPart, (Vector2) { basePos.x + 1, basePos.y + 1 }, 16, 0,
                       (Color) { 0, 0, 0, (unsigned char)(a * 0.5f) });
            DrawTextEx(g_font_reg, g_state.chat[i].text, (Vector2) { basePos.x + 1 + userSize.x, basePos.y + 1 }, 16, 0,
                       (Color) { 0, 0, 0, (unsigned char)(a * 0.5f) });

            // content
            DrawTextEx(g_font_reg, userPart, basePos, 16, 0, userColor);
            DrawTextEx(g_font_reg, g_state.chat[i].text, (Vector2) { basePos.x + userSize.x, basePos.y }, 16, 0,
                       textColor);

            chatY += 20.0f;
        }
    }

    // dim
    if (g_state.chat_focus_alpha > 0.01f) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      (Color) { 0, 0, 0, (unsigned char)(g_state.chat_focus_alpha * 100) });
    }

    if (g_state.chat_focus_alpha > 0.01f) {
        float falpha = g_state.chat_focus_alpha;
        unsigned char a = (unsigned char)(falpha * 255);
        float boxW = 320.0f;
        float boxH = 34.0f;

        float boxX = 80.0f - (1.0f - falpha) * 40.0f;
        float boxY = SnapIsBottom(g_state.snap_pos) ? 30.0f : 110.0f;

        DrawRectangleRounded((Rectangle) { boxX, boxY, boxW, boxH }, 0.2f, 8,
                             (Color) { 20, 20, 20, (unsigned char)(a * 0.95f) });
        DrawRectangleRoundedLines((Rectangle) { boxX, boxY, boxW, boxH }, 0.2f, 8, 2, (Color) { 168, 85, 247, a });
        static float cursor_timer = 0;
        cursor_timer += GetFrameTime();
        const char *input_text = g_state.chat_input;
        Vector2 text_size = MeasureTextEx(g_font_reg, input_text, 18, 0);

        DrawTextEx(g_font_reg, input_text, (Vector2) { boxX + 10, boxY + 8 }, 18, 0, (Color) { 255, 255, 255, a });

        if (falpha > 0.9f && ((int)(cursor_timer * 2) % 2 == 0)) {
            DrawRectangle((int)(boxX + 10 + text_size.x + 2), (int)(boxY + 8), 2, 18, (Color) { 168, 85, 247, a });
        }

        const char *label = g_state.has_set_name ? "Say something..." : "Set your username:";
        DrawTextEx(g_font_reg, label, (Vector2) { boxX + 2, boxY - 18 }, 14, 0, (Color) { 168, 85, 247, a });
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
}

void DrawUI(void) {
    int isSide = SnapIsSide(g_state.snap_pos);
    int isRight = SnapIsRight(g_state.snap_pos);

    if (isSide) {
        rlPushMatrix();
        if (!isRight) {
            rlTranslatef(0.0f, 500.0f, 0.0f);
            rlRotatef(-90.0f, 0.0f, 0.0f, 1.0f);
        } else {
            rlTranslatef(150.0f, 0.0f, 0.0f);
            rlRotatef(90.0f, 0.0f, 0.0f, 1.0f);
        }
        DrawScene();
        rlPopMatrix();
    } else {
        DrawScene();
    }

    // drag indicator
    if (g_state.drag_snap >= 0) {
        float tx, ty;
        switch (g_state.snap_pos) {
        case SNAP_LEFT_TOP:
        case SNAP_LEFT_CENTER:
        case SNAP_LEFT_BOTTOM:
            tx = 50.0f;
            ty = 40.0f;
            break;
        case SNAP_RIGHT_TOP:
        case SNAP_RIGHT_CENTER:
        case SNAP_RIGHT_BOTTOM:
            tx = 100.0f;
            ty = 460.0f;
            break;
        default: // default: top and bottom
            tx = 460.0f;
            ty = 50.0f;
            break;
        }
        float pulse = 0.5f + 0.5f * sinf((float)GetTime() * 8.0f);
        float r = 38.0f + pulse * 8.0f;
        unsigned char a = (unsigned char)(120 + pulse * 100);
        DrawCircleLinesV((Vector2) { tx, ty }, r, (Color) { 236, 72, 153, a });
        DrawCircleLinesV((Vector2) { tx, ty }, r - 3.0f, (Color) { 139, 92, 246, (unsigned char)(a * 0.6f) });
    }
}
