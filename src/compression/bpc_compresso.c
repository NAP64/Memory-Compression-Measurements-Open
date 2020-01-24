/*

    Wrap code for bpc (compresso version) compression to run with program

    By Yuqing Liu
    HEAP Lab, Virginia Tech 
    Aug 2019
*/

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#include <bpc_compresso.h>   //int16*32
#include <plugin_struct.h>

struct compression COMPRESSION_NODE_NAME;

static uint64_t bpc_compresso_compression(struct compression * c_p, uint8_t * start, uint16_t ** report)
{
    uint8_t bpctemp[34*2+2];
    uint16_t bpcrev[32];
    uint64_t i, cache_size = 0;
    uint64_t sum = 0;
    if (CACHELINE_SIZE % 64 == 0) // aligned to multiple
        *report = calloc(sizeof(uint16_t), PAGE_SIZE / CACHELINE_SIZE);
    for (i = 0; i < PAGE_SIZE; i += 64)//cacheline size is fixed here for compression
    {
        int j;
        int s = bpcCompressData(((uint16_t *)(start + i)), bpctemp); // in bits
        if (COMPRESSION_NODE_NAME.sharedv->validate)
        {
            int s1 = bpcDecompressData(bpctemp, bpcrev);
            if (s1 != s)
            {
                printf("bpc_compresso size Error: offset=%"PRIx64":%d:%d\n", i, s, s1);
                return ERROR_SIZE;
            }
            for (j = 0; j < 32; j++)
                if (((uint16_t *)(start + i))[j] != bpcrev[j])
                {
                    printf("bpc_compresso Error: offset=%"PRIx64"\n", i + j * 2);
                    bpcCompressData(((uint16_t *)(start + i)), bpctemp);
                    bpcDecompressData(bpctemp, bpcrev);
                    return ERROR_SIZE;
                }
        }
        if (COMPRESSION_NODE_NAME.sharedv->parse_switch)
            s = s > 64 * 8 ? 64 * 8 : s;
        sum += s;
        cache_size += s;
        if (report != NULL && (i + 64) % CACHELINE_SIZE == 0)
        {
            (*report)[i / CACHELINE_SIZE] = cache_size;
            cache_size = 0;
        }
    }
    return sum;
}

struct compression COMPRESSION_NODE_NAME = {
    .next = NULL,
    .name = "bpc_compresso",    // int16*32
    .compress = (run_compression_t)bpc_compresso_compression
};