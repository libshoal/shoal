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
#define SHL_HUGEPAGE 1

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
#define SHL_PARTITION 1

/**
 * enable the numa trim feature
 */
#define SHL_NUMA_TRIM 1

/**
 * force using a static schedule for OpenMP loops
 */
#define SHL_STATIC 0

/**
 * allocation stride for distributed arrays
 */
#define SHL_DISTRIBUTION_STRIDE PAGESIZE

/**
 * threshold for DMA copies
 */
#define SHL_DMA_COPY_THRESHOLD (1024 * 1024)

/**
 * map the DMA device directly into our own address space
 */
#define SHL_BARRELFISH_DMA_DIRECT 1


/*
 * ===============================================================================
 * Backend specifict configuration
 * ===============================================================================
 */

#define SHL_BARRELFISH_XEON_PHI 0

/**
 * deprecated...
 */
#define SHL_BARRELFISH_USE_SHARED 0


/* forward declarations */
struct shl_mi_data;



int shl__node_get_range(int node, uintptr_t *min_base, uintptr_t *max_limit);


int shl__barrelfish_init(uint32_t *num_threads);

int shl__barrelfish_share_frame(struct shl_mi_data *mi);

/* additional allocation functions */
void* shl__malloc_distributed(size_t size, int opts, int *pagesize, size_t stride, void **ret_mi);
void *shl__malloc_partitioned(size_t size, int opts, int *pagesize, void **ret_mi);


#endif
