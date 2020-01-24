/*

    Bit-Plane Compression Code

    Implemented based on paper:
    https://dl.acm.org/citation.cfm?id=3001172

    By Yuqing Liu
    HEAP Lab, Virginia Tech 
    Jun 2019
    
*/

#include <stdint.h>
#include <BitStream64.h>

static uint32_t bpctransform(uint32_t in[32], uint32_t out[33])
{
    uint32_t base = in[0];
    uint32_t delta[31];
    out[32] = 0;
    int i, j;
    for (i = 1; i < 32; i++)
    {
        delta[i - 1] = in[i] - in[i - 1];
        if (in[i] < in[i - 1])
            out[32] |= 1 << (i - 1);
    }
    for (i = 0; i < 32; i++)
    {
        out[i] = 0;
        for (j = 0; j < 31; j++)
            out[i] |= ((delta[j] >> i) & 1) << j;
    }
    return base;
}

static void bpctransform_rev(uint32_t base, uint32_t in[33], uint32_t out[32])
{
    out[0] = base;
    uint32_t delta[31];
    int i, j;
    for (i = 0; i < 31; i++)
    {
        delta[i] = 0;
        for (j = 0; j < 32; j++)
            delta[i] |= ((in[j] >> i) & 1) << j;
    }
    for (i = 1; i < 32; i++)
    {
        if (!((in[32] >> (i - 1)) & 1))
            out[i] = out[i - 1] + delta[i - 1];
        else
            out[i] = out[i - 1] - (~delta[i - 1] + 1);
    }
}

static int bpcCompressData(uint32_t in[32], uint8_t* out)
{
    uint32_t plane[33];
    uint32_t base = bpctransform(in, plane);
    int i;
    BitStream64_t BSout;
    BitStream64_write_init(&BSout, out);
    //write base with orig encoder
    if (base == 0)  //all 0
    {
        BitStream64_write(&BSout, 0, 3);
    }
    else if (!(0xfffffff8 & base) || !(0xfffffff8 & ~base)) // we have 4-bit
    {
        BitStream64_write(&BSout, 1, 3);
        BitStream64_write(&BSout, base & 0xf, 4);
    }
    else if (!(0xffffff80 & base) || !(0xffffff80 & ~base)) // we have 8-bit
    {
        BitStream64_write(&BSout, 2, 3);
        BitStream64_write(&BSout, base & 0xff, 8);
    }
    else if (!(0xffff8000 & base) || !(0xffff8000 & ~base)) // we have 16-bit
    {
        BitStream64_write(&BSout, 3, 3);
        BitStream64_write(&BSout, base & 0xffff, 16);
    }
    else         // we have 32-bit base
    {
        BitStream64_write(&BSout, 1, 1);
        BitStream64_write(&BSout, base, 32);
    }

    // encode plane in ZRL encoder
    int j;
    uint32_t DBP = plane[32];
    uint32_t DBX;
    int temp;
    int zeros = 0;

    for (i = 32; i >= 0; i--)
    {
        if (i == 32)
            DBX = DBP;
        else
        {
            DBP = plane[i + 1];
            DBX = DBP ^ plane[i];
        }
        //handle 0-run length
        if (DBX == 0)
        {
            zeros++;
            continue;
        }
        if (zeros == 1)
        {
            BitStream64_write(&BSout, 1, 3);
            zeros = 0;
        }
        else if (zeros > 1)
        {
            zeros -= 2;
            BitStream64_write(&BSout, 1, 2);
            BitStream64_write(&BSout, zeros, 5);
            zeros = 0;
        }
        // 1 and consecutive 1
        temp = -1;
        uint32_t one = 1;
        for (j = 0; j < 32; j++)
        {
            if (DBX == one)
                break;
            uint32_t po = one;
            one = one << 1;
            if (j < 31 && DBX == (po | one))
            {
                temp = j;
                break;
            }
        }
        if (j < 32)
        {
            if (temp == j)
                BitStream64_write(&BSout, 2, 5);
            else
                BitStream64_write(&BSout, 3, 5);
            BitStream64_write(&BSout, j, 5);
            continue;
        }
        if (DBX == 0x7fffffff)
            BitStream64_write(&BSout, 0, 5);
        else if (!!DBX && !plane[i])
            BitStream64_write(&BSout, 1, 5);
        else
        {
            BitStream64_write(&BSout, 1, 1);
            BitStream64_write(&BSout, DBX, 31);
        }

    }
    if (zeros == 1)
    {
        BitStream64_write(&BSout, 1, 3);
        zeros = 0;
    }
    else if (zeros > 1)
    {
        zeros -= 2;
        BitStream64_write(&BSout, 1, 2);
        BitStream64_write(&BSout, zeros, 5);
    }
    uint32_t bits = BSout.d_offset * 8 + BSout.b_offset;
    BitStream64_write_finish(&BSout);
    return bits;
}

static int bpcDecompressData(uint8_t* in, uint32_t out[32])
{
    uint32_t base = 0;
    int i, j;
    BitStream8_t bs;
    BitStream8_read_init(&bs, in);
    uint32_t plane[33];
    if (BitStream8_read(&bs, 1) == 1)
        base = BitStream8_read(&bs, 32);
    else
    {
        switch(BitStream8_read(&bs, 2))
        {
            case 1:
                base = BitStream8_read(&bs, 4);
                if (!!(base & 8))
                    base |= 0xfffffff0;
                break;
            case 2:
                base = BitStream8_read(&bs, 8);
                if (!!(base & 0x80))
                    base |= 0xffffff00;
                break;
            case 3:
                base = BitStream8_read(&bs, 16);
                if (!!(base & 0x8000))
                    base |= 0xffff0000;
                break;
        }
    }
    
    uint32_t prevDBP = 0;
    for (i = 32; i >= 0; i--)
    {
        if (BitStream8_read(&bs, 1) == 1)
            plane[i] = BitStream8_read(&bs, 31) ^ prevDBP;
        else if (BitStream8_read(&bs, 1) == 1)
        {
            int temp = BitStream8_read(&bs, 5);
            for (j = 0; j < temp + 2; j++)
                plane[i - j] = prevDBP;
            i -= temp + 1;
        }
        else if (BitStream8_read(&bs, 1) == 1)
            plane[i] = prevDBP;
        else
        {
            uint32_t temp;
            switch (BitStream8_read(&bs, 2))
            {
                case 0:
                    plane[i] = 0x7fffffff ^ prevDBP;
                    break;
                case 1:
                    plane[i] = 0;
                    break;
                case 2:
                    temp = BitStream8_read(&bs, 5);
                    plane[i] = ((1 << temp) | (1 << (temp + 1))) ^ prevDBP;
                    break;
                case 3:
                    plane[i] = (1 << BitStream8_read(&bs, 5)) ^ prevDBP;
                    break;
            }
        }
        prevDBP = plane[i];
    }
    bpctransform_rev(base, plane, out);
    return bs.s_offset * 8 + bs.b_offset;
}