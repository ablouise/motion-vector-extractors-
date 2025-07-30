#ifndef CABAC_H
#define CABAC_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    const uint8_t *buffer;
    size_t size;
    int offset;

    int range;
    unsigned value;
    int bits_left;
} CABACContext;

void cabac_init(CABACContext *c, const uint8_t *buf, size_t size);
int cabac_decode_mvd(CABACContext *c);

#endif
