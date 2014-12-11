#include <sys/time.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <limits.h>

#include <cstdlib>

#include "shl.h"
#include "shl_internal.h"
#include "shl_timer.hpp"
#include "shl_configuration.hpp"

static Timer shl__default_timer;

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

    t_start = shl__timer_get_timestamp();
}

double Timer::get(void)
{
    unsigned long t_diff;
    if (t_end < t_start) {
        t_diff = ((ULONG_MAX - t_start) + t_end);
    } else {
        t_diff = t_end - t_start;
    }
    return (double)t_diff / 1000.0;
}

void Timer::reset(void)
{
    running = false;
    t_end = 0;
    t_start = 0;
}

/**
 * \brief stops the timer
 *
 * \returns
 */
double Timer::stop(void)
{
    if (!running) {
        return 0;
    }

    t_end = shl__timer_get_timestamp();

    return get();
}

/**
 * \brief prints the elapsed time
 *
 * \param label associated label with the timer
 */
void Timer::print(char *label)
{
    printf("Timer: %s %f", label, get());
}


#if !SHL_TIMER_USE_MULTI

void shl__start_timer(int steps)
{
    shl__default_timer.start();
}

double shl__end_timer(const char *label)
{
    return shl__default_timer.stop();
}

double shl__get_timer(void)
{
    return shl__default_timer.get();
}


void shl__step_timer(const char *label)
{
    /* no op */
}


#endif