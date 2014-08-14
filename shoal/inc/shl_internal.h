#ifndef __SHL_INTERNAL_H
#define __SHL_INTERNAL_H

#include "shl.h"

#ifdef PAPI
#include <papi.h>
#endif

void
aff_set_oncpu(unsigned int cpu);

#endif /* __SHL_INTERNAL_H */
