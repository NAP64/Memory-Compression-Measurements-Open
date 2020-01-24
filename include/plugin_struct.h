/*

    C header file for Memory Compression Measurement program.
    Defines compression and memory layout shared object structure and functions

    See Readme and driver.c for more details.

    By Yuqing Liu
    HEAP Lab, Virginia Tech 
    Nov 2019

*/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdlib.h>

#ifndef COMPRESSION_NODE_NAME
    #define COMPRESSION_NODE_NAME compression_node
#endif
#ifndef LAYOUT_NODE_NAME
    #define LAYOUT_NODE_NAME layout_node
#endif
#ifndef PAGE_SIZE
    #define PAGE_SIZE (4096)        //in bytes
#endif
#ifndef CACHELINE_SIZE
    #define CACHELINE_SIZE (64)     //in bytes, keep it lower than 4096 bytes if reset
#endif

#define ZERO_SIZE (65535);          //Since 8*4096 = 32768, use larger number to represent the size of a page that is filled with 0
#define ZERO_CACHELINE(s) (~s);     //Use this for cacheline filled with zero. It should be greater than 32768
#define ERROR_SIZE ((uint64_t)-1)   //If compression results in error, return this
#define IS_ZERO_CACHELINE(s) (s>32768)
#define NORM_CACHELINE(s) ( IS_ZERO_CACHELINE(s) ? (uint16_t)~s : s )

//Folder that conatins the shared objects and names of the structs in shared objects.
#define compression_folder "bin/compression"
#define compression_node_name "compression_node"
#define layout_folder "bin/layout"
#define layout_name "layout_node"

//will be defined in driver.c. Add variable that is shared between compression, memory layout, and driver here.
struct shared
{
    int validate;       //validation flag, will try to decompress and check if result is the same               default: off
    int parse_switch;   //Turn on to avoid having compressed size larger than original size                     default: on
    char * filename;
    int threads;        //Threads to use. For multithreaded memory layout calculations
    int header;         //flag to control whether a header of csv file (title of fields) needs to be printed.   default: on
};

struct compression;
//function call for compression, returns compressed size in bits. NULL on error.
typedef uint64_t (* run_compression_t) (struct compression * c_p, uint8_t * data_to_compress, uint16_t ** malloc_cacheline_report_on_demand);

//this structure will be chained as a list to be run by driver
//manually adding multiple compressions in one .so is allowed. 
//Make the first node <compression_name> and link following nodes to it. driver will automatically add all to list
struct compression //name it compression_node so driver can access it
{
    struct compression * next;  //reserved if is last node in shared object
    char * name;                //assign a string, will prints to report
    run_compression_t compress; //implement this, return in bits
    pthread_mutex_t slock;      //reserved
    uint64_t size;              //reserved
    struct shared * sharedv;    //reserved for shared variables
    uint16_t * page_report;     //reserved, will hold compressed page size in bits.
};

//current layout only perform calculations

//initialize layout program. i.e. see if required compressions present in list
typedef void (* layout_init_t) (struct compression ** begin_of_linked_list);
//gather data from compression. will be called after every single page is comrpessed in every compression
//notice: this is multithreaded. Use locks and per-thread objects
typedef void (* layout_page_report_t) (struct compression * c_p, uint16_t cacheline_report[PAGE_SIZE/CACHELINE_SIZE], uint16_t page_size);
//This gives all compression with page reports to layout.
//You can perform essential calculations here. There is no other thread runninng with this call
//notice: This is not multithreaded, implement your own multithreaded program to speed up
typedef void (* layout_final_report_t) (struct compression * begin_of_linked_list, uint64_t totalpages);
//Clean up before a thread exit. 
typedef void (* layout_thread_clean_t) ();
//Clean up before you leave. You can also print measurement of layout here
typedef void (* layout_clean_t) ();

//Hint: Use seperators other than "," in layouts results to allow easier text processing

struct layout
{
    struct layout * next;               //reserved if is last node in shared object
    char * name;                        //assign a string, will prints to report
    int priority;                       //higher will run earlier. i.e. granurity smaller than best-of
    layout_init_t L_init;               //implement this. Will run in priority order
    layout_page_report_t L_page_r;      //implement this. Will run in parallel with compressions
    layout_final_report_t L_final_r;    //implement this. Will run after compressions are done and before redsult is printed
    layout_thread_clean_t L_thread_clean_r;    //implement this. Will run in the end before each thread exits
    layout_clean_t L_clean_r;           //implement this. Will run in the end before exit
    struct compression * reports;       //dummy reports to print in results alongside with compressions
    int report_count;                   //count of dummy reports
    struct shared * sharedv;            //reserved for shared variables
};
