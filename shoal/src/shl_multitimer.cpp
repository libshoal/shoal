#include <sys/time.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <limits.h>

#include <cstdlib>

#include "shl_internal.h"
#include "shl.h"
#include "shl_multitimer.hpp"
#include "shl_configuration.hpp"

static MultiTimer *shl_default_mtimer = NULL;

/*
 * -------------------------------------------------------------------------------
 * Multi Timer class definitions
 * -------------------------------------------------------------------------------
 */
static inline unsigned long time_diff(unsigned long t0, unsigned long t1)
{
    if (t0 <= t1) {
        return t1 - t0;
    } else {
        return (ULONG_MAX - t0) + t1;
    }
}


void MultiTimer::start(void)
{
    if (running) {
        return;
    }

    t_start = shl__timer_get_timestamp();
    t_prev = t_start;
    running = true;
}

void MultiTimer::stop(string name)
{
    if (!running) {
        return;
    }

    running = false;

    step(name);
}

void MultiTimer::reset(int nsteps)
{
    running = false;
    times.clear();
    times.reserve(nsteps);
    step_times.clear();
    step_times.reserve(nsteps);
    labels.clear();
    labels.reserve(nsteps);
}

double MultiTimer::get(void)
{
    return time_diff(t_start, t_current);
}

void MultiTimer::step(string name)
{
    t_current = shl__timer_get_timestamp();

    times.push_back((double)(time_diff(t_start, t_current)));
    if (running == false) {
        step_times.push_back(time_diff(t_prev, t_current));
    } else {
        step_times.push_back(time_diff(t_start, t_current));
    }
    labels.push_back(name);

    t_prev = t_current;
}

void MultiTimer::print()
{
    printf("-------------------------------\n");
    printf("%-30s %-15s %-15s \n", "Step", "Step Time", "Time Total");
    for (unsigned i = 0; i < times.size(); ++i) {
        printf("%-30s %14.4f %14.4f\n", labels.at(i).c_str(), step_times.at(i), times.at(i));
    }
    printf("-------------------------------\n");
}


#if SHL_TIMER_USE_MULTI

void shl__start_timer(int steps)
{
    if (shl_default_mtimer == NULL) {
        shl_default_mtimer = new MultiTimer(steps);
    } else {
        shl_default_mtimer->reset(steps);
    }
    shl_default_mtimer->start();
}

double shl__end_timer(const char *label)
{
    shl_default_mtimer->stop(label);
    shl_default_mtimer->print();
    return shl_default_mtimer->get();
}

double shl__get_timer(void)
{
    return shl_default_mtimer->get();
}

void shl__step_timer(const char *label)
{
    shl_default_mtimer->step(label);
}

#endif
