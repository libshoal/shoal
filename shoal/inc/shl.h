#ifndef __SHL_H
#define __SHL_H

void shl__init(void)
{
    printf("SHOAL (v %s) initialization .. ", VERSION);
    printf("done\n");
}

static int shl__get_num_replicas(void)
{
    return 2;
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


#endif
