#ifndef SHL__BARRELFISH_H
#define SHL__BARRELFISH_H

#define SHL_BARRELFISH_USE_SHARED 0
#define SHL_STATIC 1

#include <barrelfish/caddr.h> // for capref

struct mem_info {
    struct capref frame;
    lvaddr_t vaddr;
    size_t size;
    uint32_t opts;
};

///
#define SHL_RAM_MIN_BASE (64UL * 1024 * 1024 * 1024)

///
#define SHL_RAM_MAX_LIMIT (512UL * 1024 * 1024 * 1024)

#define SHL_HUGEPAGE 1
#define SHL_REPLICATION 1
#define SHL_DISTRIBUTION 1
#define SHL_PARTITION 0

int numa_max_node(void);
long numa_node_size(int, long*);
int numa_available(void);
void numa_set_strict(int);

int shl__barrelfish_init(size_t num_threads);
int shl__barrelfish_share_frame(struct mem_info *mi);

#define MALLOC_VADDR_START (1UL << 36)

struct mem_info;


#endif
