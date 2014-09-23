/*
 * Copyright (c) 2014 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef __SHL_ARRAY
#define __SHL_ARRAY

#include <cstdlib>
#include <iostream>

#include "shl.h"
#include "shl_configuration.hpp"

//#define PROFILE

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

#ifdef PROFILE
    int64_t num_wr;
    int64_t num_rd;
#endif

    void *mem;

    bool is_used;
    bool is_dynamic;

public:
    shl_array(size_t s, const char *_name)
    {
        size = s;
        shl_array<T>::name = _name;
        use_hugepage = get_conf()->use_hugepage;
        read_only = false;
        alloc_done = false;

#ifdef PROFILE
        num_wr = 0;
        num_rd = 0;
#endif
    }

    virtual void alloc(void)
    {
        if (!do_alloc())
            return;

        print();
        assert (!alloc_done);

        int pagesize;
        array = (T*) shl__malloc(size*sizeof(T), get_options(), &pagesize, &mem);

        alloc_done = true;
    }

    virtual bool do_alloc(void)
    {
        return is_used;
    }

    virtual bool do_copy_in(void)
    {
        return is_used && !is_dynamic;
    }

    virtual T get(size_t i)
    {
#ifdef PROFILE
        __sync_fetch_and_add(&num_rd, 1);
#endif
        return array[i];
    }

    virtual void set(size_t i, T v)
    {
#ifdef PROFILE
        __sync_fetch_and_add(&num_wr, 1);
#endif
        array[i] = v;
    }

    virtual void print_statistics(void)
    {
#ifdef PROFILE
        printf("Number of writes %10d\n", num_wr);
        printf("Number of reads  %10d\n", num_rd);
#endif
    }

    /*
     * Read only data does NOT have to be copied back. Also,
     * dynamically allocated stuff and arrays that are not used
     */
    virtual bool do_copy_back(void)
    {
        return is_used && !read_only && !is_dynamic;
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

    virtual int get_options(void)
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
        if (!do_copy_in())
            return;

#ifdef SHL_DBG_ARRAY
        printf("Copying array %s\n", name);
#endif
        assert (alloc_done);

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
        if (!do_copy_back())
            return;

        assert (alloc_done);

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

    void set_used(bool used)
    {

        shl_array<T>::is_used = used;
    }

    void set_dynamic(bool dynamic)
    {

        shl_array<T>::is_dynamic = dynamic;
    }


protected:
    virtual void print_options(void)
    {
        printf("Array[%20s]: elements=%10zu-", name, size); print_number(size);
        printf(" size=%10zu-", size*sizeof(T)); print_number(size*sizeof(T));
        printf(" -- ");
        printf("hugepage=[%c] ", use_hugepage ? 'X' : ' ');
    }
};


/**
 * \brief Array implementing a distributed array
 *
 */
template <class T>
class shl_array_distributed : public shl_array<T>
{
public:
    /**
     * \brief Initialize distributed array
     */
    shl_array_distributed(size_t s, const char *_name)
        : shl_array<T>(s, _name) {};

    virtual int get_options(void)
    {
        return shl_array<T>::get_options() | SHL_MALLOC_DISTRIBUTED;
    }

protected:
    virtual void print_options(void)
    {
        shl_array<T>::print_options();
        printf("distribution=[X]");
    }

};


/**
 * \brief Array implementing a partitioned array
 *
 */
template <class T>
class shl_array_partitioned : public shl_array<T>
{
public:
    /**
     * \brief Initialize partitioned array
     */
    shl_array_partitioned(size_t s, const char *_name)
        : shl_array<T>(s, _name) {};

    virtual int get_options(void)
    {
        return shl_array<T>::get_options() | SHL_MALLOC_PARTITION;
    }

protected:
    virtual void print_options(void)
    {
        shl_array<T>::print_options();
        printf("partition=[X]");
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

    void **mem_array;

public:
    /**
     * \brief Initialize replicated array
     */
    shl_array_replicated(size_t s, const char *_name,
                         int (*f_lookup)(void))
        : shl_array<T>(s, _name)
    {
        shl_array<T>::read_only = true;
        lookup = f_lookup;
        master_copy = NULL;
    }

    virtual void alloc(void)
    {
        if (!shl_array<T>::do_alloc())
            return;

        assert (!shl_array<T>::alloc_done);

        shl_array<T>::print();

        rep_array = (T**) shl_malloc_replicated(shl_array<T>::size*sizeof(T),
                                                &num_replicas,
                                                shl_array<T>::get_options());

        mem_array = ((void **)rep_array) + num_replicas;


        assert (num_replicas>0);
        for (int i=0; i<num_replicas; i++)
            assert (rep_array[i]!=NULL);

        shl_array<T>::alloc_done = true;
    }

    virtual void copy_from(T* src)
    {
        if (!shl_array<T>::do_copy_in())
            return;

        assert (shl_array<T>::alloc_done);
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
        if (shl_array<T>::alloc_done) {
            return rep_array[lookup()];
        } else {
            return NULL;
        }
    }

    virtual T get(size_t i)
    {
#ifdef PROFILE
        __sync_fetch_and_add(&shl_array<T>::num_rd, 1);
#endif
        return rep_array[lookup()][i];
    }

    virtual void set(size_t i, T v)
    {
#ifdef PROFILE
        __sync_fetch_and_add(&shl_array<T>::num_wr, 1);
#endif
        for (int j=0; j<num_replicas; j++)
            rep_array[j][i] = v;
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
        assert (shl_array<T>::alloc_done);
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
 * \param size Number of elements of type T in array
 * \param name Name of the array (for debugging purposes)
 * \param is_ro Indicate if array is read-only
 * \param is_dynamic Indicate that array is dynamically allocated (i.e.
 *    does not require copy in and copy out)
 * \param is_used Some arrays are allocated, but never used (such as
 *    parts of the graph in Green Marl). We want to avoid copying these,
 *    so we pass this information on.
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
shl_array<T>* shl__malloc(size_t size,
            const char *name,
            bool is_ro,
            bool is_dynamic,
            bool is_used,
            bool is_graph,
            bool is_indexed,
            bool initialize) {

    // Policy for memory allocation
    // --------------------------------------------------

    // 1) Always partition indexed arrays
    bool partition = is_indexed && get_conf()->use_partition;

    // 2) Replicate if array is read-only and can't be partitioned
    bool replicate = !partition && // none of the others
        is_ro && get_conf()->use_replication;

    // 3) Distribute if nothing else works and there is more than one node
    bool distribute = !replicate && !partition && // none of the others
        shl__get_num_replicas()>1 &&
        get_conf()->use_distribution && initialize;

    shl_array<T> *res = NULL;

    if (partition) {
#ifdef SHL_DBG_ARRAY
        printf("Allocating partitioned array\n");
#endif
        res = new shl_array_partitioned<T>(size, name);
    } else if (replicate) {
#ifdef SHL_DBG_ARRAY
        printf("Allocating replicated array\n");
#endif
        res = new shl_array_replicated<T>(size, name, shl__get_rep_id);
    } else if (distribute) {
#ifdef SHL_DBG_ARRAY
        printf("Allocating distributed array\n");
#endif
        res =  new shl_array_distributed<T>(size, name);
    } else {
#ifdef SHL_DBG_ARRAY
        printf("Allocating regular array\n");
#endif
        res = new shl_array<T>(size, name);
    }

    // These are used internally in array to decide if copy-in and
    // copy-out of source arrays are required
    res->set_dynamic (is_dynamic);
    res->set_used (is_used);

    return res;
}

#endif /* __SHL_ARRAY */
