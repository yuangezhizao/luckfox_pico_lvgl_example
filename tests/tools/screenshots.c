/* screenshots 目标：输出主屏截图（PPM）；用法：luckfox_screenshots <480|720> <drag|popup|no-device> <输出前缀> */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_env.h"
#include "fake_fs.h"

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
        fprintf(stderr, "usage: %s <480|720> <drag|popup|no-device> <prefix>\n", argv[0]);
        return 2;
    }
    res = atoi(argv[1]);
    scenario = argv[2];
    prefix = argv[3];
    if (strcmp(scenario, "drag") != 0 && strcmp(scenario, "popup") != 0 && strcmp(scenario, "no-device") != 0) {
        fprintf(stderr, "unknown scenario: %s\n", scenario);
        return 2;
    }
    if (strcmp(scenario, "no-device") != 0) {
        tst_mkdirs("sys/class/backlight/backlight");
        tst_write_file("sys/class/backlight/backlight/max_brightness", "255\n");
        tst_write_file("sys/class/backlight/backlight/brightness", "204\n");
    }
    tst_app_init(res, 1, 0);
    tst_app_setup_ui();

    if (strcmp(scenario, "drag") == 0) {
        save(prefix, "initial");
        tst_drag(240, 30, 290);
        save(prefix, "left");
    } else if (strcmp(scenario, "popup") == 0) {
        tst_tap(240, 410);
        save(prefix, "popup");
    } else {
        save(prefix, "no-device");
    }
    return 0;
}
