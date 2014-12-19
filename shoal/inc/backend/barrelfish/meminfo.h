#ifndef SHL__BACKEND_BARRELFISH_MEMINFO_H
#define SHL__BACKEND_BARRELFISH_MEMINFO_H

#include <barrelfish/barrelfish.h>

///
#define SHL_RAM_MIN_BASE (64UL * 1024 * 1024 * 1024)

///
#define SHL_RAM_MAX_LIMIT (512UL * 1024 * 1024 * 1024)

#define MALLOC_VADDR_START (1UL << 36)

/**
 * \brief
 */
struct shl_mi_data {
    struct capref frame;    ///<
    lvaddr_t vaddr;         ///<
    lpaddr_t paddr;         ///<
    size_t size;            ///<
    uint32_t opts;          ///<
};

struct shl_mi_header {
    size_t num;
    size_t stride;
    struct memobj_numa memobj;
    struct vregion vregion;
    struct shl_mi_data *data;
};

#include <dma/dma.h>



#endif /* SHL__BACKEND_BARRELFISH_MEMINFO_H */
