#ifndef __SHL_INTERNAL_H
#define __SHL_INTERNAL_H

#include "shl.h"

#ifdef PAPI
#include <papi.h>
#endif

// --------------------------------------------------
// Defines
#define SHL__ARRAY_NAME_LEN_MAX 100

// --------------------------------------------------
// Other things
void aff_set_oncpu(unsigned int cpu);

// --------------------------------------------------
// Query array configuration
void shl__lua_init(void);
void shl__lua_deinit(void);
bool shl__get_array_conf(const char* array_name, int feature, bool def);


#endif /* __SHL_INTERNAL_H */
