#include <cstdio>
#include <cstdlib>

#include "shl_internal.h"
#include "shl.h"

void
aff_set_oncpu(unsigned int cpu)
{
    assert (!"Not yet implemented");
}

int shl__get_proc_for_node(int node)
{
    assert (!"Not yet implemented");
    return -1;
}

extern coreid_t *affinity_conf;
void shl__bind_processor_aff(int id)
{
    assert (!"Not yet implemented");
}

void shl__bind_processor(int id)
{
    assert (!"Not yet implemented");
}

int numa_cpu_to_node(int)
{
    assert (!"Not yet implemented");
    return 0;
}

int numa_max_node(void)
{
    assert (!"Not yet implemented");
    return 0;
}
long numa_node_size(int, long*)
{
    assert (!"Not yet implemented");
    return 0;
}
int numa_available(void)
{
    assert (!"Not yet implemented");
    return 0;
}
void numa_set_strict(int)
{
    assert (!"Not yet implemented");
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
    assert(!"NYI");
    return NULL;
}
