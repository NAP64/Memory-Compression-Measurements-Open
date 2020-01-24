/*

    Bit-Plane Compression Code for Compresso

    Implemented based on paper:
    https://dl.acm.org/citation.cfm?id=3001172
    Modified based on Compresso paper:
    lph.ece.utexas.edu/merez/uploads/MattanErez/micro18_compresso.pdf

    By Yuqing Liu
    HEAP Lab, Virginia Tech 
    Jun 2019
    
*/

#include <stdint.h>
#include <BitStream64.h>

static uint32_t bpctransform(uint32_t in[16], uint16_t out[32])
{
    uint32_t base = in[0];
    uint32_t delta[15];
    int i, j;
    for (i = 1; i < 16; i++)
        delta[i - 1] = in[i] - in[i - 1];
    for (i = 0; i < 32; i++)
    {
        out[i] = 0;
        for (j = 0; j < 15; j++)
            out[i] |= ((delta[j] >> i) & 1) << j;
    }
    return base;
}

static void bpctransform_rev(uint32_t base, uint16_t in[32], uint32_t out[16])
{
    out[0] = base;
    uint32_t delta[15];
    int i, j;
    for (i = 0; i < 15; i++)
    {
        delta[i] = 0;
        for (j = 0; j < 32; j++)
            delta[i] |= ((in[j] >> i) & 1) << j;
    }
    for (i = 1; i < 16; i++)
            out[i] = out[i - 1] + delta[i - 1];
}

static int bpcCompressData(uint16_t in[16], uint8_t* out)
{
    uint16_t plane[32];
    uint32_t base = bpctransform((uint32_t*)in, plane);
    int i;
    BitStream64_t BSout;
    BitStream64_write_init(&BSout, out);
    BitStream64_write(&BSout, 0, 1);    //default bpc
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
    uint16_t * op = plane;
    uint32_t bits2, bits = 0;
    uint32_t bytes;
    int j, temp, count;
    uint8_t out2 [34*2+2];
    for (count = 0; count < 2; count ++)
    {
        uint16_t DBP = op[31];
        uint16_t DBX;
        int zeros = 0;

        for (i = 31; i >= 0; i--)
        {
            if (i == 31)
                DBX = DBP;
            else
            {
                DBP = op[i + 1];
                DBX = DBP ^ op[i];
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
            for (j = 0; j < 16; j++)
            {
                if (DBX == one)
                    break;
                uint32_t po = one;
                one = one << 1;
                if (j < 15 && DBX == (po | one))
                {
                    temp = j;
                    break;
                }
            }
            if (j < 16)
            {
                if (temp == j)
                    BitStream64_write(&BSout, 2, 5);
                else
                    BitStream64_write(&BSout, 3, 5);
                BitStream64_write(&BSout, j, 4);
                continue;
            }
            if (DBX == 0x7fff)
                BitStream64_write(&BSout, 0, 5);
            else if (!!DBX && !op[i])
                BitStream64_write(&BSout, 1, 5);
            else
            {
                BitStream64_write(&BSout, 1, 1);
                BitStream64_write(&BSout, DBX, 15 + count);
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
        bits2 = BSout.d_offset * 8 + BSout.b_offset;
        bytes = BitStream64_write_finish(&BSout);
        if (count == 0)
        {
            bits = bits2;
            op = in;
            BitStream64_write_init(&BSout, out2);
            BitStream64_write(&BSout, 1, 1);    //modified bpc
        }
    }
    if (bits2 < bits)
    {
        for (i = 0; i < bytes; i++)
            out[i] = out2[i];
        return bits2;
    }
    return bits;
}

static int bpcDecompressData(uint8_t* in, uint16_t out[32])
{
    uint16_t * target;
    int i, j;
    BitStream8_t bs;
    BitStream8_read_init(&bs, in);
    uint32_t base = 0;
    uint16_t plane[32];
    if (BitStream8_read(&bs, 1) == 1)
        target = out;
    else
    {
        target = plane;
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
    }
    
    uint16_t prevDBP = 0;
    for (i = 31; i >= 0; i--)
    {
        if (BitStream8_read(&bs, 1) == 1)
            target[i] = BitStream8_read(&bs, 15 + (target == out ? 1 : 0)) ^ prevDBP;
        else if (BitStream8_read(&bs, 1) == 1)
        {
            int temp = BitStream8_read(&bs, 5);
            for (j = 0; j < temp + 2; j++)
                target[i - j] = prevDBP;
            i -= temp + 1;
        }
        else if (BitStream8_read(&bs, 1) == 1)
            target[i] = prevDBP;
        else
        {
            uint16_t temp;
            switch (BitStream8_read(&bs, 2))
            {
                case 0:
                    target[i] = 0x7fff ^ prevDBP;
                    break;
                case 1:
                    target[i] = 0;
                    break;
                case 2:
                    temp = BitStream8_read(&bs, 4);
                    target[i] = ((1 << temp) | (1 << (temp + 1))) ^ prevDBP;
                    break;
                case 3:
                    target[i] = (1 << BitStream8_read(&bs, 4)) ^ prevDBP;
                    break;
            }
        }
        prevDBP = target[i];
    }
    if (target == plane)
        bpctransform_rev(base, plane, (uint32_t*)out);
    return bs.s_offset * 8 + bs.b_offset;
}