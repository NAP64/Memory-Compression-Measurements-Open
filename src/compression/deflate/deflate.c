/*

    Wrap code for deflate compression to run with program
    Used code from zlib to control zlib behavior

    By Yuqing Liu
    HEAP Lab, Virginia Tech 
    Jun 2019
*/

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#include <zlib/zlib.h>
#include <plugin_struct.h>

struct compression COMPRESSION_NODE_NAME;

//copied from zlib. modified
int ZEXPORT compress4k (dest, destLen, source, sourceLen)
    Bytef *dest;
    uLongf *destLen;
    const Bytef *source;
    uLong sourceLen;
{
    z_stream stream;
    int err;
    const uInt max = (uInt)-1;
    uLong left;

    left = *destLen;
    *destLen = 0;

    stream.zalloc = (alloc_func)0;
    stream.zfree = (free_func)0;
    stream.opaque = (voidpf)0;

    //modified for no header and 4k window size
    err = deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -12, 8, Z_DEFAULT_STRATEGY);

    if (err != Z_OK) return err;

    stream.next_out = dest;
    stream.avail_out = 0;
    stream.next_in = (z_const Bytef *)source;
    stream.avail_in = 0;

    do {
        if (stream.avail_out == 0) {
            stream.avail_out = left > (uLong)max ? max : (uInt)left;
            left -= stream.avail_out;
        }
        if (stream.avail_in == 0) {
            stream.avail_in = sourceLen > (uLong)max ? max : (uInt)sourceLen;
            sourceLen -= stream.avail_in;
        }
        err = deflate(&stream, sourceLen ? Z_NO_FLUSH : Z_FINISH);
    } while (err == Z_OK);

    *destLen = stream.total_out;
    deflateEnd(&stream);
    return err == Z_STREAM_END ? Z_OK : err;
}

int ZEXPORT uncompress4k (dest, destLen, source, sourceLen)
    Bytef *dest;
    uLongf *destLen;
    const Bytef *source;
    uLong *sourceLen;
{
    z_stream stream;
    int err;
    const uInt max = (uInt)-1;
    uLong len, left;
    Byte buf[1];    /* for detection of incomplete stream when *destLen == 0 */

    len = *sourceLen;
    if (*destLen) {
        left = *destLen;
        *destLen = 0;
    }
    else {
        left = 1;
        dest = buf;
    }

    stream.next_in = (z_const Bytef *)source;
    stream.avail_in = 0;
    stream.zalloc = (alloc_func)0;
    stream.zfree = (free_func)0;
    stream.opaque = (voidpf)0;

    //modified for raw
    err = inflateInit2(&stream, -12);

    if (err != Z_OK) return err;

    stream.next_out = dest;
    stream.avail_out = 0;

    do {
        if (stream.avail_out == 0) {
            stream.avail_out = left > (uLong)max ? max : (uInt)left;
            left -= stream.avail_out;
        }
        if (stream.avail_in == 0) {
            stream.avail_in = len > (uLong)max ? max : (uInt)len;
            len -= stream.avail_in;
        }
        err = inflate(&stream, Z_NO_FLUSH);
    } while (err == Z_OK);

    *sourceLen -= len + stream.avail_in;
    if (dest != buf)
        *destLen = stream.total_out;
    else if (stream.total_out && err == Z_BUF_ERROR)
        left = 1;

    inflateEnd(&stream);
    return err == Z_STREAM_END ? Z_OK :
           err == Z_NEED_DICT ? Z_DATA_ERROR  :
           err == Z_BUF_ERROR && left + stream.avail_out ? Z_DATA_ERROR :
           err;
}

static uint64_t deflate_method(struct compression * c_p, uint8_t * start, uint16_t ** report)
{
    uint8_t compressed[(int)(1.2*PAGE_SIZE)];
    uLongf size = (int)(1.2*PAGE_SIZE);
    compress4k(compressed, &size, start, PAGE_SIZE);
    uint64_t ret = 0;
    ret = size * 8;
    if (compression_node.sharedv->validate)
    {
        uint8_t decompressed[PAGE_SIZE];
        uLongf size2 = (int)(1.2*PAGE_SIZE);
        uLong size3 = size;
        uncompress4k(decompressed, &size2, compressed, &size3);
        if (size2 != PAGE_SIZE)
            printf("Defalte Error: size=%d!=PAGE_SIZE\n", size2);
        for (size2 = 0; size2 < PAGE_SIZE; size2++)
            if (start[size2] != decompressed[size2])
            {
                printf("Defalte Error: offset=%d\n", size2);
                return ERROR_SIZE;
            }
    }
    return ret;
}

struct compression COMPRESSION_NODE_NAME = {
    .next = NULL,
    .name = "deflate",
    .compress = (run_compression_t)deflate_method,
};
