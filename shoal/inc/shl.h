#ifndef __SHL_H
#define __SHL_H

#include <numa.h>

void shl__init(void)
{
    printf("SHOAL (v %s) initialization .. ", VERSION);
    printf("done\n");
}

static int shl__get_num_replicas(void)
{
    return numa_max_node()+1;
}

void** shl__copy_array(void *src, size_t size, bool is_used,
                       bool is_ro, const char* array_name)
{
    int num_replicas = is_ro ? shl__get_num_replicas() : 1;

    printf("array: [%-30s] copy [%c] -- replication [%c] (%d)\n", array_name,
           is_used ? 'X' : ' ', is_ro ? 'X' : ' ', num_replicas);

    void **tmp = (void**) (malloc(num_replicas*sizeof(void*)));

    for (int i=0; i<num_replicas; i++)
        tmp[i] = malloc(size);

    assert(tmp!=NULL);
    if (is_used) {
        for (int i=0; i<num_replicas; i++)
            memcpy(tmp[i], src, size);
    }
    return tmp;
}

void shl__copy_back_array(void **src, void *dest, size_t size, bool is_copied,
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
