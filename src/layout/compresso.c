/*

    Compresso Layout code

    Implemented based on paper:
    lph.ece.utexas.edu/merez/uploads/MattanErez/micro18_compresso.pdf

    By Yuqing Liu
    HEAP Lab, Virginia Tech 
    Jun 2019

*/

#include <inttypes.h>
#include <string.h>

#include <plugin_struct.h>

#define COMPRESSONAME "bpc_compresso"

struct layout LAYOUT_NODE_NAME;
struct compression COMPRESSION_NODE_NAME;

const static uint8_t allowed_cacheline_sizes [] = {0, 8, 32, 64};
#define allowed_cacheline_sizes_len 4

const static uint16_t allowed_page_sizes [] = {512, 1024, 1536, 2048, 2560, 3072, 3584, 4096};
#define allowed_page_sizes_len 8

static uint64_t raw_cacheline_count[allowed_cacheline_sizes_len];
static uint64_t raw_page_count[allowed_page_sizes_len];

static uint64_t raw_cacheline_size[allowed_cacheline_sizes_len];
static uint64_t raw_page_size[allowed_page_sizes_len];
static uint64_t raw_page_size_aligned[allowed_page_sizes_len];

static pthread_mutex_t raw_cacheline_lock[allowed_cacheline_sizes_len];
static pthread_mutex_t raw_page_lock[allowed_page_sizes_len];

__thread uint32_t psize;
__thread uint32_t psizealigned;

static void compresso_init(struct compression ** c_p)
{
    LAYOUT_NODE_NAME.report_count = 0;
    if (*c_p == NULL)
        return;
    struct compression * p;
    for (p = *c_p; p != NULL; p = p->next)
    {
        if (!strcmp(p->name, COMPRESSONAME))
            LAYOUT_NODE_NAME.report_count = 2;
        if (p->next == NULL)
        {
            p->next = &COMPRESSION_NODE_NAME;
            break;
        }
    }
    if (LAYOUT_NODE_NAME.report_count)
    {
        int i;
        for (i = 0; i < allowed_cacheline_sizes_len ; i++)
        {
            raw_cacheline_lock[i] = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
            raw_cacheline_size[i] = 0;
            raw_cacheline_count[i] = 0;
        }
        for (i = 0; i < allowed_page_sizes_len ; i++)
        {
            raw_page_lock[i] = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
            raw_page_size[i] = 0;
            raw_page_count[i] = 0;
            raw_page_size_aligned[i] = 0;
        }
    }
    return;
}

static void compresso_pr(struct compression * c_p, uint16_t * cl_list, uint16_t page_size)
{   
    if (!LAYOUT_NODE_NAME.report_count || !!strcmp(c_p->name, COMPRESSONAME))
        return;
    int i, j;
    psizealigned = 0;
    for (i = 0; i < PAGE_SIZE / CACHELINE_SIZE; i++)
    {
        for (j = 0; j < allowed_cacheline_sizes_len; j++)
            if (IS_ZERO_CACHELINE(cl_list[i]) || cl_list[i] <= allowed_cacheline_sizes[j] * 8)
                break;
        j = j == allowed_cacheline_sizes_len ? j - 1 : j;
        psizealigned += allowed_cacheline_sizes[j];
        pthread_mutex_lock(&(raw_cacheline_lock[j]));
        raw_cacheline_count[j]++;
        raw_cacheline_size[j] += NORM_CACHELINE(cl_list[i]);
        pthread_mutex_unlock(&(raw_cacheline_lock[j]));
    }
    for (j = 0; j <= allowed_page_sizes_len; j++)
        if (psizealigned < allowed_page_sizes[j])
            break;
    j = j == allowed_page_sizes_len ? j - 1 : j;
    psize = allowed_page_sizes[j] + 64;
    psize *= 8;
    psizealigned *= 8;
    pthread_mutex_lock(&(raw_page_lock[j]));
    raw_page_count[j]++;
    raw_page_size[j] += page_size;
    raw_page_size_aligned[j] += psizealigned;
    pthread_mutex_unlock(&(raw_page_lock[j]));
    return;
}

static void compresso_fr(struct compression * begin_of_linked_list, uint64_t totalpages)
{   return; }

static void compresso_tcr()
{   return; }

static void compresso_cr()
{
    if (LAYOUT_NODE_NAME.report_count)
    {
        int i;
        printf("Cache count:");
        uint64_t total1, total2, total3, total4;
        total1 = total2 = total3 = 0;
        for (i = 0; i < allowed_cacheline_sizes_len ; i++)
        {
            pthread_mutex_destroy(&raw_cacheline_lock[i]);
            printf("%lu:", raw_cacheline_count[i]);
            total1 += raw_cacheline_size[i];
            total2 += raw_cacheline_count[i] * allowed_cacheline_sizes[i] * 8;
            total3 += raw_cacheline_count[i] * CACHELINE_SIZE * 8;
        }
        printf("\nCache raw:");
        for (i = 0; i < allowed_cacheline_sizes_len ; i++)
            printf("%lu:", raw_cacheline_size[i]);
        printf("\nCache ratio:");
        for (i = 0; i < allowed_cacheline_sizes_len ; i++)
            printf("%lf:", (raw_cacheline_count[i] * allowed_cacheline_sizes[i] * 8) / (double)raw_cacheline_size[i]);
        printf("\nCache-reconstruct:%lf:%lf", total3 / (0.0 + total1), total3 / (0.0 + total2));
        printf("\nPage count:");
        total1 = total2 = total3 = total4 = 0;
        for (i = 0; i < allowed_page_sizes_len ; i++)
        {
            pthread_mutex_destroy(&raw_page_lock[i]);
            printf("%lu:", raw_page_count[i]);
            total1 += raw_page_size[i];
            total2 += raw_page_size_aligned[i];
            total3 += raw_page_count[i] * allowed_page_sizes[i] * 8;
            total4 += raw_page_count[i] * PAGE_SIZE * 8;
        }
        printf("\nPage raw:");
        for (i = 0; i < allowed_page_sizes_len ; i++)
            printf("%lu:", raw_page_size[i]);
        printf("\nPage raw ratio:");
        for (i = 0; i < allowed_page_sizes_len ; i++)
            printf("%lf:", (raw_page_count[i] * allowed_page_sizes[i] * 8) / (double)raw_page_size[i]);
        printf("\nPage-reconstruct:%lf:%lf:%lf", total4 / (double)total1, total4 / (double)total2, total4 / (double)total3);
        printf("\nPage cache_aligned:");
        for (i = 0; i < allowed_page_sizes_len ; i++)
            printf("%lu:", raw_page_size_aligned[i]);
        printf("\nPage cache_aligned ratio:");
        for (i = 0; i < allowed_page_sizes_len ; i++)
            printf("%lf:", (raw_page_count[i] * allowed_page_sizes[i] * 8) / (double)raw_page_size_aligned[i]);
        printf("\n");
    }
    return;
}

static uint64_t compresso_cp(struct compression * c_p, uint8_t * start, uint16_t ** report)
{
    return psize;
}

static uint64_t compresso_cp2(struct compression * c_p, uint8_t * start, uint16_t ** report)
{
    return psizealigned;
}

struct compression Compreeso_Cache = {
    .next = NULL,
    .name = "compresso_cache",
    .compress = (run_compression_t)compresso_cp2
};

struct compression COMPRESSION_NODE_NAME = {
    .next = &Compreeso_Cache,
    .name = "compresso",
    .compress = (run_compression_t)compresso_cp
};

struct layout LAYOUT_NODE_NAME = {
    .next = NULL,
    .name = "",
    .L_init = (layout_init_t) compresso_init,
    .L_page_r = (layout_page_report_t) compresso_pr,
    .L_final_r = (layout_final_report_t) compresso_fr,
    .L_thread_clean_r = (layout_thread_clean_t) compresso_tcr,
    .L_clean_r = (layout_clean_t) compresso_cr,
    .reports = &COMPRESSION_NODE_NAME,
    .report_count = 0,
    .priority = -10
};