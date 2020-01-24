/*

    Wrap code for bdi compression to run with program

    By Yuqing Liu
    HEAP Lab, Virginia Tech 
    Aug 2019
*/

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#include <bdi.h>
#include <plugin_struct.h>

struct compression COMPRESSION_NODE_NAME;

static uint64_t bdi_compression(struct compression * c_p, uint8_t * start, uint16_t ** report)
{
    uint8_t bditemp[65];
    uint8_t bdirev[64];
    uint64_t i, cache_size = 0;
    if (CACHELINE_SIZE % 64 == 0) // aligned to multiple
        *report = calloc(sizeof(uint16_t), PAGE_SIZE / CACHELINE_SIZE);
    uint64_t sum = 0;
    for (i = 0; i < PAGE_SIZE; i += 64)//cacheline size is fixed here for compression
    {
        int s = bdiCompressData(start + i, bditemp); // in bytes
        if (COMPRESSION_NODE_NAME.sharedv->parse_switch)
            s = s > 64 ? 64 : s;
        s *= 8;
        cache_size += s;
        if (report != NULL && (i + 64) % CACHELINE_SIZE == 0)
        {
            (*report)[i / CACHELINE_SIZE] = cache_size;
            cache_size = 0;
        }
        sum += s;
        if (COMPRESSION_NODE_NAME.sharedv->validate && !(s >= 64 * 8 && COMPRESSION_NODE_NAME.sharedv->parse_switch))
        {
            bdiDecompressData(bditemp,bdirev);
            int j;
            for (j = 0; j < 64; j++)
                if (start[i + j] != bdirev[j])
                {
                    printf("bdi Error: offset=%"PRIx64"\n", i);
                    return ERROR_SIZE;
                }
        }
    }
    return sum;
}

struct compression COMPRESSION_NODE_NAME = {
    .next = NULL,
    .name = "bdi",
    .compress = (run_compression_t)bdi_compression
};