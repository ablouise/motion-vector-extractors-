#include "slice.h"

BitstreamReader bsr_init(const uint8_t *data, size_t len) {
    return (BitstreamReader){ data, len, 0, 0 };
}

int bsr_read_bit(BitstreamReader *bs) {
    if (bs->byte_pos >= bs->size)
        return 0;
    int result = (bs->buffer[bs->byte_pos] >> (7 - bs->bit_pos)) & 1;
    bs->bit_pos++;
    if (bs->bit_pos == 8) {
        bs->bit_pos = 0;
        bs->byte_pos++;
    }
    return result;
}

uint32_t bsr_read_ue(BitstreamReader *bs) {
    int zeros = 0;
    while (bsr_read_bit(bs) == 0) zeros++;
    uint32_t value = 1;
    for (int i = 0; i < zeros; i++) {
        value <<= 1;
        if (bsr_read_bit(bs)) value |= 1;
    }
    return value - 1;
}

int parse_slice_header(BitstreamReader *bs, SliceHeader *sh) {
    bsr_read_ue(bs);       // first_mb_in_slice
    sh->slice_type = bsr_read_ue(bs) % 5;
    sh->frame_num = bsr_read_ue(bs);
    return 1;
}
