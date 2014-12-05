/*
 * Copyright (c) 2014 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef __SHL_ARRAY_PARTITIONED
#define __SHL_ARRAY_PARTITIONED

#include <cstdlib>
#include <cstdarg>
#include <cstring> // memset
#include <iostream>
#include <limits>

#include "shl.h"
#include "shl_timer.hpp"
#include "shl_configuration.hpp"
#include "shl_array.hpp"
/**
 * \brief Array implementing a partitioned array
 *
 */
template<class T>
class shl_array_partitioned : public shl_array<T> {
 public:
    /**
     * \brief Initialize partitioned array
     */
    shl_array_partitioned(size_t _size, const char *_name) :
                    shl_array<T>(_size, _name)
    {
    }

    /**
     *
     * @param _size
     * @param _name
     * @param mem
     * @param data
     */
    shl_array_partitioned(size_t _size, const char *_name, void *mem, void *data) :
                    shl_array<T>(_size, _name, mem, data)
    {
    }


    virtual void alloc(void)
    {
        shl_array<T>::alloc();

        // We need to force memory allocation in here (which sucks,
        // since this file is supposed to be platform independent),
        // since in shl_malloc, we do not know the size of elements,
        // and hence, we cannot establish a correct memory mapping for
        // partitioning in there.
#ifndef BARRELFISH
#pragma omp parallel for schedule(static, 1024)
#endif
        for (unsigned int i = 0; i < shl_array<T>::size; i++) {

            shl_array<T>::array[i] = 0;
        }

    }

    virtual int get_options(void)
    {
        return shl_array<T>::get_options() | SHL_MALLOC_PARTITION;
    }

 protected:
    virtual void print_options(void)
    {
        shl_array<T>::print_options();
        printf("partition=[X]");
    }

};

#endif /* __SHL_ARRAY_PARTITIONED */
