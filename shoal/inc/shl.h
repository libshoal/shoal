#ifndef __SHL_H
#define __SHL_H

#include <numa.h>
#include <assert.h>
#include <papi.h>

#include <cstdio>
#include <omp.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>

static int EventSet;

#define KILO 1000
#define MEGA (KILO*1000)
#define GIGA (MEGA*1000)
#define MAXCORES 100

// --------------------------------------------------
// Hardcoded page sizes
// --------------------------------------------------
#define PAGESIZE_HUGE (2*1024*1024)
#define PAGESIZE (4*1024)

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
#define ENABLE_HUGEPAGE

// --------------------------------------------------
// Typedefs
// --------------------------------------------------
typedef uint32_t coreid_t;

// --------------------------------------------------
// in misc.c
// --------------------------------------------------
double shl__end_timer(void);
double shl__get_timer(void);
void shl__start_timer(void);
void shl__init(size_t);
int numa_cpu_to_node(int);
coreid_t *parse_affinity(bool);
void shl__init_thread(void);

// --------------------------------------------------
// OS specific stuff
// --------------------------------------------------
void shl__bind_processor(int proc);
void shl__bind_processor_aff(int proc);
int shl__get_proc_for_node(int node);

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

/**
 *\ brief Allocate array
 *
 * Several choices here:
 *
 * - replication: Three policies:
 * 1) Replica if read-only.
 * 2) Replica if w/r ratio below a certain threshold.
 *  2.1) Write to all replicas
 *  2.2) Write locally; maintain write-set; sync call when updates need to
 *       be propagated.
 *
 * - huge pages: Use if working set < available RAM && alloc_real/size < 1.xx
 *   ? how to determine available RAM?
 *   ? how to know the size of the working set?
 *
 */
//shl_array shl__malloc(size_t size, bool is_ro);

extern int replica_lookup[];

// --------------------------------------------------
// Timer
// --------------------------------------------------
class Timer {
 public:
    void start();
    double stop();
    double timer_secs = 0.0;
 private:
    struct timeval TV1, TV2;
};

// --------------------------------------------------
// Program configuration
// --------------------------------------------------
class Configuration {

 public:
    Configuration(void) {

        // Configuration based on environemnt
        use_hugepage = get_env_int("SHL_HUGEPAGE", 1);
        use_replication = get_env_int("SHL_REPLICATION", 1);

        // NUMA information
        num_nodes = numa_max_node();
        node_mem_avail = new long[num_nodes];
        mem_avail = 0;

        for (int i=0; i<=numa_max_node(); i++) {

            numa_node_size(i, &(node_mem_avail[i]));
            mem_avil += node_mem_avail[i];
        }
    }

    ~Configuration(void) {

        delete node_mem_size;
    }

    // Should large pages be used
    bool use_hugepage;

    // Should replication be used
    bool use_replication;

    // Number of NUMA nodes
    int num_nodes;

    // How much memory is available on the machine
    long mem_avil;

    // How much memory is available on each node
    long node_mem_avail[];
};

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
#include <stdint.h>

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

#ifdef PAPI
static void papi_stop(void)
{
    // Stop PAPI events
    long long values[1];
    if (PAPI_stop(EventSet, values) != PAPI_OK) handle_error(1);
    printf("Stopping PAPI .. ");
    print_number(values[0]); printf("\n");
    printf("END PAPI\n");
}

static void papi_init(void)
{
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
}

static void papi_start(void)
{
    printf("Starting PAPI .. ");

    // Add events to be monitored
    EventSet = PAPI_NULL;
    if (PAPI_create_eventset(&EventSet) != PAPI_OK) handle_error(1);
    if (PAPI_add_event(EventSet, PAPI_L2_DCM) != PAPI_OK) handle_error(1);

    // PAPI_TLB_DM

    // Start monitoring
    if (PAPI_start(EventSet) != PAPI_OK)
        handle_error(1);
    printf("DONE\n");
}
#endif

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
    //  if (is_ro)
    //  copy_back = false;

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
        //  assert (num_replicas == 1);

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

#define SHL_MALLOC_NONE (0)
#define SHL_MALLOC_HUGEPAGE (0x1<<0)
static void* shl__malloc(size_t size, int opts, int *pagesize)
{
    void *res;
    bool use_hugepage = opts & SHL_MALLOC_HUGEPAGE;

    // Round up to next multiple of page size
    *pagesize = use_hugepage ? PAGESIZE_HUGE : PAGESIZE;
    int alloc_size = size;
    while (alloc_size % *pagesize != 0)
        alloc_size++;

    // Set options for mmap
    int options = MAP_ANONYMOUS | MAP_PRIVATE;
    if (use_hugepage)
        options |= MAP_HUGETLB;

    printf("alloc: %zu, huge=%d\n", alloc_size, use_hugepage);

    // Allocate
    res = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, options, -1, 0);
    if (res==MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    return res;
}

static void** shl__malloc_replicated(size_t size,
                                     int* num_replicas,
                                     int options)
{
    *num_replicas = shl__get_num_replicas();
    int pagesize;

    void **tmp = (void**) (malloc(*num_replicas*sizeof(void*)));

    for (int i=0; i<*num_replicas; i++) {

        // Allocate memory
        // --------------------------------------------------

        // Allocate
        tmp[i] = shl__malloc(size, options, &pagesize);

        // Allocate on proper node; leverage Linux's first touch strategy
        // --------------------------------------------------

        cpu_set_t cpu_mask_org;
        int err = sched_getaffinity(0, sizeof(cpu_set_t), &cpu_mask_org);
        if (err) {
            perror("sched_getaffinity");
            exit(1);
        }

        // bind processor
        shl__bind_processor(shl__get_proc_for_node(i));

        // write once on every page
        for (int j=0; j<size; j+=(4096))
            *((char*)(tmp[i])+j) = 0;

        // move processor back to original mask
        err = sched_setaffinity(0, sizeof(cpu_set_t), &cpu_mask_org);
        if (err) {
            perror("sched_setaffinity");
            exit(1);
        }
    }
    return tmp;
}

static void shl__repl_sync(void* src, void **dest, size_t num_dest, size_t size)
{
    omp_set_dynamic(0);     // Explicitly disable dynamic teams
    omp_set_num_threads(32); // Use 4 threads for all consecutive parallel regions

    for (int i=0; i<num_dest; i++) {

        #pragma omp parallel for
        for (int j=0; j<size; j++) {

            ((char*) dest[i])[j] = ((char*) src)[j];
        }
    }

    omp_set_num_threads(0);
}

static void shl__init_thread(int thread_id)
{
#ifdef PAPI
    PAPI_register_thread();
#endif
}

#endif
