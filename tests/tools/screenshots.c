/* screenshots 目标：输出截图（PPM）；用法：luckfox_screenshots <480|720> <drag|popup|no-device|music|pad> <输出前缀>
 * music：3 个前缀相同的长文件名，进入音乐页并打开列表；pad：画板上轻点 3 下、画一条线。 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "app_env.h"
#include "fake_fs.h"
#include "gui_guider.h"
#include "lvgl.h"

extern int32_t fd_mpv;

static void save(const char *prefix, const char *name)
{
    char path[512];

    tst_fmt(path, sizeof(path), "%s-%s.ppm", prefix, name);
    if (tst_save_ppm(path) != 0)
        exit(1);
    printf("wrote %s\n", path);
}

int main(int argc, char **argv)
{
    const char *scenario;
    const char *prefix;
    int res;

    if (argc != 4 || (strcmp(argv[1], "480") != 0 && strcmp(argv[1], "720") != 0)) {
        fprintf(stderr, "usage: %s <480|720> <drag|popup|no-device|music|pad> <prefix>\n", argv[0]);
        return 2;
    }
    res = atoi(argv[1]);
    scenario = argv[2];
    prefix = argv[3];
    if (strcmp(scenario, "drag") != 0 && strcmp(scenario, "popup") != 0 && strcmp(scenario, "no-device") != 0 &&
        strcmp(scenario, "music") != 0 && strcmp(scenario, "pad") != 0) {
        fprintf(stderr, "unknown scenario: %s\n", scenario);
        return 2;
    }
    if (strcmp(scenario, "no-device") != 0) {
        tst_mkdirs("sys/class/backlight/backlight");
        tst_write_file("sys/class/backlight/backlight/max_brightness", "255\n");
        tst_write_file("sys/class/backlight/backlight/brightness", "204\n");
    }
    if (strcmp(scenario, "music") == 0) {
        static const char *const names[] = {"01", "02", "03"};
        int sv[2];

        /* mpv 命令写进 socketpair，不启动 mpv。 */
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
            return 1;
        fd_mpv = sv[0];
        tst_mkdirs("music");
        for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
            char rel[256];

            tst_fmt(rel, sizeof(rel), "music/luckfox_roller_clip_test_long_common_prefix_abcdefghijklmnopq_%s.mp3", names[i]);
            tst_write_file(rel, "");
        }
    }
    tst_app_init(res, 1, strcmp(scenario, "music") == 0 ? 1 : 0);
    tst_app_setup_ui();

    if (strcmp(scenario, "drag") == 0) {
        save(prefix, "initial");
        tst_drag(240, 30, 290);
        save(prefix, "left");
    } else if (strcmp(scenario, "popup") == 0) {
        tst_tap(240, 410);
        save(prefix, "popup");
    } else if (strcmp(scenario, "music") == 0) {
        lv_event_send(guider_ui.Main_Music_btn, LV_EVENT_CLICKED, NULL);
        tst_run_ms(600);
        save(prefix, "music");
        lv_obj_add_state(guider_ui.Music_player_music_list_btn, LV_STATE_CHECKED);
        lv_event_send(guider_ui.Music_player_music_list_btn, LV_EVENT_CLICKED, NULL);
        tst_run_ms(600);
        save(prefix, "music-list");
    } else if (strcmp(scenario, "pad") == 0) {
        lv_event_send(guider_ui.Main_PAD_btn, LV_EVENT_CLICKED, NULL);
        tst_run_ms(600);
        tst_tap(120, 100);
        tst_tap(240, 100);
        tst_tap(360, 100);
        tst_drag(120, 360, 200);
        save(prefix, "pad");
    } else {
        save(prefix, "no-device");
    }
    return 0;
}
