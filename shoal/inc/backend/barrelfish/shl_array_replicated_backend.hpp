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
void shl_array_replicated<T>::copy_from(T* src)
{
    if (!shl_array<T>::do_copy_in())
        return;

    assert(shl_array<T>::alloc_done);

    printf("shl_array_replicated[%s]: Copying to %d replicas\n",
           shl_base_array::name, num_replicas);

    int copied = 0;

    if (get_conf()->use_dma && this->meminfo) {
        copied = shl__memcpy_dma_from(src, this->meminfo,0, sizeof(T)*this->size);
    }

    if (copied == 0) {
        printf("shl_array_replicated[%s]: falling back to manual copy\n",
               this->name);
        for (int j = 0; j < num_replicas; j++) {
            for (unsigned int i = 0; i < shl_array<T>::size; i++) {
                rep_array[j][i] = src[i];
            }
        }
    }
}

template<class T>
int shl_array_replicated<T>::copy_from_array(shl_array<T> *src)
{
    int copied = 0;

    if (get_conf()->use_dma && this->meminfo) {
        size_t elements = (src->get_size() > this->size) ? this->size : src->get_size();
        copied = shl__memcpy_dma_array(src->get_meminfo(), this->meminfo, sizeof(T) * elements);
    }

    if (copied == 0) {
        printf("shl_array_single_node<T>::copy_from_array copied == 0\n");
        return shl_array<T>::copy_from_array(src);
    }

    return 0;
}

template<class T>
int shl_array_replicated<T>::init_from_value(T value)
{
    int written = 0;

    assert(this->alloc_done);
    if (get_conf()->use_dma && this->meminfo) {
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

        size_t bytes = sizeof(T) * this->size;
        bytes = (bytes + sizeof(uint64_t) - 1)& ~(sizeof(uint64_t)  - 1);

        written = shl__memset_dma(this->meminfo, val, bytes);
    }

    if (written == 0) {
        return shl_array<T>::init_from_value(value);
    }

    return 0;
}

#endif /* __SHL_ARRAY */
