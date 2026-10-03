/* 先建立非空旧列表，再让选项串分配失败，验证选项与节点同时清空。 */
#include <string.h>
#include <unistd.h>
#include "app_env.h"
#include "check.h"
#include "custom.h"
#include "music_env.h"

extern int tst_music_alloc_fail_at;
extern int tst_music_alloc_calls;
int tst_music_list_empty(void);

static void options_alloc_failure(void)
{
    char path[1024], selected[32], empty_options[LV_ROLLER_INF_PAGES];

    music_env_socketpair();
    music_env_file("old.mp3");
    tst_app_init(480, 0, 1);
    setup_scr_Music_player(&guider_ui);
    CHECK(strcmp(music_roller_options(), "old.mp3") == 0);
    tst_path(path, sizeof(path), "music/old.mp3");
    CHECK(unlink(path) == 0);
    music_env_file("new.mp3");
    tst_music_alloc_calls = 0;
    tst_music_alloc_fail_at = 2; /* 第一次是节点，第二次是选项串。 */
    CHECK_EQ_INT(music_scan_list(), -1);
    CHECK_EQ_INT(tst_music_alloc_calls, 2);
    CHECK(strcmp(music_roller_options(), "") == 0);
    CHECK(tst_music_list_empty());
    lv_roller_set_options(guider_ui.Music_player_roller_1, music_roller_options(), LV_ROLLER_MODE_INFINITE);
    /* INFINITE 模式把一项空文本复制为多页，内部仅含页间换行。 */
    memset(empty_options, '\n', sizeof(empty_options) - 1);
    empty_options[sizeof(empty_options) - 1] = '\0';
    CHECK(strcmp(lv_roller_get_options(guider_ui.Music_player_roller_1), empty_options) == 0);
    CHECK_EQ_INT(lv_roller_get_option_cnt(guider_ui.Music_player_roller_1), 1);
    lv_roller_get_selected_str(guider_ui.Music_player_roller_1, selected, sizeof(selected));
    CHECK(strcmp(selected, "") == 0);
    tst_music_alloc_fail_at = 0;
    CHECK_EQ_INT(music_scan_list(), 0);
    CHECK(strcmp(music_roller_options(), "new.mp3") == 0);
}

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {{"options_alloc_failure", options_alloc_failure}};
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
