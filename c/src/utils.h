#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdio.h>
#include <time.h>


/* I have no idea what I'm doing.png */
/* https://stackoverflow.com/a/10119699/1336774 */
#pragma GCC system_header
#define IS_SET(macro) IS_SET_(macro)
#define MACROTEST_1 ,
#define IS_SET_(value) IS_SET__(MACROTEST_##value)
#define IS_SET__(comma) IS_SET___(comma 1, 0)
#define IS_SET___(_, v, ...) v

#define LIKELY(x)      __builtin_expect(!!(x), 1)
#define UNLIKELY(x)    __builtin_expect(!!(x), 0)

#define VERIFY_NOT(x, err) if(UNLIKELY(x==err)){perror(#x);exit(EXIT_FAILURE);}
#define VERIFY_ZERO(x) {int rc=(x);if(UNLIKELY(rc!=0)){perror(#x);exit(rc);}}
#define VERIFY_NONZERO(x) {if(UNLIKELY(!(x))){perror(#x);exit(EXIT_FAILURE);}}
#define VERIFY_POSITIVE(x) {intptr_t rc=(intptr_t)(x);\
        if(UNLIKELY(rc<0)){perror(#x);exit(EXIT_FAILURE);}}

#define CHECK_POSITIVE(x) {int rc=(x);if(UNLIKELY(rc<0)){     \
            if(!(IS_SET(NDEBUG)))                             \
                fprintf(stderr, "%s failed (%d).\n", #x, rc); \
            goto cleanup;}}
#define CHECK_NONZERO(x) {if(UNLIKELY(!(x))) {          \
            if(!(IS_SET(NDEBUG)))                       \
                fprintf(stderr, "%s failed.\n", #x);    \
            goto cleanup;}}
#define CHECK_ZERO(x) {rc=(x);if(UNLIKELY(rc)){      \
            if(!(IS_SET(NDEBUG)))                    \
                fprintf(stderr, "%s failed.\n", #x); \
            goto cleanup;}}

#define MEASURE_DURATION(x, s) {struct timespec start, end;              \
    clock_gettime(CLOCK_MONOTONIC_RAW, &start); {x;}                     \
    clock_gettime(CLOCK_MONOTONIC_RAW, &end); double ms =                \
        (end.tv_sec-start.tv_sec)*1000.+(end.tv_nsec-start.tv_nsec)/1e6; \
    if(LIKELY(verbose)) printf("%s took %.2f ms\n", s, ms); }

extern int phase_hack;
extern int verbose;

#endif  // _UTILS_H_
