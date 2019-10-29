#ifndef TIME_UTIL_H
#define TIME_UTIL_H

#define _GNU_SOURCE
#include <sys/time.h>
#include <time.h>

struct timeval timeval_now();

// helper functions for difference of two struct timeval's (compute "a - b")
long msec_diff(struct timeval a, struct timeval b);
struct timeval timeval_diff(struct timeval a, struct timeval b);

struct timeval msec_after(struct timeval tv, long millis);

#endif // TIME_UTIL_H
