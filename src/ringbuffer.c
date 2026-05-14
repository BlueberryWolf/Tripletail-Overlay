#include "ringbuffer.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

struct RingBuffer {
    uint8_t *buffer;
    size_t size;
    size_t head;
    size_t tail;
    size_t count;
    pthread_mutex_t mutex;
    pthread_cond_t cond_has_data;
    pthread_cond_t cond_has_space;
    int is_closed;
};

RingBuffer *rb_create(size_t size) {
    RingBuffer *rb = (RingBuffer *)malloc(sizeof(RingBuffer));
    if (!rb) return NULL;
    rb->buffer = (uint8_t *)malloc(size);
    if (!rb->buffer) {
        free(rb);
        return NULL;
    }
    rb->size = size;
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    rb->is_closed = 0;
    pthread_mutex_init(&rb->mutex, NULL);
    pthread_cond_init(&rb->cond_has_data, NULL);
    pthread_cond_init(&rb->cond_has_space, NULL);
    return rb;
}

void rb_destroy(RingBuffer *rb) {
    if (!rb) return;
    pthread_mutex_destroy(&rb->mutex);
    pthread_cond_destroy(&rb->cond_has_data);
    pthread_cond_destroy(&rb->cond_has_space);
    free(rb->buffer);
    free(rb);
}

size_t rb_write(RingBuffer *rb, const uint8_t *data, size_t len) {
    if (!rb || !data || len == 0) return 0;
    size_t written = 0;

    pthread_mutex_lock(&rb->mutex);
    while (written < len) {
        while (rb->count == rb->size && !rb->is_closed) {
            pthread_cond_wait(&rb->cond_has_space, &rb->mutex);
        }
        if (rb->is_closed) break;

        size_t space = rb->size - rb->count;
        size_t to_write = len - written;
        if (to_write > space) to_write = space;

        size_t chunk1 = rb->size - rb->head;
        if (chunk1 > to_write) chunk1 = to_write;

        memcpy(rb->buffer + rb->head, data + written, chunk1);
        rb->head = (rb->head + chunk1) % rb->size;
        written += chunk1;
        rb->count += chunk1;

        if (chunk1 < to_write) {
            size_t chunk2 = to_write - chunk1;
            memcpy(rb->buffer + rb->head, data + written, chunk2);
            rb->head = (rb->head + chunk2) % rb->size;
            written += chunk2;
            rb->count += chunk2;
        }

        pthread_cond_broadcast(&rb->cond_has_data);
    }
    pthread_mutex_unlock(&rb->mutex);
    return written;
}

size_t rb_read(RingBuffer *rb, uint8_t *data, size_t len) {
    if (!rb || !data || len == 0) return 0;
    size_t read = 0;

    pthread_mutex_lock(&rb->mutex);
    while (read < len) {
        while (rb->count == 0 && !rb->is_closed) {
            pthread_cond_wait(&rb->cond_has_data, &rb->mutex);
        }
        if (rb->count == 0 && rb->is_closed) break;

        size_t to_read = len - read;
        if (to_read > rb->count) to_read = rb->count;

        size_t chunk1 = rb->size - rb->tail;
        if (chunk1 > to_read) chunk1 = to_read;

        memcpy(data + read, rb->buffer + rb->tail, chunk1);
        rb->tail = (rb->tail + chunk1) % rb->size;
        read += chunk1;
        rb->count -= chunk1;

        if (chunk1 < to_read) {
            size_t chunk2 = to_read - chunk1;
            memcpy(data + read, rb->buffer + rb->tail, chunk2);
            rb->tail = (rb->tail + chunk2) % rb->size;
            read += chunk2;
            rb->count -= chunk2;
        }

        pthread_cond_broadcast(&rb->cond_has_space);

        break;
    }
    pthread_mutex_unlock(&rb->mutex);
    return read;
}

size_t rb_read_nonblocking(RingBuffer *rb, uint8_t *data, size_t len) {
    if (!rb || !data || len == 0) return 0;
    size_t read = 0;

    pthread_mutex_lock(&rb->mutex);
    if (rb->count > 0) {
        size_t to_read = len;
        if (to_read > rb->count) to_read = rb->count;

        size_t chunk1 = rb->size - rb->tail;
        if (chunk1 > to_read) chunk1 = to_read;

        memcpy(data, rb->buffer + rb->tail, chunk1);
        rb->tail = (rb->tail + chunk1) % rb->size;
        read += chunk1;
        rb->count -= chunk1;

        if (chunk1 < to_read) {
            size_t chunk2 = to_read - chunk1;
            memcpy(data + read, rb->buffer + rb->tail, chunk2);
            rb->tail = (rb->tail + chunk2) % rb->size;
            read += chunk2;
            rb->count -= chunk2;
        }
        pthread_cond_broadcast(&rb->cond_has_space);
    }
    pthread_mutex_unlock(&rb->mutex);
    return read;
}

void rb_close(RingBuffer *rb) {
    if (!rb) return;
    pthread_mutex_lock(&rb->mutex);
    rb->is_closed = 1;
    pthread_cond_broadcast(&rb->cond_has_data);
    pthread_cond_broadcast(&rb->cond_has_space);
    pthread_mutex_unlock(&rb->mutex);
}

int rb_is_closed(RingBuffer *rb) {
    if (!rb) return 1;
    return rb->is_closed;
}
