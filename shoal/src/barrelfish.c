#include <cstdio>
#include <cstdlib>

#include <sched.h>
#include <numa.h>

#include "shl_internal.h"
#include "shl.h"

void
aff_set_oncpu(unsigned int cpu)
{
    assert (!"Not yet implemented");
}

int shl__get_proc_for_node(int node)
{
    assert (!"Not yet implemented");
    return -1;
}

extern coreid_t *affinity_conf;
void shl__bind_processor_aff(int id)
{
    assert (!"Not yet implemented");
}

void shl__bind_processor(int id)
{
    assert (!"Not yet implemented");
}
