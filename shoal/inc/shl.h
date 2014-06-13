#ifndef __SHL_H
#define __SHL_H

void shl__init(void)
{
    printf("SHOAL (v %s) initialization .. ", VERSION);
    printf("done\n");
}

void* shl__copy_array(void *src, size_t size, bool is_used, const char* array_name)
{
    void *tmp = malloc(size);
    assert(tmp!=NULL);
    if (is_used) {
        printf("copying array [%s] content .. \n", array_name);
        memcpy(tmp, src, size);
    } else {
        printf("NOT copying array [%s] content .. \n", array_name);
    }
    return tmp;
}


#endif
