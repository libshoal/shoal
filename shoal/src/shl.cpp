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
#include "shl_multitimer.hpp"
#include "shl_configuration.hpp"

#ifndef BARRELFISH
#include "numa.h"
#endif

#ifdef BARRELFISH
#include <pci/devids.h>

// for debugging purposes
#define MEMCPY_DMA_DEVICE 0x0e20
#define MEMCPY_DMA_VENDOR PCI_VENDOR_INTEL
#define MEMCPY_DMA_COUNT 0

#endif

#ifdef PAPI
static int EventSet;
#endif


Configuration::Configuration(void) {
#ifdef BARRELFISH
    use_hugepage = shl__get_global_conf("global", "hugepage", SHL_HUGEPAGE);
    use_replication = shl__get_global_conf("global", "replication", SHL_REPLICATION);
    use_distribution = shl__get_global_conf("global", "distribution", SHL_DISTRIBUTION);
    use_partition = shl__get_global_conf("global", "partitioning", SHL_PARTITION);
    numa_trim = shl__get_global_conf("global", "trim", SHL_NUMA_TRIM);
    stride = shl__get_global_conf("global", "stride", SHL_DISTRIBUTION_STRIDE);
    static_schedule = shl__get_global_conf("global", "static", SHL_STATIC);

    int dma_enable = shl__get_global_conf("dma", "enable", 0);
    if (dma_enable) {
        memcpy_setup.device.id = shl__get_global_conf("dma", "device", MEMCPY_DMA_DEVICE);
        memcpy_setup.device.vendor = shl__get_global_conf("dma", "vendor", MEMCPY_DMA_VENDOR);
        memcpy_setup.count = shl__get_global_conf("dma", "device_count", MEMCPY_DMA_COUNT);
        assert(memcpy_setup.count);
        memcpy_setup.pci = (struct shl__pci_address *)calloc(memcpy_setup.count, sizeof(struct shl__pci_address));
        assert(memcpy_setup.pci);
        for (uint32_t i = 0; i < memcpy_setup.count; ++i) {
            char buf[15];
            snprintf(buf, 15, "dma_device_%u", i);
            memcpy_setup.pci[i].bus = shl__get_global_conf(buf, "bus", PCI_DONT_CARE);
            memcpy_setup.pci[i].device = shl__get_global_conf(buf, "dev", PCI_DONT_CARE);
            memcpy_setup.pci[i].function = shl__get_global_conf(buf, "fun", PCI_DONT_CARE);
            memcpy_setup.pci[i].count = shl__get_global_conf(buf, "count", 0);
        }


    } else {
        memset(&memcpy_setup, 0, sizeof(struct shl__memcpy_setup));
    }

#else
    // Configuration based on environemnt
    use_hugepage = shl__get_global_conf("global", "hugepage", get_env_int("SHL_HUGEPAGE", 1));
    use_replication = shl__get_global_conf("global", "replication", get_env_int("SHL_REPLICATION", 1));
    use_distribution = shl__get_global_conf("global", "distribution", get_env_int("SHL_DISTRIBUTION", 1));
    use_partition = shl__get_global_conf("global", "partitioning", get_env_int("SHL_PARTITION", 1));
    numa_trim = shl__get_global_conf("global", "trim", get_env_int("SHL_NUMA_TRIM", 1));
    stride = shl__get_global_conf("global", "stride", PAGESIZE);
    static_schedule = shl__get_global_conf("global", "static", SHL_STATIC);
#endif

    // NUMA information
    num_nodes = shl__max_node();
    num_nodes_active = 0;
    node_mem_avail = new long[num_nodes];
    mem_avail = 0;
    use_dma = 0;
    for (int i=0; i<=shl__max_node(); i++) {
        shl__node_size(i, &(node_mem_avail[i]));
        mem_avail += node_mem_avail[i];
    }
}

static MultiTimer CTimer(1);

void shl__start(void)
{
    CTimer.start();
#ifdef PAPI
    papi_start();
#endif
}

void shl__thread_init(void)
{
#ifdef PAPI
    if (PAPI_thread_init(pthread_self) != PAPI_OK)
        exit(1);
#endif
}

void shl__end(void)
{
#ifdef PAPI
    papi_stop();
#endif
    CTimer.stop("SOAL_Computation");
    CTimer.print();

    shl__lua_deinit();
#ifdef DEBUG
    printf("Number of lookups: %ld\n", num_lookup);
#endif
}

#ifdef PAPI
#define PAPI_EVENTS_MAX 16
static bool shl__papi_running = 0;
static bool shl__papi_init_done = 0;
void papi_stop(void)
{
    if (!shl__papi_running)
        return;

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
    char* papi_events =  get_env_str("SHL_PAPI", "");
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

    if (i>0) {
        // Start monitoring
        if (PAPI_start(EventSet) != PAPI_OK) handle_error(1);
        printf("DONE\n");

        shl__papi_running = 1;
    } else {

        printf(ANSI_COLOR_BLUE "PAPI: " ANSI_COLOR_RESET "no events given, will not activate");
    }
}
#endif

int shl__get_rep_id(void)
{
#ifdef BARRELFISH
    return shl__lookup_rep_id(omp_get_thread_num());
#else
    return shl__lookup_rep_id(omp_get_thread_num());
#endif
}

/**
 * \brief Lookup replica to be used by given _virtual_ core.
 *
 * Virtual cores are always contiguous (0..num_threads-1)
 */
int shl__lookup_rep_id(int core)
{
#ifdef DEBUG
    __sync_fetch_and_add(&num_lookup, 1);
#endif
#ifndef BARRELFISH
    assert(omp_get_num_threads()<MAXCORES);
#endif
    assert(core<MAXCORES);
    assert(replica_lookup[core]>=0);

    int repid = replica_lookup[core];

    return repid/get_conf()->numa_trim;
}

int shl__get_num_replicas(void)
{
#ifdef BARRELFISH
    return get_conf()->num_nodes_active;
#else
    int trim = get_conf()->numa_trim;
    int num_nodes = shl__max_node()+1;

    assert (num_nodes%2 == 0 || trim == 1);
    assert (num_nodes>=trim);

    return num_nodes/trim;
#endif
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

#ifdef BARRELFISH
    if (shl__barrelfish_init(num_threads)) {
        printf(ANSI_COLOR_RED "ERROR: Could not initialize Barrelfish backend\n"
               ANSI_COLOR_RESET);
        exit(EXIT_FAILURE);
    }
#endif

    printf("LUA init ..\n ");
    Timer t; t.start();
    shl__lua_init();
    printf("done (%f).\n", t.stop());
    Configuration *conf = get_conf();

    if (shl__check_numa_availability() < 0) {
        printf(" \n .. " ANSI_COLOR_RED "FAILURE:" ANSI_COLOR_RESET \
               " NUMA not available .. \n");
        exit(1);
    }

    assert (shl__check_numa_availability()>=0);

    conf->num_threads = num_threads;

    if (conf->memcpy_setup.count) {
        if (shl__memcpy_init(&conf->memcpy_setup)) {
            printf("DMA Copying: " ANSI_COLOR_RED "\n Disabled (Error in initializing). "
                            ANSI_COLOR_RESET "\n");
        } else {
            printf("DMA Copying: " ANSI_COLOR_GREEN "\n Enabled. " ANSI_COLOR_RESET "\n");
            conf->use_dma = 1;
        }

    } else {
        printf("DMA Copying: " ANSI_COLOR_YELLOW "\n Disabled. " ANSI_COLOR_RESET "\n");
    }

#ifndef BARRELFISH

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

    affinity_conf = parse_affinity (false);
#else
    affinity_conf = (coreid_t *)-1;
#endif

    for (int i=0; i<MAXCORES; i++)
        replica_lookup[i] = -1;

    if (affinity_conf==NULL) {
        printf(ANSI_COLOR_RED "WARNING:" ANSI_COLOR_RESET " no affinity set! "
               "Disabling replication! "
               "Use SHL_CPU_AFFINITY\n");
        conf->use_replication = false;
        assert (!"Do we really want to support runs without affinity?");
    }

#ifdef BARRELFISH
#define CPU_AFF_CONF i
#else
#define CPU_AFF_CONF affinity_conf[i]
#endif

    int max_node = 0;
    int node_id = 0;
    for (coreid_t i=0; i<num_threads; i++) {
        node_id = shl__node_from_cpu(CPU_AFF_CONF);
        replica_lookup[i] = node_id;
        if (max_node < node_id) {
            max_node = node_id;
        }

        if (conf->use_replication) {
            printf("replication: CPU %03" PRIuCOREID " is on node % 2d\n",
                   CPU_AFF_CONF, shl__lookup_rep_id(i));
        }
    }
    get_conf()->num_nodes_active = max_node + 1;

    for (int i=0; i<shl__get_num_replicas(); i++) {
        printf("replica %d - coordinator %d\n", i, shl__rep_coordinator(i));
    }

    // Prevent numa_alloc_onnode fall back to allocating memory elsewhere
    shl__set_strict_mode (true);


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
    printf("[%c] DMA enabled\n", conf->use_dma ? 'x' : ' ');
}

int shl__num_threads(void)
{
    return get_conf()->num_threads;
}

int shl__get_tid(void)
{
#ifndef BARRELFISH
    return omp_get_thread_num();
#else
    return 0;
#endif
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
