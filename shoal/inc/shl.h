/*
 * Copyright (c) 2014 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef __SHL_H
#define __SHL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>

#include <stdint.h>
#include <sys/time.h>

#ifdef BARRELFISH
#include "barrelfish.h"
#endif

#ifdef LINUX
#include "linux.h"
#endif


/*
 * -------------------------------------------------------------------------------
 * Configuration Switches
 * -------------------------------------------------------------------------------
 */

///< enables the DMA engine
#define SHL_DMA_ENABLE 1

///<
//#define ARRAY_COPY_DMA_RATION ((double)0.1)
#define ARRAY_COPY_DMA_RATION (0)

///< Whether or not to use replication (needs: indirection OR copy)
#define REPLICATION 1

///< Use NUMA aware memory allocation for replication
#define NUMA 1

///< which timer version to use
#define SHL_TIMER_USE_MULTI 1

///< Size of a cacheline (bytes)
#define CACHELINE 8

///< size of a huge page (2 MB)
#define PAGESIZE_HUGE (2*1024*1024)

///< size of a page (4 kB)
#define PAGESIZE (4*1024)

///< enabling and disabling debug output
#define SHL_DEBUG_ENABLED 1

/*
 * -------------------------------------------------------------------------------
 * Debugging Facilities
 * -------------------------------------------------------------------------------
 */
#if SHL_DEBUG_ENABLED
#define SHL_DEBUG_PRINT(x...) printf("SHL: " x);
#else
#define SHL_DEBUG_PRINT(x...);
#endif
#define SHL_DEBUG_NOPRINT(x...);

#define SHL_DEBUG_INIT(x...) SHL_DEBUG_PRINT(x)
//#define SHL_DEBUG_ALLOC(x...) SHL_DEBUG_PRINT(x)
#define SHL_DEBUG_ALLOC(x...)
#define SHL_DEBUG_ARRAY(x...) SHL_DEBUG_PRINT(x)
#define SHL_DEBUG_MEMCPY(x...)
//#define SHL_DEBUG_MEMCPY(x...) SHL_DEBUG_PRINT(x)
#define noprintf(x,...) void(0)


/*
 * -------------------------------------------------------------------------------
 * Base Units
 * -------------------------------------------------------------------------------
 */
#define BASE_UNIT 1000
#define KILO BASE_UNIT
#define MEGA (KILO*BASE_UNIT)
#define GIGA (MEGA*BASE_UNIT)
#define MAXCORES 100



#ifdef BARRELFISH
#define VERSION "1.0"
#endif

// --------------------------------------------------
// Array features
// --------------------------------------------------

// Update the names in:
#define SHL_ARR__NUM_FEAT 5
typedef enum shl__arr_feature {
    SHL_ARR_FEAT_PARTITIONING,
    SHL_ARR_FEAT_REPLICATION,
    SHL_ARR_FEAT_DISTRIBUTION,
    SHL_ARR_FEAT_LARGEPAGE,
    SHL_ARR_FEAT_HUGEPAGE
} shl_arr_feature_t;


///< pointer to the array feature table
extern const char *shl__arr_feature_table[];

// --------------------------------------------------
// Typedefs
// --------------------------------------------------
#ifdef BARRELFISH
typedef uint8_t coreid_t; /// XXX: barrelfish has uint8_t as core id
#else
typedef uint32_t coreid_t;
#ifdef __cplusplus
#include <cinttypes>
#else
#include <inttypes.h>
#endif
#define PRIuCOREID PRIu32
#endif

// --------------------------------------------------
// in misc.c
// --------------------------------------------------
double shl__end_timer(const char *label);
double shl__get_timer(void);
void shl__start_timer(int steps);
void shl__step_timer(const char *label);

unsigned long shl__timer_get_timestamp(void);

coreid_t *parse_affinity(bool);
char* get_env_str(const char*, const char*);
int get_env_int(const char*, int);
void print_number(long long);
void convert_number(long long, char *);

// --------------------------------------------------
// OS specific stuff
// --------------------------------------------------
void shl__bind_processor(int proc);
void shl__bind_processor_aff(int proc);
int shl__check_numa_availability(void);
int shl__check_num_cores(void);
void shl__set_strict_mode(int id);
int shl__get_proc_for_node(int node);
    // This is NOT the number of nodes, but returns the highest node ID
int shl__max_node(void);
long shl__node_size(int node, long *freep);
int shl__node_from_cpu(int core_id);
void* shl__malloc(size_t size, int opts, int *pagesize, int node, void **ret_mi);
void** shl__malloc_replicated(size_t size, int* num_replicas, int* pagesize, int options, void ** ret_mi);

bool shl__check_hugepage_support(void);
bool shl__check_largepage_support(void);

void loc(size_t, int, int*, void **);

// --------------------------------------------------
// SHOAL
// --------------------------------------------------
void shl__end(void);
void shl__start(void);
void shl__thread_init(void);
int  shl__get_rep_id(void);
int  shl__lookup_rep_id(int);
void shl__repl_sync(void*, void**, size_t, size_t);
void shl__init_thread(int);
void handle_error(int);
int  shl__get_num_replicas(void);
size_t shl__init(uint32_t,bool);
int  shl__num_threads(void);
int  shl__get_tid(void);
int  shl__rep_coordinator(int);
bool shl__is_rep_coordinator(int);
// --------------------------------------------------
// PAPI
// --------------------------------------------------
void papi_stop(void);
void papi_init(void);
void papi_start(void);
void papi_th_init(void);
// array helpers
// --------------------------------------------------
void** shl__copy_array(void *src, size_t size, bool is_used,
                       bool is_ro, const char* array_name);
void shl__copy_back_array(void **src, void *dest, size_t size, bool is_copied,
                          bool is_ro, bool is_dynamic, const char* array_name);
void shl__copy_back_array_single(void *src, void *dest, size_t size, bool is_copied,
                                 bool is_ro, bool is_dynamic, const char* array_name);
void *shl__alloc_struct_shared(size_t size);

unsigned long shl__calculate_crc(void *array, size_t elements, size_t element_size);

// --------------------------------------------------
// Auto-tuning interface
//
// Non of these functions are implemented. These are just meant as
// ideas for a future potential auto-tuning interface.
// --------------------------------------------------

/**
 * \brief Find sensible thread placement.
 *
 * Right now, a simple heuristic to decide how many threads to use is:
 * - 1) barriers -> one thread per physical core
 * - 2) no barriers -> one thread per HW context
 *
 * The thread placement would then be 1) to bind the threads such that
 * one thread is running on every physical core (i.e. not to have
 * threads on two HW contexts mapped to the same physical core) and 2)
 * one thread per HW context.
 *
 * But obviously, this is more complicated. Ultimately, we probably
 * want to specify an upper limit for the number of threads (maybe
 * also cores) to use (default: unlimited). Maybe also a bitmask of
 * CPUs to run on.
 *
 * The system then needs to figure out how many threads to spawn, and
 * where to place them. E.g. we know from newRTS that it is _not_
 * always good to spread out to several nodes. Somethings it is better
 * to use up all H/W threads on a node before going to the next one,
 * even if some of the threads will be hyperthreads.
 */
void shl__auto_tune_bind(int *num_cores,
                         coreid_t *bind,
                         bool uses_barriers);

extern int replica_lookup[];



// --------------------------------------------------
// Colors!
// --------------------------------------------------
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"


// --------------------------------------------------
// Defines for memory allocation
// --------------------------------------------------
#define SHL_MALLOC_NONE        (0)
#define SHL_MALLOC_HUGEPAGE    (0x1<<0)   // for GB pages
#define SHL_MALLOC_DISTRIBUTED (0x1<<1)
#define SHL_MALLOC_PARTITION   (0x1<<2)
#define SHL_MALLOC_REPLICATED  (0x1<<3)
#define SHL_MALLOC_SINGLE_NODE (0x1<<4)
#define SHL_MALLOC_LARGEPAGE   (0x1<<5)   // for MB pages

#define SHL_NUMA_IGNORE (-1)

// --------------------------------------------------
// Includes depending on configuration
// --------------------------------------------------
#include <sys/mman.h>
#include <stdint.h>

#ifdef DEBUG
static uint64_t num_lookup = 0;
#endif

#ifdef __cplusplus
}
#endif

struct array_cache {

    int rid;
    int tid;
};

#endif
