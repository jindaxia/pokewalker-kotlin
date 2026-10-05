#include <aaudio/AAudio.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "audio.h"
#include "debug_log.h"
#include "picowalker_core.h"

#include "pw_android.h"

/*
 * Audio driver: synthesises the walker's square-wave beeps (same algorithm as
 * picowalker-sdl) and plays them through AAudio on a dedicated thread, so the
 * core loop never blocks on playback.
 */

typedef struct snd_node {
    struct snd_node *next;
    size_t frames;
    int16_t data[];
} snd_node_t;

static AAudioStream *stream = NULL;
static int32_t out_sample_rate = 44100;

static pthread_mutex_t q_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t q_cv = PTHREAD_COND_INITIALIZER;
static snd_node_t *q_head = NULL;
static snd_node_t *q_tail = NULL;

static pthread_t audio_thread;
static bool thread_ready = false;

static void close_stream(void) {
    if (stream != NULL) {
        AAudioStream_requestStop(stream);
        AAudioStream_close(stream);
        stream = NULL;
    }
}

static bool open_stream(void) {
    if (stream != NULL) return true;

    AAudioStreamBuilder *builder = NULL;
    aaudio_result_t res = AAudio_createStreamBuilder(&builder);
    if (res != AAUDIO_OK) return false;

    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setChannelCount(builder, 1);
    AAudioStreamBuilder_setSampleRate(builder, AAUDIO_UNSPECIFIED);
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);

    res = AAudioStreamBuilder_openStream(builder, &stream);
    AAudioStreamBuilder_delete(builder);
    if (res != AAUDIO_OK || stream == NULL) {
        stream = NULL;
        pw_log_warn("AAudio open failed: %s\n", AAudio_convertResultToText(res));
        return false;
    }

    out_sample_rate = AAudioStream_getSampleRate(stream);
    AAudioStream_requestStart(stream);
    return true;
}

static void *audio_thread_fn(void *arg) {
    (void)arg;

    for (;;) {
        pthread_mutex_lock(&q_mu);
        while (q_head == NULL) pthread_cond_wait(&q_cv, &q_mu);
        snd_node_t *node = q_head;
        q_head = node->next;
        if (q_head == NULL) q_tail = NULL;
        pthread_mutex_unlock(&q_mu);

        if (!open_stream()) {
            free(node);
            continue;
        }

        uint8_t *cursor = (uint8_t *)node->data;
        size_t frames_left = node->frames;
        while (frames_left > 0) {
            aaudio_result_t w = AAudioStream_write(stream, cursor, frames_left, 3000000000LL);
            if (w == AAUDIO_ERROR_DISCONNECTED) {
                close_stream();
                break;
            }
            if (w <= 0) break;
            frames_left -= (size_t)w;
            cursor += (size_t)w * sizeof(int16_t);
        }
        free(node);
    }
    return NULL;
}

void pw_audio_init() {
    if (thread_ready) return;

    if (pthread_create(&audio_thread, NULL, audio_thread_fn, NULL) == 0) {
        pthread_detach(audio_thread);
        thread_ready = true;
    } else {
        pw_log_warn("Failed to create audio thread\n");
    }
}

static void enqueue(snd_node_t *node) {
    node->next = NULL;
    pthread_mutex_lock(&q_mu);
    if (q_tail == NULL) {
        q_head = node;
        q_tail = node;
    } else {
        q_tail->next = node;
        q_tail = node;
    }
    pthread_cond_signal(&q_cv);
    pthread_mutex_unlock(&q_mu);
}

typedef struct {
    int16_t *data;
    size_t size;   /* bytes */
    size_t cap;    /* bytes */
} audio_buf_t;

static void buf_reserve(audio_buf_t *b, size_t extra_bytes) {
    if (b->size + extra_bytes <= b->cap) return;
    size_t new_cap = (b->cap == 0) ? 4096 : b->cap;
    while (new_cap < b->size + extra_bytes) new_cap *= 2;
    int16_t *p = (int16_t *)realloc(b->data, new_cap);
    if (p == NULL) return;
    b->data = p;
    b->cap = new_cap;
}

static void add_note(audio_buf_t *b, uint8_t period, uint16_t duration) {
    double period_samples = ((double)out_sample_rate / 65536.0) * (double)period;
    size_t dur_samples = (size_t)(((double)out_sample_rate / 32768.0) * (double)duration);
    if (period_samples < 1.0) period_samples = 1.0;
    if (dur_samples == 0) return;

    buf_reserve(b, dur_samples * sizeof(int16_t));
    if (b->data == NULL) return;

    pw_volume_t volume = pw_audio_get_volume();
    int16_t *it = (int16_t *)((uint8_t *)b->data + b->size);
    for (size_t i = 0; i < dur_samples; i++) {
        it[i] = (int16_t)(volume * volume * (8191 * ((int)((double)i / period_samples) % 2) - 8191));
    }
    b->size += dur_samples * sizeof(int16_t);
}

static void add_silence(audio_buf_t *b, size_t samples) {
    if (samples == 0) return;
    buf_reserve(b, samples * sizeof(int16_t));
    if (b->data == NULL) return;

    int16_t *it = (int16_t *)((uint8_t *)b->data + b->size);
    memset(it, 0, samples * sizeof(int16_t));
    b->size += samples * sizeof(int16_t);
}

void pw_audio_play_sound_data(const pw_sound_frame_t *sound_data, size_t sz) {
    if (sound_data == NULL || sz == 0) return;
    if (pw_audio_get_volume() == VOLUME_NONE) return;
    if (!thread_ready) pw_audio_init();
    if (!thread_ready) return;

    audio_buf_t buf = {0};
    uint8_t sh = 0;

    for (const pw_sound_frame_t *f = sound_data; f < sound_data + sz; f++) {
        uint8_t info = f->info;
        uint8_t idx = f->period_idx;

        if (idx == 0x7b) { /* start of sound data */
            sh = info;
            continue;
        }
        if (idx == 0x7f) { /* end of sound data */
            break;
        }
        if (idx == 0x7d) { /* unhandled command */
            pw_log_warn("Unhandled sound command 0x7d\n");
            continue;
        }
        if (sh == 0) continue;

        bool negative = idx > 0x80;
        uint32_t dur = negative ? ((0x14000u * info) / sh) : (((0x14000u * info) / sh) - 0x140u);
        uint8_t period = PW_AUDIO_PERIODTAB[idx & 0x7f];

        add_note(&buf, period, (uint16_t)(dur > 0xffff ? 0xffff : dur));
        if (!negative) {
            add_silence(&buf, 300);
        }
    }

    if (buf.size == 0 || buf.data == NULL) {
        free(buf.data);
        return;
    }

    snd_node_t *node = (snd_node_t *)malloc(sizeof(snd_node_t) + buf.size);
    if (node == NULL) {
        free(buf.data);
        return;
    }
    node->frames = buf.size / sizeof(int16_t);
    memcpy(node->data, buf.data, buf.size);
    free(buf.data);

    enqueue(node);
}

bool pw_audio_is_playing_sound() {
    bool playing;
    pthread_mutex_lock(&q_mu);
    playing = q_head != NULL;
    pthread_mutex_unlock(&q_mu);
    return playing;
}
