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
    return shl_array<T>::alloc();
}


template<class T>
void shl_array_distributed<T>::copy_from(T* src)
{
    shl_array<T>::copy_from(src);
}

template<class T>
int shl_array_distributed<T>::copy_from_array(shl_array<T> *src)
{
    return shl_array<T>::copy_from_array(src);
}
template<class T>
int shl_array_distributed<T>::init_from_value(T value)
{
    return shl_array<T>::init_from_value(value);
}

#endif /* __SHL_ARRAY_DISTRIBUTED_BACKEND */
