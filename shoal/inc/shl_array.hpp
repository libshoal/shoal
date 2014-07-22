#ifndef __SHL_ARRAY
#define __SHL_ARRAY

#include <cstdlib>

#include "shl.h"

// --------------------------------------------------
// Implementations
// --------------------------------------------------

/**
 * \brief Base class representing shoal arrays
 *
 */
template <class T>
class shl_array {

protected:
    size_t size;
    bool use_hugepage;
    T* array = NULL;

public:
    shl_array(size_t s)
    {
        printf("Array: size="); print_number(s); printf("\n");

        size = s;
        use_hugepage = true; // XXX make argument

        array = (T*) malloc(s*sizeof(T));
        assert (array!=NULL);
    }

    ~shl_array(void)
    {
        if (array!=NULL) {
            free(array);
        }
    }

    T* get_array(void)
    {
        return array;
    }

    int get_options(void)
    {
        int options = SHL_MALLOC_NONE;

        if (use_hugepage)
            options |= SHL_MALLOC_HUGEPAGE;

        return options;
    }


    /**
     *\ brief Allocate array
     *
     * Several choices here:
     *
     * - replication: Three policies:
     * 1) Replica if read-only.
     * 2) Replica if w/r ratio below a certain threshold.
     *  2.1) Write to all replicas
     *  2.2) Write locally; maintain write-set; sync call when updates need to
     *       be propagated.
     *
     * - huge pages: Use if working set < available RAM && alloc_real/size < 1.xx
     *   ? how to determine available RAM?
     *   ? how to know the size of the working set?
     *
     */
    static shl_array<T>* shl__malloc(size_t size, bool is_ro) {

        return new shl_array<T>(size);
    }

};

/**
 * \brief Array abstraction for single copy array
 *
 */
template <class T>
class shl_array_single : public shl_array<T> {
    T* array;
public:
    shl_array_single(size_t s)
        : shl_array<T>(s)
    {
        array = NULL;
    }

    /**
     * \brief Return array
     *
     * Array is allocated lazily
     */
    T* shl_get(void)
    {
        if (array==NULL) {
            array = shl__malloc(shl_array<T>::size, shl_array<T>::get_options());
        }

        return array;
    }
};

/**
 * \brief Array implementing replication
 *
 */
template <class T>
class shl_array_replicated : public shl_array<T>
{
    T* master_copy;
    T** array;
    size_t num_replicas;
    int (*lookup)(void);

    /**
     * \brief Initialize replicated array
     */
    shl_array_replicated(size_t s,
                         size_t num_repl,
                         int(*f_lookup)(void),
                         T* master)
        : shl_array<T>(s)
    {
        lookup = f_lookup;
        num_replicas = num_repl;
        master_copy = master;
        array = shl_malloc_replicated(s, num_repl, shl_array<T>::get_options());
    }

    /**
     * \brief Return pointer to beginning of replica
     */
    T* shl_get(void)
    {
        return array[lookup()];
    }

    ~shl_array_replicated(void)
    {
        // Free replicas
        for (int i=0; i<num_replicas; i++) {
            free(array[i]);
        }
        // Clean up meta-data
        delete array;
    }

    void synchronize(void)
    {
        shl__repl_sync(master_copy, array, num_replicas, shl_array<T>::size*sizeof(T));
    }
};

#endif /* __SHL_ARRAY */
