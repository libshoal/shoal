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
int shl_array<T>::copy_from_array_async(shl_array<T> *src_array, size_t elements)
{
    return -1;
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
int shl_array<T>::init_from_value_async(T value, size_t elements)
{
    return -1;
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
int shl_array<T>::copy_from_async(T* src, size_t elements)
{
    return -1;
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
int shl_array<T>::copy_back_async(T* dest, size_t elements)
{
    return -1;
}

#endif /* __SHL_ARRAY_PARTITIONED */
