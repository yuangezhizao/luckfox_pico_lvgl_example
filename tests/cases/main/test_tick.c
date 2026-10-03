/* custom_tick_get()：墙钟回拨后 tick 不倒退（spec §5.1 ⑬）。 */
#include <stdint.h>
#include <sys/time.h>
#include <time.h>

#include "check.h"
#include "custom_tick.h"

static long long fake_wall_sec = 1700000000;

int gettimeofday(struct timeval *tv, void *tz)
{
    (void)tz;
    tv->tv_sec = (time_t)fake_wall_sec;
    tv->tv_usec = 0;
    return 0;
}

static void step_back_host(void)
{
    uint32_t a = custom_tick_get();
    uint32_t b;
    const struct timespec pause = {.tv_sec = 0, .tv_nsec = 20000000};

    fake_wall_sec -= 100;
    CHECK_EQ_INT(nanosleep(&pause, NULL), 0);
    b = custom_tick_get();
    CHECK((uint32_t)(b - a) < 1000u);
    CHECK((uint32_t)(b - a) > 0u);
}

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {{"step_back_host", step_back_host}};
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
