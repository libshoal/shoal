/*
 * Copyright (c) 2014 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef __SHL_ARRAY_DISTRIBUTED_BACKEND
#define __SHL_ARRAY_DISTRIBUTED_BACKEND

template<class T>
int shl_array_distributed<T>::alloc(void)
{
    if (!this->do_alloc())
            return 0;

    this->print();

    assert(!this->alloc_done);

    this->array = (T *)shl__malloc_distributed(this->size * sizeof(T),
                                               this->get_options(), &this->pagesize,
                                               get_conf()->stride, &this->meminfo);

    if (this->array == NULL) {
        return -1;
    }

    this->alloc_done = true;

    return 0;

}

template<class T>
void shl_array_distributed<T>::copy_from(T* src)
{
    if (!shl_array<T>::do_copy_in())
        return;

    assert(shl_array<T>::alloc_done);

    shl_array<T>::copy_from(src);
}

template<class T>
int shl_array_distributed<T>::copy_from_array(shl_array<T> *src)
{
    if (get_conf()->use_dma && this->meminfo) {
        size_t elements = (src->get_size() > this->size) ? this->size : src->get_size();
        copied = shl__memcpy_dma_array(src->get_meminfo(), this->meminfo, sizeof(T) * elements);
    }

    if (copied == 0) {
        return shl_array<T>::copy_from_array(src);
    }
}

template<class T>
int shl_array_distributed<T>::init_from_value(T value)
{
    return shl_array<T>::init_from_value(value);
}


#endif /* __SHL_ARRAY_DISTRIBUTED_BACKEND */
