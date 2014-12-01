#ifndef SHL_ARRAY_WR_REP__H
#define SHL_ARRAY_WR_REP__H

#include "shl.h"
#include "shl_timer.hpp"

#include <map>
#include <vector>
#include <cstring>

/**
 * \brief Implements  replication with write-support
 */

// Configuration
// --------------------------------------------------

// Number of replicas
#define NUM_REPLICAS 2

#ifdef SHL_DBG_ARRAY
#define debug_printf(x...) printf(x)
#else
#define debug_printf(x...) void()
#endif

#define CONCAT(x,y) x ## y

// Macros for write access depending on number of replicas
// --------------------------------------------------
// wrappers
#define WR_REP__WR(arr,n,i,v) { CONCAT(WR_REP__WR_N,n) (arr, i, v) }
// recursion
#define WR_REP__WR_N1(arr,i,v) WR_REP__WR_E (arr,i,v,1)
#define WR_REP__WR_N2(arr,i,v) WR_REP__WR_N1(arr,i,v) WR_REP__WR_E(arr,i,v,2)
#define WR_REP__WR_N3(arr,i,v) WR_REP__WR_N2(arr,i,v) WR_REP__WR_E(arr,i,v,3)
#define WR_REP__WR_N4(arr,i,v) WR_REP__WR_N3(arr,i,v) WR_REP__WR_E(arr,i,v,4)
// entry
#define WR_REP__WR_E(arr,i,v,num) shl__ ## arr.ptr ## num[i] = v;

// Macros for copy function
// --------------------------------------------------
// wrappers
#define WR_REP__CPY(n) { CONCAT(WR_REP__CPY_N,n) () }
// recursion
#define WR_REP__CPY_N1() WR_REP__CPY_E (0)
#define WR_REP__CPY_N2() WR_REP__CPY_N1() WR_REP__CPY_E(1)
#define WR_REP__CPY_N3() WR_REP__CPY_N2() WR_REP__CPY_E(2)
#define WR_REP__CPY_N4() WR_REP__CPY_N3() WR_REP__CPY_E(3)
// entry
#define WR_REP__CPY_E(num) \
    shl_array_replicated<T>::rep_array[num][j]= src->array[j];

// Macros for thread_init
// --------------------------------------------------
// wrappers
#define WR_REP__INIT(n) { CONCAT(WR_REP__INIT_N,n) () }
// recursion
#define WR_REP__INIT_N1() WR_REP__INIT_E (1,0)
#define WR_REP__INIT_N2() WR_REP__INIT_N1() WR_REP__INIT_E(2,1)
#define WR_REP__INIT_N3() WR_REP__INIT_N2() WR_REP__INIT_E(3,2)
#define WR_REP__INIT_N4() WR_REP__INIT_N3() WR_REP__INIT_E(4,3)
// entry
#define WR_REP__INIT_E(num, idx) \
    p->ptr ## num = btc->rep_array[idx];

// Macros for array cache struct
// --------------------------------------------------
// wrappers
#define WR_REP__ARR_CACHE(n) CONCAT(WR_REP__ARR_CACHE_N,n) ()
// recursion
#define WR_REP__ARR_CACHE_N1() WR_REP__ARR_CACHE_E (1)
#define WR_REP__ARR_CACHE_N2() WR_REP__ARR_CACHE_N1() WR_REP__ARR_CACHE_E(2)
#define WR_REP__ARR_CACHE_N3() WR_REP__ARR_CACHE_N2() WR_REP__ARR_CACHE_E(3)
#define WR_REP__ARR_CACHE_N4() WR_REP__ARR_CACHE_N3() WR_REP__ARR_CACHE_E(4)
// entry
#define WR_REP__ARR_CACHE_E(num) \
    T *ptr ## num;


/**
 * \brief Profide cached array access information for wr-rep.
 *
 * This is per-thread state
 */
template <class T>
class arr_thread_ptr {

public:
    T *rep_ptr;
    WR_REP__ARR_CACHE(NUM_REPLICAS);
    struct array_cache c;
};

/**
 * \brief Return the ID of the replica this thread should operate on.
 *
 * Since we only have two replicas here, we apply a modulo operation
 * to the output of shl__get_rep_id (which assumes there is one
 * replica on every NUMA node).
 *
 * This results in a valid replication ID, while still maintaining
 * local accesses on the nodes that actually hold the replicas (0 and
 * 1).
 */
int shl__get_wr_rep_rid(void);


/**
 * \brief Initialize per-thread state for use of wr-rep arrays.
 *
 * This is essentially a set of pointers, all of them pointing to each
 * replica.
 */
template <class T>
void shl__wr_rep_ptr_thread_init(shl_array<T> *base,
                                 class arr_thread_ptr<T> *p)
{
    shl_array_replicated<T> *btc = dynamic_cast<shl_array_replicated<T>*>(base);

    if  (btc == NULL)
        return;

    if (btc->rep_array==NULL)
        return;

    // Used for reads
    p->rep_ptr = btc->rep_array[shl__get_wr_rep_rid()];

    // Used for writes
    WR_REP__INIT(NUM_REPLICAS);

    struct array_cache ac;
    ac.rid = shl__get_rep_id();
    ac.tid = shl__get_tid();
    /*= {
        .rid = rep_id,
        .tid = shl__get_tid()
    }; */

    p->c = ac;
}


template <class T>
class shl_array_wr_rep : public shl_array_replicated<T>
{

public:
    /**
     * \brief Initialize replicated array
     */
    shl_array_wr_rep(size_t s, const char *_name, int (*f_lookup)(void))
        : shl_array_replicated<T>(s, _name, f_lookup)
    {
        printf("shl_array_wr_rep: setting %d threads\n", shl__num_threads());
        assert (shl__get_num_replicas()>=2); // Otherwise wr-rep will SEG-FAULT
    }

    /**
     * \brief Allocate both arrays and switch to collapsed mode
     */
    virtual void alloc(void)
    {
        printf("Allocating wr-rep array\n");

        printf(" .. allocating replicas .. \n");
        shl_array_replicated<T>::alloc();

        // Use arrays on far away NUMA node
        // shl_array_replicated<T>::rep_array[0] =
        //     shl_array_replicated<T>::rep_array[0];
        // shl_array_replicated<T>::rep_array[1] =
        //     shl_array_replicated<T>::rep_array[shl__get_num_replicas()-1];

        shl_array_replicated<T>::num_replicas = NUM_REPLICAS;

        assert (shl_array_replicated<T>::rep_array != NULL);
    }

    virtual ~shl_array_wr_rep(void)
    {
    }

protected:
    virtual void print_options(void)
    {
        shl_array<T>::print_options();
        printf("wr_rep=[X]");
    }

    virtual void copy_back(T* a)
    {
        printf("Copy back wr_rep (from copy 0)\n");

        for (unsigned int i=0; i<shl_array<T>::size; i++) {

            shl_array<T>::array_copy[i] =
                shl_array_replicated<T>::rep_array[0][i];
        }

    }

    virtual bool do_copy_back(void)
    {
        return true;
    }

#define COPY_BATCH_SIZE 1024
    virtual void copy_from_array(shl_array<T> *src)
    {
        static Timer t;
        t.start();

        unsigned int j = 0;
#ifdef COPY_MEMCPY
        // --------------------------------------------------
        // Use memcpy
        assert (!"Currently assumes NUM_REPLICAS==2");
#pragma omp parallel for
        for (j=0; j<shl_array<T>::size-COPY_BATCH_SIZE; j+=COPY_BATCH_SIZE) {

            memcpy(shl_array_replicated<T>::rep_array[0]+j, src->array+j, COPY_BATCH_SIZE);
            memcpy(shl_array_replicated<T>::rep_array[1]+j, src->array+j, COPY_BATCH_SIZE);
        }

#pragma omp parallel for
        for (j=j; j<shl_array<T>::size; j++) {

            shl_array_replicated<T>::rep_array[0][j] = src->array[j];
            shl_array_replicated<T>::rep_array[1][j] = src->array[j];
        }
#else
#pragma omp parallel for schedule(static,1024)
        // --------------------------------------------------
        // Use regular per-element copy, but write BOTH rep's in same iteration

        for (j=0; j<shl_array<T>::size; j++) {

            WR_REP__CPY(NUM_REPLICAS);
        }

#endif
            printf("time for copy_from_array is %lf\n", t.stop());
    }

    virtual void init_from_value(T value)
    {

        assert (!"Number of replicas hardcoded, generate macros for this");

#pragma omp parallel for schedule(static,1024)
        for (unsigned int j=0; j<shl_array<T>::size; j++) {

            shl_array_replicated<T>::rep_array[0][j] = value;
            shl_array_replicated<T>::rep_array[1][j] = value;
            shl_array_replicated<T>::rep_array[2][j] = value;
            shl_array_replicated<T>::rep_array[3][j] = value;
        }

    }

};

#endif /* SHL_ARRAY_WR_REP_H */
