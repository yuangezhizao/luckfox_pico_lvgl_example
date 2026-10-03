/* 32 位 time_t 下 custom_tick_get() 的差值按 uint32_t 取模正确（spec §2.4、§5.1 ⑬）；须以 -U_TIME_BITS -U_FILE_OFFSET_BITS 编译。 */
#include <stdint.h>
#include <sys/time.h>
#include <time.h>

#include "check.h"

uint32_t custom_tick_get(void);

static uint64_t fake_us;

int gettimeofday(struct timeval *tv, void *tz)
{
    (void)tz;
    tv->tv_sec = (time_t)(fake_us / 1000000u);
    tv->tv_usec = (suseconds_t)(fake_us % 1000000u);
    return 0;
}

int clock_gettime(clockid_t clk, struct timespec *ts)
{
    (void)clk;
    ts->tv_sec = (time_t)(fake_us / 1000000u);
    ts->tv_nsec = (long)(fake_us % 1000000u) * 1000;
    return 0;
}

static uint32_t tick_at_us(uint64_t us)
{
    fake_us = us;
    return custom_tick_get();
}

static void check_delta_us(uint64_t start_us, uint64_t delta_us)
{
    uint32_t a = tick_at_us(start_us);
    uint32_t b = tick_at_us(start_us + delta_us);

    CHECK_EQ_INT((uint32_t)(b - a), (uint32_t)(delta_us / 1000u));
}

int main(void)
{
    CHECK_EQ_INT(sizeof(time_t), 4);
    tick_at_us(10000000u);                                   /* 起点 10 s：非 0 且小于 2148 s */
    check_delta_us(10000000u, 1000000u);                     /* Δ=1 s，旧实现溢出前也正确 */
    check_delta_us(10250000u, 1500000u);                     /* 带亚秒部分 */
    check_delta_us(2100000000ull, 100000000ull);             /* 跨过绝对秒 2148 */
    check_delta_us(10000000u, 3000000000ull);                /* Δ=3000 s */
    check_delta_us(10000000u, 50ull * 86400u * 1000000u);    /* Δ=50 天，超过 uint32_t 毫秒 */
    check_delta_us(1700000000ull * 1000000u, 1000000u);      /* 板上量级 */
    check_delta_us(1700000000ull * 1000000u, 3600ull * 1000000u);
    return tst_finish();
}
