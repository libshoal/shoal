#ifndef __SHL_H
#define __SHL_H

#include <numa.h>
#include <assert.h>
#include <papi.h>

static int EventSet;

static void handle_error(int retval)
{
    printf("PAPI error %d: %s\n", retval, PAPI_strerror(retval));
    exit(1);
}

static void shl__end(void)
{
    // Stop PAPI events
    long long values[1];
    if (PAPI_stop(EventSet, values) != PAPI_OK) handle_error(1);
    printf("Stopping PAPI .. \n");
    printf("%lld\n",values[0]);
    printf("END PAPI\n");
}

static void shl__init(void)
{
    printf("SHOAL (v %s) initialization .. ", VERSION);
    printf("done\n");

    printf("Initializing PAPI .. ");

    // Initialize PAPI and make sure version matches
    int retval = PAPI_library_init(PAPI_VER_CURRENT);
    if (retval != PAPI_VER_CURRENT && retval > 0) { fprintf(stderr,"PAPI library version mismatch!\n"); exit(1); }

    // More checks that PAPI is running?
    if (retval < 0) handle_error(retval);
    retval = PAPI_is_initialized();
    if (retval != PAPI_LOW_LEVEL_INITED) handle_error(retval);

    if (PAPI_thread_init(pthread_self) != PAPI_OK)
        exit(1);

    printf("DONE\n");

    printf("Starting PAPI .. ");

    // Add events to be monitored
    EventSet = PAPI_NULL;
    if (PAPI_create_eventset(&EventSet) != PAPI_OK) handle_error(1);
    if (PAPI_add_event(EventSet, PAPI_TOT_INS) != PAPI_OK) handle_error(1);

    // Start monitoring
    if (PAPI_start(EventSet) != PAPI_OK)
        handle_error(1);
    printf("DONE\n");
}

static inline int shl__get_rep_id(void) {
    return (omp_get_thread_num()) % (numa_max_node()+1);
    //    return 0;
}

static int shl__get_num_replicas(void)
{
    return numa_max_node()+1;
}

static void** shl__copy_array(void *src, size_t size, bool is_used,
                              bool is_ro, const char* array_name)
{
    int num_replicas = is_ro ? shl__get_num_replicas() : 1;

    printf("array: [%-30s] copy [%c] -- replication [%c] (%d)\n", array_name,
           is_used ? 'X' : ' ', is_ro ? 'X' : ' ', num_replicas);

    void **tmp = (void**) (malloc(num_replicas*sizeof(void*)));

    for (int i=0; i<num_replicas; i++) {
        //tmp[i] = malloc(size);
        tmp[i] = numa_alloc_onnode(size, i);
    }

    assert(tmp!=NULL);
    if (is_used) {
        for (int i=0; i<num_replicas; i++) {
            memcpy(tmp[i], src, size);
        }
    }
    return tmp;
}

static void shl__copy_back_array(void **src, void *dest, size_t size, bool is_copied,
                                 bool is_ro, bool is_dynamic, const char* array_name)
{
    bool copy_back = true;
    int num_replicas = is_ro ? shl__get_num_replicas() : 1;

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
            memcpy(dest, src[0], size);
    }
}


#endif
