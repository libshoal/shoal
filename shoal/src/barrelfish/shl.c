#include <stdio.h>
#include <barrelfish/barrelfish.h>
#include <bench/bench.h>
#include <vfs/vfs.h>
#include <numa.h>

#include <omp.h>
#include <xomp/xomp.h>

#include "shl_internal.h"
#include <backend/barrelfish/meminfo.h>

/**
 * \brief initializes the barrelfish backend
 */
int shl__barrelfish_init(uint32_t *num_threads)
{
     errval_t err;

    // initialize bench library
    bench_init();

    // initialize vfs for loading settings file
    vfs_init();

    // initialize libnuma
    err = numa_available();
    if (err_is_fail(err)) {
        return -1;
    }

    /*
     * check for the maximum number of threads
     * if the parameter is null, take the maximum of available cores
     */
    uint32_t threads_max = numa_num_configured_cpus();
    if (num_threads) {
        if (threads_max < *num_threads) {
            *num_threads = threads_max;
        } else {
            threads_max = *num_threads;
        }
    }



    // for OpenMP coy
    //bomp_bomp_init(threads_max);

#if SHL_BARRELFISH_XEON_PHI

    struct xomp_args args = {
        .type = XOMP_ARG_TYPE_UNIFORM,
        .uniform = {
            .nthreads = 3,
            .worker_loc = XOMP_WORKER_LOC_REMOTE,
            .nphi = num_threads,
            .argc = xomp_master_get_argc(),
            .argv = xomp_master_get_argv()
        }
    };

    err = bomp_xomp_init(&args);
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "initializing xomp master");
        return -1;
    }

#endif



    return 0;
}

int shl__get_rep_id(void)
{
    return numa_current_node();
    //return shl__lookup_rep_id(disp_get_core_id());
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
