/* wifi_backend_release()：启动后不进 WIFI 页即退出（定时器从未创建），或重复调用，都不能崩溃。 */
#include "app_env.h"
#include "check.h"
#include "lvgl.h"

extern lv_timer_t *wifi_update_timer;
void wifi_backend_release(void);

static void noop_timer_cb(lv_timer_t *timer)
{
    (void)timer;
}

static void case_without_timer(void)
{
    tst_app_init(TST_DESIGN_RES, 1, 0);
    wifi_backend_release();
    CHECK(wifi_update_timer == NULL);
}

static void case_twice(void)
{
    tst_app_init(TST_DESIGN_RES, 1, 0);
    wifi_update_timer = lv_timer_create(noop_timer_cb, 5000, NULL);
    CHECK(wifi_update_timer != NULL);
    wifi_backend_release();
    wifi_backend_release();
    CHECK(wifi_update_timer == NULL);
}

static const tst_case_t cases[] = {
    { "without_timer", case_without_timer },
    { "twice", case_twice },
};

int main(int argc, char **argv)
{
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
