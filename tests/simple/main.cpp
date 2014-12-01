#include <iostream>
#include "shl.h"
#include "shl_array.hpp"

using namespace std;

static bool test_alloc(size_t s)
{
    std::cout << "Allocating array..." << std::endl;

    shl_array<float> *ac = shl__malloc(s, "test array", 0,0,0,0,0,0);
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

int main()
{
    shl__init(1, true);
    std::cout << "Hello World!" << std::endl;

    test_alloc(1);
    test_alloc(1024);
   // test_alloc(1024*1024);
   // test_alloc(1024*1024*1024);

    return 0;
}
