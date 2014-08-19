#ifndef SHL__BARRELFISH_H
#define SHL__BARRELFISH_H

#define SHL_USE_SHARED 0

#define SHL_HUGEPAGE 1
#define SHL_REPLICATION 0
#define SHL_DISTRIBUTION 1

int numa_max_node(void);
long numa_node_size(int, long*);
int numa_available(void);
void numa_set_strict(int);

int shl__barrelfish_init(size_t num_threads);

#define MALLOC_VADDR_START (1UL << 36)

struct mem_info;


#endif
