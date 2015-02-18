#include <barrelfish/barrelfish.h>
#include <bench/bench.h>
#include <omp.h>


#include "bench_init.h"

void shl_bench_init(void)
{
    bench_init();

    bomp_init(BOMP_THREADS_ALL);
}

static cycles_t timer;

void timer_start(void)
{
    timer = bench_tsc();
}

uint64_t timer_stop(void)
{
    cycles_t now = bench_tsc();
    timer = bench_time_diff(timer, now);
    return timer;
    return bench_tsc_to_ms(timer);
}
