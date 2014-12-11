/*
 * Copyright (c) 2014 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef __SHL_ARRAY_SINGLE_NODE
#define __SHL_ARRAY_SINGLE_NODE

#include <cstdlib>
#include <cstdarg>

#include "shl.h"
#include "shl_configuration.hpp"

/*
 * \brief Array implementing single-node replication
 */
template<class T>
class shl_array_single_node : public shl_array<T> {
 public:
    /**
     * \brief Initialize single_node array
     */
    shl_array_single_node(size_t s, const char *_name) :
                    shl_array<T>(s, _name)
    {
    }
    ;

    shl_array_single_node(size_t s, const char *_name, void *mem, void *data) :
                    shl_array<T>(s, _name, mem, data)
    {
    }
    ;

    virtual int alloc(void)
    {
        assert (shl_array<T>::alloc()==0);

        if (this->is_used) {

            // Map everything on single node
            printf("Forcing single-node allocation .. %p\n", shl_array<T>::array);
            for (unsigned int i=0; i<shl_array<T>::size; i++) {

                shl_array<T>::array[i] = 0;
            }
        }

        return 0;
    }

    virtual int get_options(void)
    {
        return shl_array<T>::get_options() | SHL_MALLOC_PARTITION;
    }

 protected:
    virtual void print_options(void)
    {
        shl_array<T>::print_options();
        printf("single-node=[X]");
    }

};

#endif
