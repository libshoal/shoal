#include <cstdio>
#include <cstdlib>

#include <sched.h>
#include <numa.h>

#include "shl_internal.h"
#include "shl.h"

void
aff_set_oncpu(unsigned int cpu)
{
    cpu_set_t cpu_mask;
    int err;

    CPU_ZERO(&cpu_mask);
    CPU_SET(cpu, &cpu_mask);

    err = sched_setaffinity(0, sizeof(cpu_set_t), &cpu_mask);
    if (err) {
        perror("sched_setaffinity");
        exit(1);
    }
}

int shl__get_proc_for_node(int node)
{
    for (int i=0; i<MAXCORES; i++)
        if (numa_cpu_to_node(i)==node)
            return i;

    assert (!"Cannot find processor on given node");
    return -1;
}

extern coreid_t *affinity_conf;
void shl__bind_processor_aff(int id)
{
    printf("Binding [%d] to [%d]\n", id, affinity_conf[id]);
    if (affinity_conf!=NULL) {
        aff_set_oncpu(affinity_conf[id]);
    }
}

void shl__bind_processor(int id)
{
    aff_set_oncpu(id);
}

int
numa_cpu_to_node(int cpu)
{
    int ret    = -1;
    int ncpus  = numa_num_possible_cpus();
    int node_max = numa_max_node();
    struct bitmask *cpus = numa_bitmask_alloc(ncpus);

    for (int node=0; node <= node_max; node++) {
        numa_bitmask_clearall(cpus);
        if (numa_node_to_cpus(node, cpus) < 0) {
            perror("numa_node_to_cpus");
            fprintf(stderr, "numa_node_to_cpus() failed for node %d\n", node);
            abort();
        }

        if (numa_bitmask_isbitset(cpus, cpu)) {
            ret = node;
        }
    }

    numa_bitmask_free(cpus);
    if (ret == -1) {
        fprintf(stderr, "%s failed to find node for cpu %d\n",
                __FUNCTION__, cpu);
        abort();
    }

    return ret;
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
        for (size_t j=0; j<size; j+=PAGESIZE)
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
void* shl__malloc(size_t size, int opts, int *pagesize)
{
    void *res;
    bool use_hugepage = opts & SHL_MALLOC_HUGEPAGE;
    bool distribute = opts & SHL_MALLOC_DISTRIBUTED;

    // Round up to next multiple of page size (in case of hugepage)
    *pagesize = use_hugepage ? PAGESIZE_HUGE : PAGESIZE;
    size_t alloc_size = size;
    while (use_hugepage && (alloc_size % *pagesize != 0))
        alloc_size++;

    // Set options for mmap
    int options = MAP_ANONYMOUS | MAP_PRIVATE;
    if (use_hugepage)
        options |= MAP_HUGETLB;

    printf("shl__alloc: %zu, huge=%d, distribute=%d",
           alloc_size, use_hugepage, distribute);

    // Allocate
    res = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, options, -1, 0);
    if (res==MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    // Distribute memory
    // --------------------------------------------------
    if (distribute) {

#pragma omp parallel for
        for (int i=0; i<alloc_size;i ++) {

            ((char *) res)[i] = 0;
        }
    }

    printf("\n");

    return res;
}
