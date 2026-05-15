#include "audio.h"
#include "kiss_fft.h"
#include "state.h"
#include <math.h>
#include <opus/opusfile.h>
#include <stdlib.h>
#include <string.h>

static kiss_fft_cpx g_fft_in[FFT_SIZE];
static kiss_fft_cpx g_fft_out_cpx[FFT_SIZE];
static float g_hanning[FFT_SIZE];
static int g_fft_idx = 0;
static int g_bin_map[VIS_BARS];
static float g_bass_smooth = 0.0f;
static double g_last_audio_time = 0;

void InitAudioSystem(void) {
    // fft setup
    g_fft_cfg = kiss_fft_alloc(FFT_SIZE, 0, NULL, NULL);
    for (int i = 0; i < FFT_SIZE; i++) {
        g_hanning[i] = 0.5f * (1.0f - cosf(2.0f * PI * i / (FFT_SIZE - 1)));
    }

    // mapping frequencies
    float bin_min = 2.0f;
    float bin_max = (float)(FFT_SIZE / 2.5f);
    for (int i = 0; i < VIS_BARS; i++) {
        float t = (float)i / (VIS_BARS - 1);

        // more aggressive skew to spread the lows out
        float skewed_t = powf(t, 1.3f);

        float log_t = powf(bin_max / bin_min, skewed_t);
        float lin_t = skewed_t * bin_max;
        g_bin_map[i] = (int)(bin_min * (log_t * 0.9f + lin_t * 0.1f));

        if (i > 0 && g_bin_map[i] <= g_bin_map[i - 1]) {
            g_bin_map[i] = g_bin_map[i - 1] + 1;
        }
    }

    pthread_mutex_init(&g_vis.mutex, NULL);
    g_last_audio_time = GetTime();
}

void CleanupAudioSystem(void) {
    if (g_fft_cfg) kiss_fft_free(g_fft_cfg);
    pthread_mutex_destroy(&g_vis.mutex);
}

void AppAudioCallback(void *bufferData, unsigned int frames) {
    if (!g_pcm_rb) return;

    // grabbing the pcm data from the ringbuffer
    int bytes_to_read = frames * CHANNELS * sizeof(int16_t);
    int bytes_read = rb_read_nonblocking(g_pcm_rb, (uint8_t *)bufferData, bytes_to_read);
    if (bytes_read < bytes_to_read) {
        memset((uint8_t *)bufferData + bytes_read, 0, bytes_to_read - bytes_read);
    }

    int16_t *pcm = (int16_t *)bufferData;
    int samples = bytes_read / (CHANNELS * sizeof(int16_t));

    float vol_sum = 0.0f;
    for (int i = 0; i < samples; i++) {
        // mono mixdown for the visualizer
        float mono = (pcm[i * CHANNELS] + pcm[i * CHANNELS + 1]) / 65536.0f;
        vol_sum += fabsf(mono);

        g_fft_in[g_fft_idx].r = mono * g_hanning[g_fft_idx];
        g_fft_in[g_fft_idx].i = 0;

        if (++g_fft_idx >= FFT_SIZE) {
            // finally doing the math
            kiss_fft(g_fft_cfg, g_fft_in, g_fft_out_cpx);

            pthread_mutex_lock(&g_vis.mutex);
            for (int k = 0; k < VIS_BARS; k++) {
                int bin = g_bin_map[k];
                if (bin >= FFT_SIZE / 2) bin = FFT_SIZE / 2 - 1;

                float r = g_fft_out_cpx[bin].r;
                float i = g_fft_out_cpx[bin].i;
                float mag = sqrtf(r * r + i * i) / FFT_SIZE;

                // gate the noise floor
                if (mag < 0.0002f) mag = 0.0f;

                // aggressive slope to bring out the highs
                float base_slope = sqrtf((float)k + 1.0f);
                float high_boost = 1.0f + powf((float)k / VIS_BARS, 2.5f) * 6.0f;
                float slope = base_slope * high_boost;

                float vis_val = powf(mag * slope, 0.9f) * 260.0f;
                if (vis_val < 0) vis_val = 0;

                // smoothing
                if (vis_val > g_vis.bins[k])
                    g_vis.bins[k] = g_vis.bins[k] * 0.15f + vis_val * 0.85f;
                else
                    g_vis.bins[k] = g_vis.bins[k] * 0.85f + vis_val * 0.15f;
            }
            pthread_mutex_unlock(&g_vis.mutex);
            g_fft_idx = 0;
        }
    }

    // smooth the bass for the tail animation
    float current_vol = (vol_sum / samples) * 4.0f;
    if (current_vol > 0.001f) g_last_audio_time = GetTime();
    
    if (current_vol > 1.5f) current_vol = 1.5f;
    g_bass_smooth = g_bass_smooth * 0.85f + current_vol * 0.15f;
}

// opus needs callbacks
static int rb_read_cb(void *stream, unsigned char *ptr, int nbytes) {
    return (int)rb_read((RingBuffer *)stream, ptr, nbytes);
}

void *DecodeThread(void *lpParam) {
    RingBuffer *rb = (RingBuffer *)lpParam;
    int16_t pcm[4096 * CHANNELS];
    OpusFileCallbacks cb = { rb_read_cb, 0, 0, 0 };

    while (!rb_is_closed(rb)) {
        int err = 0;
        OggOpusFile *op = op_open_callbacks(rb, &cb, NULL, 0, &err);
        if (!op) {
            WaitTime(1.0);
            continue;
        }

        // decoder loop
        while (!rb_is_closed(rb)) {
            int samples = op_read(op, pcm, sizeof(pcm) / sizeof(int16_t), NULL);
            if (samples <= 0) {
                if (samples < 0) break;
                WaitTime(0.01);
                continue;
            }
            if (g_pcm_rb) rb_write(g_pcm_rb, (uint8_t *)pcm, samples * CHANNELS * sizeof(int16_t));
        }
        op_free(op);
        if (!rb_is_closed(rb)) WaitTime(0.1);
    }
    return NULL;
}

float GetBassLevel(void) { return g_bass_smooth; }
double GetLastAudioTime(void) { return g_last_audio_time; }
