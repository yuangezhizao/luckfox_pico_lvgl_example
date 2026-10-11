/* roller 选中 100 字节文件名：按下标找到节点并显示（spec D5）；列表不循环重复（spec Q32）。 */
#include <string.h>
#include <unistd.h>

#include "app_env.h"
#include "check.h"
#include "gui_guider.h"
#include "lvgl.h"
#include "music_env.h"

void Music_player_list_roller_event_handler(lv_event_t *e);

static void select_long_name(void)
{
    alarm(5);
    char longname[101], buf[256];
    lv_obj_t *roller;
    uint16_t i, cnt;

    memset(longname, 'L', 96);
    memcpy(longname + 96, ".mp3", 5);
    music_env_socketpair();
    music_env_file("a.mp3");
    music_env_file(longname);
    tst_app_init(480, 0, 1);
    setup_scr_Music_player(&guider_ui);
    roller = guider_ui.Music_player_roller_1;
    cnt = lv_roller_get_option_cnt(roller);
    for (i = 0; i < cnt; i++) {
        lv_roller_set_selected(roller, i, LV_ANIM_OFF);
        lv_roller_get_selected_str(roller, buf, sizeof(buf));
        /* roller 显示缩写名（spec §5.1 F4）：以 L 开头的那项就是长文件名。 */
        if (buf[0] == 'L')
            break;
    }
    CHECK(i < cnt);
    lv_obj_add_event_cb(roller, Music_player_list_roller_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_event_send(roller, LV_EVENT_VALUE_CHANGED, NULL);
    CHECK(strcmp(lv_label_get_text(guider_ui.Music_player_music_name), longname) == 0);
}

/* roller 实际显示的行数：LV_ROLLER_MODE_INFINITE 会把选项复制 LV_ROLLER_INF_PAGES 份，行数是歌曲数的 7 倍。 */
static uint16_t shown_rows(lv_obj_t *roller)
{
    const char *opts = lv_roller_get_options(roller);
    uint16_t rows = 1;

    for (; *opts != '\0'; opts++)
        rows += *opts == '\n';
    return rows;
}

/* 选中末项后再按下：普通模式停在末项，循环模式会绕回第一项。 */
static void check_no_wrap(lv_obj_t *roller)
{
    uint16_t cnt = lv_roller_get_option_cnt(roller);
    uint32_t key = LV_KEY_DOWN;

    lv_roller_set_selected(roller, cnt - 1, LV_ANIM_OFF);
    lv_event_send(roller, LV_EVENT_KEY, &key);
    CHECK_EQ_INT(lv_roller_get_selected(roller), cnt - 1);
}

static void check_rows(lv_obj_t *roller, uint16_t songs)
{
    CHECK_EQ_INT(lv_roller_get_option_cnt(roller), songs);
    CHECK_EQ_INT(shown_rows(roller), songs);
    check_no_wrap(roller);
}

/* 1 首与 4 首时每首只显示一行，滚到末项不再绕回（spec Q32、N5）；空目录时也不出现 7 个空行。 */
static void no_repeat_rows(void)
{
    alarm(5);
    music_env_socketpair();
    tst_mkdirs("music");
    tst_app_init(480, 0, 1);
    setup_scr_Music_player(&guider_ui);
    CHECK_EQ_INT(shown_rows(guider_ui.Music_player_roller_1), 1);
    lv_obj_del(guider_ui.Music_player);

    music_env_file("a.mp3");
    setup_scr_Music_player(&guider_ui);
    check_rows(guider_ui.Music_player_roller_1, 1);
    lv_obj_del(guider_ui.Music_player);

    music_env_file("b.mp3");
    music_env_file("c.mp3");
    music_env_file("d.mp3");
    setup_scr_Music_player(&guider_ui);
    check_rows(guider_ui.Music_player_roller_1, 4);
}

static void remove_music(const char *name)
{
    char rel[256], path[1024];

    tst_fmt(rel, sizeof(rel), "music/%s", name);
    tst_path(path, sizeof(path), rel);
    CHECK(unlink(path) == 0);
}

/* 运行中加歌后重新进入：music_refresh_list() 重建的列表同样不循环重复；删光后重建为空列表时也不出现 7 个空行。 */
static void no_repeat_after_rescan(void)
{
    alarm(10);
    music_env_socketpair();
    music_env_file("a.mp3");
    tst_app_init(480, 0, 1);
    tst_app_setup_ui();
    lv_event_send(guider_ui.Main_Music_btn, LV_EVENT_CLICKED, NULL);
    tst_run_ms(400);
    CHECK(lv_scr_act() == guider_ui.Music_player);
    check_rows(guider_ui.Music_player_roller_1, 1);
    lv_event_send(guider_ui.Music_player_back_btn, LV_EVENT_CLICKED, NULL);
    tst_run_ms(400);
    music_env_file("b.mp3");
    lv_event_send(guider_ui.Main_Music_btn, LV_EVENT_CLICKED, NULL);
    tst_run_ms(400);
    CHECK(lv_scr_act() == guider_ui.Music_player);
    check_rows(guider_ui.Music_player_roller_1, 2);
    lv_event_send(guider_ui.Music_player_back_btn, LV_EVENT_CLICKED, NULL);
    tst_run_ms(400);
    remove_music("a.mp3");
    remove_music("b.mp3");
    /* MUSIC 按钮会拦下空目录，这里直接加载音乐页，走 music_refresh_list() 的空列表分支。 */
    lv_scr_load(guider_ui.Music_player);
    tst_run_ms(400);
    CHECK_EQ_INT(lv_roller_get_option_cnt(guider_ui.Music_player_roller_1), 1);
    CHECK_EQ_INT(shown_rows(guider_ui.Music_player_roller_1), 1);
}

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {{"select_long_name", select_long_name}, {"no_repeat_rows", no_repeat_rows},
                                       {"no_repeat_after_rescan", no_repeat_after_rescan}};
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
