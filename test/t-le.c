#include <stdlib.h>
#include <stdio.h>
#include "le.h"

// For twitter:
// N = 41652230, M = 1468365182
#define N 41652230
#define E 1468365182

/**
 * \brief Based on http://stackoverflow.com/questions/1156572/evaluating-mathematical-expressions-using-lua
 */

int main(int argc, char **argv)
{
    int cookie, cookie2;
    int k;
    char *msg = NULL, *msg2 = NULL;

    if (!le_init()) {
        printf("can't init LE\n");
        return 1;
    }
    if (argc<3) {
        printf("Usage: t-le \"write_cost\" \"read_cost\"\n");
        return 1;
    }

    // Load write cost
    cookie = le_loadexpr(argv[1], &msg);
    if (msg) {
        printf("can't load: %s\n", msg);
        free(msg);
        return 1;
    }

    // Load read cost
    cookie2 = le_loadexpr(argv[2], &msg2);
    if (msg2) {
        printf("can't load: %s\n", msg2);
        free(msg2);
        return 1;
    }

    printf("  x    wr    rd\n"
           "------ --------\n");

    for (k=1; k<11; ++k) {
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
    }
}
