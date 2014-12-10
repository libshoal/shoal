#include <stdio.h>
#include <barrelfish/barrelfish.h>

#include "shl_internal.h"

/*
 * This file contains functions to obtain machine related information on the
 * Barrelfish OS.
 */

/*
 * machine specifier. We apply static machine information here
 * TODO: obtain this from SKB / libnuma
 */
#define SHL_MACHINE_BABYBEL 1
#define SHL_MACHINE_QEMU    2
#define SHL_MACHINE SHL_MACHINE_QEMU


/**
 * \brief obtains the node ID from a core id
 *
 * \param core_id the ID of the core
 *
 * \return node ID of the CPU
 */
int shl__node_from_cpu(int core_id)
{
#if SHL_MACHINE == SHL_MACHINE_BABYBEL
    if (core_id < 10) {
        return 0;   // assume that cores 0..9 are NUMA 0
    } else {
        return 1;
    }
#elif SHL_MACHINE == SHL_MACHINE_QEMU
    return core_id;
#else
#error unknown machine
#endif

}

/**
 * \brief checks availability of the NUMA
 *
 * \returns  0 iff NUMA is available
 *          -1 iff NUMA is not available
 */
int shl__check_numa_availability(void)
{
    return 0;
}

/**
 * \brief TODO
 * @param id
 */
void shl__set_strict_mode(int id)
{
    /* no-op */
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
#if SHL_MACHINE == SHL_MACHINE_BABYBEL
    /*
     * Linux dmesg:
     * [    0.000000] Initmem setup node 0 [mem 0x00000000-0x203fffffff]
     * [    0.000000] Initmem setup node 1 [mem 0x2040000000-0x403fffffff]
     */
    if (node == 0) {
        mb = 0x00000000UL;
        ml = 0x203fffffffUL;
    } else if (node == 1) {
        mb = 0x2040000000UL;
        ml = 0x403fffffffUL;
    } else {
        return -1;
    }
#elif SHL_MACHINE == SHL_MACHINE_QEMU
    if (node == 0) {
        mb = 0;
        ml = 512UL * 1024 * 1014;
    } else if (node == 1){
        mb = 512UL * 1024 * 1014;
        ml = 1024UL * 1024 * 1014 - 1;
    } else {
        return -1;
    }
#else
#error unknown machine
#endif

    if (min_base) {
        *min_base = mb;
    }

    if (max_limit) {
        *max_limit = ml;
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
#if SHL_MACHINE == SHL_MACHINE_BABYBEL
    return 0x203fffffffUL; // from Linux dmesg
#elif SHL_MACHINE == SHL_MACHINE_QEMU
    return (1UL << 29); // setting 212MB for QEMU
#else
#error unknown machine
#endif

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
#if SHL_MACHINE == SHL_MACHINE_BABYBEL
    return 1; // babybel has two nodes
#elif SHL_MACHINE == SHL_MACHINE_QEMU
    return 1;   // qemu emulates 2 nodess
#else
#error unknown machine
#endif
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
