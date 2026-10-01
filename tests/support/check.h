/* 头文件自带实现的极简断言与用例分派；32 位测试也直接包含本文件。 */
#ifndef TST_CHECK_H
#define TST_CHECK_H

#include <stdio.h>
#include <string.h>

typedef struct {
    const char *name;
    void (*run)(void);
} tst_case_t;

static int tst_failures __attribute__((unused));

static inline void tst_check_(int ok, const char *expr, const char *file, int line)
{
    if (ok) {
        printf("PASS %s\n", expr);
    } else {
        printf("FAIL %s:%d: %s\n", file, line, expr);
        tst_failures++;
    }
}

static inline void tst_check_eq_int_(long got, long want, const char *expr, const char *file, int line)
{
    if (got == want) {
        printf("PASS %s == %ld\n", expr, want);
    } else {
        printf("FAIL %s:%d: %s: got %ld, want %ld\n", file, line, expr, got, want);
        tst_failures++;
    }
}

static inline void tst_check_contains_(const char *hay, const char *needle, const char *file, int line)
{
    if (hay != NULL && strstr(hay, needle) != NULL) {
        printf("PASS output contains \"%s\"\n", needle);
    } else {
        printf("FAIL %s:%d: output lacks \"%s\"\n", file, line, needle);
        tst_failures++;
    }
}

#define CHECK(cond) tst_check_((cond) ? 1 : 0, #cond, __FILE__, __LINE__)
#define CHECK_EQ_INT(got, want) tst_check_eq_int_((long)(got), (long)(want), #got, __FILE__, __LINE__)
#define CHECK_STR_CONTAINS(hay, needle) tst_check_contains_((hay), (needle), __FILE__, __LINE__)
#define TST_COUNT(array) (sizeof(array) / sizeof((array)[0]))

static inline int tst_finish(void)
{
    printf("RESULT %s (%d failed)\n", tst_failures ? "FAIL" : "PASS", tst_failures);
    fflush(stdout);
    return tst_failures ? 1 : 0;
}

/* 按唯一的命令行参数选一个用例运行；每个用例独立进程（spec D5）。 */
static inline int tst_run_case(const tst_case_t *cases, size_t count, int argc, char **argv)
{
    size_t i;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <case>\n", argv[0]);
        return 2;
    }
    for (i = 0; i < count; i++) {
        if (strcmp(cases[i].name, argv[1]) == 0) {
            cases[i].run();
            return tst_finish();
        }
    }
    fprintf(stderr, "unknown case: %s\n", argv[1]);
    return 2;
}

#endif
