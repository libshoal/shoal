#include <sys/time.h>
#include <cstdlib>
#include "shl.h"

double timer_secs = 0.0;
struct timeval TV1, TV2;

int replica_lookup[MAXCORES];

void shl__start_timer(void)
{
    gettimeofday(&TV1, NULL);
 }

double shl__end_timer(void)
{
    gettimeofday(&TV2, NULL);
    timer_secs += (TV2.tv_sec - TV1.tv_sec)*1000 + (TV2.tv_usec - TV1.tv_usec)*0.001;
    return timer_secs;
}

double shl__get_timer(void)
{
    return timer_secs;
}

int
numa_cpu_to_node(int cpu)
{
    int ret    = -1;
    int ncpus  = numa_num_possible_cpus();
    int node_max = numa_max_node();
    struct bitmask *cpus = numa_bitmask_alloc(ncpus);

    for (int node=0; node <= node_max; node++) {
        numa_bitmask_clearall(cpus);
        if (numa_node_to_cpus(node, cpus) < 0) {
            perror("numa_node_to_cpus");
            fprintf(stderr, "numa_node_to_cpus() failed for node %d\n", node);
            abort();
        }

        if (numa_bitmask_isbitset(cpus, cpu)) {
            ret = node;
        }
    }

    numa_bitmask_free(cpus);
    if (ret == -1) {
        fprintf(stderr, "%s failed to find node for cpu %d\n",
                __FUNCTION__, cpu);
        abort();
    }

    return ret;
}
