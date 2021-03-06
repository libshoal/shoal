/*
 * Copyright (c) 2014 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef __SHL_ARRAY_SINGLE_NODE_BACKEND
#define __SHL_ARRAY_SINGLE_NODE_BACKEND

template<class T>
int shl_array_single_node<T>::alloc(void)
{
    return shl_array<T>::alloc();
}

#endif
