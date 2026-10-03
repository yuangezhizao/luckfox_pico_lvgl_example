/* /music 为空：进入音乐页、上一曲、下一曲、roller 选中都不解引用空节点（spec §3.2 C1）。 */
#include <string.h>

#include "app_env.h"
#include "check.h"
#include "gui_guider.h"
#include "lvgl.h"
#include "music_env.h"

extern int tst_music_alloc_fail_at;
extern int tst_music_alloc_calls;
int music_scan_list(void);
int music_app_init(void);
const char *music_roller_options(void);
void Music_player_next_btn_event_handler(lv_event_t *e);
void Music_player_pre_btn_event_handler(lv_event_t *e);
void Music_player_list_roller_event_handler(lv_event_t *e);

static void no_crash(void)
{
    lv_obj_t *btn;

    music_env_socketpair();
    tst_mkdirs("music");
    tst_app_init(480, 0, 1);
    setup_scr_Music_player(&guider_ui);
    btn = lv_btn_create(lv_scr_act());
    lv_obj_add_event_cb(btn, Music_player_next_btn_event_handler, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(btn, Music_player_pre_btn_event_handler, LV_EVENT_RELEASED, NULL);
    lv_event_send(btn, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(guider_ui.Music_player_roller_1, Music_player_list_roller_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_event_send(guider_ui.Music_player_roller_1, LV_EVENT_VALUE_CHANGED, NULL);
    CHECK(strcmp(music_roller_options(), "") == 0);
}

/* 选项串分配失败后，空节点仍可安全进入页面并响应列表操作。 */
static void alloc_failure_no_crash(void)
{
    music_env_socketpair();
    music_env_file("song.mp3");
    tst_app_init(480, 0, 1);
    setup_scr_Music_player(&guider_ui);
    tst_music_alloc_calls = 0;
    tst_music_alloc_fail_at = 2;
    CHECK_EQ_INT(music_scan_list(), -1);
    CHECK_EQ_INT(tst_music_alloc_calls, 2);
    CHECK(strcmp(music_roller_options(), "") == 0);
    lv_roller_set_options(guider_ui.Music_player_roller_1, music_roller_options(), LV_ROLLER_MODE_INFINITE);
    CHECK_EQ_INT(music_app_init(), -1);
    lv_obj_t *btn = lv_btn_create(lv_scr_act());
    lv_obj_add_event_cb(btn, Music_player_next_btn_event_handler, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(btn, Music_player_pre_btn_event_handler, LV_EVENT_RELEASED, NULL);
    lv_event_send(btn, LV_EVENT_RELEASED, NULL);
    lv_event_send(guider_ui.Music_player_roller_1, LV_EVENT_VALUE_CHANGED, NULL);
}

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {{"no_crash", no_crash}, {"alloc_failure_no_crash", alloc_failure_no_crash}};
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
