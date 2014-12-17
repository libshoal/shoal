/*
 * Copyright (c) 2014 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */
#ifndef __SHL_TIMER
#define __SHL_TIMER

#include <vector>
#include <string>

using namespace std;

/*
 * -------------------------------------------------------------------------------
 * Timer
 * -------------------------------------------------------------------------------
 */

/**
 * \brief timer class
 */
class Timer {
 public:
    /**
     * constructor
     */
    Timer() {
        reset();
    }

    /**
     * \brief starts the timer
     */
    void start(void);

    /**
     * \brief stops the timer
     *
     * \returns
     */
    double stop(void);

    /**
     * returns the time elapsed in seconds
     *
     * \returns
     */
    double get(void);

    /**
     * \brief resets the timer values
     */
    void reset(void);

    /**
     * \brief prints the elapsed time
     *
     * \param label associated label with the timer
     */
    void print(char *label);

 private:
    bool running;            ///< flag indicating that the timer is running

    unsigned long t_start;   ///< timestamp of the start in msec
    unsigned long t_elapsed; ///< time elapes since start [msec]
};

#endif /* __SHL_TIMER */

