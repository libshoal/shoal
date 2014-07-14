#include <cstdio>
#include <cstdlib>

#include <sched.h>
#include <numa.h>

#include "shl.h"

void
aff_set_oncpu(unsigned int cpu)
{
    cpu_set_t cpu_mask;
    int err;

    CPU_ZERO(&cpu_mask);
    CPU_SET(cpu, &cpu_mask);

    err = sched_setaffinity(0, sizeof(cpu_set_t), &cpu_mask);
    if (err) {
        perror("sched_setaffinity");
        exit(1);
    }
}

int shl__get_proc_for_node(int node)
{
    for (int i=0; i<MAXCORES; i++)
        if (numa_cpu_to_node(i)==node)
            return i;

    assert (!"Cannot find processor on given node");
    return -1;
}

extern coreid_t *affinity_conf;
void shl__bind_processor_aff(int id)
{
    //    printf("Binding [%d] to [%d]\n", id, affinity_conf[id]);
    aff_set_oncpu(affinity_conf[id]);
}

void shl__bind_processor(int id)
{
    aff_set_oncpu(id);
}


//#include <hugetlbfs.h>
//int getpagesizes(long pagesizes[], int n_elem);
