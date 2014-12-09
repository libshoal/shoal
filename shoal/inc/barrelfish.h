#ifndef SHL__BARRELFISH_H
#define SHL__BARRELFISH_H

#include <barrelfish/caddr.h> // for capref

/**
 * \brief This file contains Barrelfish specific declarations
 */

/*
 * ===============================================================================
 * SHOAL configuration / environment settings
 * ===============================================================================
 */

/**
 * use hugepages for memory allocation
 */
#define SHL_HUGEPAGE 0

/**
 * use replicated arrays
 */
#define SHL_REPLICATION 1

/**
 * use distributed arrays
 */
#define SHL_DISTRIBUTION 0

/**
 * use partitioned arrays
 */
#define SHL_PARTITION 0

/**
 * enable the numa trim feature
 */
#define SHL_NUMA_TRIM 1

/**
 * force using a static schedule for OpenMP loops
 */
#define SHL_STATIC 1


/**
 *
 */
//#define SHL_DBG_ARRAY

/*
 * ===============================================================================
 * Backend specifict configuration
 * ===============================================================================
 */

/**
 *
 */
#define SHL_BARRELFISH_USE_SHARED 0

///
#define SHL_RAM_MIN_BASE (64UL * 1024 * 1024 * 1024)

///
#define SHL_RAM_MAX_LIMIT (512UL * 1024 * 1024 * 1024)


/**
 * \brief
 */
struct shl_mi_data {
    struct capref frame;    ///<
    lvaddr_t vaddr;         ///<
    size_t size;            ///<
    uint32_t opts;          ///<
};

struct shl_mi_header {
    size_t num;
    struct shl_mi_data *data;
};


int shl__node_get_range(int node,
                   uintptr_t *min_base,
                  uintptr_t *max_limit);


int shl__barrelfish_init(size_t num_threads);
int shl__barrelfish_share_frame(struct shl_mi_data *mi);

#define MALLOC_VADDR_START (1UL << 36)



#endif
