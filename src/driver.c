/*

    Memory Compression Measurement program.
    A multithreaded program that loads shared compression objects and execute
    compressions and simulate memory layouts in parallel to gather compression data
    for different compression algorithms and memory layouts.

    See Readme for usage

    By Yuqing Liu
    HEAP Lab, Virginia Tech 
    Feb 2019
    
*/

#include <string.h>
#include <semaphore.h>
#include <getopt.h>
#include <sys/types.h>
#include <dlfcn.h>
#include <limits.h>
#include <termios.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <inttypes.h>
#include <math.h>
#include <elf.h>

#include <plugin_struct.h>

//Size of slices of memoory dump for threads to run.
//It sould be small enough to ultilize multiprocessor,
//but not too small to introduce too much overhead.
#define BLOCK (1*1024*PAGE_SIZE)

static sem_t thread_ctrl;
//Start of compression list
static struct compression * compressionp = NULL;
//Next-to end of compression list. Compression structures added by layouts start here
static struct compression * compressione = NULL;
//Start of layout list
static struct layout * layoutp = NULL;
//count of zero pages. Set -z to disable
static int64_t zero_count;
static pthread_mutex_t zero_lock;
//-z flag
static int zero_switch;
//shared structure between all layouts and compressions.
static struct shared * sh;

#ifdef TIME
clock_t g_clock;
static pthread_mutex_t clock_lock;
#endif

//Prints error message and quit the program
static void errorlog(char * errmsg)
{
    fprintf(stderr, "%s\n", errmsg);
    exit(EXIT_FAILURE);
}

/*
    Uses elf.h to read and parse dump file.
    Automatically revert to parsed file if the input file is not an elf file.
    Looks for start and end of program section(page-aligned memory section) and uses it for compression.

    This always assumes 4096byte page size for real-word dump files.
*/
static void auto_elf_parse(char * fn, uint64_t * start, uint64_t * end)
{
    Elf64_Ehdr header;
    FILE * ef = fopen(fn, "rb");
    if (!ef)
        errorlog("elf parse failed at open file");
    if (fread(&header, 1, sizeof(header), ef) <= 0)
        errorlog("elf parse failed at read");
    *start = 0;
    if (!!memcmp(header.e_ident, ELFMAG, SELFMAG))
        //assume parsed
        *end = -1;
    else
    {
        *end = header.e_shoff; // end of program section. start of next section
        *start = 0;
        fseek(ef, header.e_phoff, SEEK_SET);
        int i;
        for (i = 0; i < header.e_phnum; i++) // Find the first aligned program section
        {
            Elf64_Phdr pHdr;
            if (fread(&pHdr, 1, sizeof(Elf64_Phdr), ef) <= 0)
                errorlog("elf parse failed at reading psect");
            if (pHdr.p_memsz != 0 && (pHdr.p_memsz & (0xfff)) == 0)
            {
                *start = pHdr.p_offset;
                break;
            }
        }
    }
    fseek(ef, 0L, SEEK_END);
    uint64_t cap = ftell(ef);
    if (cap < *end)
        //for elf file that failed half way generating
        *end = cap;
    fclose(ef);
    *end = *start + ((*end - *start) & ~0xfff);
}

/*
    Multithreaded function that performes compression with compression nodes and provide data to simulate layouts.
    results are added to compressions and global variables. no return value
*/
static void * run_compress(void * block)
{
    uint8_t * file = *(uint8_t **)block;                    //file pointer with offset
    uint64_t cur, size = *((uint64_t *)block + 1);          //slice length for this thread to measure
    uint64_t index = *((uint64_t *)block + 2) / PAGE_SIZE;  //index of page_report
    int zeroc = 0;
    //iterate through slice, page by page
    for (cur = 0; cur < size; cur += PAGE_SIZE)
    {
        int j, i = 0;
        uint8_t c_map[PAGE_SIZE/CACHELINE_SIZE];
        if (zero_switch)
        {
            for (i = 0; i < PAGE_SIZE; i += CACHELINE_SIZE)
            {
                for (j = 0; j < CACHELINE_SIZE; j++)
                    if ((file + cur + i)[j] != 0)
                        j = CACHELINE_SIZE + 1;
                c_map[i/CACHELINE_SIZE] = j == CACHELINE_SIZE ? 1: 0;
            }
            for (i = 0; i < PAGE_SIZE/CACHELINE_SIZE; i++)
                i = !(c_map[i]) ? PAGE_SIZE/CACHELINE_SIZE + 1 : i;
            zeroc += (i == PAGE_SIZE/CACHELINE_SIZE);
        }
        struct compression * p;
        for (p = compressionp; p != NULL; p = p->next)
        {
            if (zero_switch && i == PAGE_SIZE/CACHELINE_SIZE) //zero page. fill page report entry by ZERO_SIZE
            {
                if (layoutp != NULL)
                    p->page_report[index + cur / PAGE_SIZE] = ZERO_SIZE;
                continue;
            }
            uint16_t * cachereport = NULL;
            #ifdef TIME
            clock_t time_clock = clock();
            #endif
            uint64_t result = p->compress(p, file + cur, &cachereport); //get compression result
            #ifdef TIME
            time_clock = clock() - time_clock;
            pthread_mutex_lock(&(clock_lock));
            g_clock += time_clock;
            pthread_mutex_unlock(&(clock_lock));
            #endif
            if (result == ERROR_SIZE) // on error
            {
                printf("at %"PRIu64"\n", index + cur / PAGE_SIZE);
                errorlog("compression/validation failed");
            }
            if (sh->parse_switch)
                result = result > PAGE_SIZE * 8 ? PAGE_SIZE * 8 : result;
            if (cachereport != NULL)
                for (j = 0; j < PAGE_SIZE/CACHELINE_SIZE; j++)
                    if (c_map[j])
                        cachereport[j] = ZERO_CACHELINE(cachereport[j]);
            struct layout * lp = layoutp;
            for (lp = layoutp; lp != NULL; lp = lp->next)
                lp->L_page_r(p, cachereport, result);
            if (cachereport != NULL)
                free(cachereport);
            pthread_mutex_lock(&(p->slock));
            p->size += result;
            pthread_mutex_unlock(&(p->slock));
            if (layoutp != NULL)
                p->page_report[index + cur / PAGE_SIZE] = result;
        }
    }
    if (zero_switch)
    {
        pthread_mutex_lock(&zero_lock);
        zero_count += zeroc;
        pthread_mutex_unlock(&zero_lock);
    }
    sem_post(&thread_ctrl);
    pthread_detach(pthread_self());
    free(block);
    struct layout * lp = layoutp;
    for (lp = layoutp; lp != NULL; lp = lp->next)
        lp->L_thread_clean_r();
    return NULL;
}

//insert layout to layout list according to priority
static void layout_insert(struct layout * l)
{
    if (layoutp == NULL || l->priority >= layoutp->priority)
    {
        l->next = layoutp;
        layoutp = l;
    }
    else
    {
        struct layout * prev, * next;
        for (prev = layoutp; ; prev = prev->next)
        {
            next = prev->next;
            if (next == NULL || l->priority >= next->priority)
            {
                prev->next = l;
                l->next = next;
                break;
            }
        }
    }
}

//Loads layouts and compressions from .so files
static void load_initialize_compressions(uint64_t pg_count, int load_layouts)
{
    DIR * dir = opendir(compression_folder);
    if (dir == NULL)
        return;
    struct dirent * dentry;
    void *handle;
    compressionp = NULL;
    compressione = NULL;
    layoutp = NULL;
    struct compression * cp;
    struct layout * tl;
    while ((dentry = readdir(dir)) != NULL)
    {
        if (!strstr(dentry->d_name, ".so"))
            continue;
        char modname[PATH_MAX + 1];
        snprintf(modname, sizeof modname, "./%s/%s", compression_folder, dentry->d_name);
        handle = dlopen(modname, RTLD_LAZY);
        if (handle == NULL)
        {
            printf("can't open %s:%s\n", modname, dlerror());
            errorlog("error in loading compression");
        }
        struct compression * p = dlsym(handle, compression_node_name);
        if (p == NULL)
        {
            printf("can't open node in %s:%s\n", modname, dlerror());
            dlclose(handle);
            errorlog("error in loading compression");
        }
        cp = p;
        while (cp->next != NULL)
            cp = cp->next;
        cp->next = compressionp;
        if (compressione == NULL)
            compressione = cp;  //record tail
        compressionp = p;       //add to top
    }
    closedir(dir);
    if (load_layouts)
    {
        dir = opendir(layout_folder);
        if (dir != NULL)
        {
            while ((dentry = readdir(dir)) != NULL)
            {
                if (!strstr(dentry->d_name, ".so"))
                    continue;
                char modname[PATH_MAX + 1];
                snprintf(modname, sizeof modname, "./%s/%s", layout_folder, dentry->d_name);
                handle = dlopen(modname, RTLD_LAZY);
                if (handle == NULL)
                {
                    printf("can't open %s:%s\n", modname, dlerror());
                    errorlog("error in loading memory layout");
                }
                struct layout * p = dlsym(handle, layout_name);
                if (p == NULL)
                {
                    printf("can't open node in %s:%s\n", modname, dlerror());
                    dlclose(handle);
                    errorlog("error in loading memory layout");
                }
                while (p != NULL)
                {
                    tl = p;
                    p = p->next;
                    tl->next = NULL;
                    layout_insert(tl);
                }
            }
            closedir(dir);
        }
    }
    //initialize layouts first
    for (tl = layoutp; tl != NULL; tl = tl->next)
    {
        tl->sharedv = sh;
        tl->L_init(&compressionp);
    }
    //initialize compressions and layout dummy compressions
    for (cp = compressionp; cp != NULL; cp = cp->next)
    {
        cp->slock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
        cp->size = 0;
        cp->sharedv = sh;
        if (layoutp != NULL)
            cp->page_report = calloc(sizeof(uint16_t), pg_count);
    }
    if (compressione != NULL)
        compressione = compressione->next; // next to tail
    else
        compressione = compressionp;
    
}

//prints usage and quit
static void usage(char * name, char * errmsg)
{
    printf("Usage: %s [-f filename] [-n thread_count] [-v] [-z] [-p] [-h] [-l] [-a]\n", name);
    printf("Where -n thread count, default is 4\n");
    printf("      -v is for validation (check decompression).\n");
    printf("      -z if you want to include zero pages in calculation.\n");
    printf("      -p will allow compressed size to be larger than compressed unit.\n");
    printf("      -h removes report header.\n");
    printf("      -l run without memory layouts\n");
    printf("      -a print compressed size in bits. default is ratio\n");

    if (errmsg != NULL)
        errorlog(errmsg);
    exit(EXIT_FAILURE);
}

//Main function.
int main(int argc, char ** argv)
{
    int opt;
    sh = malloc(sizeof(struct shared));
    sh->filename = NULL;
    sh->validate = 0;
    sh->threads = 4;
    zero_switch = 1;
    sh->parse_switch = 1;
    sh->header = 1;
    int actual_size = 0;
    int load_layouts = 1;
    while ((opt = getopt(argc, argv, "hpvf:n:zla")) != -1)
        switch (opt) {
            case 'l':
                load_layouts = 0;
                break;
            case 'h':
                sh->header = 0;
                break;
            case 'p':
                sh->parse_switch = 0;
                break;
            case 'f':
                sh->filename = optarg;
                break;
            case 'n':
                sh->threads = strtol(optarg, NULL, 0);
                break;
            case 'v':
                sh->validate = 1;
                break;
            case 'z':
                zero_switch = 0;
                break;
            case 'a':
                actual_size = 1;
                break;
            default:
                usage(argv[0], NULL);
        }
    if (sh->filename == NULL)
        usage(argv[0], "Filename required.");
    if (sh->threads <= 0)
        usage(argv[0], "thread count invalid");
    
    //parse and load file and shared objects
    uint64_t start, cur, size;
    auto_elf_parse(sh->filename, &start, &size);
    cur = start;
    int fd = open(sh->filename, O_RDONLY);
    if (fd < 0) usage(argv[0], "Cannot open file.");

    #ifdef TIME
    g_clock = 0;
    clock_lock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    #endif
    load_initialize_compressions((size - cur) / PAGE_SIZE, load_layouts);
    uint8_t * file = mmap(0, size, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0);
    //ready for compressions
    sem_init(&thread_ctrl, 0, sh->threads);
    zero_count = 0;
    pthread_t tid;
    while (cur < size)
    {
        sem_wait(&thread_ctrl);
        uint64_t * block = malloc(sizeof(uint64_t) * 3);
        block[0] = (uint64_t)file + cur;
        block[1] = BLOCK > (size - cur) ? (size - cur) : BLOCK;
        block[2] = cur - start;
        pthread_create(&tid, NULL, run_compress, block);
        cur += BLOCK;
    }
    int i;
    for (i = 0; i < sh->threads; i++)
        sem_wait(&thread_ctrl);
    //print report
    struct compression * p;
    struct layout * lp;
    for (lp = layoutp; lp != NULL; lp = lp->next)
        lp->L_final_r(compressionp, (size - start) / PAGE_SIZE);
    if (sh->header)
    {
        printf("file name,file size,elf,");
        if (zero_switch)
            printf("zero pages,");
        for (p = compressionp; p != compressione; p = p-> next)
            printf("%s,", p->name);
        for (lp = layoutp; lp != NULL; lp = lp->next)
        {
            struct compression * lmp = lp->reports;
            int j;
            for (j = 0; j < lp->report_count; j++)
            {
                printf("%s_%s,", lp->name, lmp->name);
                lmp = lmp->next;
            }
        }
        printf("\n");
    }
    printf("%s,%"PRIu64",%c,", sh->filename, size - start, start == 0 ? 'p' : 'e');
    if (zero_switch)
        printf("%"PRIu64",", zero_count);
    uint64_t actualsize = ((size - start) - zero_count * PAGE_SIZE) * 8;
    for (p = compressionp; p != compressione; p = p->next)
    {
        if (actual_size)
            printf("%"PRIu64",", p->size);
        else
        {
            double td = actualsize;
            td = td / p->size;
            printf("%lf,", td);
        }
        if (layoutp != NULL)
            free(p->page_report);
    }
    for  (lp = layoutp; lp != NULL; lp = lp->next)
    {
        struct compression * lmp = lp->reports;
        int j;
        for (j = 0; j < lp->report_count; j++)
        {
            if (actual_size)
                printf("%"PRIu64",", lmp->size);
            else
            {
                double td = actualsize;
                td = td / lmp->size;
                printf("%lf,", td);
            }
            free(lmp->page_report);
            lmp = lmp->next;
        }
    }
    printf("\n");
    lp = layoutp;
    while (lp != NULL)
    {
        lp->L_clean_r();
        lp = lp->next;
    }
    free(sh);
    #ifdef TIME
    double sec = ((double)(g_clock))/CLOCKS_PER_SEC;
    printf("Time spent on compress: %lf\n", sec);
    pthread_mutex_destroy(&clock_lock);
    #endif
}
