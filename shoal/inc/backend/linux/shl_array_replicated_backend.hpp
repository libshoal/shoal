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

template<class T>
void shl_array_replicated<T>::copy_from(T* src)
{
    if (!shl_array<T>::do_copy_in())
        return;

    assert(shl_array<T>::alloc_done);

    printf("shl_array_replicated[%s]: Copying to %d replicas\n",
           shl_base_array::name, num_replicas);

    for (int j = 0; j < num_replicas; j++) {
        for (unsigned int i = 0; i < shl_array<T>::size; i++) {

            rep_array[j][i] = src[i];
        }
    }
}

template<class T>
int shl_array_replicated<T>::init_from_value(T value)
{
    return shl_array<T>::init_from_value(value);
}

template<class T>
int shl_array_replicated<T>::copy_from_array(shl_array<T> *src)
{
   return shl_array<T>::copy_from_array(src);
}

#endif /* __SHL_ARRAY */
