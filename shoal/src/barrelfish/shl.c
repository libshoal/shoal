#include <stdio.h>
#include <barrelfish/barrelfish.h>
#include <bench/bench.h>
#include <vfs/vfs.h>

#include <xomp/xomp.h>

#include "shl_internal.h"
#include <backend/barrelfish/meminfo.h>

/**
 * \brief initializes the barrelfish backend
 */
int shl__barrelfish_init(size_t num_threads)
{
#if 0
#if SHL_BARRELFISH_USE_SHARED
    bomp_bomp_init(num_threads);
    return 0;
#else
    errval_t err;

    printf("Starting as XOMP Master\n");
    struct xomp_args args;
    args.type = XOMP_ARG_TYPE_UNIFORM;
    args.args.uniform.nthreads = num_threads;
    args.core_stride = 2;
    args.args.uniform.nphi = 2;
    /*
    args.args.uniform.argc = argcount;
    args.args.uniform.argv = argvals;
    */
    args.args.uniform.worker_loc = XOMP_WORKER_LOC_MIXED;

    err = bomp_xomp_init(&args);
    if (err_is_fail(err)) {
        USER_PANIC("XOMP master initialization failed.\n");
    }

    omp_set_num_threads(num_threads);
#endif
#endif

    // initialize bench library
    bench_init();

    // initialize vfs
    vfs_init();

    return 0;
}


int shl__barrelfish_share_frame(struct shl_mi_data *mi)
{
#if !SHL_BARRELFISH_USE_SHARED
    xomp_frame_type_t type = XOMP_FRAME_TYPE_SHARED_RW;
    if (mi->opts & SHL_MALLOC_PARTITION) {
        assert("NYI");
    } else if (mi->opts & SHL_MALLOC_REPLICATED) {
        type = XOMP_FRAME_TYPE_REPL_RW;
    } else if (mi->opts & SHL_MALLOC_DISTRIBUTED) {
        assert("NYI");
    }

    //return xomp_master_add_memory(mi->frame, mi->vaddr, type);
    return 0;
#endif
    assert(!"should not be called");
    return -1;
}


unsigned long shl__timer_get_timestamp(void)
{
    cycles_t cycles = bench_tsc();
    return bench_tsc_to_ms(cycles);
}
