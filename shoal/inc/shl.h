#ifndef __SHL_H
#define __SHL_H

#include <numa.h>
#include <assert.h>
#include <papi.h>
#include "gm.h"

#include <cstdio>
#include <omp.h>

static int EventSet;

#define KILO 1000
#define MEGA (KILO*1000)
#define GIGA (MEGA*1000)
#define MAXCORES 100

#define PAGESIZE (2*1024*1024)

// --------------------------------------------------
// Configuration
// --------------------------------------------------

// Use indirection: rather than accessing arrays directly, there is
// one additional lookup
#define INDIRECTION

// Copies data using shl__copy_array, but does not replicate
//#define COPY

// Whether or not to use replication (needs: indirection OR copy)
#define REPLICATION

// Use NUMA aware memory allocation for replication
#define NUMA

// This does not seem to be currently used
//#define LOOKUP

// Add some additional debug checks! This will harm performance a LOT.
//#define DEBUG

// Enable PAPI support
#define PAPI

// Enable support for hugepages
//#define ENABLE_HUGEPAGE

// --------------------------------------------------
// in misc.c
// --------------------------------------------------
void shl__start_timer(void);
double shl__end_timer(void);
double shl__get_timer(void);
int numa_cpu_to_node(int);

// --------------------------------------------------
// OS specific stuff
// --------------------------------------------------
void shl__bind_processor(int proc);
int shl__get_proc_for_node(int node);

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
// Includes depending on configuration
// --------------------------------------------------
#include <sys/mman.h>

#ifdef DEBUG
static uint64_t num_lookup = 0;
#endif

static void print_number(long long number)
{
    if (number>GIGA)
        printf("%.2f G", number/((double) GIGA));
    else if (number>MEGA)
        printf("%.2f M", number/((double) MEGA));
    else if (number>KILO)
        printf("%.2f K", number/((double) KILO));
    else
        printf("%lld", number);
}

static void handle_error(int retval)
{
    printf("PAPI error %d: %s\n", retval, PAPI_strerror(retval));
    exit(1);
}

static void shl__end(void)
{
    printf("Time for copy: %.6f\n", shl__get_timer());
#ifdef DEBUG
    printf("Number of lookups: %ld\n", num_lookup);
#endif
}

static void papi_stop(void)
{
#ifdef PAPI
    // Stop PAPI events
    long long values[1];
    if (PAPI_stop(EventSet, values) != PAPI_OK) handle_error(1);
    printf("Stopping PAPI .. \n");
    print_number(values[0]); printf("\n");
    printf("END PAPI\n");
#endif
}

static void shl__init(void)
{
    printf("SHOAL (v %s) initialization .. ", VERSION);
    printf("done\n");

    assert (numa_available()>=0);

    for (int i=0; i<MAXCORES; i++)
        replica_lookup[i] = -1;

    for (int i=0; i<gm_rt_get_num_threads(); i++) {

        replica_lookup[i] = numa_cpu_to_node(i);
        printf("replication: CPU %d is on node %d\n", i, replica_lookup[i]);
    }

    // Prevent numa_alloc_onnode fall back to allocating memory elsewhere
    numa_set_strict(true);

    /* for (int i=0; i<=numa_max_node(); i++) { */

    /*     numa_node_size( */
    /* } */

#ifdef DEBUG
    printf(ANSI_COLOR_RED "WARNING:" ANSI_COLOR_RESET " debug enabled\n");
#endif

#ifdef COPY
    printf("[x] Copy\n");
#else
    printf("[ ] Copy\n");
#endif
#ifdef INDIRECTION
    printf("[x] Indirection\n");
#else
    printf("[ ] Indirection\n");
#endif
#ifdef REPLICATION
    printf("[x] Replication\n");
#else
    printf("[ ] Replication\n");
#endif
#ifdef LOOKUP
    printf("[x] Lookup\n");
#else
    printf("[ ] Lookup\n");
#endif
#ifdef ENABLE_HUGEPAGE
    printf("[x] Hugepage\n");
#else
    printf("[ ] Hugepage\n");
#endif

#ifdef NUMA
    printf("alloc: libnuma\n");
#else
#ifdef ARRAY
    printf("alloc: C++ array\n");
#else
    printf("alloc: malloc\n");
#endif
#endif
}

static void papi_start(void) {
#ifdef PAPI
    printf("Initializing PAPI .. ");

    // Initialize PAPI and make sure version matches
    int retval = PAPI_library_init(PAPI_VER_CURRENT);
    if (retval != PAPI_VER_CURRENT && retval > 0) { fprintf(stderr,"PAPI library version mismatch!\n"); exit(1); }

    // More checks that PAPI is running?
    if (retval < 0) handle_error(retval);
    retval = PAPI_is_initialized();
    if (retval != PAPI_LOW_LEVEL_INITED) handle_error(retval);

    if (PAPI_thread_init(pthread_self) != PAPI_OK)
        exit(1);

    printf("DONE\n");

    printf("Starting PAPI .. ");

    // Add events to be monitored
    EventSet = PAPI_NULL;
    if (PAPI_create_eventset(&EventSet) != PAPI_OK) handle_error(1);
    if (PAPI_add_event(EventSet, PAPI_TLB_DM) != PAPI_OK) handle_error(1);

    // Start monitoring
    if (PAPI_start(EventSet) != PAPI_OK)
        handle_error(1);
    printf("DONE\n");
#endif
}

static bool dbg_done = false;
static inline int shl__get_rep_id(void)
{
/* #ifdef DEBUG */
/*     __sync_fetch_and_add(&num_lookup, 1); */
/* #endif */
/*     int d = omp_get_thread_num(); */
/*     int m = numa_max_node(); */
/* #if DEBUG */
/*     assert (m<omp_get_num_threads()); */
/* #endif */
/*     return (d) % (m+1); */
#ifdef DEBUG
    assert(omp_get_num_threads()<MAXCORES);
    __sync_fetch_and_add(&num_lookup, 1);
    assert(replica_lookup[omp_get_thread_num()]>=0);
#endif
    return replica_lookup[omp_get_thread_num()];
}

static int shl__get_num_replicas(void)
{
    return numa_max_node()+1;
}

static void** shl__copy_array(void *src, size_t size, bool is_used,
                              bool is_ro, const char* array_name)
{
    bool replicate = is_ro;
    int num_replicas = replicate ? shl__get_num_replicas() : 1;
#ifndef REPLICATION
    num_replicas = 1;
#endif

    //    printf(ANSI_COLOR_RED "Warning: " ANSI_COLOR_RESET "malloc for rep1\n");
    printf("array: [%-30s] copy [%c] -- replication [%c] (%d) -- ", array_name,
           is_used ? 'X' : ' ', is_ro ? 'X' : ' ', num_replicas);

    bool omp_copy = true;
    void **tmp = (void**) (malloc(num_replicas*sizeof(void*)));

    for (int i=0; i<num_replicas; i++) {
#ifdef NUMA
        if (replicate && num_replicas>1) {
#ifdef ENABLE_HUGEPAGE
            // Make size be alligned multiple of page size
            int alloc_size = size;
            while (alloc_size % PAGESIZE != 0)
                alloc_size++;

            printf("mmap(size=0x%x)\n", alloc_size);
            tmp[i] = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE,
                          MAP_ANONYMOUS | MAP_PRIVATE | MAP_HUGETLB, -1, 0);
            if (tmp[i]==MAP_FAILED) {
                perror("mmap");
                exit(1);
            }

            //            omp_copy = false;

            cpu_set_t cpu_mask_org;
            int err = sched_getaffinity(0, sizeof(cpu_set_t), &cpu_mask_org);
            if (err) {
                perror("sched_getaffinity");
                exit(1);
            }

            // bind processor
            shl__bind_processor(shl__get_proc_for_node(i));

            // copy
            for (int j=0; j<size; j+=PAGESIZE)
                *((char*)(tmp[i])+j) = *((char*)src+j);

            // move processor back to original mask
            err = sched_setaffinity(0, sizeof(cpu_set_t), &cpu_mask_org);
            if (err) {
                perror("sched_setaffinity");
                exit(1);
            }
#else
            tmp[i] = numa_alloc_onnode(size, i);
            printf("numa_alloc_onnode(%d) ", i);
#endif
        } else {
            // If data is not replicated, still copy, but don't specify node
            // Our allocation function will spread the data in the machine
            int alloc_size = size;
            while (alloc_size % PAGESIZE != 0)
                alloc_size++;
            int flags = MAP_ANONYMOUS | MAP_PRIVATE;
#ifdef ENABLE_HUGEPAGE
            flags |= MAP_HUGETLB;
#endif
            tmp[i] = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE,
                          flags, -1, 0);
            if (tmp[i]==MAP_FAILED) {
                perror("mmap");
                exit(1);
            }


        }
#else
#ifdef ARRAY
        tmp[i] = new double[size/8];
        printf("new ");
#else
        tmp[i] = malloc(size);
        printf("malloc ");
#endif
#endif
        assert(tmp[i]!=NULL);
    }
    printf("\n");

    assert(sizeof(char)==1);
    assert(tmp!=NULL);
    if (is_used && omp_copy) {
        for (int i=0; i<num_replicas; i++) {
            #pragma omp parallel for
            for (int j=0; j<size; j++)
                *((char*)(tmp[i])+j) = *((char*)src+j);
        }
    }
    return tmp;
}

static void shl__copy_back_array(void **src, void *dest, size_t size, bool is_copied,
                                 bool is_ro, bool is_dynamic, const char* array_name)
{
    bool copy_back = true;
    int num_replicas = is_ro ? shl__get_num_replicas() : 1;

#ifndef REPLICATION
    num_replicas = 1;
#endif

    // read-only: don't have to copy back, data is still the same
    if (is_ro)
        copy_back = false;

    // dynamic: array was created dynamically in GM algorithm function,
    // no need to copy back
    if (is_dynamic)
        copy_back = false;

    printf("array: [%-30s] -- copied [%c] -- copy-back [%c] (%d)\n",
           array_name, is_copied ? 'X' : ' ', copy_back ? 'X' : ' ',
           num_replicas);

    if (copy_back) {
        // replicated data is currently read-only (consistency issues)
        // so everything we have to copy back is not replicated
        assert (num_replicas == 1);

        for (int i=0; i<num_replicas; i++)
            memcpy(dest, src[0], size);
    }
}

static void shl__copy_back_array_single(void *src, void *dest, size_t size, bool is_copied,
                                        bool is_ro, bool is_dynamic, const char* array_name)
{
    bool copy_back = true;
    int num_replicas = is_ro ? shl__get_num_replicas() : 1;

#ifndef REPLICATION
    num_replicas = 1;
#endif

    // read-only: don't have to copy back, data is still the same
    if (is_ro)
        copy_back = false;

    // dynamic: array was created dynamically in GM algorithm function,
    // no need to copy back
    if (is_dynamic)
        copy_back = false;

    printf("array: [%-30s] -- copied [%c] -- copy-back [%c] (%d)\n",
           array_name, is_copied ? 'X' : ' ', copy_back ? 'X' : ' ',
           num_replicas);

    if (copy_back) {
        // replicated data is currently read-only (consistency issues)
        // so everything we have to copy back is not replicated
        assert (num_replicas == 1);

        for (int i=0; i<num_replicas; i++)
            memcpy(dest, src, size);
    }
}

#endif
