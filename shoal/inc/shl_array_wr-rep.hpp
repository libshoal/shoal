#ifndef SHL_ARRAY_WR_REP__H
#define SHL_ARRAY_WR_REP__H

#include "shl.h"
#include "shl_timer.hpp"

#include <map>
#include <vector>
#include <cstring>

/**
 * \brief Implements  replication with write-support
 *
 * This is currently limited to only TWO replicas.
 */

#ifdef SHL_DBG_ARRAY
#define debug_printf(x...) printf(x)
#else
#define debug_printf(x...) void()
#endif

#define NUM_REPLICAS 2

/**
 * \brief Return the ID of the replica this thread should operate on.
 *
 * Since we only have two replicas here, we apply a module operation
 * to the output of shl__get_rep_id (which assumes there is one
 * replica on every NUMA node).
 *
 * This results in a valid replication ID, while still maintaining
 * local accesses on the nodes that actually hold the replicas (0 and
 * 1).
 */
static int shl__get_wr_rep_rid(void)
{
    return shl__get_rep_id() % NUM_REPLICAS;
}


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

    // Used for reads
    p->rep_ptr = btc->rep_array[shl__get_wr_rep_rid()];

    // Used for writes
    p->ptr1 = btc->rep_array[0];
    p->ptr2 = btc->rep_array[1];

    p->c = (struct array_cache) {
        .rid = shl__get_rep_id(),
        .tid = shl__get_tid()
    };
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

        //  Force two replicas
        shl_array_replicated<T>::num_replicas = 2;

        printf(" .. allocating replicas .. \n");
        shl_array_replicated<T>::alloc();

        assert (shl_array_replicated<T>::num_replicas == 2);
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
};

#endif /* SHL_ARRAY_WR_REP_H */
