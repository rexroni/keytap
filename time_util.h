#ifndef TIME_UTIL_H
#define TIME_UTIL_H

#define _GNU_SOURCE
#include <sys/time.h>
#include <time.h>

struct timeval timeval_now();

// helper function for difference of two struct timespec's (computes "a - b")
long msec_diff(struct timeval a, struct timeval b);

struct timeval msec_after(struct timeval tv, long millis);

#endif // TIME_UTIL_H
