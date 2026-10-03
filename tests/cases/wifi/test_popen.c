/* WiFi 的 popen 失败：状态刷新不 pclose(NULL)，加载配置提示并返回而不退出进程（spec D18）。 */
#include <stdio.h>
#include <string.h>

#include "app_env.h"
#include "check.h"
#include "gui_guider.h"
#include "lvgl.h"
#include "wifi_env.h"

void tst_wifi_status_update(void);
void tst_wifi_conf_load(const char *ssid, const char *psk);
#include "wifi_ui.h"

static int popen_calls;

FILE *popen(const char *command, const char *type)
{
    ++popen_calls;
    (void)command;
    (void)type;
    return NULL;
}

static void failure(void)
{
    wifi_env_setup(WIFI_ENV_CONF);
    tst_app_init(480, 1, 0);
    setup_scr_WIFI(&guider_ui);
    popen_calls = 0;
    /* 失败应正常返回且保持原有连接指示；UBSan 同时检查 pclose(NULL)。 */
    lv_obj_add_flag(guider_ui.WIFI_loading_spinner, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(guider_ui.WIFI_wifi_log_img, LV_OBJ_FLAG_HIDDEN);
    tst_wifi_status_update();
    CHECK_EQ_INT(popen_calls, 1);
    CHECK(lv_obj_has_flag(guider_ui.WIFI_loading_spinner, LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_flag(guider_ui.WIFI_wifi_log_img, LV_OBJ_FLAG_HIDDEN));
    tst_wifi_conf_load("home", "homepass1");
    CHECK(wifi_test_hint() != NULL && !lv_obj_has_flag(wifi_test_hint(), LV_OBJ_FLAG_HIDDEN));
    CHECK(strcmp(lv_label_get_text(wifi_test_hint()), "Cannot run wpa_cli") == 0);
}

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {{"failure", failure}};
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
