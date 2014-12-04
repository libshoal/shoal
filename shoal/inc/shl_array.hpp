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
extern "C" {
// SK: This is in contrib/pycrc and should be platform independent
#include "crc.h"
}
#endif

///< Enables array access range checks
#define ENABLE_RANGE_CHECK 1

#if ENABLE_RANGE_CHECK
#define RANGE_CHECK(_i) assert(array && _i < size);
#else
#define RANGE_CHECK(_i)
#endif

///< Enables array access profiling
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


 protected:
    /**
     * Size of array in elements (and not bytes)
     */
    size_t size;        ///< size of the array in elements

    /* ------------------------- Flags ------------------------- */
    bool use_hugepage;  ///< flag indicating the use of huge pages
    bool read_only;     ///< flag indicating that this is a read only array
    bool alloc_done;    ///< flag indicating the allocation is done

                        /// We have several different array pointers
                        /// (depending on the array implementation),
                        /// so we keep an extra bool to simplify
                        /// tracking of whether arrays have been
                        /// allocated

    bool is_used;       ///< flag indicating that this array is used.

                        /// Some arrays might be allocated, but are
                        /// never used (such as part of the graph data
                        /// structure for some of the Green Marl
                        /// programs). If we know that arrays are not
                        /// used, we don't have to copy them
                        /// around. Alternatively, we could (and
                        /// should) remove this from the library and
                        /// deal with this in the Green Marl compiler

    bool is_dynamic;    ///< flag indicating the array is dynamic

                        /// This is an array attribute I found useful
                        /// to deal with GM programs. There are arrays
                        /// that are allocated prior to executing the
                        /// main routine, and others that are
                        /// dynamically allocated within
                        /// programs. Latter ones do not have to be
                        /// copied back. We probably want to get rid
                        /// of this flag and deal with this in GM.

    void *meminfo;      ///< backend specific memory information
    T* array;           ///< pointer to the backing memory region


#ifdef PROFILE
    int64_t num_wr;     ///< number of writes to that array
    int64_t num_rd;     ///< number of reads from this array
#endif


 public:

    /*
     * ---------------------------------------------------------------------------
     * Constructors / Destructors
     * ---------------------------------------------------------------------------
     */

    /**
     * \brief Generic array constructor. 
     *
     * \param _size the element count of this array
     * \param _name name of the array for identification purposes
     *
     * This constructor does not allocate the backing memory for the array.
     */
    shl_array(size_t _size, const char *_name) :
                    shl_base_array(_name)
    {
        size = _size;
        use_hugepage = get_conf()->use_hugepage;
        read_only = false;
        alloc_done = false;
        is_dynamic = false;
        is_used = false;
        meminfo = NULL;

        array = NULL;

#ifdef PROFILE
        num_wr = 0;
        num_rd = 0;
#endif
    }

    /**
     * \brief Specialized array constructor
     *
     * @param _size
     * @param _name
     * @param mem
     * @param data
     *
     * This constructor uses the supplied memory region for the array data.
     * The caller must ensure that the size of the memory region is large
     * enough to hold all the array elements.
     */
    shl_array(size_t _size, const char *_name, void *mem, void *data) :
                    shl_base_array(_name)
    {
        size = _size;
        is_dynamic = false;
        is_used = false;
        use_hugepage = get_conf()->use_hugepage;
        read_only = false;
        array = (T*) data;
        meminfo = mem;
        alloc_done = true;
    }

    /**
     * \brief Array destructor
     */
    virtual ~shl_array(void)
    {
        // TODO: implementation
        // if (array!=NULL) {
        //     free(array);
        // }

        // array = NULL;
    }

    /*
     * ---------------------------------------------------------------------------
     * Array Options
     * ---------------------------------------------------------------------------
     */

    /**
     * \brief checks if the array is used and memory is being allocated
     *
     * \returns TRUE  iff the array is used by the program
     *          FALSE iff the array is not used by the program
     */
    virtual bool do_alloc(void)
    {
        return is_used;
    }

    /**
     * \brief checks whether data has to be copied into the array
     *
     *
     * \return TRUE  iff the array is used by the program
     *         FALSE iff the array is not used or only used internally
     */
    virtual bool do_copy_in(void)
    {
        return is_used && !is_dynamic;
    }


    /**
     *
     * \brief checks if data has to be copied back from the array
     *
     * \returns TRUE  iff a copy has to be done
     *          FALSE otherwise
     *
     * Read only data does NOT have to be copied back. Also,
     * dynamically allocated stuff and arrays that are not used
     */
    virtual bool do_copy_back(void)
    {
        return is_used && !read_only && !is_dynamic;
    }

    /**
     * \brief returns a bit field of the enabled options for this array
     *
     * \returns bitfield repesentation of the flags
     */
    virtual int get_options(void)
    {
        int options = SHL_MALLOC_NONE;

        if (use_hugepage)
            options |= SHL_MALLOC_HUGEPAGE;

        return options;
    }


    /**
     * \brief modifies the used state of the array
     *
     * \param used  new value for the used flag
     */
    void set_used(bool used)
    {
        shl_array<T>::is_used = used;
    }

    /**
     * \brief modifies the dynamic state of the array
     *
     * \param dynamic   new value for the used flag
     */
    void set_dynamic(bool dynamic)
    {
        shl_array<T>::is_dynamic = dynamic;
    }

    /*
     * ---------------------------------------------------------------------------
     * Array Access Methods
     * ---------------------------------------------------------------------------
     */

    /**
     * \brief gets the i-th element of the array
     *
     * \param i index of the element
     *
     * \return value of element
     */
    virtual T get(size_t i)
    {
#ifdef PROFILE
        __sync_fetch_and_add(&num_rd, 1);
#endif
        RANGE_CHECK(i);

        return array[i];
    }

    /**
     * \brief writes to the i-th element
     *
     * \param i element index
     * \param v new element value
     */
    virtual void set(size_t i, T v)
    {
#ifdef PROFILE
        __sync_fetch_and_add(&num_wr, 1);
#endif
        RANGE_CHECK(i);

        array[i] = v;
    }

    /**
     * \brief returns a raw pointer to the begining of the array
     *
     * \return pointer to the array
     *         NULL if not initialized
     */
    virtual T* get_array(void)
    {
        return array;
    }

    /**
     * \brief returns the number of elements of the array
     *
     * \returns element count
     */
    size_t get_size(void)
    {
        return size;
    }

    /*
     * ---------------------------------------------------------------------------
     * Array Debug Helpers
     * ---------------------------------------------------------------------------
     */

    /**
     * \brief prints the array access statistics
     *
     * Note: The programm has to be compiled with enabled PROFILE
     */
    virtual void print_statistics(void)
    {
#ifdef PROFILE
        printf("Number of writes %10d\n", num_wr);
        printf("Number of reads  %10d\n", num_rd);
#endif
    }

    /**
     * \brief
     */
    void print(void)
    {
        print_options();
        printf("\n");
    }

        /*
     * ---------------------------------------------------------------------------
     * Array Initialization Methods
     * ---------------------------------------------------------------------------
     */

    /**
     * \brief allocates a memory region to hold the array data
     *
     * XXX: provide number of which NUMA node to allocate?
     */
    virtual int alloc(void)
    {
        /* don't alloc if the array pointer is present */
        if (array) {
            return -1;
        }

        if (!do_alloc()) {
            /* the array is not used. Skip allocation. */
            return 0;
        }

        assert(!alloc_done);

        print();

        /*
         * shl__malloc determines the pagesize to be used for an array.
         *
         * This might depend on the size of the array, access
         * patterns, and hardware we are running on.
         *
         * Alternatively, the policy on when to use hugepages could
         * also be part of the array class.
         */
        int pagesize;

        array = (T*) shl__malloc(size * sizeof(T), get_options(), &pagesize,
                                 SHL_NUMA_IGNORE, &meminfo);

        alloc_done = true;

        return 0;
    }


    /**
     * \brief Optimized method for copying data between two arrays
     *
     * \param src   The source array to copy from
     *
     * \returns 0        if the copy was successful
     *          non-zero if there was an error
     *
     *
     * This is useful for example for "double-buffering" for parallel
     * OpenMP loops.
     */
    virtual int copy_from_array(shl_array<T> *src)
    {
        assert(!"Not yet implemented");
        if (!array || !src->get_array()) {
            return -1;
        }

        if (size != src->get_size()) {
            return -1;
        }

        memcpy(array, src->get_array(), size * sizeof(T));

        return 0;
    }

    /**
     * \brief initializes the array to a specific value
     *
     * \param value data two fill the array with
     *
     * \returns 0 if the array has been initialized
     *          non-zero if the array is not yet allocated
     */
    virtual int init_from_value(T value)
    {
        if (array) {
            memset(array, value, size);
            return 0;
        }
        return -1;
    }

    /**
     * \brief Copy data from existing array
     *
     * \param src pointer to a memory location of data to copy in
     *
     * XXX WARNING: The sice of array src is not checked.
     * Assumption: sizeof(src) == this->size
     */
    virtual void copy_from(T* src)
    {
        if (!do_copy_in()) {
            return;
        }

#ifdef SHL_DBG_ARRAY
        printf("Copying array %s\n", shl_base_array::name);
#endif

        printf("shl_array[%s]: copy_from\n", shl_array<T>::name);

        for (unsigned int i = 0; i < size; i++) {
            /* XXX: does this also work with complex types ?*/
            array[i] = src[i];
        }
    }

    /**
     * \brief Copy data to existing array
     *
     * \param dest  destination array
     *
     * XXX WARNING: The sice of array src is not checked. Assumption:
     * sizeof(src) == this->size
     */
    virtual void copy_back(T* dest)
    {
        if (!do_copy_back()) {
            return;
        }

        assert(alloc_done);

        printf("shl_array[%s]: Copying back\n", shl_base_array::name);
        for (unsigned int i = 0; i < size; i++) {

#ifdef SHL_DBG_ARRAY
            if( i<5 ) {
                std::cout << dest[i] << " " << array[i] << std::endl;
            }
#endif

            dest[i] = array[i];
        }
    }


 protected:

    /**
     * \brief prints the options for this array
     */
    virtual void print_options(void)
    {
        printf("Array[%20s]: elements=%10zu-", shl_base_array::name, size);
        print_number(size);
        printf(" size=%10zu-", size * sizeof(T));
        print_number(size * sizeof(T));
        printf(" -- ");
        printf("hugepage=[%c] ", use_hugepage ? 'X' : ' ');
        printf(" -- ");
        printf("used=[%c] ", is_used ? 'X' : ' ');
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

    virtual void alloc(void)
    {
        shl_array<T>::alloc();

        // We need to force memory allocation in here (which sucks,
        // since this file is supposed to be platform independent),
        // since in shl_malloc, we do not know the size of elements,
        // and hence, we cannot establish a correct memory mapping for
        // partitioning in there.
#ifndef BARRELFISH
#pragma omp parallel for schedule(static, 1024)
#endif
        for (unsigned int i=0; i<shl_array<T>::size; i++) {

            shl_array<T>::array[i] = 0;
        }

    }

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
