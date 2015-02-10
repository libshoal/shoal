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
    assert(this->dma_total_tx == 0 && this->dma_compl_tx == 0);

    if (!get_conf()->use_dma || !meminfo) {
        return -1;
    }

    T *src = src_array->get_array();
    if (!array || !src) {
        printf("shl_array<T>::copy_from_array: Failed no pointers\n");
        return -1;
    }

    if (size != src_array->get_size()) {
        printf("shl_array<T>::copy_from_array: not matching sizes\n");
        return -1;
    }

    dma_total_tx = shl__memcpy_dma_array(src_array->get_meminfo(), meminfo,
                                         sizeof(T) * elements,
                                         &dma_compl_tx);

    if (dma_total_tx == 0) {
        dma_compl_tx = 0;
        return -1;
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
int shl_array<T>::init_from_value_async(T value, size_t elements)
{
    assert(dma_total_tx == 0 && dma_compl_tx == 0);

    if (!get_conf()->use_dma || !meminfo) {
        return -1;
    }

    if (sizeof(T) > sizeof(uint64_t)) {
        return -1;
    }

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

    size_t bytes = sizeof(T) * elements;
    bytes = (bytes + sizeof(uint64_t) - 1)& ~(sizeof(uint64_t)  - 1);

    this->dma_total_tx = shl__memset_dma(meminfo, val, bytes, &dma_compl_tx);

    if (dma_total_tx == 0) {
        dma_compl_tx = 0;
        return -1;
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
int shl_array<T>::copy_from_async(T* src, size_t elements)
{
    if (!do_copy_in()) {
        return 0;
    }

    if (!get_conf()->use_dma || !meminfo) {
        return -1;
    }

    assert(dma_total_tx == 0 && dma_compl_tx == 0);

    dma_total_tx = shl__memcpy_dma_from(src, meminfo, sizeof(T)*elements,
                                        &dma_compl_tx);

    if (dma_total_tx == 0) {
        dma_compl_tx = 0;
        return -1;
    }

    return 0;
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
    assert(dma_total_tx == 0 && dma_compl_tx == 0);

    if (!get_conf()->use_dma || !meminfo) {
        return -1;
    }

    dma_total_tx = shl__memcpy_dma_to(meminfo, dest, sizeof(T) * elements,
                                      &dma_compl_tx);

    if (dma_total_tx == 0) {
        dma_compl_tx = 0;
        return -1;
    }

    return 0;
}

template<class T>
int copy_from_array(shl_array<T> *src_array)
{
    size_t elements = (src_array->get_size() > size) ? size : src_array->get_size();

    size_t start = (elements * ARRAY_COPY_DMA_RATION);

    tPrepare.start();

    if ((start > 0) && copy_from_array_async(src_array, start) != 0) {
        start = 0;
    }

    tPrepare.stop();


    tCopy.start();
    if (start < elements) {
        T* src = src_array->get_array();
#pragma omp parallel for
        for (size_t i = start; i < elements; ++i) {
            array[i] = src[i];
        }
    }
    tCopy.stop();

    tBarrier.start();
    copy_barrier();
    tBarrier.stop();

    return 0;
}

template<class T>
void copy_barrier(void)
{
    if (dma_total_tx == 0) {
        return;
    }

    volatile size_t *vol_dma_compl = &dma_compl_tx;

    do {
        poll_count++;
        shl__memcpy_poll();
    } while(dma_total_tx != *vol_dma_compl);

    dma_total_tx = 0;
    dma_compl_tx = 0;
}


#endif /* __SHL_ARRAY_PARTITIONED */
