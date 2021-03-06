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
        return 0;

    this->print();

    assert(!this->alloc_done);

    rep_array = (T**) shl__malloc_replicated(this->size * sizeof(T),
                                              &num_replicas, &this->pagesize,
                                              this->get_options(),
                                              &this->meminfo);
    if (this->rep_array == NULL) {
        return -1;
    }

    this->alloc_done = true;

    return 0;
}

template<class T>
int shl_array_replicated<T>::copy_from_async(T* src, size_t elements)
{
    if (!this->do_copy_in())
        return 0;

    return -1;
}

template<class T>
int shl_array_replicated<T>::copy_from_array_async(shl_array<T> *src, size_t elements)
{
    return -1;
}

template<class T>
int shl_array_replicated<T>::init_from_value_async(T value, size_t elements)
{
    return -1;
}

template<class T>
int shl_array_replicated<T>::copy_back_async(T* dest, size_t elements)
{
    return -1;
}


#endif /* __SHL_ARRAY */
