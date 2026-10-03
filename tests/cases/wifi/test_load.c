/* 非法输入按 Load：不调用 wpa_cli、不改配置、提示标签可见（spec §5.2 ⑳）。 */
#include <string.h>

#include "app_env.h"
#include "check.h"
#include "gui_guider.h"
#include "lvgl.h"
#include "wifi_env.h"

#include "wifi_ui.h"
void WIFI_load_btn_event_handler(lv_event_t *e);

static void invalid_no_wpa_call(void)
{
    lv_obj_t *btn;
    char *log, *in, *conf;

    wifi_env_setup(WIFI_ENV_CONF);
    tst_app_init(480, 1, 0);
    setup_scr_WIFI(&guider_ui);
    wifi_env_reset_logs();
    lv_textarea_set_text(guider_ui.WIFI_ssid_ta, "home");
    lv_textarea_set_text(guider_ui.WIFI_psw_ta, "1234567");
    btn = lv_btn_create(lv_scr_act());
    lv_obj_add_event_cb(btn, WIFI_load_btn_event_handler, LV_EVENT_ALL, NULL);
    lv_event_send(btn, LV_EVENT_RELEASED, NULL);
    tst_run_ms(300);
    log = wifi_env_read("cmd.log");
    in = wifi_env_read("wpa_cli.stdin");
    conf = wifi_env_read("wpa_supplicant.conf");
    CHECK(log[0] == '\0');
    CHECK(in[0] == '\0');
    CHECK(strcmp(conf, WIFI_ENV_CONF) == 0);
    CHECK(wifi_test_hint() != NULL && !lv_obj_has_flag(wifi_test_hint(), LV_OBJ_FLAG_HIDDEN));
    free(log);
    free(in);
    free(conf);
}


/* 必须在文本框过滤非法字节前拒绝，不能把 Caf\xe9 静默改成 Caf。 */
static void dropdown_utf8(void)
{
    static const char *const invalid[] = {"Caf\xe9", "a\xff""b", "\xd6\xd0\xce\xc4"};
    wifi_env_setup(WIFI_ENV_CONF);
    tst_app_init(480, 1, 0);
    setup_scr_WIFI(&guider_ui);
    wifi_env_reset_logs();
    for (size_t i = 0; i < TST_COUNT(invalid); i++) {
        lv_textarea_set_text(guider_ui.WIFI_ssid_ta, "home");
        lv_dropdown_set_options(guider_ui.WIFI_wifi_list, invalid[i]);
        lv_event_send(guider_ui.WIFI_wifi_list, LV_EVENT_VALUE_CHANGED, NULL);
        CHECK(strcmp(lv_textarea_get_text(guider_ui.WIFI_ssid_ta), "home") == 0);
        CHECK(wifi_test_hint() != NULL && !lv_obj_has_flag(wifi_test_hint(), LV_OBJ_FLAG_HIDDEN));
        if (wifi_test_hint() != NULL)
            CHECK_STR_CONTAINS(lv_label_get_text(wifi_test_hint()), "UTF-8");
    }
    lv_dropdown_set_options(guider_ui.WIFI_wifi_list, "中文网络");
    lv_event_send(guider_ui.WIFI_wifi_list, LV_EVENT_VALUE_CHANGED, NULL);
    CHECK(strcmp(lv_textarea_get_text(guider_ui.WIFI_ssid_ta), "中文网络") == 0);
    CHECK(lv_obj_has_flag(wifi_test_hint(), LV_OBJ_FLAG_HIDDEN));
    char *log = wifi_env_read("cmd.log");
    char *conf = wifi_env_read("wpa_supplicant.conf");
    CHECK(log[0] == '\0');
    CHECK(strcmp(conf, WIFI_ENV_CONF) == 0);
    free(log);
    free(conf);
}

/* 原始扫描项超出字节上限时不能把截断的名字作为可提交输入。 */
static void dropdown_overlong(void)
{
    static const char *const invalid[] = {
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "中文网络中文网络中文网"
    };
    wifi_env_setup(WIFI_ENV_CONF);
    tst_app_init(480, 1, 0);
    setup_scr_WIFI(&guider_ui);
    lv_obj_t *btn = lv_btn_create(lv_scr_act());
    lv_obj_add_event_cb(btn, WIFI_load_btn_event_handler, LV_EVENT_RELEASED, NULL);
    for (size_t i = 0; i < TST_COUNT(invalid); i++) {
        wifi_env_reset_logs();
        lv_textarea_set_text(guider_ui.WIFI_ssid_ta, "");
        lv_textarea_set_text(guider_ui.WIFI_psw_ta, "12345678");
        lv_dropdown_set_options(guider_ui.WIFI_wifi_list, invalid[i]);
        lv_event_send(guider_ui.WIFI_wifi_list, LV_EVENT_VALUE_CHANGED, NULL);
        CHECK(strcmp(lv_textarea_get_text(guider_ui.WIFI_ssid_ta), "") == 0);
        CHECK(!lv_obj_has_flag(wifi_test_hint(), LV_OBJ_FLAG_HIDDEN));
        CHECK_STR_CONTAINS(lv_label_get_text(wifi_test_hint()), "Invalid SSID");
        lv_event_send(btn, LV_EVENT_RELEASED, NULL);
        char *log = wifi_env_read("cmd.log");
        char *in = wifi_env_read("wpa_cli.stdin");
        char *conf = wifi_env_read("wpa_supplicant.conf");
        CHECK(log[0] == '\0');
        CHECK(in[0] == '\0');
        CHECK(strcmp(conf, WIFI_ENV_CONF) == 0);
        free(log);
        free(in);
        free(conf);
    }
}

/* 两个输入框真实编辑都应隐藏提示，初始回填完成后提示也应隐藏。 */
static void edit_hides_hint(void)
{
    wifi_env_setup(WIFI_ENV_CONF);
    tst_app_init(480, 1, 0);
    setup_scr_WIFI(&guider_ui);
    lv_obj_t *hint = wifi_test_hint();
    CHECK(hint != NULL);
    if (hint == NULL)
        return;
    CHECK(lv_obj_has_flag(hint, LV_OBJ_FLAG_HIDDEN));
    lv_obj_t *btn = lv_btn_create(lv_scr_act());
    lv_obj_add_event_cb(btn, WIFI_load_btn_event_handler, LV_EVENT_ALL, NULL);
    lv_obj_t *fields[] = {guider_ui.WIFI_ssid_ta, guider_ui.WIFI_psw_ta};
    for (size_t i = 0; i < TST_COUNT(fields); i++) {
        lv_textarea_set_text(guider_ui.WIFI_psw_ta, "short");
        lv_event_send(btn, LV_EVENT_RELEASED, NULL);
        CHECK(!lv_obj_has_flag(hint, LV_OBJ_FLAG_HIDDEN));
        lv_textarea_add_text(fields[i], "x");
        CHECK(lv_obj_has_flag(hint, LV_OBJ_FLAG_HIDDEN));
    }
}

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {{"invalid_no_wpa_call", invalid_no_wpa_call}, {"dropdown_utf8", dropdown_utf8}, {"dropdown_overlong", dropdown_overlong}, {"edit_hides_hint", edit_hides_hint}};
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
