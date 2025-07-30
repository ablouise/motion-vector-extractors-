#ifndef SLICE_H
#define SLICE_H

#include <stdint.h>
#include <stddef.h>

#define SLICE_P 0
#define SLICE_B 1
#define SLICE_I 2 // unused

typedef struct {
    int slice_type;
    int frame_num;
} SliceHeader;

typedef struct {
    const uint8_t *buffer;
    size_t size;
    size_t byte_pos;
    int bit_pos;
} BitstreamReader;

BitstreamReader bsr_init(const uint8_t *data, size_t len);
int bsr_read_bit(BitstreamReader *bs);
uint32_t bsr_read_ue(BitstreamReader *bs);
int parse_slice_header(BitstreamReader *bs, SliceHeader *sh);

#endif
