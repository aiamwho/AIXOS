#ifndef AIXOS_TEST_H
#define AIXOS_TEST_H

#include <stdio.h>

extern int test_failures;
extern int test_checks;

#define CHECK(expr) do { \
    test_checks++; \
    if (!(expr)) { \
        test_failures++; \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
    } \
} while (0)

#define RUN_TEST(fn) do { \
    int before = test_failures; \
    fn(); \
    printf("%s %s\n", test_failures == before ? "PASS" : "FAIL", #fn); \
    fflush(stdout); \
} while (0)

#endif
