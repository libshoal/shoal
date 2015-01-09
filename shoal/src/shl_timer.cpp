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
    if (!running) {
        return t_elapsed;
    }

    unsigned long t_current = shl__timer_get_timestamp();

    if (t_current < t_start) {
        t_current = ((ULONG_MAX - t_start) + t_current);
    } else {
        t_current = t_current - t_start;
    }
    return (double)t_elapsed + (double)t_current / 1000.0;
}

void Timer::reset(void)
{
    running = false;
    t_elapsed = 0;
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

    running = false;

    unsigned long t_current = shl__timer_get_timestamp();

    if (t_current < t_start) {
        t_current = ((ULONG_MAX - t_start) + t_current);
    } else {
        t_current = t_current - t_start;
    }

    t_elapsed += t_current;

    return t_elapsed;
}

/**
 * \brief prints the elapsed time
 *
 * \param label associated label with the timer
 */
void Timer::print(string label)
{
    printf("Timer: %s %5.10f\n", label.c_str(), get());
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
