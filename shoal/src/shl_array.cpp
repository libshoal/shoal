#include <stdio.h>

#include "shl_array.hpp"
#include "shl_alloc.hpp"

extern "C" {
#include "crc.h"
}

int shl__estimate_working_set_size(int num, ...)
{
    // Determine working set size

    printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
    printf("Number of arrays is: %d\n", num);

    va_list a_list;
    va_start(a_list, num);

    int sum = 0;

    for (int x = 0; x<num; x++) {

        int tmp = va_arg(a_list, int);
        printf("Size of array %10d in bytes is %10d\n",
               x, tmp);

        sum += tmp;
    }
    va_end (a_list);

    printf("Total working set size: %d bytes - ", sum);
    print_number(sum);
    printf("\n");

    // Figure out minimum NUMA node
    long minsize = std::numeric_limits<long>::max();
    for (int i=0; i<shl__max_node()+1; i++) {

        long tmp = shl__node_size(i, NULL);
        minsize = std::min(minsize, tmp);
        printf("Size of node %2d is %10ld\n", i, tmp);
    }

    printf("minimum node size is: %ld - ", minsize);
    print_number(minsize);
    printf("\n");

    if (minsize>sum) {

        printf("working set fits into each node\n");
    } else {

        printf(ANSI_COLOR_RED "working set does NOT fit into each node"
               ANSI_COLOR_RESET "\n");
    }

    printf("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
    return 0;
}


unsigned long shl__calculate_crc(void *array, size_t elements, size_t element_size)
{
    crc_t crc = crc_init();

    uintptr_t max = element_size * elements;
    crc = crc_update(crc, (unsigned char*) array, max);

    crc_finalize(crc);

    return (unsigned long) crc;
}

