#include <cstdio>
#include <cstdlib>

#include <sched.h>
#include <numa.h>

#include "shl_internal.h"
#include "shl_configuration.hpp"
#include "shl.h"

void *shl__alloc_struct_shared(size_t size)
{
    return malloc(size);
}

void
aff_set_oncpu(unsigned int cpu)
{
    cpu_set_t cpu_mask;
    int err;

    CPU_ZERO(&cpu_mask);
    CPU_SET(cpu, &cpu_mask);

    err = sched_setaffinity(0, sizeof(cpu_set_t), &cpu_mask);
    if (err) {
        perror("sched_setaffinity");
        exit(1);
    }
}

int shl__get_proc_for_node(int node)
{
    for (int i=0; i<MAXCORES; i++)
        if (numa_cpu_to_node(i)==node)
            return i;

    assert (!"Cannot find processor on given node");
    return -1;
}

extern coreid_t *affinity_conf;
void shl__bind_processor_aff(int id)
{
    printf("Binding [%d] to [%d]\n", id, affinity_conf[id]);
    if (affinity_conf!=NULL) {
        aff_set_oncpu(affinity_conf[id]);
    }
}

void shl__bind_processor(int id)
{
    aff_set_oncpu(id);
}

int
numa_cpu_to_node(int cpu)
{
    int ret    = -1;
    int ncpus  = numa_num_possible_cpus();
    int node_max = numa_max_node();
    struct bitmask *cpus = numa_bitmask_alloc(ncpus);

    for (int node=0; node <= node_max; node++) {
        numa_bitmask_clearall(cpus);
        if (numa_node_to_cpus(node, cpus) < 0) {
            perror("numa_node_to_cpus");
            fprintf(stderr, "numa_node_to_cpus() failed for node %d\n", node);
            abort();
        }

        if (numa_bitmask_isbitset(cpus, cpu)) {
            ret = node;
        }
    }

    numa_bitmask_free(cpus);
    if (ret == -1) {
        fprintf(stderr, "%s failed to find node for cpu %d\n",
                __FUNCTION__, cpu);
        abort();
    }

    return ret;
}
void** shl__copy_array(void *src, size_t size, bool is_used,
                       bool is_ro, const char* array_name)
{
    bool replicate = is_ro;
    int num_replicas = replicate ? shl__get_num_replicas() : 1;
#ifndef REPLICATION
    num_replicas = 1;
#endif

    //    printf(ANSI_COLOR_RED "Warning: " ANSI_COLOR_RESET "malloc for rep1\n");
    printf("array: [%-30s] copy [%c] -- hugepage [%c] -- replication [%c] (%d) -- ", array_name,
           is_used ? 'X' : ' ', get_conf()->use_hugepage ? 'X' : ' ', is_ro ? 'X' : ' ', num_replicas);

    bool omp_copy = true;
    void **tmp = (void**) (malloc(num_replicas*sizeof(void*)));

    for (int i=0; i<num_replicas; i++) {
#ifdef NUMA
        if (replicate && num_replicas>1) {
            // --------------------------------------------------
            // Allocate memory using mmap

            // Make size be alligned multiple of page size
            size_t alloc_size = size;
            while (alloc_size % PAGESIZE != 0)
                alloc_size++;

            int flags = MAP_ANONYMOUS | MAP_PRIVATE;

#ifdef ENABLE_HUGEPAGE
            // hugepage support on Linux
            if (get_conf()->use_hugepage) {
                flags |= MAP_HUGETLB;
            }
#endif

            printf("mmap(size=0x%zx)\n", alloc_size);
            tmp[i] = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE,
                          flags, -1, 0);
            if (tmp[i]==MAP_FAILED) {
                perror("mmap");
                exit(1);
            }

            //            omp_copy = false;

            cpu_set_t cpu_mask_org;
            int err = sched_getaffinity(0, sizeof(cpu_set_t), &cpu_mask_org);
            if (err) {
                perror("sched_getaffinity");
                exit(1);
            }

            // bind processor
            shl__bind_processor(shl__get_proc_for_node(i));

            // copy
            for (size_t j=0; j<size; j+=PAGESIZE) {
                *((char*)(tmp[i])+j) = *((char*)src+j);
            }

            // move processor back to original mask
            err = sched_setaffinity(0, sizeof(cpu_set_t), &cpu_mask_org);
            if (err) {
                perror("sched_setaffinity");
                exit(1);
            }
        } else {
            // If data is not replicated, still copy, but don't specify node
            // Our allocation function will spread the data in the machine
            size_t alloc_size = size;
            while (alloc_size % PAGESIZE != 0)
                alloc_size++;
            int flags = MAP_ANONYMOUS | MAP_PRIVATE;
#ifdef ENABLE_HUGEPAGE
            // hugepage support on Linux
            if (get_conf()->use_hugepage) {
                flags |= MAP_HUGETLB;
            }
#endif
            tmp[i] = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE,
                          flags, -1, 0);
            if (tmp[i]==MAP_FAILED) {
                perror("mmap");
                exit(1);
            }


        }
#else
#ifdef ARRAY
        tmp[i] = new double[size/8];
        printf("new ");
#else
        tmp[i] = malloc(size);
        printf("malloc ");
#endif
#endif
        assert(tmp[i]!=NULL);
    }
    printf("\n");

    assert(sizeof(char)==1);
    assert(tmp!=NULL);
    if (is_used && omp_copy) {
        for (int i=0; i<num_replicas; i++) {
            #pragma omp parallel for
            for (size_t j=0; j<size; j++)
                *((char*)(tmp[i])+j) = *((char*)src+j);
        }
    }
    return tmp;
}

void shl__copy_back_array(void **src, void *dest, size_t size, bool is_copied,
                          bool is_ro, bool is_dynamic, const char* array_name)
{
    bool copy_back = true;
    int num_replicas = is_ro ? shl__get_num_replicas() : 1;

#ifndef REPLICATION
    num_replicas = 1;
#endif

    // read-only: don't have to copy back, data is still the same
    //  if (is_ro)
    //  copy_back = false;

    // dynamic: array was created dynamically in GM algorithm function,
    // no need to copy back
    if (is_dynamic)
        copy_back = false;

    printf("array: [%-30s] -- copied [%c] -- copy-back [%c] (%d)\n",
           array_name, is_copied ? 'X' : ' ', copy_back ? 'X' : ' ',
           num_replicas);

    if (copy_back) {
        // replicated data is currently read-only (consistency issues)
        // so everything we have to copy back is not replicated
        //  assert (num_replicas == 1);

        for (int i=0; i<num_replicas; i++)
            memcpy(dest, src[0], size);
    }
}

void shl__copy_back_array_single(void *src, void *dest, size_t size, bool is_copied,
                                 bool is_ro, bool is_dynamic, const char* array_name)
{
    bool copy_back = true;
    int num_replicas = is_ro ? shl__get_num_replicas() : 1;

#ifndef REPLICATION
    num_replicas = 1;
#endif

    // read-only: don't have to copy back, data is still the same
    if (is_ro)
        copy_back = false;

    // dynamic: array was created dynamically in GM algorithm function,
    // no need to copy back
    if (is_dynamic)
        copy_back = false;

    printf("array: [%-30s] -- copied [%c] -- copy-back [%c] (%d)\n",
           array_name, is_copied ? 'X' : ' ', copy_back ? 'X' : ' ',
           num_replicas);

    if (copy_back) {
        // replicated data is currently read-only (consistency issues)
        // so everything we have to copy back is not replicated
        assert (num_replicas == 1);

        for (int i=0; i<num_replicas; i++)
            memcpy(dest, src, size);
    }
}

/**
 * \brief ALlocate memory with the given flags.
 *
 * The array will NOT be initialized (but might be, to force the Linux
 * Kernel to map the memory as requested)
 *
 * Supported options as a bitmask in opts are:
 *
 * - SHL_MALLOC_HUGEPAGE:
 *   enable hugepage support
 *
 * - SHL_MALLOC_DISTRIBUTED:
 *    distribute memory approximately equally on nodes that have threads
 *
 * \param ret_mi Is unused on Linux
 */
void* shl__malloc(size_t size, int opts, int *pagesize, int node, void **ret_mi)
{
    void *res;
    bool use_hugepage = opts & SHL_MALLOC_HUGEPAGE;
    bool distribute = opts & SHL_MALLOC_DISTRIBUTED;
    bool partition = opts & SHL_MALLOC_PARTITION;

    // Round up to next multiple of page size (in case of hugepage)
    *pagesize = use_hugepage ? PAGESIZE_HUGE : PAGESIZE;
    size_t alloc_size = size;
    while (use_hugepage && (alloc_size % *pagesize != 0))
        alloc_size++;

    // Set options for mmap
    int options = MAP_ANONYMOUS | MAP_PRIVATE;
    if (use_hugepage)
        options |= MAP_HUGETLB;

    printf("shl__alloc: %zu, huge=%d, distribute=%d",
           alloc_size, use_hugepage, distribute);

    // Allocate (alloc_size + 1 page)
    // might have to shift if memory returned is not starting on page boundary
    res = mmap(NULL, alloc_size + *pagesize, PROT_READ | PROT_WRITE, options, -1, 0);
    if (res==MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    // Want the first element of the array starting at pageboundary
    size_t rres = (size_t) res;
    while (rres % (*pagesize) != 0) rres++;
    res = (void*) rres;

    // Distribute memory
    // --------------------------------------------------
    if (distribute) {

#pragma omp parallel for
        for (unsigned int i=0; i<alloc_size;i ++) {

            ((char *) res)[i] = 0;
        }
    }


    // Partition memory
    // --------------------------------------------------
    if (partition) {

        // SK: cannot establish mapping here, as size of array
        // elements is not known
    }

    printf("\n");

    return res;
}

/**
 *
 * \param num_replicas Specifies the number of replicas to be
 * generated. If value given is <0, shl_malloc_replicated will
 * determine the number of replicas to be used.
 */
void** shl_malloc_replicated(size_t size,
                             int* num_replicas,
                             int options)
{
    if (*num_replicas<=0) {
        *num_replicas = shl__get_num_replicas();
    }

    assert (*num_replicas>0 && *num_replicas<12); // Sanity check

    int pagesize;

    void **tmp = (void**) (malloc(*num_replicas*sizeof(void*)));

    for (int i=0; i<*num_replicas; i++) {

        // Allocate memory
        // --------------------------------------------------

        // Allocate
        tmp[i] = shl__malloc(size, options, &pagesize, SHL_NUMA_IGNORE, NULL);

        // Allocate on proper node; leverage Linux's first touch strategy
        // --------------------------------------------------

        cpu_set_t cpu_mask_org;
        int err = sched_getaffinity(0, sizeof(cpu_set_t), &cpu_mask_org);
        if (err) {
            perror("sched_getaffinity");
            exit(1);
        }

        // bind processor
        shl__bind_processor(shl__get_proc_for_node(i));

        // write once on every page
        for (size_t j=0; j<size; j+=PAGESIZE)
            *((char*)(tmp[i])+j) = 0;

        // move processor back to original mask
        err = sched_setaffinity(0, sizeof(cpu_set_t), &cpu_mask_org);
        if (err) {
            perror("sched_setaffinity");
            exit(1);
        }
    }
    return tmp;
}

long shl__node_size(int node, long  *freep)
{
    return numa_node_size(node, freep);
}

int shl__max_node(void)
{
    return numa_max_node();
}

bool shl__check_hugepage_support(void)
{
    int options = MAP_ANONYMOUS | MAP_PRIVATE | MAP_HUGETLB;
    void *res = mmap(NULL, 4096, PROT_READ | PROT_WRITE, options, -1, 0);

    if (res==MAP_FAILED) {
        return false;
    } else {
        int r = munmap(res, 4096);
        assert (r);
        return true;
    }
}
