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

    shl_array<T>::alloc();

    // We need to force memory allocation in here (which sucks,
    // since this file is supposed to be platform independent),
    // since in shl_malloc, we do not know the size of elements,
    // and hence, we cannot establish a correct memory mapping for
    // partitioning in there.
#pragma omp parallel for schedule(static, 1024)
    for (unsigned int i = 0; i < shl_array<T>::size; i++) {

        shl_array<T>::array[i] = 0;
    }

    return 0;
}

template<class T>
void shl_array_partitioned<T>::copy_from(T* src)
{
    shl_array<T>::copy_from(src);
}



#endif /* __SHL_ARRAY_PARTITIONED */
