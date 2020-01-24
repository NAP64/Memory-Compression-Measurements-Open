/*

    Wrap code for bpc compression to run with program

    By Yuqing Liu
    HEAP Lab, Virginia Tech 
    Jun 2019
*/

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#include <bpc.h>
#include <plugin_struct.h>

struct compression COMPRESSION_NODE_NAME;

static uint64_t bpc_compression(struct compression * c_p, uint8_t * start, uint16_t ** report)
{
    uint8_t bpctemp[34*4+1];
    uint32_t bpcrev[32];
    uint64_t i;
    uint64_t sum = 0;
    for (i = 0; i < PAGE_SIZE; i += 128)//cacheline size is fixed here for compression
    {
        int s = bpcCompressData(((uint32_t *)(start + i)), bpctemp); // in bits
        if (COMPRESSION_NODE_NAME.sharedv->validate)
        {
            int s1 = bpcDecompressData(bpctemp, bpcrev);
            if (s1 != s)
            {
                printf("bpc size Error: offset=%"PRIx64":%d:%d\n", i, s, s1);
                return ERROR_SIZE;
            }
            int j;
            for (j = 0; j < 32; j++)
                if (((uint32_t *)(start + i))[j] != bpcrev[j])
                {
                    printf("bpc Error: offset=%"PRIx64"\n", i + j * 4);
                    bpcCompressData(((uint32_t *)(start + i)), bpctemp);
                    bpcDecompressData(bpctemp, bpcrev);
                    return ERROR_SIZE;
                }
        }
        if (COMPRESSION_NODE_NAME.sharedv->parse_switch)
            s = s > 1024 ? 1024: s;
        sum += s;
    }
    return sum;
}

struct compression COMPRESSION_NODE_NAME = {
    .next = NULL,
    .name = "bpc",
    .compress = (run_compression_t)bpc_compression
};