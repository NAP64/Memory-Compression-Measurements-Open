/*

    Healper code for data access
    Uses 64-bit register buffer to read/write data for 8-bit compression methods
    (It works well but speedup is unmeasured)

    By Yuqing Liu
    HEAP Lab, Virginia Tech 
    Jun 2019
    
*/

#ifndef BITSTREAM64
#define BITSTREAM64
#include <stdint.h>

typedef struct
{
    uint64_t buf;
    int8_t b_offset;
    uint8_t * dest;
    uint64_t d_offset;
} BitStream64_t;

typedef struct
{
    uint8_t buf;
    int8_t b_offset;
    uint8_t * source;
    uint64_t s_offset;
} BitStream8_t;


void BitStream8_read_init(BitStream8_t * bs, uint8_t * source)
{
    bs->buf = 0;
    bs->b_offset = 0;
    bs->source = source;
    bs->s_offset = 0;
}

uint64_t BitStream8_read(BitStream8_t * bs, int8_t size)
{
    static uint8_t mask[] = {0, 1, 3, 7, 0xf, 0x1f, 0x3f, 0x7f, 0xff};
    uint64_t res;
    if (size < 8 - bs->b_offset)
    {
        res = *(bs->source + bs->s_offset);
        res = res >> (8 - size - bs->b_offset);
        res = res & mask[size];
        bs->b_offset += size;
        return res;
    }
    res = *(bs->source + bs->s_offset) & mask[8 - bs->b_offset];
    size -= (8-bs->b_offset);
    bs->s_offset++;
    while (size >= 8)
    {
        res = res << 8;
        res |= *(bs->source + bs->s_offset);
        bs->s_offset++;
        size -= 8;
    }
    bs->b_offset = size;
    res = res << size;
    res |= (*(bs->source + bs->s_offset) >> (8 - size)) & mask[size];
    return res;
}

void BitStream64_write8(uint8_t * dest, uint64_t eight, int count)
{
    switch (count)
    {
        case 8:
        *(dest + 7) = eight & 0xff;
        case 7:
        *(dest + 6) = (eight >> 8) & 0xff;
        case 6:
        *(dest + 5) = (eight >> 16) & 0xff;
        case 5:
        *(dest + 4) = (eight >> 24) & 0xff;
        case 4:
        *(dest + 3) = (eight >> 32) & 0xff;
        case 3:
        *(dest + 2) = (eight >> 40) & 0xff;
        case 2:
        *(dest + 1) = (eight >> 48) & 0xff;
        case 1:
        *(dest) = (eight >> 56) & 0xff;
    }
}

void BitStream64_write_init(BitStream64_t * bs, uint8_t * dest)
{
    bs->buf = 0;
    bs->b_offset = 0;
    bs->dest = dest;
    bs->d_offset = 0;
}

//Upper bits of payload must be 0!
void BitStream64_write(BitStream64_t * bs, uint64_t payload, int8_t size)
{
    if (bs->b_offset + size < 64)
    {
        bs->buf |= payload << (64 - bs->b_offset - size);
        bs->b_offset += size;
    }
    else
    {
        bs->b_offset = bs->b_offset + size - 64;
        bs->buf |= payload >> (bs->b_offset);
        //*(uint64_t*)(bs->dest + bs->d_offset) = bs->buf;
        BitStream64_write8(bs->dest + bs->d_offset, bs->buf, 8);
        bs->d_offset += 8;
        if (bs->b_offset == 0)
            bs->buf = 0;
        else
            bs->buf = payload << (64 - bs->b_offset);
    }
}

int BitStream64_write_finish(BitStream64_t * bs)
{
    int8_t o = bs->b_offset;
    o = !(o % 8) ? o / 8 : o / 8 + 1;
    BitStream64_write8(bs->dest + bs->d_offset, bs->buf, o);
    return bs->d_offset + o;
}

#endif
