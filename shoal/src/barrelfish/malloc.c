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
#include <backend/barrelfish/meminfo.h>

#include "shl_internal.h"

#define SHL_ALLOC_NUMA_PARTITION 0

static lvaddr_t malloc_next = MALLOC_VADDR_START;

void *shl__alloc_struct_shared(size_t shared_size)
{
#if !SHL_BARRELFISH_USE_SHARED

    struct shl_mi_data mi;
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
 * \brief Allocate memory with the given flags.
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

    struct shl_mi_header *mi = calloc(1, sizeof(*mi) + sizeof(struct shl_mi_data));
    if (mi == NULL) {
        return NULL;
    }

    mi->num = 1;
    mi->data = (struct shl_mi_data *)(mi+1);
    mi->stride = 0;

    size_t pg_size = (opts & SHL_MALLOC_HUGEPAGE) ? PAGESIZE_HUGE : PAGESIZE;

    // round up to multiple of page size
    size = (size + pg_size - 1) & ~(pg_size - 1);

    malloc_next = (malloc_next + pg_size - 1) & ~(pg_size - 1);

    mi->data[0].opts = opts;
    mi->data[0].vaddr = malloc_next;
    mi->data[0].size = size;
    mi->vaddr = malloc_next;

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

    struct frame_identity fi;
    err = frame_alloc(&mi->data[0].frame, size, &size);
    if (err_is_fail(err)) {
        goto err_alloc;
    }

    err = invoke_frame_identify(mi->data[0].frame,  &fi);
    assert(err_is_ok(err));

    mi->data[0].paddr = fi.base;

    SHL_DEBUG_ALLOC("mapping array @[0x%016"PRIx64"-0x%016"PRIx64"]\n",
                            mi->data[0].vaddr, mi->data[0].vaddr+size);

    err = vspace_map_one_frame_fixed(mi->vaddr, size, mi->data[0].frame, NULL, NULL);
    if (err_is_fail(err)) {
        goto err_map;
    }

    if (node != SHL_NUMA_IGNORE) {
        ram_set_affinity(min_base, max_limit);
    }

    malloc_next += size;

    if (pagesize) {
        *pagesize = (uint32_t)pg_size;
    }

    if (ret_mi) {
        *ret_mi = (void *) mi;
    }

    return (void *) mi->data[0].vaddr;

    err_map: cap_destroy(mi->data[0].frame);

    err_alloc: free(mi);

    return NULL;
}


static void *shl__malloc_numa(size_t size,
                              int opts,
                              int *pagesize,
                              void **ret_mi,
                              size_t stride)
{
    errval_t err;

    /* we need memory from each node */
    int num_nodes = shl__max_node() + 1;

    SHL_DEBUG_ALLOC("allocating memory for %u nodes (%s)\n", num_nodes,
                    (stride == SHL_ALLOC_NUMA_PARTITION) ? "partitioned" : "distributed");

    size_t pg_size = (opts & SHL_MALLOC_HUGEPAGE) ? PAGESIZE_HUGE : PAGESIZE;

    struct shl_mi_header *mi = calloc(1, sizeof(*mi)
                                        + num_nodes * sizeof(struct shl_mi_data));
    if (mi == NULL) {
        return NULL;
    }

    mi->num = num_nodes;
    mi->data = (struct shl_mi_data *)(mi+1);


    /* round up stride and size*/
    stride = (stride + pg_size - 1) & ~(pg_size - 1);
    size = (size + pg_size - 1) & ~(pg_size - 1);

    if (stride == SHL_ALLOC_NUMA_PARTITION) {
        stride = ((size / num_nodes) + pg_size - 1) & ~(pg_size - 1);
    }

    mi->stride = stride;

    err = memobj_create_numa(&mi->memobj, size, 0, num_nodes, stride);
    if (err_is_fail(err)) {
        free(mi);
        return NULL;
    }

    struct memobj *memobj = &mi->memobj.m;

    /* store the original memory ranges */
    uintptr_t min_base, max_limit;
    ram_get_affinity(&min_base, &max_limit);

    /* get bytes per node and round up to a multiple of the page size */
    size_t bytes_per_node = size / num_nodes;
    bytes_per_node = (bytes_per_node + pg_size - 1) & ~(pg_size - 1);

    /* round up malloc next vaddr */
    malloc_next = (malloc_next + pg_size - 1) & ~(pg_size - 1);

    mi->vaddr = malloc_next;

    /* the new memory ranges */
    uint64_t mb_new, ml_new;

    for (int i = 0; i < num_nodes; ++i) {
        if (shl__node_get_range(i, &mb_new, &ml_new)) {
            USER_PANIC("shl__node_get_range failed");
        }

        /* set affinity to allocate from that memory region */
        ram_set_affinity(mb_new, ml_new);

        SHL_DEBUG_ALLOC("allocating frame: node=%u, [%016"PRIx64", %016"PRIx64"] "
                        "of size %lu bytes\n", i, mb_new, ml_new, bytes_per_node);

        struct frame_identity fi;
        err = frame_alloc(&mi->data[i].frame, bytes_per_node, NULL);
        if (err_is_fail(err)) {
            USER_PANIC_ERR(err, "allocating frame");
        }

        err = invoke_frame_identify(mi->data[i].frame, &fi);
        assert(err_is_ok(err));

        mi->data[i].paddr = fi.base;
        mi->data[i].size = (1UL << fi.bits);
        mi->data[i].opts = opts;

        err = memobj->f.fill(memobj, i, mi->data[i].frame, 0);
        if (err_is_fail(err)) {
            USER_PANIC_ERR(err, "filling memobj");
        }
    }

    SHL_DEBUG_ALLOC("mapping array @[0x%016"PRIx64"-0x%016"PRIx64"]\n",
                    malloc_next, malloc_next+size);

    err = vregion_map_fixed(&mi->vregion, get_current_vspace(), memobj, 0, size,
                            malloc_next, VREGION_FLAGS_READ_WRITE);
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "mapping memobj");
    }

    err = memobj->f.pagefault(memobj, &mi->vregion, 0, 0);
    if (err_is_fail(err)) {
        vspace_unmap((void *)malloc_next);
        USER_PANIC_ERR(err, "pagefaulting");
    }

    /* restore the original memory ranges */
    ram_set_affinity(min_base, max_limit);

    malloc_next += size;

    if (pagesize) {
        *pagesize = (uint32_t)pg_size;
    }

    if (ret_mi) {
        *ret_mi = (void *) mi;
    }

    return (void *)(mi->vaddr);
}

void* shl__malloc_distributed(size_t size,
                              int opts,
                              int *pagesize,
                              void **ret_mi)
{
    return shl__malloc_numa(size, opts, pagesize, ret_mi, SHL_DISTRIBUTION_STRIDE);
}


void *shl__malloc_partitioned(size_t size,
                              int opts,
                              int *pagesize,
                              void **ret_mi)
{
    return shl__malloc_numa(size, opts, pagesize, ret_mi, SHL_ALLOC_NUMA_PARTITION);
}



/**
 *
 * \param num_replicas Set to the number of replicas that have been
 * used or 0 in case of error.
 */
void** shl__malloc_replicated(size_t size,
                              int* num_replicas,
                              int* pagesize,
                              int options,
                              void ** ret_mi)
{
    errval_t err;

    /* we need memory from each node */
    int num_nodes = shl__max_node() + 1;

    SHL_DEBUG_ALLOC("allocating memory for %u nodes (replicated)\n", num_nodes);

    void **arrays = calloc(num_nodes, sizeof(void *));
    if (arrays == NULL) {
        return NULL;
    }

    struct shl_mi_header *mi = calloc(1, sizeof(*mi)
                                      + num_nodes * sizeof(struct shl_mi_data));
    if (mi == NULL) {
        free(arrays);
        return NULL;
    }

    mi->num = num_nodes;
    mi->data = (struct shl_mi_data *)(mi+1);
    mi->stride = 0;
    mi->vaddr = 0;

    size_t pg_size = (options & SHL_MALLOC_HUGEPAGE) ? PAGESIZE_HUGE : PAGESIZE;

    // round up to multiple of page size
    size = (size + pg_size - 1) & ~(pg_size - 1);

    // round up malloc next to next multiple of page size
    malloc_next = (malloc_next + pg_size - 1) & ~(pg_size - 1);

    // store old memory affinity ranges
    uintptr_t min_base, max_limit;
    uintptr_t mb_new, ml_new;

    ram_get_affinity(&min_base, &max_limit);

    for (int i = 0; i < num_nodes; ++i) {
        if (shl__node_get_range(i, &mb_new, &ml_new)) {
            USER_PANIC("getting range for memory affinity\n");
        }

        SHL_DEBUG_ALLOC("Setting range to: 0x%016"PRIx64"-0x%016"PRIx64"\n",
                        mb_new, ml_new);
        ram_set_affinity(mb_new, ml_new);

        struct frame_identity fi;
        err = frame_alloc(&mi->data[i].frame, size, &size);
        if (err_is_fail(err)) {
            USER_PANIC_ERR(err, "failed to allocate memory\n");
        }

        err = invoke_frame_identify(mi->data[i].frame,  &fi);
        assert(err_is_ok(err));

        mi->data[i].size = size;
        mi->data[i].opts = options;
        mi->data[i].paddr = fi.base;
        mi->data[i].vaddr = malloc_next;

        SHL_DEBUG_ALLOC("mapping replicate @[0x%016"PRIx64"-0x%016"PRIx64"]\n",
                        mi->data[i].vaddr, mi->data[i].vaddr+size);

        err = vspace_map_one_frame_fixed(mi->data[i].vaddr, size, mi->data[i].frame, NULL, NULL);
        if (err_is_fail(err)) {
            USER_PANIC_ERR(err, "failed to vspace_map_one_frame_fixed\n");
        }

        arrays[i] = (void *)mi->data[i].vaddr;

        malloc_next += size;
    }

    // restore ram affinity
    ram_set_affinity(min_base, max_limit);

    if (num_replicas) {
        *num_replicas = num_nodes;
    }

    if (pagesize) {
        *pagesize = (uint32_t)pg_size;
    }

    if (ret_mi) {
        *ret_mi = (void *) mi;
    }

    return arrays;
}
