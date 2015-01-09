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
    assert(this->dma_total_tx == 0 && this->dma_compl_tx == 0);


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

    if (get_conf()->use_dma && meminfo) {
        dma_total_tx = shl__memcpy_dma_array(src_array->get_meminfo(), meminfo,
                                             sizeof(T) * elements,
                                             &dma_compl_tx);
    }

    if (this->dma_total_tx == 0) {
        this->dma_compl_tx = 0;
        shl__memcpy_openmp(array, src, sizeof(T), elements);
    }

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
    assert(dma_total_tx == 0 && dma_compl_tx == 0);

    if (get_conf()->use_dma && meminfo) {
        uint64_t val = 0;
        if (sizeof(T) < sizeof(uint64_t)) {
            uint8_t *ptr = (uint8_t *)&val;
            uint8_t *psrc = (uint8_t*)&value;
            for (unsigned i = 0; i < sizeof(uint64_t) / sizeof(T); ++i) {
                memcpy(ptr, psrc, sizeof(T));
                ptr += sizeof(T);
            }
        } else {
            memcpy(&val, &value, sizeof(uint64_t));
        }

        size_t bytes = sizeof(T) * size;
        bytes = (bytes + sizeof(uint64_t) - 1)& ~(sizeof(uint64_t)  - 1);

        this->dma_total_tx = shl__memset_dma(meminfo, val, bytes, &dma_compl_tx);
    }

    if (this->dma_total_tx == 0) {
        this->dma_compl_tx = 0;
        return shl__memset_openmp(array, &value, sizeof(T), size);
    }

    return 0;
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

    assert(dma_total_tx == 0 && dma_compl_tx == 0);

    if (get_conf()->use_dma && this->meminfo) {
        dma_total_tx = shl__memcpy_dma_from(src, meminfo, sizeof(T)*size,
                                            &dma_compl_tx);
    }

    if (dma_total_tx == 0) {
        this->dma_compl_tx = 0;
        shl__memcpy_openmp(array, src, sizeof(T), size);
    }
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

    if (get_conf()->use_dma && meminfo) {
        dma_total_tx = shl__memcpy_dma_to(meminfo, dest, sizeof(T) * size,
                                          &dma_compl_tx);
    }

    if (dma_total_tx == 0) {
        this->dma_compl_tx = 0;
        shl__memcpy_openmp(dest, array, sizeof(T), size);
    }
}

#endif /* __SHL_ARRAY_PARTITIONED */
