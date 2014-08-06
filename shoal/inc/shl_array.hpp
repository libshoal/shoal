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
    bool read_only;
    T* array = NULL;

public:
    shl_array(size_t s)
    {
        size = s;
        use_hugepage = get_conf()->use_hugepage;
        read_only = false;

    }

    void alloc(void)
    {
        printf("Array: size="); print_number(size); printf(" -- ");
        print_options();
        printf("\n");

        array = (T*) malloc(size*sizeof(T));
        assert (array!=NULL);
    }

    ~shl_array(void)
    {
        if (array!=NULL) {
            free(array);
        }
    }

    virtual T* get_array(void)
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
     * \brief Copy data from existing array
     *
     * Warning: The sice of array src is not checked. Assumption:
     * sizeof(src) == this->size
     */
    virtual void copy_from(T* src)
    {
        for (unsigned int i=0; i<size; i++) {

            array[i] = src[i];
        }
    }

    /**
     * \brief Copy data to existing array
     *
     * Warning: The sice of array src is not checked. Assumption:
     * sizeof(src) == this->size
     */
    virtual void copy_back(T* src)
    {
        for (unsigned int i=0; i<size; i++) {

            src[i] = array[i];
        }
    }


protected:
    virtual void print_options(void)
    {
        printf("hugepage=[%c] ", use_hugepage ? 'X' : ' ');
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
    int num_replicas;
    int (*lookup)(void);

public:
    /**
     * \brief Initialize replicated array
     */
    shl_array_replicated(size_t s,
                         int (*f_lookup)(void))
        : shl_array<T>(s)
    {
        shl_array<T>::read_only = true;
        lookup = f_lookup;
        master_copy = NULL;
    }

    void alloc(void)
    {
        array = (T**) shl_malloc_replicated(shl_array<T>::size, &num_replicas,
                                            shl_array<T>::get_options());
    }

    virtual void copy_from(T* src)
    {
        for (unsigned int j=0; j<num_replicas; j++) {
            for (unsigned int i=0; i< shl_array<T>::size; i++) {

                array[j][i] = src[i];
            }
        }

        master_copy = src;
    }

    virtual void copy_back(T* src)
    {
        // nop -- currently only replicating read-only data
        assert (shl_array<T>::read_only);
    }

    /**
     * \brief Return pointer to beginning of replica
     */
    virtual T* shl_get(void)
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

protected:
    virtual void print_options(void)
    {
        shl_array<T>::print_options();
        printf("replication=[X] ");
    }
};

// --------------------------------------------------
// Allocation
// --------------------------------------------------

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
template <class T>
shl_array<T>* shl__malloc(size_t size, bool is_ro) {

    // Replicate if array is read-only
    bool replicate = is_ro && get_conf()->use_replication;

    if (replicate) {
        return new shl_array_replicated<T>(size, shl__get_rep_id);
    } else {
        return new shl_array<T>(size);
    }
}

#endif /* __SHL_ARRAY */
