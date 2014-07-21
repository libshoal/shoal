#include <sys/time.h>
#include <ctype.h>
#include <errno.h>

#include <cstdlib>
#include "shl.h"

Timer shl__default_timer;

// virtually indexed
int replica_lookup[MAXCORES];

void Timer::start(void)
{
    gettimeofday(&TV1, NULL);
 }

double Timer::stop(void)
{
    gettimeofday(&TV2, NULL);
    timer_secs += (TV2.tv_sec - TV1.tv_sec)*1000 + (TV2.tv_usec - TV1.tv_usec)*0.001;
    return timer_secs;
}


void shl__start_timer(void)
{
    shl__default_timer.start();
}

double shl__end_timer(void)
{
    return shl__default_timer.stop();
}

double shl__get_timer(void)
{
    return shl__default_timer.timer_secs;
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

/**
 * \brief Read from environment variable as string
 */
char* get_env_str(const char *envname, const char *defaultvalue)
{
    char *env;
    env = getenv (envname);

    if (env==NULL) {

        return defaultvalue;
    }

    return env;
}

/**
 * \brief Read from environment variable as string
 */
int get_env_int(const char *envname, int defaultvalue)
{
    char *env;
    env = getenv (envname);

    if (env==NULL) {

        return defaultvalue;
    }

    return atoi(env);
}

Configuration conf;
conf get_conf(void)
{
    return &conf;
}

/**
 * \brief Parse CPU affinity string in environment
 *
 * This is borrowed from libgomp in gcc.
 */
coreid_t* parse_affinity (bool ignore)
{
    char *env, *end, *start;
    int pass;
    unsigned long cpu_beg, cpu_end, cpu_stride;
    size_t count = 0, needed;
    size_t curr = 0;

    printf("parsing affinity\n");

    coreid_t *res;

    env = getenv ("SHL_CPU_AFFINITY");
    if (env == NULL)
        return NULL;

    start = env;
    for (pass = 0; pass < 2; pass++)
        {
            env = start;
            if (pass == 1)
                {
                    if (ignore)
                        return NULL;

                    res = (coreid_t*) malloc(sizeof(coreid_t)*count);
                }
            do
                {
                    while (isspace ((unsigned char) *env))
                        ++env;

                    errno = 0;
                    cpu_beg = strtoul (env, &end, 0);
                    if (errno || cpu_beg >= 65536)
                        goto invalid;
                    cpu_end = cpu_beg;
                    cpu_stride = 1;

                    env = end;
                    if (*env == '-')
                        {
                            errno = 0;
                            cpu_end = strtoul (++env, &end, 0);
                            if (errno || cpu_end >= 65536 || cpu_end < cpu_beg)
                                goto invalid;

                            env = end;
                            if (*env == ':')
                                {
                                    errno = 0;
                                    cpu_stride = strtoul (++env, &end, 0);
                                    if (errno || cpu_stride == 0 || cpu_stride >= 65536)
                                        goto invalid;

                                    env = end;
                                }
                        }

                    needed = (cpu_end - cpu_beg) / cpu_stride + 1;
                    if (pass == 0)
                        count += needed;
                    else
                        {
                            while (needed--)
                                {
                                    res[curr++] = cpu_beg;
                                    cpu_beg += cpu_stride;
                                }
                        }

                    while (isspace ((unsigned char) *env))
                        ++env;

                    if (*env == ',')
                        env++;
                    else if (*env == '\0')
                        break;
                }
            while (1);
        }

    return res;

 invalid:
    if (res!=NULL)
        free(res);

    assert (!"Invalid value for enviroment variable GOMP_CPU_AFFINITY");
    return NULL;
}
