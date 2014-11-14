#include <stdlib.h>
#include <stdio.h>

extern "C" {
#include "le.h"
};

// For twitter:
// N = 41652230, M = 1468365182
#define N 41652230
#define E 1468365182

/**
 * \brief Based on http://stackoverflow.com/questions/1156572/evaluating-mathematical-expressions-using-lua
 */

double evaluate_cost(char* cost_wr, char* cost_rd)
{
    int cookie, cookie2;
    int k;
    char *msg = NULL, *msg2 = NULL;

    // Load write cost
    cookie = le_loadexpr(cost_wr, &msg);
    if (msg) {
        printf("can't load: %s\n", msg);
        free(msg);
        return 1;
    }

    // Load read cost
    cookie2 = le_loadexpr(cost_rd, &msg2);
    if (msg2) {
        printf("can't load: %s\n", msg2);
        free(msg2);
        return 1;
    }

    printf("  x    wr    rd\n"
           "------ --------\n");

    k = 10;
    double r, w;

    // Set variables
    le_setvar("N",N);
    le_setvar("E",E);

    // Play with constants
    le_setvar("k", k);

    // Evaluate write-cost
    w = le_eval(cookie, &msg);
    if (msg) {
        printf("can't eval: %s\n", msg);
        free(msg);
        return 1;
    }

    // Evaluate read-cost
    r = le_eval(cookie2, &msg2);
    if (msg2) {
        printf("can't eval: %s\n", msg2);
        free(msg2);
        return 1;
    }

    printf("%2d %15.3f %15.3f %1.4f\n", k, w, r, w/r);

    return w/r;
}
