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

    if (!get_conf()->use_dma || !this->meminfo) {
        return -1;
    }

    assert(shl_array<T>::alloc_done);
    assert(this->dma_total_tx == 0 && this->dma_compl_tx == 0);

    printf("shl_array_replicated[%s]: Copying to %d replicas\n",
           shl_base_array::name, num_replicas);


    this->dma_total_tx = shl__memcpy_dma_from(src, this->meminfo,
                                              sizeof(T)*elements,
                                              &this->dma_compl_tx);


    if (this->dma_total_tx == 0) {
        this->dma_compl_tx = 0;
        return -1;
    }

    return 0;
}

template<class T>
int shl_array_replicated<T>::copy_from_array_async(shl_array<T> *src, size_t elements)
{
    if (!get_conf()->use_dma || !this->meminfo) {
        return -1;
    }

    assert(this->dma_total_tx == 0 && this->dma_compl_tx == 0);

    this->dma_total_tx = shl__memcpy_dma_array(src->get_meminfo(), this->meminfo,
                                                   sizeof(T) * elements,
                                                   &this->dma_compl_tx);
    if (this->dma_total_tx == 0) {
        this->dma_compl_tx = 0;
        return -1;
    }

    return 0;
}

template<class T>
int shl_array_replicated<T>::init_from_value_async(T value, size_t elements)
{
    if (!get_conf()->use_dma || !this->meminfo) {
        return -1;
    }

    if (sizeof(T) > sizeof(uint64_t)) {
        return -1;
    }

    assert(this->dma_total_tx == 0 && this->dma_compl_tx == 0);

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

    this->dma_total_tx = shl__memset_dma(this->meminfo, val, bytes,
                                         &this->dma_compl_tx);

    if (this->dma_total_tx == 0) {
        this->dma_compl_tx = 0;
        return -1;
    }

    return 0;
}

template<class T>
int shl_array_replicated<T>::copy_back_async(T* dest, size_t elements)
{
    assert(this->dma_total_tx == 0 && this->dma_compl_tx == 0);

    return -1;

#if 0
    if (!this->do_copy_back()) {
        return;
    }


    if (get_conf()->use_dma && meminfo) {
        this->dma_total_tx = shl__memcpy_dma_to(meminfo, dest, sizeof(T) * size,
                                                &dma_compl_tx);
    }

    if (dma_total_tx == 0) {
        this->dma_compl_tx = 0;
        shl__memcpy_openmp(dest, rep_array[0], sizeof(T), size);
    }
#endif
}


#endif /* __SHL_ARRAY */
