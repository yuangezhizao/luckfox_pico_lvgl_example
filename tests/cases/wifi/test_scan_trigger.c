/* Scan 按钮先请求 wpa_supplicant 扫描，约 3 秒后再读结果；连不上守护进程时提示（spec §5.1 F1、N1）。 */
#include <string.h>
#include <unistd.h>

#include "app_env.h"
#include "check.h"
#include "gui_guider.h"
#include "lvgl.h"
#include "wifi_env.h"
#include "wifi_ui.h"

#define HDR "bssid / frequency / signal level / flags / ssid\n"
#define ROW(ssid) "aa:bb:cc:dd:ee:ff\t2412\t-40\t[WPA2-PSK-CCMP][ESS]\t" ssid "\n"

static int count_of(const char *hay, const char *needle)
{
    int n = 0;

    for (const char *p = hay; (p = strstr(p, needle)) != NULL; p += strlen(needle))
        n++;
    return n;
}

static void setup_wifi_page(void)
{
    wifi_env_setup(WIFI_ENV_CONF);
    tst_write_file("scan.txt", HDR ROW("home") ROW("cafe"));
    tst_app_init(480, 1, 0);
    setup_scr_WIFI(&guider_ui);
    lv_scr_load(guider_ui.WIFI);
    wifi_env_reset_logs();
}

/* 按下 Scan，返回按下处理结束时的 tick；读结果的定时器在处理中重置，不晚于该时刻起算。 */
static uint32_t press_scan(void)
{
    lv_event_send(guider_ui.WIFI_Scanning_btn, LV_EVENT_RELEASED, NULL);
    return lv_tick_get();
}

/* 按 LVGL 的真实 tick 运行到 start 之后 ms 毫秒，退出前再处理一次定时器。tst_run_ms() 按循环次数计时，
 * 负载高时实际经过的时间更长，3 秒定时器会在检查点之前到期。 */
static void run_until(uint32_t start, uint32_t ms)
{
    while (lv_tick_elaps(start) < ms) {
        lv_timer_handler();
        usleep(5000);
    }
    lv_timer_handler();
}

static void requests_then_reads(void)
{
    char *log;
    uint32_t t;

    setup_wifi_page();
    t = press_scan();
    log = wifi_env_read("cmd.log");
    printf("log after press=[%s]\n", log);
    CHECK(strstr(log, "wpa_cli -i wlan0 scan\n") != NULL);
    CHECK(strstr(log, "scan_results") == NULL);
    CHECK(strcmp(lv_dropdown_get_options(guider_ui.WIFI_wifi_list), "scanning") == 0);
    free(log);
    run_until(t, 1000);
    log = wifi_env_read("cmd.log");
    CHECK(strstr(log, "scan_results") == NULL);
    free(log);
    run_until(t, 3100);
    log = wifi_env_read("cmd.log");
    printf("log after 3.1 s=[%s]\n", log);
    CHECK_EQ_INT(count_of(log, "scan_results"), 1);
    CHECK(strcmp(lv_dropdown_get_options(guider_ui.WIFI_wifi_list), "home\ncafe\n...") == 0);
    free(log);
}

static void repeat_single_read(void)
{
    char *log;
    uint32_t t = 0;
    int i;

    setup_wifi_page();
    /* 每隔 0.4 s 按一次，共 3 次；结果应在最后一次按下 3 s 后才读，且只读一次。 */
    for (i = 0; i < 3; i++) {
        t = press_scan();
        if (i < 2)
            run_until(t, 400);
    }
    run_until(t, 2400);
    log = wifi_env_read("cmd.log");
    CHECK_EQ_INT(count_of(log, "scan_results"), 0);
    free(log);
    run_until(t, 3100);
    log = wifi_env_read("cmd.log");
    printf("log=[%s]\n", log);
    CHECK_EQ_INT(count_of(log, "wpa_cli -i wlan0 scan\n"), 3);
    CHECK_EQ_INT(count_of(log, "scan_results"), 1);
    free(log);
}

static void unreachable_hint(void)
{
    setup_wifi_page();
    tst_write_file("scan_reply.txt", "Failed to connect to non-global ctrl_ifname: wlan0  error: No such file or directory\n");
    press_scan();
    tst_run_ms(100);
    CHECK(wifi_test_hint() != NULL && !lv_obj_has_flag(wifi_test_hint(), LV_OBJ_FLAG_HIDDEN));
    if (wifi_test_hint() != NULL)
        CHECK_STR_CONTAINS(lv_label_get_text(wifi_test_hint()), "wpa_supplicant not reachable");
}

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {{"requests_then_reads", requests_then_reads}, {"repeat_single_read", repeat_single_read}, {"unreachable_hint", unreachable_hint}};
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
