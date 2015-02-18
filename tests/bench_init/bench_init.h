/*
 * bench_init.h
 *
 *  Created on: Jan 20, 2015
 *      Author: acreto
 */

#ifndef LIB_SHOAL_TESTS_BENCH_INIT_BENCH_INIT_H_
#define LIB_SHOAL_TESTS_BENCH_INIT_BENCH_INIT_H_


#define ARRAY_ELEMENTS (64UL * 1024 * 1024)
//#define ARRAY_ELEMENTS (2UL * 1024 * 1024 * 1024)
#define ARRAY_TYPE uint64_t

#define ARRAY_SIZE (ARRAY_ELEMENTS * sizeof(ARRAY_TYPE));

#define ARRAY_FRACTIONS 10
#define ARRAY_FRACTION_SIZE (ARRAY_ELEMENTS / ARRAY_FRACTIONS)

#define BENCH_REPETITIONS 5

void shl_bench_init(void);
void timer_start(void);
uint64_t timer_stop(void);


#endif /* LIB_SHOAL_TESTS_BENCH_INIT_BENCH_INIT_H_ */
