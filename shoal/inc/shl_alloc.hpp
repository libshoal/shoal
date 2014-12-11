/*
 * Copyright (c) 2014 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef __SHL_ARRAY_ALLOC
#define __SHL_ARRAY_ALLOC

#include <cstdlib>
#include <cstdarg>
#include <cstring> // memset
#include <iostream>
#include <limits>

#include "shl.h"
#include "shl_timer.hpp"
#include "shl_configuration.hpp"
#include "shl_arrays.hpp"


// --------------------------------------------------
// Allocation
// --------------------------------------------------

/**
 *\ brief Allocate array
 *
 * \param size Number of elements of type T in array
 * \param name Name of the array (for debugging purposes)
 * \param is_ro Indicate if array is read-only
 * \param is_dynamic Indicate that array is dynamically allocated (i.e.
 *    does not require copy in and copy out)
 * \param is_used Some arrays are allocated, but never used (such as
 *    parts of the graph in Green Marl). We want to avoid copying these,
 *    so we pass this information on.
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
template<class T>
shl_array<T>* shl__malloc_array(size_t size, const char *name,
                                bool is_ro,
                                bool is_dynamic,
                                bool is_used,
                                bool is_graph,
                                bool is_indexed,
                                bool initialize)
{
    // Policy for memory allocation
    // --------------------------------------------------
    // 1) Always partition indexed arrays
   // 1) Always partition indexed arrays
   bool partition = is_indexed && get_conf()->use_partition &&
       shl__get_array_conf(name, SHL_ARR_FEAT_PARTITIONING, true);

   // 2) Replicate if array is read-only and can't be partitioned
   bool replicate = !partition &&  // none of the others
       is_ro && get_conf()->use_replication &&
       shl__get_array_conf(name, SHL_ARR_FEAT_REPLICATION, true);

   // 3) Distribute if nothing else works and there is more than one node
   bool distribute = !replicate && !partition &&  // none of the others
       shl__get_num_replicas() > 1 && initialize &&
       get_conf()->use_distribution &&
       shl__get_array_conf(name, SHL_ARR_FEAT_DISTRIBUTION, true);

    shl_array<T> *res = NULL;

    if (partition) {
        SHL_DEBUG_ALLOC("allocating partitioned array '%s'\n", name);
        res = new shl_array_partitioned<T>(size, name);
    } else if (replicate) {
        SHL_DEBUG_ALLOC("allocating replicated array '%s'\n", name);
        res = new shl_array_replicated<T>(size, name, shl__get_rep_id);
    } else if (distribute) {
        SHL_DEBUG_ALLOC("allocating distributed array '%s'\n", name);
        res = new shl_array_distributed<T>(size, name);
    } else {
        SHL_DEBUG_ALLOC("allocating single_node array '%s'\n", name);
        res = new shl_array_single_node<T>(size, name);
    }

    // These are used internally in array to decide if copy-in and
    // copy-out of source arrays are required
    res->set_dynamic(is_dynamic);
    res->set_used(is_used);

    return res;
}

template<class T>
shl_array<T>* shl__remalloc_array(size_t size, const char *name,
                                  void *data, void *meminfo,
                                  array_t type)
{
    switch(type) {

    }
}



template<class T>
int shl__estimate_size(size_t size,
                       const char *name,
                       bool is_ro,
                       bool is_dynamic,
                       bool is_used,
                       bool is_graph,
                       bool is_indexed)
{
    return is_used ? sizeof(T) * size : 0;
}

int shl__estimate_working_set_size(int num, ...);

#endif /* __SHL_ARRAY_ALLOC */
