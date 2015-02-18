#include <iostream>
#include "shl.h"
#include "shl_arrays.hpp"

extern "C" {
#include "bench_init.h"
}

using namespace std;


int main(int argc, char *argv[])
{
    shl_bench_init();

    // This program is sometimes segfaulting on malloc .. no idea why ..
    shl__init(99, true);

    shl_array<ARRAY_TYPE> *src = new shl_array_single_node<ARRAY_TYPE>(ARRAY_ELEMENTS, "shl__source");
    shl_array<ARRAY_TYPE> *dst = new shl_array_single_node<ARRAY_TYPE>(ARRAY_ELEMENTS, "shl__dest");

    src->set_used(1);
    src->alloc();
    dst->set_used(1);
    dst->alloc();


    for (int fraction = 0; fraction <= ARRAY_FRACTIONS; ++fraction) {
        size_t elements = ((size_t)fraction) * ARRAY_FRACTION_SIZE;

        printf("Running benchmark with %u0 %% DMA transfers (%lu/%lu)\n", fraction,
               elements, ARRAY_ELEMENTS);

        printf("times [ms]: ");

        for (int rep = 0; rep < BENCH_REPETITIONS; ++rep) {
            timer_start();

            ARRAY_TYPE *dst_arr = dst->get_array();
            ARRAY_TYPE *src_arr = src->get_array();

            dst->copy_from_array_async(src, elements);
#pragma omp parallel for
            for (size_t i = elements; i < dst->get_size(); ++i) {
                dst_arr[i] = src_arr[i];
            }
            dst->copy_barrier();

            printf("%lu ", timer_stop());
        }

        printf("\n");
    }

    printf("DONE.\n");
    while(1);



    return 0;
}
