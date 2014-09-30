/*
 * Copyright (c) 2014 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */
#include <stdio.h>

extern "C" {
#include <omp.h>
}
#include "shl.h"
#include "shl_internal.h"
#include "shl_timer.hpp"
#include "shl_configuration.hpp"

#ifndef BARRELFISH
#include "numa.h"
#endif

#ifdef PAPI
static int EventSet;
#endif

Configuration::Configuration(void) {
#ifdef BARRELFISH
    use_hugepage = SHL_HUGEPAGE;
    use_replication = SHL_REPLICATION;
    use_distribution = SHL_DISTRIBUTION;
    use_partition = SHL_PARTITION;
#else
    // Configuration based on environemnt
    use_hugepage = get_env_int("SHL_HUGEPAGE", 1);
    use_replication = get_env_int("SHL_REPLICATION", 1);
    use_distribution = get_env_int("SHL_DISTRIBUTION", 1);
    use_partition = get_env_int("SHL_PARTITION", 1);
#endif
    // NUMA information
    num_nodes = numa_max_node();
    node_mem_avail = new long[num_nodes];
    mem_avail = 0;

    for (int i=0; i<=numa_max_node(); i++) {
        numa_node_size(i, &(node_mem_avail[i]));
        mem_avail += node_mem_avail[i];
    }
}

void shl__end(void)
{
    printf("Time for copy: %.6f\n", shl__get_timer());
#ifdef DEBUG
    printf("Number of lookups: %ld\n", num_lookup);
#endif
}

#ifdef PAPI
void papi_stop(void)
{
    // Stop PAPI events
    long long values[1];
    if (PAPI_stop(EventSet, values) != PAPI_OK) handle_error(1);
    printf("Stopping PAPI .. ");
    print_number(values[0]); printf("\n");
    printf("END PAPI\n");
}

void papi_init(void)
{
    printf("Initializing PAPI .. ");

    // Initialize PAPI and make sure version matches
    int retval = PAPI_library_init(PAPI_VER_CURRENT);
    if (retval != PAPI_VER_CURRENT && retval > 0) { fprintf(stderr,"PAPI library version mismatch!\n"); exit(1); }

    // More checks that PAPI is running?
    if (retval < 0) handle_error(retval);
    retval = PAPI_is_initialized();
    if (retval != PAPI_LOW_LEVEL_INITED) handle_error(retval);

    if (PAPI_thread_init(pthread_self) != PAPI_OK)
        exit(1);

    printf("DONE\n");
}

void papi_start(void)
{
    printf("Starting PAPI .. ");

    // Add events to be monitored
    EventSet = PAPI_NULL;
    if (PAPI_create_eventset(&EventSet) != PAPI_OK) handle_error(1);
    if (PAPI_add_event(EventSet, PAPI_L2_DCM) != PAPI_OK) handle_error(1);

    // PAPI_TLB_DM

    // Start monitoring
    if (PAPI_start(EventSet) != PAPI_OK)
        handle_error(1);
    printf("DONE\n");
}
#endif

int shl__get_rep_id(void)
{
    return shl__lookup_rep_id(omp_get_thread_num());

}

int shl__lookup_rep_id(int core)
{
#ifdef DEBUG
    __sync_fetch_and_add(&num_lookup, 1);
#endif
    assert(omp_get_num_threads()<MAXCORES);
    assert(replica_lookup[core]>=0);

    return replica_lookup[core];
}

int shl__get_num_replicas(void)
{
    return numa_max_node()+1;
}

void shl__repl_sync(void* src, void **dest, size_t num_dest, size_t size)
{
#ifdef BARRELFISH
    assert(!"NYI");
#else
    for (size_t i=0; i<num_dest; i++) {

        #pragma omp parallel for
        for (size_t j=0; j<size; j++) {

            ((char*) dest[i])[j] = ((char*) src)[j];
        }
    }
#endif
}

void shl__init_thread(int thread_id)
{
#ifdef PAPI
    PAPI_register_thread();
#endif
}

#ifdef PAPI
void handle_error(int retval)
{
    printf("PAPI error %d: %s\n", retval, PAPI_strerror(retval));
    exit(1);
}
#endif


// translate: virtual COREID -> physical COREID
coreid_t *affinity_conf = NULL;

void shl__init(size_t num_threads, bool partitioned_support)
{
    printf("SHOAL (v %s) initialization .. %zu threads .. ",
           VERSION, num_threads);
    assert(num_threads>0);

    Configuration *conf = get_conf();
    assert (numa_available()>=0);

    conf->num_threads = num_threads;

    assert (!get_conf()->use_partition || partitioned_support || !"Compile with -DSHL_STATIC");
    if (!get_conf()->use_partition && partitioned_support) {
        printf(ANSI_COLOR_YELLOW "WARNING: " ANSI_COLOR_RESET
               "partitioning disabled, but program is compiled with "
               "partition support\n");
    }

#ifdef BARRELFISH
    if (shl__barrelfish_init(num_threads)) {
        printf(ANSI_COLOR_RED "ERROR: Could not initialize Barrelfish backend\n"
               ANSI_COLOR_RESET);
    }
#else
    affinity_conf = parse_affinity (false);

    if (affinity_conf==NULL) {
        printf(ANSI_COLOR_RED "WARNING:" ANSI_COLOR_RESET " no affinity set! "
               "Disabling replication! "
               "Use SHL_CPU_AFFINITY\n");
        conf->use_replication = false;
    }

    for (int i=0; i<MAXCORES; i++)
        replica_lookup[i] = -1;

    if (conf->use_replication) {
        for (size_t i=0; i<num_threads; i++) {
            replica_lookup[i] = numa_cpu_to_node(affinity_conf[i]);
            printf("replication: CPU % 3zu is on node % 2d\n", i, replica_lookup[i]);
        }
    }

#endif

    // Prevent numa_alloc_onnode fall back to allocating memory elsewhere
    numa_set_strict (true);

#ifdef DEBUG
    printf(ANSI_COLOR_RED "WARNING:" ANSI_COLOR_RESET " debug enabled in #define\n");
#endif

#ifdef SHL_DEBUG
    printf(ANSI_COLOR_RED "WARNING:" ANSI_COLOR_RESET " debug enabled in Makefile\n");
#endif

#ifdef PAPI
    papi_init();
#endif

    printf("[%c] Replication\n", conf->use_replication ? 'x' : ' ');
    printf("[%c] Distribution\n", conf->use_distribution ? 'x' : ' ');
    printf("[%c] Partition\n", conf->use_partition ? 'x' : ' ');
    printf("[%c] Hugepage\n", conf->use_hugepage ? 'x' : ' ');
}

int shl__num_threads(void)
{
    return get_conf()->num_threads;
}

int shl__get_tid(void)
{
    return omp_get_thread_num();
}
