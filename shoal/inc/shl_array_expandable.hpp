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

struct array_cache {

    int rid;
    int tid;
};

#define SANITY_CHECK
#define WS_BUFFER_SIZE (1024*1024*100)

#define SHL_EXPAND_NO_HOUSEKEEPING
//#define SHL_DBG_ARRAY

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
     * \brief Check given writeset for given index
     *
     * Checks if given index is part of the write-set
     */
    bool in_writeset(size_t idx, int ws)
    {
        bool res = false;
        struct ws_thread_e *wst = &write_set[ws].p;

        for (size_t ii=0; ii<wst->ws_size; ii++) {

            if (wst->buffer[ii].index == idx)
                res = true;
        }

        return res;
    }

    /**
     * \brief Print debug information for the given index
     *
     * This is:
     * - print the current value in the master and each replica
     * - for each write-set if the given index is in there
     */
    void debug_index(size_t idx)
    {
        // Dump content of master copy
        printf("%30s %d\n", "master-copy", shl_array<T>::array[idx]);
        printf("\n");

        // Dump content of replicas
        for (int i=0; i<shl_array_replicated<T>::num_replicas; i++) {
            printf("%4d %25s %d\n", i, "replica", shl_array_replicated<T>::rep_array[i][idx]);
        }
        printf("\n");

        // Check if in any of the write sets
        for (int jinner=0; jinner<shl__num_threads(); jinner++) {
            printf("%4d %4d %20s %d\n", jinner, shl__lookup_rep_id(jinner),
                   "write-set", in_writeset(idx, jinner));
        }
    }

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
#ifdef SHL_EXPAND_NO_HOUSEKEEPING
        //        printf("ws_write_back deactivated\n");
        // XXX put an assert here!
        int tid = shl__get_tid();
        struct ws_thread_e *e = &write_set[tid].p;
        e->ws_size = 0;
        return;
#else /* SHL_EXPAND_NO_HOUSEKEEPING */
        debug_printf("Writing back on %d .. \n", shl__get_tid());

        int tid = shl__get_tid();
        int rep_id = shl_array_replicated<T>::lookup();

        t_write_back[tid].start();

        struct ws_thread_e *e = &write_set[tid].p;
        for (size_t i=0; i<e->ws_size; i++) {

            struct ws_write_t *w = &e->buffer[i];
            T tmp = shl_array_replicated<T>::rep_array[rep_id][w->index];
            shl_array<T>::array[w->index] = tmp;

            debug_printf(" .. writing %zu with %d on %d\n", w->index, tmp, tid);
        }
        // Everything written back, reset index to beginning of buffer
        e->ws_size = 0;

        t_write_back[tid].stop();
        c_write_back[tid].counter++;
#endif /* SHL_EXPAND_NO_HOUSEKEEPING */
    }

// public:
//     // Make master array used distribution
//     virtual int get_options(void)
//     {
//         return shl_array<T>::get_options() | SHL_MALLOC_DISTRIBUTED;
//     }

protected:

#ifdef SHL_EXPAND_STL
    // Elements in a write set
    typedef std::pair<size_t, T> write_set_e;

    // A single write set: vector of writeset-elements
    typedef std::vector<write_set_e> write_set_t;
    typedef typename std::vector<write_set_e>::iterator write_set_i;

    // One writeset per thread: map(tid -> writeset)
    typedef std::map<size_t, write_set_t> write_sets_t;
    typedef typename std::map<size_t, write_set_t>::iterator write_sets_i;

    write_sets_t write_sets;
#else

    // State information for writes
    //
    // XXX for sequential writes, this is inefficient
    struct ws_write_t {
        size_t index;
        //       T value; <-- don't need to store
    };

    // per-thread payload for write-set
    //
    // we keep this per-thread to avoid locks
    struct ws_thread_e {
        size_t ws_size;
        ws_write_t *buffer;
    };
    // extend write-set entry to match cachline size
    union ws_thread_t {
        struct ws_thread_e p;
        char dummy[CACHELINE];
    };

    // one cache line entry per thread
    union ws_thread_t write_set[MAXCORES];
#endif

    bool is_expanded;
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
        static size_t num_expand = 0;
        num_expand++;

        int tid = shl__get_tid();

        t_expand[tid].start();

        assert(!is_expanded);

        int r = pthread_barrier_wait(&b);
        assert(r==PTHREAD_BARRIER_SERIAL_THREAD || r==0);

#if SHL_EXPAND_NAIVE

        if (r==PTHREAD_BARRIER_SERIAL_THREAD) {

            printf("Expanding array .. SERIAL\n");

            Timer expand_timer;
            expand_timer.start();
            assert (is_expanded==false);

            is_expanded = true;
            shl__repl_sync((void*) shl_array<T>::array,
                           (void**) shl_array_replicated<T>::rep_array,
                           shl_array_replicated<T>::num_replicas,
                           shl_array<T>::size*sizeof(T));
            double s = expand_timer.stop();

            printf("Done! (t_expand: %lf ms)\n", s);
        }
        pthread_barrier_wait(&b);
#else
#if defined(SHL_DBG_ARR)
        if (r==PTHREAD_BARRIER_SERIAL_THREAD) {

            shl_array<T>::dump();
            printf("Expanding array .. SERIAL\n");
        }
#endif /* SHL_DBG_ARR */
        pthread_barrier_wait(&b);


        // Every thread copies their own data
        if (shl__rep_coordinator(shl__get_tid())) {
            memcpy((void*)shl_array_replicated<T>::rep_array[shl__get_rep_id()],
                    (void*)shl_array<T>::array,
                    shl_array<T>::size*sizeof(T));
        }
        r = pthread_barrier_wait(&b);

#if defined(SHL_DBG_ARR)
        if (r==PTHREAD_BARRIER_SERIAL_THREAD) {

            shl_array_replicated<T>::dump();

            // At this point, all arrays have to be consistent
            assert(check_consistency());
        }
#endif /* SHL_DBG_ARR */

        t_expand[tid].stop();
#ifdef SHL_DBG_ARRAY
        pthread_barrier_wait(&b);
        printf("Done! (t_expand: %lf)\n", t_expand[tid].get());
#endif

        is_expanded = true;

#endif
#if defined(SHL_DBG_ARR)
        noprintf("---%d------------------------------------\n", shl__get_tid());
        r = pthread_barrier_wait(&b);
#endif /* SHL_DBG_ARR */

        assert(is_expanded);
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
        assert (is_expanded);

        // XXX I think it also works without this barrier
        // (it was inside SHL_EXPAND_STL before)

        int r __attribute((unused)) = pthread_barrier_wait(&b);
        assert(r==PTHREAD_BARRIER_SERIAL_THREAD || r==0);

#ifdef SHL_EXPAND_STL
        if (r==PTHREAD_BARRIER_SERIAL_THREAD) {

            printf("Collapsing array .. SERIAL \n");
            write_sets_i i; size_t i_num = 0;
            for (i=write_sets.begin(); i!=write_sets.end(); ++i, i_num++) {

                int pid = shl__lookup_rep_id(i->first);
                assert(i->first == i_num);

                write_set_t t = i->second;
                printf(" .. client %02zu/%02d has %zu writes\n",
                       (i_num+1), shl__num_threads(), t.size());

                write_set_i j;
                for (j=t.begin(); j!=t.end(); ++j) {

                    size_t idx = j->first;
                    T val = j->second;
#ifdef SHL_DBG_ARRAY
                    printf("Update on [%10zu] to %10d\n", idx, val);
#endif /* SHL_DBG_ARRAY */

#ifdef SANITY_CHECK
                    assert (shl_array_replicated<T>::rep_array[pid][idx] == val);
#endif /* SANITY_CHECK */
                    shl_array<T>::array[idx] = val;
                }
            }
            is_expanded = false;
        }
#else /* SHL_EXPAND_STL */
#ifdef SHL_EXPAND_NO_HOUSEKEEPING
        if (r == PTHREAD_BARRIER_SERIAL_THREAD) {

            t_collapse.start();

#ifdef  SHL_DBG_ARRAY
            printf("housekeeping-free collapse on %d\n", shl__get_tid());

            // Determine the size of all the write-sets
            size_t ws_size = 0;
            // So we have many duplicates ..
            for (int c=0; c<shl__num_threads(); c++) {
                ws_size += write_set[c].p.ws_size;
            }
            printf("size of write-sets is %zu\n", ws_size);
#endif /* SHL_DBG_ARRAY */

            size_t num_total = shl_array<T>::size;
            size_t num_written = 0;
            for (size_t i=0; i<num_total; i++) {

                // Remeber original master value
                T master_value = shl_array<T>::array[i];

                // Last value copied from replica to master copy
                T new_value = master_value;

                bool updated = false;

                // Check all replicas for update
                for (int j=0; j<shl_array_replicated<T>::num_replicas; j++) {

                    // Read copy from replica
                    T rep_value = shl_array_replicated<T>::rep_array[j][i];
                    if (rep_value != master_value) {

                        // Value in replica is different, copy to master copy

#ifdef SHL_DBG_ARRAY
                        // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
                        // SANITY CHECK
                        printf("Sanity check start for id=%zu.. \n", i);
                        bool in_ws = false;

                        // Walk all write-sets
                        for (int jinner=0; jinner<shl__num_threads(); jinner++) {
                            // Look at the if the thread of that ws belongs to the same
                            if (shl__lookup_rep_id(jinner)==j) {

                                struct ws_thread_e *wst = &write_set[jinner].p;
                                printf("Checking write-set of %d - size is %zu\n",
                                       jinner, wst->ws_size);
                                for (size_t ii=0; ii<wst->ws_size; ii++) {

                                    if (wst->buffer[ii].index == i)
                                        in_ws = true;
                                }
                            }
                        }

                        if (!in_ws) {
                            printf("Copying idx=%zu from %d to master, but "
                                   "it's not in write-set, written before=%d\n",
                                   i, j, updated);

                            debug_index(i);
                        }

                        // otherwise we copy a value from a replica that did not record the write
                        assert (in_ws);

                        printf("Sanity check end .. \n");
                        // END SANITY CHECK
                        // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
#endif /* SHL_DBG_ARRAY */

                        shl_array<T>::array[i] = rep_value;
                        num_written++;

                        // in case of several updates, all of them are the same
                        if (updated) {
                            assert (rep_value == new_value);
                        }

                        new_value = rep_value;
                        updated = true;
                    }
                }

#if defined(SHL_DBG_ARR)
                if (!updated) {
                    printf("Did not update idx=%zu\n", i);
                }
#endif /* SHL_DBG_ARR */
            }

            for (int c=0; c<shl__num_threads(); c++) {
                write_set[c].p.ws_size = 0;
            }

            noprintf("naive copy-back done (%06zu/%06zu), %lf\n",
                     num_written, num_total, num_written*1.0/num_total);

            is_expanded = false;
            t_collapse.stop();
        }
#else /* SHL_EXPAND_NO_HOUSEKEEPING */
        ws_write_back();
        is_expanded = false;
#endif /* SHL_EXPAND_NO_HOUSEKEEPING */
#endif /* SHL_EXPAND_STL */

        pthread_barrier_wait(&b);

        if (r==PTHREAD_BARRIER_SERIAL_THREAD) { // r from above
            double t = t_collapse.get();
            printf("Collapsing .. (t_collapse=%lf)\n", t);
        }

#if defined(SHL_DBG_ARR)
        printf("---client-%-2d---collapse-done---------------------------\n",
               shl__get_tid());

        r = pthread_barrier_wait(&b);
#endif /* SHL_DBG_ARR */

#ifdef SHL_DBG_ARRAY

        if (r==PTHREAD_BARRIER_SERIAL_THREAD) {

            shl_array<T>::dump();
        }
#endif
        // After collapse, every thread sees a collapsed version of the array
        assert (!is_expanded);
    }

    /**
     * \brief Allocate both arrays and switch to collapsed mode
     */
    virtual void alloc(void)
    {
        shl_array<T>::alloc(); shl_array<T>::alloc_done = false;
        shl_array_replicated<T>::alloc();

        for (int i=0; i<shl__num_threads(); i++) {

#ifdef SHL_EXPAND_STL
            write_set_t t;
            write_sets.insert(make_pair(i, t));
            write_sets[i].reserve(1000000);
#else
            // should be NUMA aware alloc ..
            write_set[i].p.buffer = (struct ws_write_t*)
                malloc(sizeof(struct ws_write_t)*WS_BUFFER_SIZE);
            write_set[i].p.ws_size = 0;
            c_write_back[i].counter = 0;

            assert (write_set[i].p.buffer!=NULL);
#endif
        }

        is_expanded = false;
    }

    /**
     * \brief Return pointer to beginning of replica.
     *
     * Writes to this pointer will result in undefined behavior. The
     * set-function has to be used to write arrays.
     */
    virtual T* get_array(void)
    {
#ifdef SHL_DBG_ARRAY
        printf("Getting pointer for array [%s] expanded=%d\n",
               shl_array<T>::name, is_expanded);
#endif
        if (shl_array<T>::alloc_done) {
            return is_expanded ? shl_array_replicated<T>::get_array() :
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
        return is_expanded ? shl_array_replicated<T>::get(i) : shl_array<T>::get(i);
    }

    /**
     * \brief Write to array.
     *
     * Collapsed mode: write to master-copy
     *
     */
    virtual void set(size_t i, T v)
    {
        if (is_expanded) {

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

    void set_cached(size_t i, T v, struct array_cache c)
    {
        debug_printf("set_cached: %zu: %d -> %d on rep %d thread %d\n",
                     i, shl_array_replicated<T>::rep_array[c.rid][i], v,
                     c.rid, c.tid);

        shl_array_replicated<T>::rep_array[c.rid][i] = v;

#ifndef SHL_EXPAND_NO_HOUSEKEEPING
#ifdef SHL_EXPAND_STL
        // Record write-set
        write_set_e e = std::make_pair(i,v);
        write_set_t *t = &(write_sets[c.tid]);
        t->insert(t->begin(), e);
#else
        struct ws_write_t w = (struct ws_write_t) {.index = i };
        struct ws_thread_e *e = &write_set[c.tid].p;
        e->buffer[e->ws_size++] = w;

        // write back buffer if full
        assert (e->ws_size != WS_BUFFER_SIZE); // SK remove again
        if (e->ws_size == WS_BUFFER_SIZE) {
            ws_write_back();
        }
#endif
#endif /* SHL_EXPAND_NO_HOUSEKEEPING */
    }

    bool get_expanded(void)
    {
        return is_expanded;
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
};

#endif /* SHL_ARRAY_EXPANDABLE_H */
