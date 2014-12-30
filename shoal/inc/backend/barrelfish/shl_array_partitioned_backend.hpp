/*
 * Copyright (c) 2014 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef __SHL_ARRAY_PARTITIONED_BACKEND
#define __SHL_ARRAY_PARTITIONED_BACKEND




template<class T>
int shl_array_partitioned<T>::alloc(void)
{
    if (!this->do_alloc())
            return 0;

    this->print();

    assert(!this->alloc_done);

    this->array = (T *)shl__malloc_partitioned(this->size * sizeof(T),
                                               this->get_options(), &this->pagesize,
                                               &this->meminfo);

    if (this->array == NULL) {
        return -1;
    }


    this->alloc_done = true;

    return 0;
}

template<class T>
void shl_array_partitioned<T>::copy_from(T* src)
{
    if (!shl_array<T>::do_copy_in())
        return;

    assert(shl_array<T>::alloc_done);

    int copied = 0;

    if (this->meminfo) {
        copied = shl__memcpy_dma_from(src, this->meminfo, 0, sizeof(T)*this->size);
    }

    if (copied == 0) {

        printf("shl_array_partitioned[%s]: falling back to manual copy\n",
               this->name);
        shl_array<T>::copy_from(src);
    }
}


#endif /* __SHL_ARRAY_PARTITIONED */
