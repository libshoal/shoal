/*
 * Copyright (c) 2014 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#include <stdio.h>
#include <barrelfish/barrelfish.h>

#include "shl_internal.h"

static lvaddr_t malloc_next = MALLOC_VADDR_START;

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
void* shl__malloc(size_t size,
                  int opts,
                  int *pagesize,
                  int node,
                  void **ret_mi)
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
    size = (size + pg_size - 1) & ~(pg_size - 1);


    uintptr_t min_base, max_limit;
    if (node != SHL_NUMA_IGNORE) {
        uint64_t mb_new, ml_new;
        if (!shl__node_get_range(node, &mb_new, &ml_new)) {
            printf("Setting range to: 0x%016lx-0x%016lx\n", mb_new, ml_new);
            ram_get_affinity(&min_base, &max_limit);
            ram_set_affinity(mb_new, ml_new);
        } else {
            goto err_alloc;
        }
    }

    printf("Allocating of size: %lu\n", size);
    err = frame_alloc(&mi->frame, size, &size);
    if (err_is_fail(err)) {
        goto err_alloc;
    }

    err = vspace_map_one_frame_fixed(mi->vaddr, size, mi->frame, NULL, NULL);
    if (err_is_fail(err)) {
        goto err_map;
    }

    if (node != SHL_NUMA_IGNORE) {
        ram_set_affinity(min_base, max_limit);
    }

    malloc_next += size;

    if (pagesize) {
        *pagesize = pg_size;
    }

    if (ret_mi) {
        *ret_mi = (void *) mi;
    }

    return (void *) mi->vaddr;

    err_map: cap_destroy(mi->frame);

    err_alloc: free(mi);

    return NULL;
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
