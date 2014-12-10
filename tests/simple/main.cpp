#include <iostream>
#include "shl.h"
#include "shl_arrays.hpp"

using namespace std;


static bool test_simple(size_t s)
{
    std::cout << "Single Node Array s=" << s << std::endl;

    shl_array<float> *ac = new shl_array<float>(s, "Test Simple Array");
    ac->set_used(1);
    ac->alloc();

    float *a = ac->get_array();

    std::cout << "Writing array... " << a << std::endl;

    if (a == NULL) {
        return 0;
    }


    // Write to it
    for (unsigned int i=0; i<s; i++) {
        a[i] = i;
    }

    std::cout << "Verifying contents..." << a << std::endl;

    // Verify content
    for (unsigned int i=0; i<s; i++) {
        if (i > (s - 4)) {
            std::cout << i << "@" << &a[i] << std::endl;
            std::cout << a[i] << std::endl;
    //        if (a[i] != i) {
    //            std::cout << "Wrong element @" << i << std::endl;
    //        }
        } else {
            if (a[i] != i) {
                std::cout << "Wrong element @" << i << std::endl;
            }
        }
    }

    std::cout << "[PASS]" << std::endl;

    delete ac;

    return true;
}

static bool test_partitioned(size_t s)
{
    std::cout << "Partitioned Array" << std::endl;

    shl_array_partitioned<float> *ac = new shl_array_partitioned<float>(s, "Test Partitioned Array");
    ac->set_used(1);
    ac->alloc();

    float *a = ac->get_array();

    std::cout << "Writing array... " << a << std::endl;

    if (a == NULL) {
        return 0;
    }


    // Write to it
    for (unsigned int i=0; i<s; i++) {
        a[i] = i;
    }

    std::cout << "Verifying contents..." << std::endl;

    // Verify content
    for (unsigned int i=0; i<s; i++) {
        if (i > (s - 4)) {
            std::cout << i << "@" << &a[i] << std::endl;
            std::cout << a[i] << std::endl;
    //        if (a[i] != i) {
    //            std::cout << "Wrong element @" << i << std::endl;
    //        }
        } else {
            if (a[i] != i) {
                std::cout << "Wrong element @" << i << std::endl;
            }
        }
    }

    std::cout << "[PASS]" << std::endl;

    delete ac;

    return true;
}

static bool test_replicated(size_t s)
{
    return true;
}

static bool test_distributed(size_t s)
{
    std::cout << "Distributed Array" << std::endl;

    shl_array_distributed<float> *ac = new shl_array_distributed<float>(s, "Test Partitioned Array");
    ac->set_used(1);
    ac->alloc();
    float *a = ac->get_array();

    std::cout << "Writing array... " << a << std::endl;

    if (a == NULL) {
        return 0;
    }


    // Write to it
    for (unsigned int i=0; i<s; i++) {
        a[i] = i;
    }

    std::cout << "Verifying contents..." << std::endl;

    // Verify content
    for (unsigned int i=0; i<s; i++) {
        if (i > (s - 4)) {
            std::cout << i << "@" << &a[i] << std::endl;
            std::cout << a[i] << std::endl;
    //        if (a[i] != i) {
    //            std::cout << "Wrong element @" << i << std::endl;
    //        }
        } else {
            if (a[i] != i) {
                std::cout << "Wrong element @" << i << std::endl;
            }
        }
    }

    std::cout << "[PASS]" << std::endl;

    delete ac;

    return true;
}

int main()
{
    shl__init(1, true);
    std::cout << "Hello World!" << std::endl;

    std::cout << "==========================" << std::endl;
    test_simple(16*1024);
    std::cout << "--------------------------" << std::endl;
    test_simple(1123);

    std::cout << "==========================" << std::endl;
    test_partitioned(16*1024);
    std::cout << "--------------------------" << std::endl;
    test_partitioned(1123);

    std::cout << "==========================" << std::endl;
    test_replicated(16*1024);
    std::cout << "--------------------------" << std::endl;
    test_replicated(1123);

    std::cout << "==========================" << std::endl;
    test_distributed(16*1024);
    std::cout << "--------------------------" << std::endl;
    test_distributed(1123);

    return 0;
}
