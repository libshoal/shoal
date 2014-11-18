#include "shl_arrays.hpp"

int shl__get_wr_rep_rid(void)
{
    return shl__get_rep_id() % NUM_REPLICAS;
}
