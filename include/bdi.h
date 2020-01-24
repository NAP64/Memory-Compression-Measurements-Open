/*

    Base-Delta-Immediate Compression Code

    Implemented based on paper:
    https://users.ece.cmu.edu/~omutlu/pub/bdi-compression_pact12.pdf

    Also considers both endianness

    By Yuqing Liu
    HEAP Lab, Virginia Tech 
    Aug 2018
    
*/

#include <stdint.h>

static uint64_t endian(uint8_t * in, int length)
{
    int i;
    uint64_t temp = 0;
    for (i = 0; i < length; i++)
        temp = (((uint64_t)in[i]) & 0xff) | (temp << 8) ;
    return temp;
}

static uint64_t norm(uint8_t * in, int length)
{
    int i;
    uint64_t temp = 0;
    for (i = 0; i < length; i++)
        temp |= (((uint64_t)in[i]) & 0xff) << (i * 8);
    return temp;
}

static void bdicompress(uint8_t * in, uint8_t * out, uint64_t base, int im, int bl, int dl, int en)
{
    uint64_t i;
    uint64_t j = bl;
    uint64_t n;
    uint8_t offset = im * 8 / bl;
    uint64_t mask = 0xff;
    if (dl == 2) mask = 0xffff;
    if (dl == 4) mask = 0xffffffff;
    uint64_t zero = 0xffffffff;
    for (i = 0; i < bl; i++)
        *(out + offset + i) = *(((uint8_t *)&base) + i);
    for (i = 0; i < 64; i += bl)
    {
        uint64_t temp = en ? endian(in + i, bl) : norm(in + i, bl);
        if (im && temp <= mask)
            zero = zero ^ (1 << (i / bl));
        else
            temp -= base;
        for (n = 0; n < dl; n++)
            *(out + offset + j + n) = *(((uint8_t *)&temp) + n);
        j += dl;
    }
    if (!!offset)
    {
        for (i = 0; i < offset; i++)
        {
            *(out + i) = *(((uint8_t *)&zero) + i);
        }
    }
}

static void bdidecompress(uint8_t * in, uint8_t * out, int im, int bl, int dl, int en)
{
    int i;
    int j = 0;
    int n;
    uint8_t offset = im * 8 / bl;
    uint64_t zero = 0;
    uint64_t base = 0;
    if (!!offset)
    {
        for (i = 0; i < offset; i++)
        {
            *(((uint8_t *)&zero) + i) = *(in + i);
        }
    }
    for (i = 0; i < bl; i++)
        *(((uint8_t *)&base) + i) = *(in + offset + i);
    for (i = 0; j < 64; i += dl)
    {
        uint64_t temp = 0;
        for (n = 0; n < dl; n++)
            *(((uint8_t *)&temp) + n) = *(in + i + offset  + bl + n);
        if (!im || ((zero >> (i / dl)) & 1))
            temp += base;
        for (n = 0; n < bl; n++)
            if (en)
                *(out + j + n) = *(((uint8_t *)&temp) + (bl - n - 1));
            else
                *(out + j + n) = *(((uint8_t *)&temp) + n);
        j += bl;
    }
}

uint64_t bdiCompressData(uint8_t * in, uint8_t * out)
{
    uint16_t B2D1, B2D1i, tB2D1, tB2D1i, R2, B2min, tB2min;
    uint32_t B4D2, B4D2i, B4D1, B4D1i, tB4D2, tB4D2i, tB4D1, tB4D1i, R4, B4min, tB4min;
    uint64_t B8D4, B8D4i, B8D2, B8D2i, B8D1, B8D1i, tB8D4, tB8D4i, tB8D2, tB8D2i, tB8D1, tB8D1i, R8, B8min, tB8min;
    uint8_t R1, R0;
    int i;
    R0 = 1;
    R1 = in[0];
    B8min = R8 = B8D1 = B8D2 = B8D4 = B8D1i = B8D2i = B8D4i = norm(in, 8);
    tB8min = tB8D1 = tB8D2 = tB8D4 = tB8D1i = tB8D2i = tB8D4i = endian(in, 8);
    B4min = R4 = B4D2 = B4D2i = B4D1 = B4D1i = (uint32_t)(norm(in, 4) & 0xffffffff);
    tB4min = tB4D2 = tB4D2i = tB4D1 = tB4D1i = (uint32_t)(endian(in, 4) & 0xffffffff);
    B2min = R2 = B2D1 = B2D1i = (uint16_t)(norm(in, 2) & 0xffff);
    tB2min = tB2D1 = tB2D1i = (uint16_t)(endian(in, 2) & 0xffff);
    B8D1 = B8D2 = B8D4 = tB8D1 = tB8D2 = tB8D4 = B4D1 = B4D2 = tB4D1 = tB4D2 = B2D1 = tB2D1 = 1;
    for (i = 0; i < 64; i++)
    {
        if (!(i%8) && !!i)
        {
            uint64_t temp64 = norm(in + i, 8);
            uint64_t ttemp64 = endian(in + i, 8);
            if (R8 != temp64) R8 = 0;
            if (temp64 > 0xff)
                B8D1i = ((B8D1i < temp64) && (B8D1i > 0xff)) ? B8D1i : temp64;
            if (temp64 > 0xffff)
                B8D2i = ((B8D2i < temp64) && (B8D2i > 0xffff)) ? B8D2i : temp64;
            if (temp64 > 0xffffffff)
                B8D4i = ((B8D4i < temp64) && (B8D4i > 0xffffffff)) ? B8D4i : temp64;
            if (ttemp64 > 0xff)
                tB8D1i = ((tB8D1i < ttemp64) && (tB8D1i > 0xff)) ? tB8D1i : ttemp64;
            if (ttemp64 > 0xffff)
                tB8D2i = ((tB8D2i < ttemp64) && (tB8D2i > 0xffff)) ? tB8D2i : ttemp64;
            if (ttemp64 > 0xffffffff)
                tB8D4i = ((tB8D4i < ttemp64) && (tB8D4i > 0xffffffff)) ? tB8D4i : ttemp64;
            B8min = B8min < temp64 ? B8min : temp64;
            tB8min = tB8min < ttemp64 ? tB8min : ttemp64;
        }
        if (!(i%4) && !!i)
        {
            uint32_t temp32 = (uint32_t)(norm(in + i, 4) & 0xffffffff);
            uint32_t ttemp32 = (uint32_t)(endian(in + i, 4) & 0xffffffff);
            if (R4 != temp32) R4 = 0;
            if (temp32 > 0xff)
                B4D1i = ((B4D1i < temp32) && (B4D1i > 0xff)) ? B4D1i : temp32;
            if (temp32 > 0xffff)
                B4D2i = ((B4D2i < temp32) && (B4D2i > 0xffff)) ? B4D2i : temp32;
            if (ttemp32 > 0xff)
                tB4D1i = ((tB4D1i < ttemp32) && (tB4D1i > 0xff)) ? tB4D1i : ttemp32;
            if (temp32 > 0xffff)
                tB4D2i = ((tB4D2i < ttemp32) && (tB4D2i > 0xffff)) ? tB4D2i : ttemp32;
            B4min = B4min < temp32 ? B4min : temp32;
            tB4min = tB4min < ttemp32 ? tB4min : ttemp32;
        }
        if (!(i%2) && !!i)
        {
            uint16_t temp16 = (uint16_t)(norm(in + i, 2) & 0xffff);
            uint16_t ttemp16 = (uint16_t)(endian(in + i, 2) & 0xffff);
            if (R2 != temp16) R2 = 0;
            if (temp16 > 0xff)
                B2D1i = ((B2D1i < temp16) && (B2D1i > 0xff)) ? B2D1i : temp16;
            if (ttemp16 > 0xff)
                tB2D1i = ((tB2D1i < ttemp16) && (tB2D1i > 0xff)) ? tB2D1i : ttemp16;
            B2min = B2min < temp16 ? B2min : temp16;
            tB2min = tB2min < ttemp16 ? tB2min : ttemp16;
        }
        if (R1 != in[i]) R1 = 0;
        if (in[i] != 0) R0 = 0;
    }
    if (R0 == 1)
    {
        out[0] = 0;
        return 1;
    }
    if (!!R1)
    {
        out[0] = 1;
        out[1] = R1;
        return 2;
    }
    if (!!R2)
    {
        out[0] = 2;
        *((uint16_t *) (out + 1)) = R2;
        return 3;
    }
    if (!!R4)
    {
        out[0] = 3;
        *((uint32_t *) (out + 1)) = R4;
        return 5;
    }
    if (!!R8)
    {
        out[0] = 4;
        *((uint64_t *) (out + 1)) = R8;
        return 9;
    }
    for (i = 0; i < 64; i++)
    {
        if (!(i%8))
        {
            uint64_t temp64 = norm(in + i, 8);
            uint64_t ttemp64 = endian(in + i, 8);
            if (temp64 - B8min > 0xff) B8D1 = 0;
            if (temp64 - B8min > 0xffff) B8D2 = 0;
            if (temp64 - B8min > 0xffffffff) B8D4 = 0;
            if (temp64 > 0xff && temp64 - B8D1i > 0xff) B8D1i = 0;
            if (temp64 > 0xffff && temp64 - B8D2i > 0xffff) B8D2i = 0;
            if (temp64 > 0xffffffff && temp64 - B8D4i > 0xffffffff) B8D4i = 0;
            if (ttemp64 - tB8min > 0xff) tB8D1 = 0;
            if (ttemp64 - tB8min > 0xffff) tB8D2 = 0;
            if (ttemp64 - tB8min > 0xffffffff) tB8D4 = 0;
            if (ttemp64 > 0xff && ttemp64 - tB8D1i > 0xff) tB8D1i = 0;
            if (ttemp64 > 0xffff && ttemp64 - tB8D2i > 0xffff) tB8D2i = 0;
            if (ttemp64 > 0xffffffff && ttemp64 - tB8D4i > 0xffffffff) tB8D4i = 0;
        }
        if (!(i%4))
        {
            uint32_t temp32 = (uint32_t)(norm(in + i, 4) & 0xffffffff);
            uint32_t ttemp32 = (uint32_t)(endian(in + i, 4) & 0xffffffff);
            if (temp32 - B4min > 0xff) B4D1 = 0;
            if (temp32 - B4min > 0xffff) B4D2 = 0;
            if (temp32 > 0xff && temp32 - B4D1i > 0xff) B4D1i = 0;
            if (temp32 > 0xffff && temp32 - B4D2i > 0xffff) B4D2i = 0;
            if (ttemp32 - tB4min > 0xff) tB4D1 = 0;
            if (ttemp32 - tB4min > 0xffff) tB4D2 = 0;
            if (ttemp32 > 0xff && ttemp32 - tB4D1i > 0xff) tB4D1i = 0;
            if (ttemp32 > 0xffff && ttemp32 - tB4D2i > 0xffff) tB4D2i = 0;
        }

        if (!(i%2))
        {
            uint16_t temp16 = (uint16_t)(norm(in + i, 2) & 0xffff);
            uint16_t ttemp16 = (uint16_t)(endian(in + i, 2) & 0xffff);
            if (temp16 - B2min > 0xff) B2D1 = 0;
            if (temp16 > 0xff && temp16 - B2D1i > 0xff) B2D1i = 0;
            if (ttemp16 - tB2min > 0xff) tB2D1 = 0;
            if (ttemp16 > 0xff && ttemp16 - tB2D1i > 0xff) tB2D1i = 0;
        }
    }
    if (!!B8D1) {
        bdicompress(in, out + 1, B8min, 0, 8, 1, 0);
        out[0] = 5;
        return 17;
    }
    if (!!tB8D1) {
        bdicompress(in, out + 1, tB8min, 0, 8, 1, 1);
        out[0] = 6;
        return 17;
    }
    if (!!B8D1i) {
        bdicompress(in, out + 1, B8D1i, 1, 8, 1, 0);
        out[0] = 11;
        return 18;
    }
    if (!!tB8D1i) {
        bdicompress(in, out + 1, tB8D1i, 1, 8, 1, 1);
        out[0] = 12;
        return 18;
    }
    if (!!B4D1) {
        bdicompress(in, out + 1, B4min, 0, 4, 1, 0);
        out[0] = 17;
        return 21;
    }
    if (!!tB4D1) {
        bdicompress(in, out + 1, tB4min, 0, 4, 1, 1);
        out[0] = 18;
        return 21;
    }
    if (!!B4D1i) {
        bdicompress(in, out + 1, B4D1i, 1, 4, 1, 0);
        out[0] = 21;
        return 23;
    }
    if (!!tB4D1i) {
        bdicompress(in, out + 1, tB4D1i, 1, 4, 1, 1);
        out[0] = 22;
        return 23;
    }
    if (!!B8D2) {
        bdicompress(in, out + 1, B8min, 0, 8, 2, 0);
        out[0] = 7;
        return 25;
    }
    if (!!tB8D2) {
        bdicompress(in, out + 1, tB8min, 0, 8, 2, 1);
        out[0] = 8;
        return 25;
    }
    if (!!B8D2i) {
        bdicompress(in, out + 1, B8D2i, 1, 8, 2, 0);
        out[0] = 13;
        return 27;
    }
    if (!!tB8D2i) {
        bdicompress(in, out + 1, tB8D2i, 1, 8, 2, 1);
        out[0] = 14;
        return 27;
    }

    if (!!B2D1) {
        bdicompress(in, out + 1, B2min, 0, 2, 1, 0);
        out[0] = 25;
        return 35;
    }
    if (!!tB8D2) {
        bdicompress(in, out + 1, tB2min, 0, 2, 1, 1);
        out[0] = 26;
        return 35;
    }
    if (!!B4D2) {
        bdicompress(in, out + 1, B4min, 0, 4, 2, 0);
        out[0] = 19;
        return 37;
    }
    if (!!tB4D2) {
        bdicompress(in, out + 1, tB4min, 0, 4, 2, 1);
        out[0] = 20;
        return 37;
    }
    if (!!B4D2i) {
        bdicompress(in, out + 1, B4D2i, 1, 4, 2, 0);
        out[0] = 23;
        return 39;
    }
    if (!!tB4D2i) {
        bdicompress(in, out + 1, tB4D2i, 1, 4, 2, 1);
        out[0] = 24;
        return 39;
    }
    if (!!B2D1i) {
        bdicompress(in, out + 1, B2D1i, 1, 2, 1, 0);
        out[0] = 27;
        return 39;
    }
    if (!!tB8D2i) {
        bdicompress(in, out + 1, tB2D1i, 1, 2, 1, 1);
        out[0] = 28;
        return 39;
    }
    if (!!B8D4) {
        bdicompress(in, out + 1, B8min, 0, 8, 4, 0);
        out[0] = 9;
        return 41;
    }
    if (!!tB8D4) {
        bdicompress(in, out + 1, tB8min, 0, 8, 4, 1);
        out[0] = 10;
        return 41;
    }
    if (!!B8D4i) {
        bdicompress(in, out + 1, B8D4i, 1, 8, 4, 0);
        out[0] = 15;
        return 42;
    }
    if (!!tB8D4i) {
        bdicompress(in, out + 1, tB8D4i, 1, 8, 4, 1);
        out[0] = 16;
        return 42;
    }
    out[0]=0xff;
    for (i = 0; i <64; i++)
        out[i + 1] = in[i];
    return 64;
}


uint8_t bdiDecompressData(uint8_t * in, uint8_t * out)
{
    int i;
    switch ((uint8_t)in[0])
    {
        case 0:
            for (i = 0; i <64; i++)
                out[i] = 0;
            return 0;
        case 1:
            for (i = 0; i < 64; i++)
                out[i] = in[1];
            return 1;
        case 2:
            for (i = 0; i < 32; i++)
                *((uint16_t *)(out + 2 * i)) = *((uint16_t *)(in + 1));
            return 2;
        case 3:
            for (i = 0; i < 16; i++)
                *((uint32_t *)(out + 4 * i)) = *((uint32_t *)(in + 1));
            return 3;
        case 4:
            for (i = 0; i < 8; i++)
                *((uint64_t *)(out + 8 * i)) = *((uint64_t *)(in + 1));
            return 4;
        case 5:
            bdidecompress(in + 1, out, 0, 8, 1, 0);
            break;
        case 6:
            bdidecompress(in + 1, out, 0, 8, 1, 1);
            break;
        case 7:
            bdidecompress(in + 1, out, 0, 8, 2, 0);
            break;
        case 8:
            bdidecompress(in + 1, out, 0, 8, 2, 1);
            break;
        case 9:
            bdidecompress(in + 1, out, 0, 8, 4, 0);
            break;
        case 10:
            bdidecompress(in + 1, out, 0, 8, 4, 1);
            break;
        case 11:
            bdidecompress(in + 1, out, 1, 8, 1, 0);
            break;
        case 12:
            bdidecompress(in + 1, out, 1, 8, 1, 1);
            break;
        case 13:
            bdidecompress(in + 1, out, 1, 8, 2, 0);
            break;
        case 14:
            bdidecompress(in + 1, out, 1, 8, 2, 1);
            break;
        case 15:
            bdidecompress(in + 1, out, 1, 8, 4, 0);
            break;
        case 16:
            bdidecompress(in + 1, out, 1, 8, 4, 1);
            break;
        case 17:
            bdidecompress(in + 1, out, 0, 4, 1, 0);
            break;
        case 18:
            bdidecompress(in + 1, out, 0, 4, 1, 1);
            break;
        case 19:
            bdidecompress(in + 1, out, 0, 4, 2, 0);
            break;
        case 20:
            bdidecompress(in + 1, out, 0, 4, 2, 1);
            break;
        case 21:
            bdidecompress(in + 1, out, 1, 4, 1, 0);
            break;
        case 22:
            bdidecompress(in + 1, out, 1, 4, 1, 1);
            break;
        case 23:
            bdidecompress(in + 1, out, 1, 4, 2, 0);
            break;
        case 24:
            bdidecompress(in + 1, out, 1, 4, 2, 1);
            break;
        case 25:
            bdidecompress(in + 1, out, 0, 2, 1, 0);
            break;
        case 26:
            bdidecompress(in + 1, out, 0, 2, 1, 1);
            break;
        case 27:
            bdidecompress(in + 1, out, 1, 2, 1, 0);
            break;
        case 28:
            bdidecompress(in + 1, out, 1, 2, 1, 1);
            break;
        default:
            for (i = 0; i <64; i++)
                out[i] = in [i + 1];
            return 0xff;
    }
    return in[0];
}
