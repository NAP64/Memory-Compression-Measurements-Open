/*

    A memory layout which only count the smallest compressed size among compressions in list
    Calculation of cacheline reportable compresions will be conducted on the cacheline level
    then combined to compare with page-level compressions.

    A seperate report will be produced in the end for compressed size for each part

    By Yuqing Liu
    HEAP Lab, Virginia Tech 
    Nov 2019

*/

#include <string.h>
#include <inttypes.h>

#include <plugin_struct.h>
#define NAME_LIST {"bpc", "lz4"}
#define NAME "best-of"
#define LIST_LEN (2)

struct layout LAYOUT_NODE_NAME;
struct compression COMPRESSION_NODE_NAME;

char* name_list[] = NAME_LIST;
struct compression * c_list[LIST_LEN];
int run;
__thread int init = 0;
__thread uint16_t csize[PAGE_SIZE/CACHELINE_SIZE];
__thread uint16_t cindex[PAGE_SIZE/CACHELINE_SIZE];
__thread uint16_t psize;
__thread uint16_t pindex;
uint64_t portion_report[LIST_LEN];
pthread_mutex_t rlock;

static void bo_fr()
{   return;}

static void bo_tcr()
{   return;}

//Initialize with latest matching string from list
//allows taking data from other compression's result
static void bo_init(struct compression ** c_p)
{
    int i;
    for (i = 0; i < LIST_LEN; i++)
        c_list[i] = NULL;
    run = 0;
    if (*c_p == NULL)
        return;
    struct compression * p;
    for (p = *c_p; p != NULL; p = p ->next)
        for (i = 0; i < LIST_LEN; i++)
            if (!strcmp(p->name, name_list[i]))
                c_list[i] = p;
    for (i = 0; i < LIST_LEN; i++)
        if (c_list[i] == NULL)
            return;
    run = 1;
    for (p = *c_p; p->next != NULL; p = p ->next);
    p->next = &COMPRESSION_NODE_NAME;
    LAYOUT_NODE_NAME.reports = &COMPRESSION_NODE_NAME;
    LAYOUT_NODE_NAME.report_count = 1;
    rlock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    for (i = 0; i < LIST_LEN; i++)
        portion_report[i] = 0;
    return;
}

static void bo_pr(struct compression * c_p, uint16_t * cl_list, uint16_t page_size)
{
    if (!run)
        return;
    int i, j;
    if (!init)
    {
        init = 1;
        for (i = 0; i < PAGE_SIZE/CACHELINE_SIZE; i++)
            cindex[i] = LIST_LEN;
        pindex = LIST_LEN;
    }
    for (i = 0; i < LIST_LEN; i++)
        if (c_list[i] == c_p)
            break;
    if (i >= LIST_LEN)
        return;
    if (cl_list != NULL)
    {
        for (j = 0; j < PAGE_SIZE/CACHELINE_SIZE; j++)
            if (cindex[j] == LIST_LEN || csize[j] > NORM_CACHELINE(cl_list[j]))
            {
                csize[j] = NORM_CACHELINE(cl_list[j]);
                cindex[j] = i;
            }
    }
    else
        if (pindex == LIST_LEN || page_size < psize)
        {
            pindex = i;
            psize = page_size;
        }
}

//creates another report
static void bo_cr()
{
    if (!run)
        return;
    pthread_mutex_destroy(&rlock);
    int i;
    for (i = 0; i < LIST_LEN; i++)
        printf("%s, ", name_list[i]);
    printf("\n");
    for (i = 0; i < LIST_LEN; i++)
        printf("%"PRIu64", ", portion_report[i]);
    printf("\n");
}

static uint64_t bo_cp(struct compression * c_p, uint8_t * start, uint16_t ** report)
{
    uint16_t cpsize = 0;
    int i;
    for (i = 0; i < PAGE_SIZE/CACHELINE_SIZE; i++)
    {
        cpsize += csize[i];
    }
    uint64_t ret = 0;
    if (pindex == LIST_LEN || (cindex[0] != LIST_LEN && cpsize < psize))
    {
        ret = cpsize;
        pthread_mutex_lock(&rlock);
        for (i = 0; i < PAGE_SIZE/CACHELINE_SIZE; i++)
            portion_report[cindex[i]]++;
        pthread_mutex_unlock(&rlock);
    }
    else
    {
        ret = psize;
        pthread_mutex_lock(&rlock);
        portion_report[pindex] += PAGE_SIZE/CACHELINE_SIZE;
        pthread_mutex_unlock(&rlock);
    }
    if (pindex == LIST_LEN)
    {
        *report = calloc(sizeof(uint16_t), PAGE_SIZE / CACHELINE_SIZE);
        for (i = 0; i < PAGE_SIZE / CACHELINE_SIZE; i++)
            (*report)[i] = csize[i];
    }
    init = 0;
    return ret;
}

struct layout LAYOUT_NODE_NAME = {
    .next = NULL,
    .name = "best-of",
    .L_init = (layout_init_t) bo_init,
    .L_page_r = (layout_page_report_t) bo_pr,
    .L_final_r = (layout_final_report_t) bo_fr,
    .L_thread_clean_r = (layout_thread_clean_t) bo_tcr,
    .L_clean_r = (layout_clean_t) bo_cr,
    .reports = NULL,
    .report_count = 0,
    .priority = 0
};

struct compression COMPRESSION_NODE_NAME = {
    .next = NULL,
    .name = NAME,
    .compress = (run_compression_t)bo_cp
};