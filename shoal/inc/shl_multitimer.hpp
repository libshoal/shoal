/*
 * Copyright (c) 2014 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */
#ifndef __SHL_MULTITIMER
#define __SHL_MULTITIMER

#include <vector>
#include <string>

using namespace std;

class MultiTimer {
 public:
    /**
     * \brief MultiTimer constructor
     *
     * \param steps number of expected steps for this timer
     *
     * The initialization will reserve memory for steps measurements
     */
    MultiTimer(int steps)
    {
        times = vector<double>();
        times.reserve(steps);
        step_times = vector<double>();
        step_times.reserve(steps);
        labels = vector<string>();\
        labels.reserve(steps);
        running = false;

        t_start = 0;
        t_prev = 0;
        t_current = 0;
    }

    /**
     * \brief starts the timer
     */
    void start(void);

    /**
     * \brief adds a step to the timer
     *
     * \param label  associated label with the step
     */
    void step(string label);

    /**
     * \brief halts the timer
     *
     * \brief
     */
    void stop(string name);

    /**
     * \brief prints the timer valueszs
     */
    void print(void);


    /**
     * \brief gets the execution time from the start to the last step
     *
     * \returns time in seconds
     */
    double get(void);

    /**
     * \brief resets the timer
     */
    void reset(int nsteps);

 private:
    bool running;

    unsigned long t_start;
    unsigned long t_current;
    unsigned long t_prev;

    struct timeval tv_start, tv_prev, tv_current;
    vector<double> times;
    vector<double> step_times;
    vector<string> labels;

};

#endif /* __SHL_MULTITIMER */

