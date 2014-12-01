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
     * returns the time elapsed
     *
     * \returns
     */
    double get(void)
    {
        return tv_sec*1000 + tv_usec*0.0001;
    }

    /**
     * \brief resets the timer values
     */
    void reset(void)
    {
        tv_sec = 0;
        tv_usec = 0;
        running = false;
        msec = 0.0;
    }

 private:
    bool running;        ///< flag indicating that the timer is running
    long tv_sec;         ///< time value vector (sec part)
    long tv_usec;        ///< time value vector (usec part)
    double msec;         ///< elapsed time in msec
    struct timeval TV1;  ///< timestamp when the timer is started
    struct timeval TV2;  ///< timestamp when the timer was stopped
};

class MultiTimer {
    public:
        MultiTimer(int steps) {
            times = vector<double>();
            times.reserve(steps);
            step_times = vector<double>();
            step_times.reserve(steps);
            labels = vector<string>();\
            labels.reserve(steps);
        }
        void start();
        void step(string name);
        void stop();
        void print();
 private:
    struct timeval tv_start, tv_prev, tv_current;
    vector<double> times;
    vector<double> step_times;
    vector<string> labels;

};

#endif /* __SHL_TIMER */

