/*
 * Copyright (c) 2014 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef __SHL_ARRAY_BACKEND
#define __SHL_ARRAY_BACKEND



template<class T>
int shl_array<T>::copy_from_array_async(shl_array<T> *src_array)
{
    T *src = src_array->get_array();
    if (!array || !src) {
        printf("shl_array<T>::copy_from_array: Failed no pointers\n");
        return -1;
    }

    if (size != src_array->get_size()) {
        printf("shl_array<T>::copy_from_array: not matching sizes\n");
        return -1;
    }

    size_t elements = (src_array->get_size() > size) ? size : src_array->get_size();

    shl__memcpy_openmp(array, src, sizeof(T), elements);

    return 0;
}

/*
 * ===============================================================================
 */
/**
 * \brief initializes the array to a specific value
 *
 * \param value data two fill the array with
 *
 * \returns 0 if the array has been initialized
 *          non-zero if the array is not yet allocated
 */
template<class T>
int shl_array<T>::init_from_value_async(T value)
{
    return shl__memset_openmp(array, &value, sizeof(T), size);
}

/*
 * ===============================================================================
 */
/**
 * \brief Copy data from existing array
 *
 * \param src pointer to a memory location of data to copy in
 *
 * XXX WARNING: The sice of array src is not checked.
 * Assumption: sizeof(src) == this->size
 */
template<class T>
void shl_array<T>::copy_from_async(T* src)
{
    if (!do_copy_in()) {
        return;
    }

    shl__memcpy_openmp(array, src, sizeof(T), size);
}

/*
 * ===============================================================================
 */

/**
 * \brief Copy data to existing array
 *
 * \param dest  destination array
 *
 * XXX WARNING: The sice of array src is not checked. Assumption:
 * sizeof(src) == this->size
 */
template<class T>
void shl_array<T>::copy_back_async(T* dest)
{
    assert(dma_total_tx == 0 && dma_compl_tx == 0);

    if (!do_copy_back()) {
        return;
    }

    shl__memcpy_openmp(dest, array, sizeof(T), size);
}

#endif /* __SHL_ARRAY_PARTITIONED */
