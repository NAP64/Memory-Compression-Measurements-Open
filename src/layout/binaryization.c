/*

    Binaryization

    Determines how much of the compressed page/cacheline is within a certain size

    By Yuqing Liu
    HEAP Lab, Virginia Tech 
    Jun 2019

*/

#include <inttypes.h>
#include <string.h>

#include <plugin_struct.h>

struct layout LAYOUT_NODE_NAME;


#define PAGE_B (3604*8)
#define CL_B (CACHELINE_SIZE*8)

#define PAGE_CALC(a) (a > PAGE_B ? PAGE_SIZE * 8 : PAGE_SIZE * 4)
//#define CL_CALC(a) (NORM_CACHELINE(a) > CL_B ? CACHELINE_SIZE * 8 : CACHELINE_SIZE * 4)

#define interest "best-of"

static uint64_t bz_cp(struct compression * c_p, uint8_t * start, uint16_t ** report);
__thread uint64_t pgs;

static void bz_fr()
{   return;}

static void bz_init(struct compression ** c_p)
{
    if (*c_p == NULL)
        return;
    int i;
    struct compression * cp = *c_p;
    for (i = 0; ; cp = cp->next)
    {
        if (cp->next == NULL)
        {
            cp->next = LAYOUT_NODE_NAME.reports;
            break;
        }
        i++;
    }
}

static void bz_pr(struct compression * c_p, uint16_t * cl_list, uint16_t page_size)
{
    if (!strcmp(c_p->name, interest))
        pgs = PAGE_CALC(page_size);
}

static uint64_t bz_cp(struct compression * c_p, uint8_t * start, uint16_t ** report)
{
    return pgs;
}

static void bz_tcr()
{
    return;
}

static void bz_cr()
{
    return;
}

struct compression COMPRESSION_NODE_NAME = {
    .next = NULL,
    .name = "bz",
    .compress = (run_compression_t)bz_cp
};

struct layout LAYOUT_NODE_NAME = {
    .next = NULL,
    .name = "binaryization",
    .L_init = (layout_init_t) bz_init,
    .L_page_r = (layout_page_report_t) bz_pr,
    .L_final_r = (layout_final_report_t) bz_fr,
    .L_thread_clean_r = (layout_thread_clean_t) bz_tcr,
    .L_clean_r = (layout_clean_t) bz_cr,
    .reports = &COMPRESSION_NODE_NAME,
    .report_count = 1,
    .priority = -2
};