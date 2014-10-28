#ifndef SHL_ARRAY_EXPANDABLE__H
#define SHL_ARRAY_EXPANDABLE__H

#include "shl.h"
#include "shl_timer.hpp"

#include <map>
#include <vector>
#include <cstring>

extern "C" {
#include <pthread.h>
}

#define SANITY_CHECK
#define WS_BUFFER_SIZE (1024*1024*100)

/**
 * \brief Array that is initally single-copy, but can be expanded to
 * use replication
 *
 * In expanded mode, replicas are not synchronized (i.e. writes are
 * local to current replica)
 *
 * Consistency:
 * - expanded mode  -> clients read their own writes, but not the others
 * - collapsed mode -> all clients read all writes (only one identical copy)
 *
 * I think we need a lock to switch modes (or a barrier). Wait until
 * all clients call collapse() and block until operation is finished.
 *
 * NOTE on concurrency:
 * - we assume that concurrent accesses only happen in collapsed mode
 * - we assume that collapse is only called AFTER the access model changed
 *   back to single-threaded
 *
 * In expanded mode, we need to keep track of writes. We do this using write
 * sets.
 *
 * We assume that the write sets from individual clients are
 * disjunct. If they are not, the semantics of concurrent writes in
 * the presence of parallel OMP loops is undeterministc anyway, so we
 * can just write back changes in any order we want (I think).
 *
 * Consistency: This should implement release consistency, I think:
 * collapse -> release, expand -> acquire.
 */

#ifdef SHL_DBG_ARRAY
#define debug_printf(x...) printf(x)
#else
#define debug_printf(x...) void()
#endif

union counter_element {
    int counter;
    char dummy[CACHELINE];
};

template <class T>
class shl_array_expandable : public shl_array_replicated<T>
{

public:
    Timer t_collapse;
    Timer t_expand[MAXCORES];

private:
    Timer t_write_back[MAXCORES];
    union counter_element c_write_back[MAXCORES];

    /**
     * \brief Check consistency of arrays
     *
     * Checks for each replica if it is consistency with the master copy
     */
    bool check_consistency(void)
    {
        for (int j=0; j<shl_array_replicated<T>::num_replicas; j++) {

            size_t num_total = shl_array<T>::size;
            for (size_t i=0; i<num_total; i++) {

                if (shl_array<T>::array[i] !=
                    shl_array_expandable<T>::rep_array[j][i]) {

                    printf("check_consistency failed for index %d\n", i);
                    printf("    ");

                    return false;
                }
            }
        }

        return true;
    }

    /**
     * \brief Write back write set of current thread to master copy
     */
    void ws_write_back(void)
    {
        // Nothing to do here
    }

// public:
//     // Make master array used distribution
//     virtual int get_options(void)
//     {
//         return shl_array<T>::get_options() | SHL_MALLOC_DISTRIBUTED;
//     }

protected:

    bool is_expanded[MAXCORES];
    pthread_barrier_t b;

public:
    /**
     * \brief Initialize replicated array
     */
    shl_array_expandable(size_t s, const char *_name, int (*f_lookup)(void))
        : shl_array_replicated<T>(s, _name, f_lookup)
    {
        printf("shl_array_expandable: setting %d threads\n", shl__num_threads());
        pthread_barrier_init(&b, NULL, shl__num_threads());
    }

    void expand(void)
    {
        // static size_t num_expand = 0;
        // num_expand++;

        int tid = shl__get_tid();

        //        t_expand[tid].start();

        //        assert(!is_expanded);

        //        int r = pthread_barrier_wait(&b);
        //        assert(r==PTHREAD_BARRIER_SERIAL_THREAD || r==0);


        // Every thread copies their own data
        if (shl__is_rep_coordinator(shl__get_tid())) {
            printf("Thread %d executes a memcpy for replica %d\n",
                   shl__get_tid(), shl__get_rep_id());
            memcpy((void*)shl_array_replicated<T>::rep_array[shl__get_rep_id()],
                    (void*)shl_array<T>::array,
                    shl_array<T>::size*sizeof(T));
        }
        //        r = pthread_barrier_wait(&b);


        //        t_expand[tid].stop();

        is_expanded[tid] = true;

        //        assert(is_expanded);
    }

    /**
     * \brief Collapse arrays
     *
     * Writes to replicas will be synchronized back to the master copy.
     *
     * If multiple replicas have been written at the same index i1
     * since the last expand, the value i1' written back to the master
     * copy is undeterministic.
     */
    void collapse(void)
    {
        //        assert (is_expanded);

        is_expanded[shl__get_tid()] = false;

        pthread_barrier_wait(&b);

        //        assert (!is_expanded);
    }

    /**
     * \brief Allocate both arrays and switch to collapsed mode
     */
    virtual void alloc(void)
    {
        shl_array<T>::alloc(); shl_array<T>::alloc_done = false;
        shl_array_replicated<T>::alloc();

        for (int i=0; i<MAXCORES; i++) {
            is_expanded[i] = false;
        }
    }

    /**
     * \brief Return pointer to beginning of replica.
     *
     * Writes to this pointer will result in undefined behavior. The
     * set-function has to be used to write arrays.
     */
    virtual T* get_array(void)
    {
        if (shl_array<T>::alloc_done) {
            return is_expanded[shl__get_tid()] ? shl_array_replicated<T>::get_array() :
                shl_array<T>::get_array();
        } else {
            return NULL;
        }
    }

    /**
     * \brief Read data item from array.
     *
     * If array is expanded, this will read the data item from the
     * local replica. If colapsed, read from master copy.
     */
    virtual T get(size_t i)
    {
        return is_expanded[shl__get_tid()] ? shl_array_replicated<T>::get(i) : shl_array<T>::get(i);
    }

    /**
     * \brief Write to array.
     *
     * Collapsed mode: write to master-copy
     *
     */
    virtual void set(size_t i, T v)
    {
        if (is_expanded[shl__get_tid()]) {

            debug_printf("Setting new idx=%zu old=%d new=%d on thread %d\n",
                         i, get(i), v, shl__get_tid());

            int rep_id = shl_array_replicated<T>::lookup();
            set_cached(i, v, (struct array_cache) { .rid = rep_id,
                        .tid = shl__get_tid() });
        }
        else {
            shl_array<T>::set(i, v);
        }
    }

    /**
     * \brief Write to the array
     *
     * Guarantees: threads read their own writes
     */
    void set_cached(size_t i, T v, struct array_cache c)
    {
        debug_printf("set_cached: %zu: %d -> %d on rep %d thread %d\n",
                     i, shl_array_replicated<T>::rep_array[c.rid][i], v,
                     c.rid, c.tid);

        shl_array_replicated<T>::rep_array[c.rid][i] = v;
        shl_array<T>::array[i] = v;
    }

    bool get_expanded(void)
    {
        return is_expanded[shl__get_tid()];
    }

    virtual ~shl_array_expandable(void)
    {
        pthread_barrier_destroy(&b);

#if defined(SHL_DBG_ARR)
        for (int i=0; i<shl__num_threads(); i++) {

            printf("destructor %2d: t_write_back %11.6lf - c_write_back % 4d\n",
                   i, t_write_back[i].get(), c_write_back[i].counter);
        }
#endif /* SHL_DBG_ARR */
    }

protected:
    virtual void print_options(void)
    {
        shl_array<T>::print_options();
        printf("expandable=[X]");
    }

    virtual void copy_back(T* a)
    {
        shl_array<T>::copy_back(a);
    }

    virtual bool do_copy_back(void)
    {
        return true;
    }
};

#endif /* SHL_ARRAY_EXPANDABLE_H */
