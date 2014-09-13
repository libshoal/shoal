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

// --------------------------------------------------
// Timer
// --------------------------------------------------
class Timer {
 public:
    bool running = false;
    long tv_sec = 0;
    long tv_usec = 0;
    double timer_secs = 0.0; // these are actually miliseconds
    void start(void);
    double stop(void);
    double get(void);
    void reset(void);
    Timer() {
        timer_secs = 0.0;
    }
 private:
    struct timeval TV1, TV2;
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

