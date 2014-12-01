#include <iostream>
#include "shl.h"
#include "shl_array.hpp"

using namespace std;


static bool test_simple(size_t s)
{
    std::cout << "Single Node Array" << std::endl;

    shl_array<float> *ac = new shl_array<float>(s, "Test Simple Array");
    ac->alloc();

    float *a = ac->get_array();

    std::cout << "Writing array..." << std::endl;


    // Write to it
    for (unsigned int i=0; i<s; i++)
        a[i] = i;

    std::cout << "Verifying contents..." << std::endl;

    // Verify content
    for (unsigned int i=0;i<s; i++)
        assert (a[i]==i);

    delete ac;

    return true;
}

static bool test_partitioned(size_t s)
{
    return true;
}

static bool test_replicated(size_t s)
{
    return true;
}

static bool test_distributed(size_t s)
{
    return true;
}

int main()
{
    shl__init(1, true);
    std::cout << "Hello World!" << std::endl;

    test_simple(10);
    test_partitioned(10);
    test_replicated(10);
    test_distributed(10);

    return 0;
}
