/* 进度滑块：进入音乐页时为 0，不是 GUI Guider 的占位值 50（spec §5.1 F5）。 */
#include "app_env.h"
#include "check.h"
#include "gui_guider.h"
#include "lvgl.h"
#include "music_env.h"

static void initial_zero(void)
{
    music_env_socketpair();
    music_env_file("a.mp3");
    tst_app_init(480, 0, 1);
    setup_scr_Music_player(&guider_ui);
    CHECK_EQ_INT(lv_slider_get_value(guider_ui.Music_player_progress_slider), 0);
}

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {{"initial_zero", initial_zero}};
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
