/* 运行中加入音乐：点 MUSIC 时重新检查目录，进入音乐页时重新扫描（spec §5.1 F6）。 */
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "app_env.h"
#include "check.h"
#include "gui_guider.h"
#include "lvgl.h"
#include "music_env.h"

extern int MUSIC_ENABLE;

static int peer = -1;

static void start(int music_enable)
{
    peer = music_env_socketpair();
    fcntl(peer, F_SETFL, O_NONBLOCK);
    tst_app_init(480, 0, music_enable);
    tst_app_setup_ui();
}

/* 读空对端，返回本次读到的内容（静态缓冲区）。 */
static const char *drain_peer(void)
{
    static char buf[65536];
    size_t used = 0;
    ssize_t n;

    while (used < sizeof(buf) - 1 && (n = read(peer, buf + used, sizeof(buf) - 1 - used)) > 0)
        used += (size_t)n;
    buf[used] = '\0';
    return buf;
}

static void click(lv_obj_t *obj)
{
    lv_event_send(obj, LV_EVENT_CLICKED, NULL);
    tst_run_ms(400);
}

/* 启动时没有音乐（MUSIC_ENABLE=0），运行中放入文件后点 MUSIC 应进入音乐页，而不是弹出找不到。 */
static void enable_recheck(void)
{
    start(0);
    click(guider_ui.Main_Music_btn);
    CHECK(lv_scr_act() == guider_ui.Main);
    CHECK(!lv_obj_has_flag(guider_ui.Main_win_music, LV_OBJ_FLAG_HIDDEN));
    lv_obj_add_flag(guider_ui.Main_win_music, LV_OBJ_FLAG_HIDDEN);
    music_env_file("a.mp3");
    click(guider_ui.Main_Music_btn);
    CHECK(lv_scr_act() == guider_ui.Music_player);
    CHECK_EQ_INT(MUSIC_ENABLE, 1);
}

/* 进入过一次后返回，再放入一个文件；再次进入时列表与 mpv 播放列表都要更新。 */
static void rescan(void)
{
    const char *sent;

    music_env_file("a.mp3");
    start(1);
    click(guider_ui.Main_Music_btn);
    CHECK(lv_scr_act() == guider_ui.Music_player);
    CHECK_EQ_INT(lv_roller_get_option_cnt(guider_ui.Music_player_roller_1), 1);
    click(guider_ui.Music_player_back_btn);
    CHECK(lv_scr_act() == guider_ui.Main);
    drain_peer();
    music_env_file("b.mp3");
    click(guider_ui.Main_Music_btn);
    CHECK(lv_scr_act() == guider_ui.Music_player);
    CHECK_EQ_INT(lv_roller_get_option_cnt(guider_ui.Music_player_roller_1), 2);
    sent = drain_peer();
    printf("sent=[%s]\n", sent);
    CHECK(strstr(sent, "\"stop\"") != NULL);
    CHECK(strstr(sent, "b.mp3") != NULL);
    CHECK(strstr(sent, "a.mp3") != NULL);
    CHECK_EQ_INT(lv_slider_get_value(guider_ui.Music_player_progress_slider), 0);
}

/* 目录没有变化时再次进入不打断当前播放：不发 stop、不重新加载。 */
static void unchanged_keeps_playlist(void)
{
    const char *sent;

    music_env_file("a.mp3");
    start(1);
    click(guider_ui.Main_Music_btn);
    click(guider_ui.Music_player_back_btn);
    drain_peer();
    click(guider_ui.Main_Music_btn);
    CHECK(lv_scr_act() == guider_ui.Music_player);
    sent = drain_peer();
    printf("sent=[%s]\n", sent);
    CHECK(strstr(sent, "\"stop\"") == NULL);
    CHECK(strstr(sent, "loadfile") == NULL);
}

static void remove_music(const char *name)
{
    char rel[256], path[1024];

    tst_fmt(rel, sizeof(rel), "music/%s", name);
    tst_path(path, sizeof(path), rel);
    CHECK(unlink(path) == 0);
}

/* 运行中把歌删光：再点 MUSIC 弹出找不到，不进入空的音乐页（spec Q28）。 */
static void remove_all_blocks_entry(void)
{
    music_env_file("a.mp3");
    start(1);
    click(guider_ui.Main_Music_btn);
    click(guider_ui.Music_player_back_btn);
    remove_music("a.mp3");
    click(guider_ui.Main_Music_btn);
    CHECK(lv_scr_act() == guider_ui.Main);
    CHECK(!lv_obj_has_flag(guider_ui.Main_win_music, LV_OBJ_FLAG_HIDDEN));
    CHECK_EQ_INT(MUSIC_ENABLE, 0);
}

/* 删掉其中一首：再次进入时列表随之减少。 */
static void remove_one_rescans(void)
{
    music_env_file("a.mp3");
    music_env_file("b.mp3");
    start(1);
    click(guider_ui.Main_Music_btn);
    CHECK_EQ_INT(lv_roller_get_option_cnt(guider_ui.Music_player_roller_1), 2);
    click(guider_ui.Music_player_back_btn);
    remove_music("a.mp3");
    click(guider_ui.Main_Music_btn);
    CHECK(lv_scr_act() == guider_ui.Music_player);
    CHECK_EQ_INT(lv_roller_get_option_cnt(guider_ui.Music_player_roller_1), 1);
    CHECK(strstr(lv_roller_get_options(guider_ui.Music_player_roller_1), "b.mp3") != NULL);
}

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {{"enable_recheck", enable_recheck}, {"rescan", rescan}, {"unchanged_keeps_playlist", unchanged_keeps_playlist},
                                       {"remove_all_blocks_entry", remove_all_blocks_entry}, {"remove_one_rescans", remove_one_rescans}};
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
