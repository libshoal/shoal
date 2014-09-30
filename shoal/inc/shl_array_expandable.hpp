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

private:
    Timer expand_timer;
    Timer t_write_back[MAXCORES];
    union counter_element c_write_back[MAXCORES];

    /**
     * \brief Write back write set of current thread to master copy
     */
    void ws_write_back(void)
    {
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
    }

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
    #define WS_BUFFER_SIZE 10*1024

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
        int r = pthread_barrier_wait(&b);
        assert(r==PTHREAD_BARRIER_SERIAL_THREAD || r==0);

#if SHL_EXPAND_NAIVE

        if (r==PTHREAD_BARRIER_SERIAL_THREAD) {

            printf("Expanding array .. SERIAL\n");

            expand_timer.start();
            assert (is_expanded==false);

            is_expanded = true;
            shl__repl_sync((void*) shl_array<T>::array,
                           (void**) shl_array_replicated<T>::rep_array,
                           shl_array_replicated<T>::num_replicas,
                           shl_array<T>::size*sizeof(T));
            double s = expand_timer.stop();

            printf("Done! (t_expand: %lf)\n", s);
        }
        pthread_barrier_wait(&b);
#else
        if (r==PTHREAD_BARRIER_SERIAL_THREAD) {

            printf("Expanding array .. SERIAL\n");
        }

        Timer t_expand;
        t_expand.start();

        // Every thread copies their own data
        if (shl__get_tid()==shl__get_rep_id()) {
            memcpy((void*)shl_array_replicated<T>::rep_array[shl__get_rep_id()],
                    (void*)shl_array<T>::array,
                    shl_array<T>::size*sizeof(T));
        }
        pthread_barrier_wait(&b);

        if (r==PTHREAD_BARRIER_SERIAL_THREAD) {

            printf("Done! (t_expand: %lf)\n", t_expand.stop());
        }

        is_expanded = true;

#endif
    }

    void collapse(void)
    {
#ifdef SHL_EXPAND_STL
        int r = pthread_barrier_wait(&b);
        assert(r==PTHREAD_BARRIER_SERIAL_THREAD || r==0);

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
#endif

#ifdef SANITY_CHECK
                    assert (shl_array_replicated<T>::rep_array[pid][idx] == val);
#endif
                    shl_array<T>::array[idx] = val;
                }
            }
            is_expanded = false;
        }
#else
        ws_write_back();
        is_expanded = false;
#endif
        int r __attribute((unused)) = pthread_barrier_wait(&b);

#ifdef SHL_DBG_ARRAY
        printf("---client-%-2d---collapse-done---------------------------\n",
               shl__get_tid());

        if (r==PTHREAD_BARRIER_SERIAL_THREAD) {

            shl_array<T>::dump();
        }
#endif

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
     * set() function has to be used to write arrays.
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

            debug_printf("writing index %zu on %d\n", i, shl__get_tid());

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
        shl_array_replicated<T>::rep_array[c.rid][i] = v;

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
        if (e->ws_size == WS_BUFFER_SIZE) {
            ws_write_back();
        }
#endif
    }

    bool get_expanded(void)
    {
        return is_expanded;
    }

    virtual ~shl_array_expandable(void)
    {
        pthread_barrier_destroy(&b);

        for (int i=0; i<shl__num_threads(); i++) {

            printf("destructor %2d: t_write_back %11.6lf - c_write_back % 4d\n",
                   i, t_write_back[i].get(), c_write_back[i].counter);
        }
    }

protected:
    virtual void print_options(void)
    {
        shl_array<T>::print_options();
        printf("expandable=[X]");
    }
};

#endif /* SHL_ARRAY_EXPANDABLE_H */
