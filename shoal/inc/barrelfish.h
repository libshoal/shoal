#ifndef SHL__BARRELFISH_H
#define SHL__BARRELFISH_H


int numa_max_node(void);
long numa_node_size(int, long*);
int numa_available(void);
void numa_set_strict(int);


#endif
