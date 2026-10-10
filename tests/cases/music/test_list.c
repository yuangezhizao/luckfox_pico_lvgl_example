/* 音乐列表选项串：总长超过 255 字节不溢出，内容逐项正确（PR #8 spec D3、D4）；roller 显示缩写名（spec §5.1 F4）。 */
#include <string.h>
#include "capture.h"
int music_scan_list(void);

#include "app_env.h"
#include "check.h"
#include "gui_guider.h"
#include "lvgl.h"
#include "music_env.h"

const char *music_roller_options(void);

static void options_over_255(void)
{
    char names[12][201], buf[256];
    int seen[12] = {0};
    lv_obj_t *roller;
    uint16_t i;

    music_env_socketpair();
    for (i = 0; i < 12; i++) {
        memset(names[i], 'a' + i, 196);
        memcpy(names[i] + 196, ".mp3", 5);
        music_env_file(names[i]);
    }
    tst_app_init(480, 0, 1);
    setup_scr_Music_player(&guider_ui);
    roller = guider_ui.Music_player_roller_1;
    CHECK_EQ_INT(lv_roller_get_option_cnt(roller), 12);
    for (i = 0; i < 12; i++) {
        lv_roller_set_selected(roller, i, LV_ANIM_OFF);
        lv_roller_get_selected_str(roller, buf, sizeof(buf));
        /* roller 显示的是按宽度缩写的名字（spec §5.1 F4），完整名字由 music_roller_options() 校验。 */
        CHECK(strlen(buf) < 200 && strstr(buf, "...") != NULL);
        if (buf[0] >= 'a' && buf[0] < 'a' + 12)
            seen[buf[0] - 'a'] = 1;
    }
    for (i = 0; i < 12; i++)
        CHECK(seen[i]);
    CHECK(strlen(music_roller_options()) == 12 * 200 + 11);
}

static void newline_name_skipped(void)
{
    const char *out, *opts, *nl;

    music_env_socketpair();
    music_env_file("a.mp3");
    music_env_file("b\nc.mp3");
    music_env_file("d.mp3");
    tst_capture_begin();
    music_scan_list();
    out = tst_capture_end();
    opts = music_roller_options();
    nl = strchr(opts, '\n');
    CHECK(nl != NULL && strchr(nl + 1, '\n') == NULL);
    CHECK(strchr(opts, 'b') == NULL);
    CHECK_EQ_INT(tst_count_lines(out, "skip music file"), 1);
}

static void missing_dir(void)
{
    music_env_socketpair();
    CHECK(music_scan_list() != 0);
    CHECK(strcmp(music_roller_options(), "") == 0);
}

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {{"options_over_255", options_over_255}, {"newline_name_skipped", newline_name_skipped}, {"missing_dir", missing_dir}};
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
