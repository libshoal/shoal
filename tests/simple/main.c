#include <iostream>
#include "shl.h"
#include "shl_array.hpp"

using namespace std;

bool test_alloc(size_t s)
{
    shl_array<float> *ac = shl_array<float>::shl__malloc(s, false);
    float *a = ac->get_array();

    // Write to it
    for (unsigned int i=0; i<s; i++)
        a[i] = i;

    // Verify content
    for (unsigned int i=0;i<s; i++)
        assert (a[i]==i);

    delete ac;

    return true;
}

int main()
{
    shl__init(1);
    std::cout << "Hello World!" << std::endl;

    test_alloc(1);
    test_alloc(1024);
    test_alloc(1024*1024);
    test_alloc(1024*1024*1024);

    return 0;
}
