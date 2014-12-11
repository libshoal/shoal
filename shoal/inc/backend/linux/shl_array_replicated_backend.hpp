/*
 * Copyright (c) 2014 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef __SHL_ARRAY_REPLICATED_BACKEND
#define __SHL_ARRAY_REPLICATED_BACKEND

/**
 * \brief allocates the arrays
 */
template<class T>
int shl_array_replicated<T>::alloc(void)
{
    if (!shl_array<T>::do_alloc())
        return -1;

    shl_array<T>::print();

    assert(!shl_array<T>::alloc_done);

    rep_array = (T**) shl__malloc_replicated(shl_array<T>::size * sizeof(T),
                                             &this->pagesize,
                                             &num_replicas,
                                             shl_array<T>::get_options(), NULL);

    assert(num_replicas > 0);
    for (int i = 0; i < num_replicas; i++) {
        assert(rep_array[i]!=NULL);
    }

    shl_array<T>::alloc_done = true;

    return 0;
}

#endif /* __SHL_ARRAY */
