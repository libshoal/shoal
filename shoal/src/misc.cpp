#include <sys/time.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>

#include <cstdlib>

#include "shl_internal.h"
#include "shl.h"
#include "shl_timer.hpp"
#include "shl_configuration.hpp"

Timer shl__default_timer;

// virtually indexed
int replica_lookup[MAXCORES];

/*
 * -------------------------------------------------------------------------------
 * Timer class definitions
 * -------------------------------------------------------------------------------
 */

/**
 * \brief starts the timer
 */
void Timer::start(void)
{
    assert(!running);
    running = true;
    gettimeofday(&TV1, NULL);
 }

/**
 * \brief stops the timer
 *
 * \returns
 */
double Timer::stop(void)
{
    assert (running);
    gettimeofday(&TV2, NULL);
    msec += (TV2.tv_sec - TV1.tv_sec)*1000 + (TV2.tv_usec - TV1.tv_usec)*0.001;
    tv_sec += (TV2.tv_sec - TV1.tv_sec);
    tv_usec += (TV2.tv_usec - TV1.tv_usec);
    return msec;
}

/*
 * -------------------------------------------------------------------------------
 * Multi Timer class definitions
 * -------------------------------------------------------------------------------
 */

void MultiTimer::start(void)
{
    gettimeofday(&tv_start, NULL);
    gettimeofday(&tv_prev, NULL);
}

void MultiTimer::stop(void)
{
    step("Total");
}

void MultiTimer::step(string name)
{
    gettimeofday(&tv_current, NULL);
    double timer_secs = (tv_current.tv_sec - tv_start.tv_sec)*1000
                        + (tv_current.tv_usec - tv_start.tv_usec)*0.001;
    times.push_back(timer_secs);
    timer_secs = (tv_current.tv_sec - tv_prev.tv_sec)*1000
                    + (tv_current.tv_usec - tv_prev.tv_usec)*0.001;
    step_times.push_back(timer_secs);
    labels.push_back(name);
    tv_prev = tv_current;
}

void MultiTimer::print()
{
    printf("-------------------------------\n");
    printf("%-20s %-12s %-14s \n", "Step", "Time", "Step Time");
    for (unsigned i = 0; i < times.size(); ++i) {
        printf("%-20s: %.6f (%.6f)\n", labels.at(i).c_str(), times.at(i), step_times.at(i));
    }
    printf("-------------------------------\n");
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
    return shl__default_timer.get();
}


/**
 * \brief Read from environment variable as string
 */
char* get_env_str(const char *envname, const char *defaultvalue)
{
    char *env;
    env = getenv (envname);

    if (env==NULL) {

        return (char*) defaultvalue;
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
Configuration* get_conf(void)
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

    coreid_t *res = NULL;

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
                    assert (res!=NULL);
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

void print_number(long long number)
{
    char buf[1024];
    convert_number(number, buf);

    printf("%s", buf);
}

void convert_number(long long number, char *buf)
{
    if (number>=GIGA)
        sprintf(buf, "%5.2f G", number/((double) GIGA));
    else if (number>=MEGA)
        sprintf(buf, "%5.2f M", number/((double) MEGA));
    else if (number>=KILO)
        sprintf(buf, "%5.2f K", number/((double) KILO));
    else
        sprintf(buf, "%lld", number);
}
