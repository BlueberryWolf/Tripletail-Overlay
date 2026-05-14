#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <stddef.h>
#include <stdint.h>

typedef struct RingBuffer RingBuffer;

RingBuffer *rb_create(size_t size);
void rb_destroy(RingBuffer *rb);
size_t rb_write(RingBuffer *rb, const uint8_t *data, size_t len);
size_t rb_read(RingBuffer *rb, uint8_t *data, size_t len);
size_t rb_read_nonblocking(RingBuffer *rb, uint8_t *data, size_t len);
void rb_close(RingBuffer *rb);
int rb_is_closed(RingBuffer *rb);
size_t rb_available(RingBuffer *rb);

#endif
