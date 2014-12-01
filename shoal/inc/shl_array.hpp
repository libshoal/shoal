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
#include <cstdarg>
#include <cstring> // memset
#include <iostream>
#include <limits>

#include "shl.h"
#include "shl_timer.hpp"
#include "shl_configuration.hpp"

#ifndef BARRELFISH
//XXX: is this also available for barrelfish??
extern "C" {
#include "crc.h"
}
#endif

//#define PROFILE

// --------------------------------------------------
// Implementations
// --------------------------------------------------

/**
 * \brief Base class for shoal array
 */
class shl_base_array {
 public:
    const char *name;   ///< name of the array

 public:

    /**
     * \brief base array constructor
     *
     * \param name  Name of the array.
     */
    shl_base_array(const char *_name)
    {
        shl_base_array::name = _name;
    }
};

/*
 * ==============================================================================
 * Generic Array
 * ==============================================================================
 */

/**
 * \brief Generic Shoal Array
 *
 * This class implements single-node arrays.
 */
template<class T>
class shl_array : public shl_base_array {


 private:
    T* array;           ///< pointer to the backing memory region
    T* array_copy;      ///< XXX: what's that for?

 protected:
    /**
     * Size of array in elements (and not bytes)
     */
    size_t size;        ///< size of the array in elements

    /* ------------------------- Flags ------------------------- */
    bool use_hugepage;  ///< flag indicating the use of huge pages
    bool read_only;     ///< flag indicating that this is a read only array
    bool alloc_done;    ///< flag indicating the allocation is done
                        /// XXX: is this needed can't we just take the  array pointer ?
    bool is_used;       ///< flag indicating that this array is used
                        /// XXX: What does that mean?
    bool is_dynamic;    ///< flag indicating the array is dynamic
                        /// XXX: what does that mean?

#ifdef BARRELFISH
    void *meminfo;      ///< backend specific memory information
#endif


#ifdef PROFILE
    int64_t num_wr;
    int64_t num_rd;
#endif


 public:

    /*
     * ---------------------------------------------------------------------------
     * Constructors / Destructors
     * ---------------------------------------------------------------------------
     */

    /**
     * \brief Generic array constructure
     * @param s
     * @param _name
     */
    shl_array(size_t s, const char *_name) :
                    shl_base_array(_name)
    {
        size = s;
        use_hugepage = get_conf()->use_hugepage;
        read_only = false;
        alloc_done = false;
        is_dynamic = false;
        is_used = false;
        meminfo = NULL;
        array = NULL;
        array_copy = NULL;
#ifdef PROFILE
        num_wr = 0;
        num_rd = 0;
#endif
    }

    shl_array(size_t s, const char *_name, void *mem, void *data)
    {
        size = s;
        is_dynamic = false;
        is_used = false;
        shl_array<T>::name = _name;
        use_hugepage = get_conf()->use_hugepage;
        read_only = false;
        array = (T*) data;
        array_copy = NULL;
        meminfo = mem;
        alloc_done = true;
    }

    virtual ~shl_array(void)
    {
        // if (array!=NULL) {
        //     free(array);
        // }

        // array = NULL;
    }

    /*
     * ---------------------------------------------------------------------------
     * Array Initialization Methods
     * ---------------------------------------------------------------------------
     */

    /**
     * \brief
     *
     * XXX: should this not return an error in case malloc fails?
     */
    virtual void alloc(void)
    {
        if (array) {
            return;
        }

        //if (!do_alloc())
        //   return;

        assert(!alloc_done);

        print();

        /*
         * XXX: What's the case with the page size here
         */
        int pagesize;
        array = (T*) shl__malloc(size * sizeof(T), get_options(), &pagesize,
                                 SHL_NUMA_IGNORE, &meminfo);

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

    /*
     * ---------------------------------------------------------------------------
     * Array Access Methods
     * ---------------------------------------------------------------------------
     */

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
     * \brief Optimized method to copy the content of arrays between
     * each other.
     *
     * This is useful for example for "double-buffering" for parallel
     * OpenMP loops.
     */
    virtual void copy_from_array(shl_array<T> *src)
    {
        assert(!"Not yet implemented");
    }

    virtual void init_from_value(T value)
    {
        memset(array, value, size);
    }

    /**
     * \brief Copy data from existing array
     *
     * Warning: The sice of array src is not checked. Assumption:
     * sizeof(src) == this->size
     */
    virtual void copy_from(T* src)
    {
        if (!do_copy_in()) {
#if defined(BARRELFISH) && !SHL_BARRELFISH_USE_SHARED
            printf("shl_array[%s]: not copying \n", shl_array<T>::name);
#endif
            return;
        }

#ifdef SHL_DBG_ARRAY
        printf("Copying array %s\n", shl_base_array::name);
#endif
        assert(alloc_done);

        printf("shl_array[%s]: copy_from\n", shl_array<T>::name);

        assert(array_copy == NULL);
        array_copy = src;
        for (unsigned int i = 0; i < size; i++) {
            array[i] = src[i];
        }
#if defined(BARRELFISH) && !SHL_BARRELFISH_USE_SHARED
        shl__barrelfish_share_frame((struct mem_info *)meminfo);
#endif
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

        assert(alloc_done);

        printf("shl_array[%s]: Copying back\n", shl_base_array::name);
        assert(array_copy == a);
        assert(array_copy != NULL);
        for (unsigned int i = 0; i < size; i++) {

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
        printf("Array[%20s]: elements=%10zu-", shl_base_array::name, size);
        print_number(size);
        printf(" size=%10zu-", size * sizeof(T));
        print_number(size * sizeof(T));
        printf(" -- ");
        printf("hugepage=[%c] ", use_hugepage ? 'X' : ' ');
    }

    virtual void dump(void)
    {
        for (size_t i = 0; i < size; i++) {

            noprintf("idx[%3zu] is %d\n", i, array[i]);
        }
    }

 public:
    virtual unsigned long get_crc(void)
    {
#ifdef BARRELFISH
        return 0;
#else
        if (!alloc_done)
            return 0;

        crc_t crc = crc_init();

        uintptr_t max = sizeof(T) * size;
        crc = crc_update(crc, (unsigned char*) array, max);

        crc_finalize(crc);

        return (unsigned long) crc;
#endif
    }

    virtual void print_crc(void)
    {
        printf("CRC %s 0x%lx\n", shl_base_array::name, get_crc());
    }

 public:
    Timer t_collapse;
    Timer t_expand[MAXCORES];

    virtual void collapse(void)
    {
        // nop
    }

    virtual void expand(void)
    {
        // nop
    }

    virtual void set_cached(size_t i, T v, struct array_cache c)
    {
        set(i, v);
    }
};

/*
 * ==============================================================================
 * Distributed Array
 * ==============================================================================
 */

/**
 * \brief Array implementing a distributed array
 *
 */
template<class T>
class shl_array_distributed : public shl_array<T> {
 public:
    /**
     * \brief Initialize distributed array
     */
    shl_array_distributed(size_t s, const char *_name) :
                    shl_array<T>(s, _name)
    {
    }
    ;

    shl_array_distributed(size_t s, const char *_name, void *mem, void *data) :
                    shl_array<T>(s, _name, mem, data)
    {
    }
    ;

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

/*
 * ==============================================================================
 * Partitioned Array
 * ==============================================================================
 */

/**
 * \brief Array implementing a partitioned array
 *
 */
template<class T>
class shl_array_partitioned : public shl_array<T> {
 public:
    /**
     * \brief Initialize partitioned array
     */
    shl_array_partitioned(size_t s, const char *_name) :
                    shl_array<T>(s, _name)
    {
    }
    ;

    shl_array_partitioned(size_t s, const char *_name, void *mem, void *data) :
                    shl_array<T>(s, _name, mem, data)
    {
    }
    ;

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

/*
 * ==============================================================================
 * Replicated Array
 * ==============================================================================
 */

/**
 * \brief Array implementing replication
 *
 *
 * The number of replicas is given by shl_malloc_replicated.
 */
template<class T>
class shl_array_replicated : public shl_array<T> {
    T* master_copy;

 public:
    void **mem_array;
    T** rep_array;

 protected:
    int num_replicas;
 public:
    int (*lookup)(void);

 public:
    /**
     * \brief Initialize replicated array
     */
    shl_array_replicated(size_t s, const char *_name, int (*f_lookup)(void)) :
                    shl_array<T>(s, _name)
    {
        shl_array<T>::read_only = true;
        lookup = f_lookup;
        master_copy = NULL;
        num_replicas = -1;
        rep_array = NULL;
    }

    shl_array_replicated(size_t s,
                         const char *_name,
                         int (*f_lookup)(void),
                         void *mem,
                         void *data) :
                    shl_array<T>(s, _name, mem, data)
    {
        shl_array<T>::read_only = true;
        lookup = f_lookup;
        master_copy = NULL;
        num_replicas = -1;
        rep_array = NULL;
    }

    virtual void alloc(void)
    {
        if (!shl_array<T>::do_alloc())
            return;

        shl_array<T>::print();

        assert(!shl_array<T>::alloc_done);

        rep_array = (T**) shl_malloc_replicated(shl_array<T>::size * sizeof(T),
                                                &num_replicas,
                                                shl_array<T>::get_options());

        mem_array = ((void **) rep_array) + num_replicas;

        assert(num_replicas > 0);
        for (int i = 0; i < num_replicas; i++)
            assert(rep_array[i]!=NULL);

        shl_array<T>::alloc_done = true;

    }

    virtual void copy_from(T* src)
    {
        if (!shl_array<T>::do_copy_in())
            return;

        assert(shl_array<T>::alloc_done);
        assert(shl_array<T>::array_copy==NULL);

        shl_array<T>::array_copy = src;
        printf("shl_array_replicated[%s]: Copying to %d replicas\n",
               shl_base_array::name, num_replicas);

        for (int j = 0; j < num_replicas; j++) {
            for (unsigned int i = 0; i < shl_array<T>::size; i++) {

                rep_array[j][i] = src[i];
            }
        }
    }

    virtual void copy_back(T* a)
    {
        // nop -- currently only replicating read-only data
        assert(shl_array<T>::read_only);
    }

    /**
     * \brief Return pointer to beginning of replica
     */
    virtual T* get_array(void)
    {
#ifdef SHL_DBG_ARRAY
        printf("Getting pointer for array [%s]\n", shl_base_array::name);
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
        for (int j = 0; j < num_replicas; j++)
            rep_array[j][i] = v;
    }

    virtual ~shl_array_replicated(void)
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
        assert(shl_array<T>::alloc_done);
        shl__repl_sync(master_copy, (void**) rep_array, num_replicas,
                       shl_array<T>::size * sizeof(T));
    }

 protected:
    virtual void print_options(void)
    {
        shl_array<T>::print_options();
        printf("replication=[X]");
    }

    virtual void dump(void)
    {
        for (int j = 0; j < num_replicas; j++) {

            for (size_t i = 0; i < shl_array<T>::size; i++) {

                noprintf("rep[%2d] idx[%3zu] is %d\n", j, i, rep_array[j][i]);
            }
        }
    }

 public:
    virtual unsigned long get_crc(void)
    {

#ifdef BARRELFISH
        return 0;
#else
        if (!shl_array<T>::alloc_done)
            return 0;

        crc_t *crc = (crc_t*) malloc(sizeof(crc_t) * num_replicas);
        assert(crc);

        for (int i = 0; i < 1; i++) {

            crc[i] = crc_init();

            uintptr_t max = sizeof(T) * shl_array<T>::size;
            crc[i] = crc_update(crc[i], (unsigned char*) (rep_array[i]), max);

            crc[i] = crc_finalize(crc[i]);

            if (!(i == 0 || ((unsigned long) crc[i] == (unsigned long) crc[0]))) {

                printf(ANSI_COLOR_CYAN "WARNING: " ANSI_COLOR_RESET
                "replica %d's content diverges (%lx vs %lx)\n",
                       i, (unsigned long) crc[i], (unsigned long) crc[0]);
            } else {
                printf("replica %d's content is %lx\n", i, (unsigned long) crc[i]);
            }
        }

        unsigned long r = (unsigned long) crc[0];
        free(crc);
        return r;
#endif
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
template<class T>
shl_array<T>* shl__malloc_array(size_t size, const char *name,
//
                                bool is_ro,
                                bool is_dynamic,
                                bool is_used,
                                bool is_graph,
                                bool is_indexed,
                                bool initialize)
{
    // Policy for memory allocation
    // --------------------------------------------------

    // 1) Always partition indexed arrays
    bool partition = is_indexed && get_conf()->use_partition;

    // 2) Replicate if array is read-only and can't be partitioned
    bool replicate = !partition &&  // none of the others
                    is_ro && get_conf()->use_replication;

    // 3) Distribute if nothing else works and there is more than one node
    bool distribute = !replicate && !partition
                    &&  // none of the others
                    shl__get_num_replicas() > 1 && get_conf()->use_distribution
                    && initialize;

    shl_array<T> *res = NULL;

    if (partition) {
#ifdef SHL_DBG_ARRAY
        printf("Allocating partitioned array\n");
#endif
#if defined(BARRELFISH) && !SHL_BARRELFISH_USE_SHARED
        void *data;
        void *mem;
//        void *array = shl__malloc(size*sizeof(T)),
//                        SHL_MALLOC_HUGEPAGE | SHL_MALLOC_PARTITION, NULL,
//                        &mem, &data);
        res = new shl_array_partitioned<T>(size, name, mem, data);
#else
        res = new shl_array_partitioned<T>(size, name);
#endif
    } else if (replicate) {
#ifdef SHL_DBG_ARRAY
        printf("Allocating replicated array\n");
#endif
#if defined(BARRELFISH) && !SHL_BARRELFISH_USE_SHARED
        void *data;
        void *mem;
//        void *array = shl__malloc(size*sizeof(T), sizeof(shl_array<T>),
//                        SHL_MALLOC_HUGEPAGE | SHL_MALLOC_REPLICATED,
//                        NULL, &mem, &data);
        res = new shl_array<T>(size, name, mem, data);
#else
        res = new shl_array_replicated<T>(size, name, shl__get_rep_id);
#endif
    } else if (distribute) {
#ifdef SHL_DBG_ARRAY
        printf("Allocating distributed array\n");
#endif
#if defined(BARRELFISH) && !SHL_BARRELFISH_USE_SHARED
        void *data;
        void *mem;
        //void *array = shl__malloc(size*sizeof(T), sizeof(shl_array_distributed<T>),
        //                SHL_MALLOC_HUGEPAGE | SHL_MALLOC_DISTRIBUTED,
        //                NULL, &mem, &data);
        res = new shl_array_distributed<T>(size, name, mem, data);
#else
        res = new shl_array_distributed<T>(size, name);
#endif
    } else {
#ifdef SHL_DBG_ARRAY
        printf("Allocating regular array\n");
#endif
#if defined(BARRELFISH) && !SHL_BARRELFISH_USE_SHARED
        void *data;
        void *mem;
        //void *array = shl__malloc(size*sizeof(T), sizeof(shl_array<T>), SHL_MALLOC_HUGEPAGE, NULL, &mem, &data);
        res = new shl_array<T>(size, name, mem, data);
#else
        res = new shl_array<T>(size, name);
#endif
    }

    // These are used internally in array to decide if copy-in and
    // copy-out of source arrays are required
    res->set_dynamic(is_dynamic);
    res->set_used(is_used);

    return res;
}

template<class T>
int shl__estimate_size(size_t size,
                       const char *name,
                       bool is_ro,
                       bool is_dynamic,
                       bool is_used,
                       bool is_graph,
                       bool is_indexed)
{
    return is_used ? sizeof(T) * size : 0;
}

int shl__estimate_working_set_size(int num, ...);

#endif /* __SHL_ARRAY */
