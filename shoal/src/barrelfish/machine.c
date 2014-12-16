#include <stdio.h>
#include <barrelfish/barrelfish.h>
#include <numa.h>
#include "shl_internal.h"

/*
 * This file contains functions to obtain machine related information on the
 * Barrelfish OS.
 */

/*
 * machine specifier. We apply static machine information here
 * TODO: obtain this from SKB / libnuma
 */



/**
 * \brief obtains the node ID from a core id
 *
 * \param core_id the ID of the core
 *
 * \return node ID of the CPU
 */
int shl__node_from_cpu(int core_id)
{
    return numa_node_of_cpu((coreid_t)core_id);
}

/**
 * \brief checks availability of the NUMA
 *
 * \returns  0 iff NUMA is available
 *          -1 iff NUMA is not available
 */
int shl__check_numa_availability(void)
{
    return numa_available();
}

/**
 * \brief TODO
 * @param id
 */
void shl__set_strict_mode(int id)
{
    numa_set_strict(id);
}

/**
 * \brief obtains the memory range for a node
 *
 * \param node      ID of the node
 * \param min_base  returns the minimum address belonging to this node
 * \param max_limit returns the maximum address belonging to this node
 *
 * \return 0         on SUCCESS
 *         non-zero  on FAILURE
 */
int shl__node_get_range(int node,
                        uintptr_t *min_base,
                        uintptr_t *max_limit)
{
    uintptr_t mb, ml;

    mb = numa_node_base(node);
    if (mb == NUMA_NODE_INVALID) {
        return -1;
    }

    ml = numa_node_size(node, NULL);
    if (ml == NUMA_NODE_INVALID) {
        return -1;
    }

    if (min_base) {
        *min_base = mb;
    }

    if (max_limit) {
        *max_limit = (mb + ml);
    }

    return 0;
}

/**
 * \brief returns the size of the node in bytes
 *
 * \param node  ID of the node
 * \param freep free pointer
 *
 * \returns node size in bytes
 *
 * XXX: what is the idea behind the free pointer
 */
long shl__node_size(int node, long  *freep)
{
    return numa_node_size(node, (uintptr_t *)freep);
}

/**
 * \brief returns the maximum ID of the nodes
 *
 * @return max ID of the nodes
 *
 * The total number of available nodes is shl__max_node() + 1
 */
int shl__max_node(void)
{
    return numa_max_node();
}

/**
 * \brief checks if the OS supports huge pages
 *
 * \returns TRUE if hugep ages are supported
 *          FALSE if huge pages are not supported
 */
bool shl__check_hugepage_support(void)
{
    return 0; // Barrelfish does not support huge pages
}

/**
 * \brief checks if the OS supports large pages
 *
 * \returns TRUE if large pages are supported
 *          FALSE if large pages are not supported
 *
 * on X86_64 this is 1GiB
 */
bool shl__check_largepage_support(void)
{
    return 0; // Barrelfish does not support large pages
}
