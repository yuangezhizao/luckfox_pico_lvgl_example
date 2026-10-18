/* WiFi 页圆环与连接图标按 wpa_cli status 的 wpa_state 分组显示；连不上 wpa_supplicant 时圆环停止（spec §3.1、§4，N7）。 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "app_env.h"
#include "check.h"
#include "gui_guider.h"
#include "lvgl.h"
#include "wifi_env.h"

void tst_wifi_status_update(void);

/* 连接中的 6 个状态与未连接的状态（含 wpa_supplicant 的 UNKNOWN 与一个未知取值），取值见 hostap wpa_supplicant_state_txt()。 */
static const char *const CONNECTING[] = {"SCANNING", "AUTHENTICATING", "ASSOCIATING", "ASSOCIATED", "4WAY_HANDSHAKE", "GROUP_HANDSHAKE"};
static const char *const IDLE[] = {"DISCONNECTED", "INACTIVE", "INTERFACE_DISABLED", "UNKNOWN", "SOME_FUTURE_STATE"};

static void setup_wifi_page(void)
{
    wifi_env_setup(WIFI_ENV_CONF);
    tst_app_init(480, 1, 0);
    setup_scr_WIFI(&guider_ui);
    lv_scr_load(guider_ui.WIFI);
    wifi_env_reset_logs();
}

/* 写入假 wpa_cli 的 status 输出（与 v2.10 不带 -i 时的格式相同）；state 为 NULL 时删除，即连不上守护进程。 */
static void set_state(const char *state)
{
    char buf[512];

    if (state == NULL) {
        tst_path(buf, sizeof(buf), "status.txt");
        unlink(buf);
        return;
    }
    snprintf(buf, sizeof(buf),
             "Selected interface 'wlan0'\nbssid=aa:bb:cc:dd:ee:ff\nfreq=2412\nssid=home\nid=0\nmode=station\n"
             "wpa_state=%s\naddress=11:22:33:44:55:66\n",
             state);
    tst_write_file("status.txt", buf);
}

static void query(const char *state)
{
    set_state(state);
    tst_wifi_status_update();
}

static int shown(lv_obj_t *obj)
{
    return !lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

/* 打印上下文后断言圆环与连接图标（图标与 SSID 标签同显同隐）。 */
static void expect(const char *what, int spinner, int icon)
{
    printf("%s: spinner=%d icon=%d label=%d\n", what, shown(guider_ui.WIFI_loading_spinner), shown(guider_ui.WIFI_wifi_log_img),
           shown(guider_ui.WIFI_loaded_wifi_label));
    CHECK_EQ_INT(shown(guider_ui.WIFI_loading_spinner), spinner);
    CHECK_EQ_INT(shown(guider_ui.WIFI_wifi_log_img), icon);
    CHECK_EQ_INT(shown(guider_ui.WIFI_loaded_wifi_label), icon);
}

/* 按 LVGL 的真实 tick 运行到 start 之后 ms 毫秒（同 test_scan_trigger.c）；tst_run_ms() 按循环次数计时，负载高时不准。 */
static void run_until(uint32_t start, uint32_t ms)
{
    while (lv_tick_elaps(start) < ms) {
        lv_timer_handler();
        usleep(5000);
    }
    lv_timer_handler();
}

static int status_queries(void)
{
    char *log = wifi_env_read("cmd.log");
    int n = 0;

    for (const char *p = log; (p = strstr(p, "wpa_cli status\n")) != NULL; p++)
        n++;
    free(log);
    return n;
}

static void unreachable_stops(void)
{
    setup_wifi_page();
    query("SCANNING");
    expect("SCANNING", 1, 0);
    query(NULL);
    expect("SCANNING -> unreachable", 0, 0);
    query("COMPLETED");
    expect("COMPLETED", 0, 1);
    query(NULL);
    expect("COMPLETED -> unreachable", 0, 0);
}

static void idle_states_stop(void)
{
    char what[96];

    setup_wifi_page();
    for (size_t i = 0; i < sizeof(IDLE) / sizeof(IDLE[0]); i++) {
        query("SCANNING");
        query(IDLE[i]);
        snprintf(what, sizeof(what), "SCANNING -> %s", IDLE[i]);
        expect(what, 0, 0);
        query("COMPLETED");
        query(IDLE[i]);
        snprintf(what, sizeof(what), "COMPLETED -> %s", IDLE[i]);
        expect(what, 0, 0);
    }
}

static void connecting_states_spin(void)
{
    char what[96];

    setup_wifi_page();
    for (size_t i = 0; i < sizeof(CONNECTING) / sizeof(CONNECTING[0]); i++) {
        query("COMPLETED");
        query(CONNECTING[i]);
        snprintf(what, sizeof(what), "COMPLETED -> %s", CONNECTING[i]);
        expect(what, 1, 0);
    }
}

static void completed_shows_icon(void)
{
    setup_wifi_page();
    query("SCANNING");
    query("COMPLETED");
    expect("SCANNING -> COMPLETED", 0, 1);
    CHECK(strcmp(lv_label_get_text(guider_ui.WIFI_loaded_wifi_label), "home") == 0);
}

/* SSID 可以含 wpa_state= 这样的文字，status 输出中 ssid= 行在 wpa_state= 行之前；只认行首的 wpa_state=。 */
static void state_line_only(void)
{
    setup_wifi_page();
    query("COMPLETED");
    tst_write_file("status.txt", "bssid=aa:bb:cc:dd:ee:ff\nssid=wpa_state=COMPLETED\nid=0\nwpa_state=ASSOCIATING\n");
    tst_wifi_status_update();
    expect("ssid=wpa_state=COMPLETED, ASSOCIATING", 1, 0);
    tst_write_file("status.txt", "bssid=aa:bb:cc:dd:ee:ff\nssid=wpa_state=SCANNING\nid=0\nwpa_state=COMPLETED\n");
    tst_wifi_status_update();
    expect("ssid=wpa_state=SCANNING, COMPLETED", 0, 1);
}

/* 走真实的 5 s 定时器：直接查询一次得到 SCANNING，再改为连不上，下一次轮询后圆环停止。只等一次轮询；
 * 先运行 50 ms，使进入页面时可能发生的查询在设成 SCANNING 之前执行完。 */
static void timer_unreachable_stops(void)
{
    uint32_t t;

    setup_wifi_page();
    t = lv_tick_get();
    run_until(t, 50);
    query("SCANNING");
    expect("SCANNING", 1, 0);
    set_state(NULL);
    wifi_env_reset_logs();
    t = lv_tick_get();
    run_until(t, 5100);
    CHECK(status_queries() >= 1);
    expect("timer unreachable", 0, 0);
}

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {
        {"unreachable_stops", unreachable_stops},
        {"idle_states_stop", idle_states_stop},
        {"connecting_states_spin", connecting_states_spin},
        {"completed_shows_icon", completed_shows_icon},
        {"state_line_only", state_line_only},
        {"timer_unreachable_stops", timer_unreachable_stops},
    };
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
