#include "shl.h"
#include "shl_internal.h"

Configuration::Configuration(void) {

    // Configuration based on environemnt
    use_hugepage = get_env_int("SHL_HUGEPAGE", 1);
    use_replication = get_env_int("SHL_REPLICATION", 1);

    // NUMA information
    num_nodes = numa_max_node();
    node_mem_avail = new long[num_nodes];
    mem_avail = 0;

    for (int i=0; i<=numa_max_node(); i++) {

        numa_node_size(i, &(node_mem_avail[i]));
        mem_avail += node_mem_avail[i];
    }
}

void shl__end(void)
{
    printf("Time for copy: %.6f\n", shl__get_timer());
#ifdef DEBUG
    printf("Number of lookups: %ld\n", num_lookup);
#endif
}

#ifdef PAPI
void papi_stop(void)
{
    // Stop PAPI events
    long long values[1];
    if (PAPI_stop(EventSet, values) != PAPI_OK) handle_error(1);
    printf("Stopping PAPI .. ");
    print_number(values[0]); printf("\n");
    printf("END PAPI\n");
}

void papi_init(void)
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

void papi_start(void)
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
int shl__get_rep_id(void)
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

int shl__get_num_replicas(void)
{
    return numa_max_node()+1;
}

void** shl__copy_array(void *src, size_t size, bool is_used,
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
#if 1

            // --------------------------------------------------
            // Allocate memory using mmap

            // Make size be alligned multiple of page size
            int alloc_size = size;
            while (alloc_size % PAGESIZE != 0)
                alloc_size++;

            int flags = MAP_ANONYMOUS | MAP_PRIVATE;

#ifdef ENABLE_HUGEPAGE
            // hugepage support on Linux
            flags |= MAP_HUGETLB;
#endif

            printf("mmap(size=0x%x)\n", alloc_size);
            tmp[i] = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE,
                          flags, -1, 0);
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
            for (int j=0; j<size; j+=PAGESIZE) {
                *((char*)(tmp[i])+j) = *((char*)src+j);
            }

            // move processor back to original mask
            err = sched_setaffinity(0, sizeof(cpu_set_t), &cpu_mask_org);
            if (err) {
                perror("sched_setaffinity");
                exit(1);
            }
#else // !1
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

void shl__copy_back_array(void **src, void *dest, size_t size, bool is_copied,
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

void shl__copy_back_array_single(void *src, void *dest, size_t size, bool is_copied,
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

void* shl__malloc(size_t size, int opts, int *pagesize)
{
    void *res;
    bool use_hugepage = opts & SHL_MALLOC_HUGEPAGE;

    // Round up to next multiple of page size
    *pagesize = use_hugepage ? PAGESIZE_HUGE : PAGESIZE;
    size_t alloc_size = size;
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

void** shl__malloc_replicated(size_t size,
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

void shl__repl_sync(void* src, void **dest, size_t num_dest, size_t size)
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

void shl__init_thread(int thread_id)
{
#ifdef PAPI
    PAPI_register_thread();
#endif
}

void handle_error(int retval)
{
    printf("PAPI error %d: %s\n", retval, PAPI_strerror(retval));
    exit(1);
}

// translate: virtual COREID -> physical COREID
coreid_t *affinity_conf = NULL;

void shl__init(size_t num_threads)
{
    printf("SHOAL (v %s) initialization .. ", VERSION);

    Configuration *conf = get_conf();
    assert (numa_available()>=0);

    affinity_conf = parse_affinity (false);

    if (affinity_conf==NULL) {
        printf(ANSI_COLOR_RED "WARNING:" ANSI_COLOR_RESET " no affinity set! "
               "Disabling replication! "
               "Use SHL_CPU_AFFINITY\n");
        conf->use_replication = false;
    }

    for (int i=0; i<MAXCORES; i++)
        replica_lookup[i] = -1;

    if (conf->use_replication) {
        for (int i=0; i<num_threads; i++) {

            replica_lookup[i] = numa_cpu_to_node(affinity_conf[i]);
            printf("replication: CPU %d is on node %d\n", i, replica_lookup[i]);
        }
    }

    // Prevent numa_alloc_onnode fall back to allocating memory elsewhere
    numa_set_strict (true);

#ifdef DEBUG
    printf(ANSI_COLOR_RED "WARNING:" ANSI_COLOR_RESET " debug enabled in #define\n");
#endif

#ifdef SHL_DEBUG
    printf(ANSI_COLOR_RED "WARNING:" ANSI_COLOR_RESET " debug enabled in Makefile\n");
#endif

#ifdef PAPI
    papi_init();
#endif

    printf("[%c] Replication\n", conf->use_replication ? 'x' : ' ');
    printf("[%c] Hugepage\n", conf->use_hugepage ? 'x' : ' ');
}
