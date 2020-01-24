/*

    Wrap code for lz4 compression to run with program

    HEAP Lab, Virginia Tech 
    Aug 2019
*/

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#include <lz4/lz42.h>
#include <plugin_struct.h>

struct compression COMPRESSION_NODE_NAME;

static uint64_t lz4_compression(struct compression * c_p, uint8_t * start, uint16_t ** report)
{
    uint8_t compressed[(int)(PAGE_SIZE * 1.2)];
    int size = PAGE_SIZE*1.2;
    int csize = LZ4_compress_default((const char *)start, (char *)compressed, PAGE_SIZE, size);
    if (COMPRESSION_NODE_NAME.sharedv->validate)
    {
        uint8_t decompressed[PAGE_SIZE];
        size = PAGE_SIZE;
        size = LZ4_decompress_safe((const char *)compressed, (char *)decompressed, csize, size);
        if (size != PAGE_SIZE)
            printf("Lz4 Error: size=%d!=page\n", size);
        for (size = 0; size < PAGE_SIZE; size++)
            if (start[size] != decompressed[size])
            {
                printf("Lz4 Error: offset=%d\n", size);
                return ERROR_SIZE;
            }
    }
    return csize * 8;
}

struct compression COMPRESSION_NODE_NAME = {
    .next = NULL,
    .name = "lz4",
    .compress = (run_compression_t)lz4_compression
};
