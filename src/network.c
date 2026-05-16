#ifdef _WIN32
#define Rectangle Rectangle_Windows
#define CloseWindow CloseWindow_Windows
#define ShowCursor ShowCursor_Windows
#endif

#include "network.h"
#include <curl/curl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#undef Rectangle
#undef CloseWindow
#undef ShowCursor
#undef LoadImage
#undef DrawText
#undef DrawTextEx
#undef PlaySound

#define sleep_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#define sleep_ms(ms) usleep((ms) * 1000)
#endif

#include "platform.h"
#include "raylib.h"
#include "state.h"

static TrackMetadata g_meta = { "Connecting...", "Tripletail FM", "", 0, 0 };
static pthread_mutex_t g_meta_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_meta_changed = 1;
static volatile int g_reconnect_requested = 0;
static char g_pending_chat[128] = { 0 };
static pthread_mutex_t g_chat_send_mutex = PTHREAD_MUTEX_INITIALIZER;

void RequestStreamReconnect(void) { g_reconnect_requested = 1; }

static Color ParseHexColor(const char *hex) {
    if (!hex || hex[0] != '#') return (Color) { 255, 255, 255, 255 };
    int r, g, b;
    if (sscanf(hex + 1, "%02x%02x%02x", &r, &g, &b) == 3) {
        return (Color) { (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 };
    }
    return (Color) { 255, 255, 255, 255 };
}

static void PushChatMessage(const char *user, const char *text, Color color) {
    int slot = 0;
    for (int i = 0; i < MAX_VISIBLE_CHAT; i++) {
        if (g_state.chat[i].timer <= 0.0f) {
            slot = i;
            break;
        }
        if (g_state.chat[i].timer < g_state.chat[slot].timer) slot = i;
    }
    strncpy(g_state.chat[slot].user, user, 31);
    strncpy(g_state.chat[slot].text, text, 127);
    g_state.chat[slot].color = color;
    g_state.chat[slot].timer = 8.0f;
    g_state.chat[slot].alpha = 0.0f;
    g_state.chat[slot].y_offset = -20.0f;
}

void SendChatMessage(const char *text) {
    pthread_mutex_lock(&g_chat_send_mutex);
    strncpy(g_pending_chat, text, 127);
    pthread_mutex_unlock(&g_chat_send_mutex);
}

static void HandleChatIncoming(CURL *curl, char *buf) {
    if (strncmp(buf, "42", 2) == 0 && strstr(buf, "\"chat_message\"")) {
        char user[32] = { 0 }, text[128] = { 0 }, colorStr[16] = { 0 };
        char *u = strstr(buf, "\"user\":\"");
        char *t = strstr(buf, "\"text\":\"");
        char *c = strstr(buf, "\"user_color\":\"");
        if (u && t) {
            sscanf(u + 8, "%31[^\"]", user);
            sscanf(t + 8, "%127[^\"]", text);
            if (c) sscanf(c + 14, "%15[^\"]", colorStr);
            PushChatMessage(user, text, ParseHexColor(colorStr));
        }
    } else if (buf[0] == '0') {
        const char *resp = "40";
        size_t sent;
        curl_ws_send(curl, resp, 2, &sent, 0, CURLWS_TEXT);
    } else if (buf[0] == '2') {
        const char *resp = "3";
        size_t sent;
        curl_ws_send(curl, resp, 1, &sent, 0, CURLWS_TEXT);
    }
}

static void *ChatWebSocketThread(void *arg) {
    char recv_buf[4096];
    while (1) {
        CURL *curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL,
                             "wss://tripletail-socket.blueberry.coffee/socket.io/?EIO=4&transport=websocket");
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 2L);
            CURLcode res = curl_easy_perform(curl);

            if (res == CURLE_OK) {
                double last_ping = GetTime();
                while (1) {
                    pthread_mutex_lock(&g_chat_send_mutex);
                    if (g_pending_chat[0] != '\0') {
                        char msg[512];
                        char colorHex[8];
                        snprintf(colorHex, sizeof(colorHex), "#%02x%02x%02x", g_state.user_color.r,
                                 g_state.user_color.g, g_state.user_color.b);
                        snprintf(msg, sizeof(msg),
                                 "42[\"chat_message\",{\"user\":\"%s\",\"text\":\"%s\",\"user_color\":\"%s\"}]",
                                 g_state.username, g_pending_chat, colorHex);
                        size_t sent;
                        curl_ws_send(curl, msg, strlen(msg), &sent, 0, CURLWS_TEXT);
                        g_pending_chat[0] = '\0';
                    }
                    pthread_mutex_unlock(&g_chat_send_mutex);

                    if (GetTime() - last_ping > 20.0) {
                        const char *ping = "2";
                        size_t sent;
                        curl_ws_send(curl, ping, 1, &sent, 0, CURLWS_TEXT);
                        last_ping = GetTime();
                    }

                    size_t rlen;
                    const struct curl_ws_frame *meta;
                    res = curl_ws_recv(curl, recv_buf, sizeof(recv_buf) - 1, &rlen, &meta);
                    if (res == CURLE_OK) {
                        recv_buf[rlen] = 0;
                        HandleChatIncoming(curl, recv_buf);
                    } else if (res != CURLE_AGAIN) {
                        break;
                    }

                    sleep_ms(100);
                }
            } else {
            }
            curl_easy_cleanup(curl);
        }
        sleep_ms(5000);
    }
    return NULL;
}

void InitNetwork(void) {
    curl_global_init(CURL_GLOBAL_ALL);
    pthread_mutex_init(&g_meta_mutex, NULL);
    FILE *f = fopen("name.txt", "r");
    if (f) {
        if (fscanf(f, " %31[^\r\n]", g_state.username) == 1) {
            g_state.has_set_name = 1;
        }
        fclose(f);
    }

    if (!g_state.has_set_name) {
        snprintf(g_state.username, sizeof(g_state.username), "Floofer%d", GetRandomValue(100, 999));
        g_state.user_color = (Color) { (unsigned char)GetRandomValue(100, 255), (unsigned char)GetRandomValue(100, 255),
                                       (unsigned char)GetRandomValue(100, 255), 255 };
    } else {
        unsigned int hash = 5381;
        for (int i = 0; g_state.username[i]; i++) hash = ((hash << 5) + hash) + g_state.username[i];
        g_state.user_color = (Color) { (unsigned char)(100 + (hash % 155)), (unsigned char)(100 + ((hash >> 8) % 155)),
                                       (unsigned char)(100 + ((hash >> 16) % 155)), 255 };
    }

    pthread_t chat_tid;
    pthread_create(&chat_tid, NULL, ChatWebSocketThread, NULL);
    pthread_detach(chat_tid);
}

void CleanupNetwork(void) {
    curl_global_cleanup();
    pthread_mutex_destroy(&g_meta_mutex);
}

int GetLatestMetadata(TrackMetadata *meta) {
    pthread_mutex_lock(&g_meta_mutex);
    if (meta) *meta = g_meta;
    int changed = g_meta_changed;
    g_meta_changed = 0;
    pthread_mutex_unlock(&g_meta_mutex);
    return changed;
}

static void ExtractJsonString(const char *json, const char *key, char *out, int out_size) {
    char s_key[128];
    snprintf(s_key, sizeof(s_key), "\"%s\"", key);
    const char *p = strstr(json, s_key);
    if (!p) return;

    p += strlen(s_key);
    while (*p && (*p == ' ' || *p == ':' || *p == '\"')) p++;

    const char *start = p;
    while (*p && *p != '\"') p++;

    int len = (int)(p - start);
    if (len >= out_size) len = out_size - 1;
    if (len > 0) {
        strncpy(out, start, len);
        out[len] = '\0';

        char *src = out, *dst = out;
        while (*src) {
            if (src[0] == '\\' && src[1] == '/') {
                *dst++ = '/';
                src += 2;
            } else
                *dst++ = *src++;
        }
        *dst = '\0';
    }
}

static int ExtractJsonInt(const char *json, const char *key) {
    char s_key[128];
    snprintf(s_key, sizeof(s_key), "\"%s\"", key);
    const char *p = strstr(json, s_key);
    if (!p) return 0;

    p += strlen(s_key);
    while (*p && (*p == ' ' || *p == ':' || *p == '\"')) p++;
    while (*p && (*p < '0' || *p > '9') && *p != '-') p++;
    return atoi(p);
}

static void ParseMetadataJson(const char *buffer) {
    if (!buffer || strlen(buffer) < 5) return;

    pthread_mutex_lock(&g_meta_mutex);
    const char *song_sec = strstr(buffer, "\"song\":");
    if (!song_sec) song_sec = buffer;

    char title[256] = { 0 }, artist[256] = { 0 };
    ExtractJsonString(song_sec, "title", title, sizeof(title));
    ExtractJsonString(song_sec, "artist", artist, sizeof(artist));

    if (strlen(title) > 0) {
        if (strcmp(g_meta.title, title) != 0 || strcmp(g_meta.artist, artist) != 0) {
            strncpy(g_meta.title, title, sizeof(g_meta.title) - 1);
            strncpy(g_meta.artist, artist, sizeof(g_meta.artist) - 1);
            g_meta_changed = 1;
        }

        char art_path[512] = { 0 };
        ExtractJsonString(song_sec, "art", art_path, sizeof(art_path));
        if (strlen(art_path) > 0) {
            char new_url[512];
            if (art_path[0] == '/')
                snprintf(new_url, sizeof(new_url), "https://tripletaildash.blueberry.coffee%s", art_path);
            else
                strncpy(new_url, art_path, sizeof(new_url));

            if (strcmp(g_meta.art_url, new_url) != 0) {
                strncpy(g_meta.art_url, new_url, sizeof(g_meta.art_url) - 1);
                g_meta_changed = 1;
            }
        }

        g_meta.duration = ExtractJsonInt(buffer, "duration");
        g_meta.elapsed = ExtractJsonInt(buffer, "elapsed");
    }
    pthread_mutex_unlock(&g_meta_mutex);
}

struct MemoryBuffer {
    uint8_t *data;
    size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryBuffer *mem = (struct MemoryBuffer *)userp;
    uint8_t *ptr = realloc(mem->data, mem->size + realsize + 1);
    if (!ptr) return 0;
    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;
    return realsize;
}

static void FetchInitialMetadata(void) {
    CURL *curl = curl_easy_init();
    if (!curl) return;
    struct MemoryBuffer chunk = { 0 };
    curl_easy_setopt(curl, CURLOPT_URL, "https://tripletaildash.blueberry.coffee/api/nowplaying/tripletail");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    if (curl_easy_perform(curl) == CURLE_OK && chunk.data) {
        ParseMetadataJson((char *)chunk.data);
    }
    if (chunk.data) free(chunk.data);
    curl_easy_cleanup(curl);
}

static size_t WSWriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    char *buf = malloc(realsize + 1);
    if (!buf) return 0;
    memcpy(buf, contents, realsize);
    buf[realsize] = 0;

    if (strstr(buf, "\"title\":"))
        ParseMetadataJson(buf);
    else if (strstr(buf, "\"client\"")) {
        CURL *curl = (CURL *)userp;
        const char *sub = "{\"subs\":{\"station:tripletail\":{\"recover\":true}}}";
        size_t sent;
        curl_ws_send(curl, sub, strlen(sub), &sent, 0, CURLWS_TEXT);
    }

    free(buf);
    return realsize;
}

static void *WebSocketThread(void *lpParam) {
    (void)lpParam;
    while (1) {
        FetchInitialMetadata();
        CURL *curl = curl_easy_init();
        if (!curl) {
            sleep_ms(5000);
            continue;
        }

        curl_easy_setopt(curl, CURLOPT_URL, "wss://tripletaildash.blueberry.coffee/api/live/nowplaying/websocket");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WSWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, curl);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Tripletail-Desktop/1.0");
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 0L);

        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        sleep_ms(5000);
    }
    return NULL;
}

static void *PollThread(void *lpParam) {
    (void)lpParam;
    while (1) {
        sleep_ms(20000);
        FetchInitialMetadata();
    }
    return NULL;
}

uint8_t *DownloadCoverArt(const char *url, size_t *out_size) {
    if (!url || !out_size || strlen(url) < 10) return NULL;

    CURL *curl = curl_easy_init();
    if (!curl) return NULL;
    struct MemoryBuffer chunk = { 0 };
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Tripletail-Desktop/1.0");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

    if (curl_easy_perform(curl) == CURLE_OK && chunk.size > 0) {
        *out_size = chunk.size;
        curl_easy_cleanup(curl);
        return chunk.data;
    }

    if (chunk.data) free(chunk.data);
    curl_easy_cleanup(curl);
    return NULL;
}

static size_t StreamWriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    RingBuffer *rb = (RingBuffer *)userp;
    if (rb_is_closed(rb)) return 0;
    rb_write(rb, (uint8_t *)contents, (int)realsize);
    return realsize;
}

static int StreamProgressCallback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal,
                                  curl_off_t ulnow) {
    (void)clientp;
    (void)dltotal;
    (void)dlnow;
    (void)ultotal;
    (void)ulnow;
    if (g_reconnect_requested) {
        g_reconnect_requested = 0;
        return 1;
    }
    return 0;
}

static void *StreamThread(void *lpParam) {
    PlatformSetCurrentThreadPriority(g_platform, 1);
    RingBuffer *rb = (RingBuffer *)lpParam;
    while (!rb_is_closed(rb)) {
        CURL *curl = curl_easy_init();
        if (!curl) {
            sleep_ms(1000);
            continue;
        }

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Cache-Control: no-cache");
        headers = curl_slist_append(headers, "Pragma: no-cache");
        headers = curl_slist_append(headers, "Icy-MetaData: 0");

        curl_easy_setopt(curl, CURLOPT_URL, "https://radio.blueberry.coffee/listen/tripletail/radio.ogg");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, StreamWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)rb);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Tripletail-Desktop/1.0");
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1000L);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 15L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

        // progress callback for aborting
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, StreamProgressCallback);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

        curl_easy_perform(curl);

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (!rb_is_closed(rb)) sleep_ms(1000);
    }
    return NULL;
}

void StartAudioStream(RingBuffer *rb) {
    pthread_t ws_thread, stream_thread, poll_thread;
    pthread_create(&ws_thread, NULL, WebSocketThread, NULL);
    pthread_detach(ws_thread);
    pthread_create(&poll_thread, NULL, PollThread, NULL);
    pthread_detach(poll_thread);
    pthread_create(&stream_thread, NULL, StreamThread, rb);
    pthread_detach(stream_thread);
}
