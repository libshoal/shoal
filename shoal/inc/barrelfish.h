#ifndef SHL__BARRELFISH_H
#define SHL__BARRELFISH_H

#include "xomp/xomp.h"

int numa_max_node(void);
long numa_node_size(int, long*);
int numa_available(void);
void numa_set_strict(int);

struct timeval {
    long long      tv_sec;     /* seconds */
    long long tv_usec;    /* microseconds */
};

#endif
