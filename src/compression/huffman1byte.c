/*

    Wrap code for huffman compression to run with program

    By Yuqing Liu
    HEAP Lab, Virginia Tech 
    Jun 2019
*/

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#include <huffman1byte.h>
#include <plugin_struct.h>

struct compression COMPRESSION_NODE_NAME;

static uint64_t huff1_compression(struct compression * c_p, uint8_t * start, uint16_t ** report)
{
    uint8_t comp[5000] = {0};
    uint64_t res = Huffman1_encode(start, comp, 4096);
    if (COMPRESSION_NODE_NAME.sharedv->validate && !(res >= 4096 && COMPRESSION_NODE_NAME.sharedv->parse_switch))
    {
        uint8_t rev[5000] = {0};
        uint64_t res1 = Huffman1_decode(comp, rev, 4096);
        if (res != res1)
            printf("huffman1 Error: sizet=%"PRId64" != %"PRId64"\n", res, res1);
        int i;
        for (i = 0; i < 4096; i++)
        {
            if (rev[i]!=start[i])
            {
                printf("huffman1 Error: offset=%x\n", i);
                return ERROR_SIZE;
            }
        }
    }
    return res * 8;
}

struct compression COMPRESSION_NODE_NAME = {
    .next = NULL,
    .name = "huffman1",
    .compress = (run_compression_t)huff1_compression
};