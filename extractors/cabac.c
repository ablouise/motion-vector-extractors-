#include "cabac.h"
#include <stdio.h>

void cabac_refill(CABACContext *c) {
    while (c->bits_left <= 24 && c->offset < c->size)
        c->value = (c->value << 8) | c->buffer[c->offset++], c->bits_left += 8;
}

void cabac_init(CABACContext *c, const uint8_t *buf, size_t size) {
    c->buffer = buf;
    c->size = size;
    c->offset = 0;
    c->range = 510;
    c->value = (buf[0] << 16) | (buf[1] << 8) | buf[2];
    c->offset = 3;
    c->bits_left = 24;
}

int cabac_get_bit(CABACContext *c) {
    cabac_refill(c);
    c->range >>= 1;
    unsigned bit = c->value >= c->range;
    if (bit) c->value -= c->range;
    return bit;
}

// Hardcoded exponential Golomb decoder mimic
int cabac_decode_ue(CABACContext *c) {
    int zeroes = 0;
    while (cabac_get_bit(c) == 0) zeroes++;
    int value = 1 << zeroes;
    for (int i = 0; i < zeroes; i++)
        value |= cabac_get_bit(c) << (zeroes - i - 1);
    return value - 1;
}

int cabac_decode_se(CABACContext *c) {
    int val = cabac_decode_ue(c);
    if (val & 1)
        return (val + 1) >> 1;
    else
        return -(val >> 1);
}

int cabac_decode_mvd(CABACContext *c) {
    return cabac_decode_se(c);  // approximation of mvd_x/mvd_y decode
}
