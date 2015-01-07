/*
 * Copyright (c) 2014 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef __SHL_CONFIGURATION
#define __SHL_CONFIGURATION

// --------------------------------------------------
// Program configuration
// --------------------------------------------------
class Configuration {

 public:
    Configuration(void);


    ~Configuration(void) {

        //        delete node_mem_avail;
    }

    // Should large pages be used
    bool use_hugepage = false;

    // Should replication be used
    bool use_replication = false;

    // Should distribution be used
    bool use_distribution = false;

    // Should partitioning be used
    bool use_partition = false;

    // Number of NUMA nodes
    int num_nodes = 0;

    int num_nodes_active = 0;

    // How much memory is available on the machine
    long mem_avail = 0;

    // How much memory is available on each node
    long* node_mem_avail = 0;

    // Number of threads
    size_t num_threads = 0;

    // NUMA trim
    // Use this to instruct the runtime to not replica on all n NUMA nodes,
    // but only n/trim_factor ones.
    int numa_trim = 1;

    // stride for mapping distributed
    size_t stride = 0;

    /* */
    bool use_dma = false;

    /* DMA device related settings */
    struct shl__memcpy_setup memcpy_setup;
};

Configuration* get_conf(void);

#endif
