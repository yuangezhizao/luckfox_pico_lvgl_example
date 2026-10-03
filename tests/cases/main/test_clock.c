/* _time_update()：0/11/12/13/23 点的 12 小时制小时与 AM/PM（spec §5.1 ⑫）。 */
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "app_env.h"
#include "check.h"

void tst_time_update(void);
extern int Main_digital_clock_1_hour_value;
extern char Main_digital_clock_1_meridiem[];

static time_t fake_now;

time_t time(time_t *t)
{
    if (t != NULL)
        *t = fake_now;
    return fake_now;
}

static void meridiem(void)
{
    static const struct { int hour; int want_hour; const char *want_ampm; } rows[] = {
        {0, 12, "AM"}, {11, 11, "AM"}, {12, 12, "PM"}, {13, 1, "PM"}, {23, 11, "PM"},
    };
    size_t i;

    setenv("TZ", "UTC", 1);
    tzset();
    tst_app_init(480, 0, 0);
    tst_app_setup_ui();
    for (i = 0; i < TST_COUNT(rows); i++) {
        fake_now = (time_t)1700006400 + rows[i].hour * 3600;   /* 2023-11-15 00:00:00 UTC 起 */
        tst_time_update();
        CHECK_EQ_INT(Main_digital_clock_1_hour_value, rows[i].want_hour);
        CHECK(strcmp(Main_digital_clock_1_meridiem, rows[i].want_ampm) == 0);
    }
}

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {{"meridiem", meridiem}};
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
