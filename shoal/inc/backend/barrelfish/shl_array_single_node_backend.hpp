/*
 * Copyright (c) 2014 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef __SHL_ARRAY_SINGLE_NODE_BACKEND
#define __SHL_ARRAY_SINGLE_NODE_BACKEND

#include <cstdlib>
#include <cstdarg>

#include "shl.h"
#include "shl_configuration.hpp"
/*
 * \brief Array implementing single-node replication
 */
template<class T>
int shl_array_single_node<T>::alloc(void)
{
    assert(shl_array<T>::alloc() == 0);

    if (this->is_used) {

        // Map everything on single node
        printf("Forcing single-node allocation .. %p\n", shl_array<T>::array);
        for (unsigned int i = 0; i < shl_array<T>::size; i++) {
            shl_array<T>::array[i] = 0;
        }
    }

    return 0;
}

template<class T>
int shl_array_single_node<T>::copy_from_array(shl_array<T> *src)
{
    return shl_array<T>::copy_from_array(src);
}

/**
 * \brief initializes the array to a specific value
 *
 * \param value data two fill the array with
 *
 * \returns 0 if the array has been initialized
 *          non-zero if the array is not yet allocated
 */
template<class T>
int shl_array_single_node<T>::init_from_value(T value)
{
    return shl_array<T>::init_from_value(value);
}

template<class T>
void shl_array_single_node<T>::copy_from(T* src)
{
    int copied = 0;

    /* try DMA copy */
    if (this->meminfo) {
        copied = shl__memcpy_dma_from(src, this->meminfo, 0, sizeof(T)*this->size);
    }

    /* fallback */
    if (copied == 0) {
        shl_array<T>::copy_from(src);
    }
}

template<class T>
void shl_array_single_node<T>::copy_back(T* dest)
{
    return shl_array<T>::copy_back(dest);
}

#endif
