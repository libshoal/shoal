#ifndef SHL_ARRAY_EXPANDABLE__H
#define SHL_ARRAY_EXPANDABLE__H

#include "shl.h"

#include <map>

extern "C" {
#include <pthread.h>
}

#define SANITY_CHECK
#define SHL_DBG_ARRAY

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


template <class T>
class shl_array_expandable : public shl_array_replicated<T>
{
    typedef std::map<size_t, T> write_set_t;
    typedef std::map<size_t, write_set_t> write_sets_t;
    typedef typename std::map<size_t, T>::iterator write_set_i;
    typedef typename std::map<size_t, write_set_t>::iterator write_sets_i;

    bool is_expanded;
    write_sets_t write_sets;
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

        if (r==PTHREAD_BARRIER_SERIAL_THREAD) {

            assert (is_expanded==false);

            printf("Expanding array .. SERIAL\n");
            is_expanded = true;
            shl__repl_sync((void*) shl_array<T>::array,
                           (void**) shl_array_replicated<T>::rep_array,
                           shl_array_replicated<T>::num_replicas,
                           shl_array<T>::size*sizeof(T));
        }
        pthread_barrier_wait(&b);
    }

    void collapse(void)
    {
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

//                 if (write_sets[i].size()<1)
//                     continue;

                write_set_i j;
                for (j=t.begin(); j!=t.end(); ++j) {

                    size_t idx = j->first;
                    T val = j->second;
#ifdef DEBUG
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
        pthread_barrier_wait(&b);
    }

    /**
     * \brief Allocate both arrays and switch to collapsed mode
     */
    virtual void alloc(void)
    {
        shl_array<T>::alloc(); shl_array<T>::alloc_done = false;
        shl_array_replicated<T>::alloc();

        for (int i=0; i<shl__num_threads(); i++) {

            write_set_t t;
            write_sets.insert(make_pair(i, t));
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
            int rep_id = shl_array_replicated<T>::lookup();
            shl_array_replicated<T>::rep_array[rep_id][i] = v;
            // Record write-set
            write_sets[shl__get_tid()].insert(std::make_pair(i, v));
        }

        else {
            shl_array<T>::set(i, v);
        }
    }

    ~shl_array_expandable(void)
    {
        pthread_barrier_destroy(&b);
    }

protected:
    virtual void print_options(void)
    {
        shl_array<T>::print_options();
        printf("replication=[X]");
    }
};

#endif /* SHL_ARRAY_EXPANDABLE_H */
