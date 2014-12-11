/*
 * Copyright (c) 2014 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#include <stdio.h>
#include <barrelfish/barrelfish.h>

#include "shl_internal.h"




/**
 * \brief initializes the copy infrastructure
 *
 * \returns 0 on SUCCESS
 *          non zero on FAILURE
 */
int shl__copy_init(void);
int shl__copy_init(void)
{
    return 0;
}


void** shl__copy_array(void *src,
                       size_t size,
                       bool is_used,
                       bool is_ro,
                       const char* array_name)
{


    assert(!"NYI");
    return 0;
}

void shl__copy_back_array(void **src,
                          void *dest,
                          size_t size,
                          bool is_copied,
                          bool is_ro,
                          bool is_dynamic,
                          const char* array_name)
{
    assert(!"NYI");
}

void shl__copy_back_array_single(void *src,
                                 void *dest,
                                 size_t size,
                                 bool is_copied,
                                 bool is_ro,
                                 bool is_dynamic,
                                 const char* array_name)
{
    assert(!"NYI");
}
