#ifndef __SHL_ARRAY
#define __SHL_ARRAY

#include <cstdlib>
#include <iostream>

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
    bool alloc_done;
    const char *name;
    T* array = NULL;
    T* array_copy = NULL;

public:
    shl_array(size_t s, const char *name)
    {
        size = s;
        shl_array<T>::name = name;
        use_hugepage = get_conf()->use_hugepage;
        read_only = false;

    }

    virtual void alloc(void)
    {
        print();

        int pagesize;
        array = (T*) shl__malloc(size*sizeof(T), get_options(), &pagesize);
    }

    void print(void)
    {
        print_options();
        printf("\n");
    }

    ~shl_array(void)
    {
        // if (array!=NULL) {
        //     free(array);
        // }

        // array = NULL;
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
        assert (array_copy == NULL);
        array_copy = src;
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
    virtual void copy_back(T* a)
    {
        printf("shl_array[%s]: Copying back\n", shl_array<T>::name);
        assert (array_copy == a);
        assert (array_copy != NULL);
        for (unsigned int i=0; i<size; i++) {

#ifdef SHL_DBG_ARRAY
            if( i<5 ) {
                std::cout << array_copy[i] << " " << array[i] << std::endl;
            }
#endif

            array_copy[i] = array[i];
        }
    }


protected:
    virtual void print_options(void)
    {
        printf("Array[%20s]: size=%10zu-", name, size); print_number(size);
        printf(" -- ");
        printf("hugepage=[%c] ", use_hugepage ? 'X' : ' ');
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
    T** rep_array;
    int num_replicas;
    int (*lookup)(void);

public:
    /**
     * \brief Initialize replicated array
     */
    shl_array_replicated(size_t s, const char *name,
                         int (*f_lookup)(void))
        : shl_array<T>(s, name)
    {
        shl_array<T>::read_only = true;
        lookup = f_lookup;
        master_copy = NULL;
    }

    virtual void alloc(void)
    {
        shl_array<T>::print();

        rep_array = (T**) shl_malloc_replicated(shl_array<T>::size*sizeof(T),
                                                &num_replicas,
                                                shl_array<T>::get_options());
        printf("Using %d replicas\n", num_replicas);
        assert (num_replicas>0);
        for (int i=0; i<num_replicas; i++)
            assert (rep_array[i]!=NULL);
    }

    virtual void copy_from(T* src)
    {
        assert (shl_array<T>::array_copy==NULL);
        shl_array<T>::array_copy = src;
        printf("shl_array_replicated[%s]: Copying to %d replicas\n",
               shl_array<T>::name, num_replicas);

        for (int j=0; j<num_replicas; j++) {
            for (unsigned int i=0; i< shl_array<T>::size; i++) {

                rep_array[j][i] = src[i];
            }
        }
    }

    virtual void copy_back(T* a)
    {
        // nop -- currently only replicating read-only data
        assert (shl_array<T>::read_only);
    }

    /**
     * \brief Return pointer to beginning of replica
     */
    virtual T* get_array(void)
    {
#ifdef SHL_DBG_ARRAY
        printf("Getting pointer for array [%s]\n", shl_array<T>::name);
#endif
        return rep_array[lookup()];
    }

    ~shl_array_replicated(void)
    {
        // // Free replicas
        // for (int i=0; i<num_replicas; i++) {
        //     free(rep_array[i]);
        // }
        // // Clean up meta-data
        // delete rep_array;
    }

    void synchronize(void)
    {
        shl__repl_sync(master_copy, rep_array, num_replicas, shl_array<T>::size*sizeof(T));
    }

protected:
    virtual void print_options(void)
    {
        shl_array<T>::print_options();
        printf("replication=[X]");
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
shl_array<T>* shl__malloc(size_t size, const char *name, bool is_ro) {

    // Replicate if array is read-only
    bool replicate = is_ro && get_conf()->use_replication;

    if (replicate) {
        return new shl_array_replicated<T>(size, name, shl__get_rep_id);
    } else {
        return new shl_array<T>(size, name);
    }
}

#endif /* __SHL_ARRAY */
