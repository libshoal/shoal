/*
 * Copyright (c) 2014 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */
#include <stdio.h>
#include <algorithm>
#include <climits>

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
    numa_trim = get_env_int("SHL_NUMA_TRIM", 1);
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

void shl__start(void)
{
#ifdef PAPI
    papi_start();
#endif
}

void shl__end(void)
{
#ifdef PAPI
    papi_stop();
#endif

    printf("Time for copy: %.6f\n", shl__get_timer());
#ifdef DEBUG
    printf("Number of lookups: %ld\n", num_lookup);
#endif
}

#ifdef PAPI
#define PAPI_EVENTS_MAX 16
void papi_stop(void)
{
    // Stop PAPI events
    long long values[PAPI_EVENTS_MAX];
    int events[PAPI_EVENTS_MAX];

    printf("Stopping PAPI .. \n");

    // Read performance counters
    if (PAPI_stop(EventSet, values) != PAPI_OK) handle_error(1);

    // Read event IDs
    int num_events = PAPI_EVENTS_MAX;
    if (PAPI_list_events(EventSet, events, &num_events) != PAPI_OK) handle_error(1);

    for (int i=0; i<num_events; i++) {

        char event_name[PAPI_MAX_STR_LEN];
        if (PAPI_event_code_to_name(events[i], event_name) != PAPI_OK) handle_error(1);

        char number_buf[1024];
        convert_number(values[i], number_buf);

        printf(ANSI_COLOR_BLUE "PAPI: " ANSI_COLOR_RESET "%20s %15lld %15s\n",
               event_name, values[i], number_buf);
    }
}

static bool shl__papi_init_done = 0;
void papi_init(void)
{
    if (shl__papi_init_done)
        return;
    shl__papi_init_done = 1;
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
    printf("Starting PAPI .. \n");

    // Create event set
    EventSet = PAPI_NULL;
    if (PAPI_create_eventset(&EventSet) != PAPI_OK) handle_error(1);

    // Read PAPI event from the environment variable
    char* papi_events =  get_env_str("SHL_PAPI", "PAPI_L2_DCM");
    char seps[] = ",";

    char input[PAPI_EVENTS_MAX][PAPI_MAX_STR_LEN];
    char* token;
    int i = 0;

    token = strtok (papi_events, seps);
    while (token != NULL) {
        sscanf (token, "%s", input[i++]);
        token = strtok (NULL, seps);
    }

    // Add PAPI events
    for (int j=0; j<i; j++) {

        printf("PAPI: monitoring event [%s] .. ", input[j]);

        // Translate ..
        int event_code;
        if (PAPI_event_name_to_code(input[j], &event_code) != PAPI_OK) handle_error(1);
        printf(" %d\n", event_code);

        // Add events to be monitored
        if (PAPI_add_event(EventSet, event_code) != PAPI_OK) handle_error(1);
    }

    // Start monitoring
    if (PAPI_start(EventSet) != PAPI_OK) handle_error(1);
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
    assert(core<MAXCORES);
    assert(replica_lookup[core]>=0);

    int repid = replica_lookup[core];

    return repid/get_conf()->numa_trim;
}

int shl__get_num_replicas(void)
{
    int trim = get_conf()->numa_trim;
    int num_nodes = numa_max_node()+1;

    assert (num_nodes%2 == 0 || trim == 1);
    assert (num_nodes>=trim);

    return num_nodes/trim;
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

/**
 * \brief Initialize shoal library
 *
 */
void shl__init(size_t num_threads, bool partitioned_support)
{
    printf("SHOAL (v %s) initialization .. %zu threads .. ",
           VERSION, num_threads);
    assert(num_threads>0);

    Configuration *conf = get_conf();
    assert (numa_available()>=0);

    conf->num_threads = num_threads;

#ifdef PAPI
    printf(ANSI_COLOR_BLUE "PAPI .. " ANSI_COLOR_RESET);
#endif

    assert (!get_conf()->use_partition || partitioned_support || !"Compile with -DSHL_STATIC");
    if (!get_conf()->use_partition && partitioned_support) {
        printf(ANSI_COLOR_YELLOW "\n .. WARNING: " ANSI_COLOR_RESET
               "partitioning disabled, but program is compiled with "
               "partition support\n");
    }

    if (get_conf()->use_hugepage) {
        if (!shl__check_hugepage_support()) {

            printf(" \n .. " ANSI_COLOR_RED "FAILURE:" ANSI_COLOR_RESET \
                   " no hugepage support on this machine, aborting .. \n");
            exit(2);
        }
    }

#ifdef BARRELFISH
    if (shl__barrelfish_init(num_threads)) {
        printf(ANSI_COLOR_RED "ERROR: Could not initialize Barrelfish backend\n"
               ANSI_COLOR_RESET);
    }
#else
    affinity_conf = parse_affinity (false);

    for (int i=0; i<MAXCORES; i++)
        replica_lookup[i] = -1;

    if (affinity_conf==NULL) {
        printf(ANSI_COLOR_RED "WARNING:" ANSI_COLOR_RESET " no affinity set! "
               "Disabling replication! "
               "Use SHL_CPU_AFFINITY\n");
        conf->use_replication = false;
    }
    else {
        for (size_t i=0; i<num_threads; i++) {
            replica_lookup[i] = numa_cpu_to_node(affinity_conf[i]);

            if (conf->use_replication) {
                printf("replication: CPU %03zu is on node % 2d\n",
                       i, shl__lookup_rep_id(i));
            }
        }
        for (int i=0; i<shl__get_num_replicas(); i++) {

            printf("replica %d - coordinator %d\n", i, shl__rep_coordinator(i));
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

    // Print configuration
    printf("[%c] Replication\n", conf->use_replication ? 'x' : ' ');
    printf("[%c] Distribution\n", conf->use_distribution ? 'x' : ' ');
    printf("[%c] Partition\n", conf->use_partition ? 'x' : ' ');
    printf("[%c] Hugepage\n", conf->use_hugepage ? 'x' : ' ');
    printf("[%d] NUMA trim\n", conf->numa_trim);
}

int shl__num_threads(void)
{
    return get_conf()->num_threads;
}

int shl__get_tid(void)
{
    return omp_get_thread_num();
}

int shl__rep_coordinator(int rep)
{
    int c = INT_MAX;
    for (int i=0; i<shl__num_threads(); i++) {

        if (shl__lookup_rep_id(i)==rep)
            c = std::min(c, i);
    }

    return c;
}

bool shl__is_rep_coordinator(int tid)
{
    for (int j=0; j<shl__get_num_replicas(); j++) {

        if( shl__rep_coordinator(j)==tid)
            return true;
    }

    return false;
}
