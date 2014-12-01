#include <stdio.h>
#include <barrelfish/barrelfish.h>
#include <omp.h>
#include <xomp/xomp.h>

#include "shl_internal.h"

extern char **argvals;
extern int argcount;

static lvaddr_t malloc_next = MALLOC_VADDR_START;

/**
 * \brief initializes the barrelfish backend
 */
int shl__barrelfish_init(size_t num_threads)
{
#if 0
#if SHL_BARRELFISH_USE_SHARED
    bomp_bomp_init(num_threads);
    return 0;
#else
    errval_t err;

    printf("Starting as XOMP Master\n");
    struct xomp_args args;
    args.type = XOMP_ARG_TYPE_UNIFORM;
    args.args.uniform.nthreads = num_threads;
    args.core_stride = 2;
    args.args.uniform.nphi = 2;
    /*
    args.args.uniform.argc = argcount;
    args.args.uniform.argv = argvals;
    */
    args.args.uniform.worker_loc = XOMP_WORKER_LOC_MIXED;

    err = bomp_xomp_init(&args);
    if (err_is_fail(err)) {
        USER_PANIC("XOMP master initialization failed.\n");
    }

    omp_set_num_threads(num_threads);
#endif
#endif
    return 0;
}

void aff_set_oncpu(unsigned int cpu)
{
    assert(!"Not yet implemented");
}

int shl__get_proc_for_node(int node)
{
    assert(!"Not yet implemented");
    return -1;
}

extern coreid_t *affinity_conf;
void shl__bind_processor_aff(int id)
{
    assert(!"Not yet implemented");
}

void shl__bind_processor(int id)
{
    assert(!"Not yet implemented");
}

int numa_cpu_to_node(int cpu)
{
    if (cpu < 20) {
        return 0;   // assume that cores 0..19 are NUMA 0
    } else {
        return 1;
    }
}

int numa_max_node(void)
{
    // we have two numa nodes on babybel
    return 1;
}

long numa_node_size(int node,
                    long* memsize)
{
    if (node < 1) {
        if (memsize) {
            // babybel has 256GB RAM, so half of that per NUMA node
            *memsize = (128UL * 1024 * 1024 * 1024);
            return 0;
        } else {
            return -1;
        }
    }

    return -1;
}

int numa_available(void)
{
    /* If it returns -1, all other functions in this library are undefined. */
    return 0;
}
void numa_set_strict(int id)
{
    /* no op */
}

/**
 *
 * \param num_replicas Set to the number of replicas that have been
 * used or 0 in case of error.
 */
void** shl_malloc_replicated(size_t size,
                             int* num_replicas,
                             int options)
{
    assert(!"NYI");
    return NULL;
}

void *shl__alloc_struct_shared(size_t shared_size)
{
#if !SHL_BARRELFISH_USE_SHARED

    struct mem_info mi;
    errval_t err;

    uint64_t min_base, max_limit;
    ram_get_affinity(&min_base, &max_limit);
    ram_set_affinity(SHL_RAM_MIN_BASE, SHL_RAM_MAX_LIMIT);


    err = frame_alloc(&mi.frame, shared_size, &shared_size);
    if (err_is_fail(err)) {
        return NULL;
    }

    ram_set_affinity(min_base, max_limit);

    mi.vaddr = malloc_next;

    err = vspace_map_one_frame_fixed(mi.vaddr, shared_size, mi.frame, NULL, NULL);
    if (err_is_fail(err)) {
        cap_destroy(mi.frame);
        return NULL;
    }

    mi.opts = 0;

    malloc_next += shared_size;

    shl__barrelfish_share_frame(&mi);

    return (void *)mi.vaddr;

#else
    return calloc(1, shared_size);
#endif
}

int shl__barrelfish_share_frame(struct mem_info *mi)
{
#if !SHL_BARRELFISH_USE_SHARED
    xomp_frame_type_t type = XOMP_FRAME_TYPE_SHARED_RW;
    if (mi->opts & SHL_MALLOC_PARTITION) {
        assert("NYI");
    } else if (mi->opts & SHL_MALLOC_REPLICATED) {
        type = XOMP_FRAME_TYPE_REPL_RW;
    } else if (mi->opts & SHL_MALLOC_DISTRIBUTED) {
        assert("NYI");
    }

    return xomp_master_add_memory(mi->frame, mi->vaddr, type);
#endif
    assert(!"should not be called");
    return -1;
}

bool shl__check_hugepage_support(void)
{
    return 0;
}

/**
 * \brief ALlocate memory with the given flags.
 *
 * The array will NOT be initialized (but might be, to force the Linux
 * Kernel to map the memory as requested)
 *
 * Supported options as a bitmask in opts are:
 *
 * - SHL_MALLOC_HUGEPAGE:
 *   enable hugepage support
 *
 * - SHL_MALLOC_DISTRIBUTED:
 *    distribute memory approximately equally on nodes that have threads
 */
#if !SHL_BARRELFISH_USE_SHARED
void* shl__malloc(size_t size,
                  size_t objsize,
                  int opts,
                  int *pagesize,
                  void **ret_mi,
                  void **ret_data)
#else
void* shl__malloc(size_t size,
                  int opts,
                  int *pagesize,
                  void **ret_mi)
#endif
{
    errval_t err;

    struct mem_info *mi = calloc(1, sizeof(*mi));
    if (mi == NULL) {
        return NULL;
    }

    mi->size = size;
    mi->vaddr = malloc_next;
    mi->opts = opts;

    uint32_t pg_size = (opts & SHL_MALLOC_HUGEPAGE) ? PAGESIZE_HUGE : PAGESIZE;



    // round up to multiple of page size
#if !SHL_BARRELFISH_USE_SHARED
    size = (size + objsize + pg_size - 1) & ~(pg_size - 1);
#else
    size = (size + pg_size - 1) & ~(pg_size - 1);
#endif

    uint64_t min_base, max_limit;
    ram_get_affinity(&min_base, &max_limit);
    ram_set_affinity(SHL_RAM_MIN_BASE, SHL_RAM_MAX_LIMIT);

    err = frame_alloc(&mi->frame, size, &size);
    if (err_is_fail(err)) {
        goto err_alloc;
    }

    ram_set_affinity(min_base, max_limit);

    err = vspace_map_one_frame_fixed(mi->vaddr, size, mi->frame, NULL, NULL);
    if (err_is_fail(err)) {
        goto err_map;
    }

    malloc_next += size;

    if (pagesize) {
        *pagesize = pg_size;
    }

    if (ret_mi) {
        *ret_mi = (void *) mi;
    }

#if !SHL_BARRELFISH_USE_SHARED
    if (ret_data) {
        *ret_data = (void *)mi->vaddr;
    }
    return (void *)(mi->vaddr + ((mi->size + 64 - 1) & ~(64 - 1)));
#endif
    return (void *) mi->vaddr;

    err_map: cap_destroy(mi->frame);

    err_alloc: free(mi);

    return 0;
}

void** shl__copy_array(void *src,
                       size_t size,
                       bool is_used,
                       bool is_ro,
                       const char* array_name)
{
    assert(!"NYI");
    return 0;
}

void shl__copy_back_array(void **src,
                          void *dest,
                          size_t size,
                          bool is_copied,
                          bool is_ro,
                          bool is_dynamic,
                          const char* array_name)
{
    assert(!"NYI");
}

void shl__copy_back_array_single(void *src,
                                 void *dest,
                                 size_t size,
                                 bool is_copied,
                                 bool is_ro,
                                 bool is_dynamic,
                                 const char* array_name)
{
    assert(!"NYI");
}

long shl__node_size(int node, long  *freep)
{
    assert(!"NYI");
}

int shl__max_node(void)
{
    assert(!"NYI");
}
