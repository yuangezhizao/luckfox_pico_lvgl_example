/* 亮度滑条经真实挂载路径（setup_ui → main_app_init → brightness_ui_create）的行为断言，480 分辨率；括号内为 PR #6 check.sh 的断言编号。 */
#include <stdio.h>
#include <sys/inotify.h>
#include <unistd.h>

#include "app_env.h"
#include "capture.h"
#include "check.h"
#include "fake_fs.h"

#define BL_DEV      "sys/class/backlight/backlight"
#define SLIDER_Y    290
#define MUSIC_BTN_X 240
#define MUSIC_BTN_Y 410

static void backlight(const char *max, const char *cur)
{
    tst_mkdirs(BL_DEV);
    tst_write_file(BL_DEV "/max_brightness", max);
    tst_write_file(BL_DEV "/brightness", cur);
}

static long brightness(void)
{
    return tst_read_long(BL_DEV "/brightness");
}

/* WIFI 键显示时 MUSIC 键在 x=195..285；MUSIC_ENABLE=0 时点它弹出「Can't find music!」。 */
static void start_main(void)
{
    tst_app_init(TST_DESIGN_RES, 1, 0);
    tst_app_setup_ui();
}

/* (A) */
static void case_drag_writes_min_and_max(void)
{
    char using_line[512];
    const char *out;
    long left;
    long right;

    backlight("255\n", "204\n");
    tst_capture_begin();
    start_main();
    tst_drag(240, 30, SLIDER_Y);
    left = brightness();
    tst_drag(240, 460, SLIDER_Y);
    right = brightness();
    out = tst_capture_end();

    tst_fmt(using_line, sizeof(using_line), "brightness: using %s/backlight (max 255)", tst_backlight_root());
    CHECK_EQ_INT(left, 26);
    CHECK_EQ_INT(right, 255);
    CHECK_EQ_INT(tst_count_lines(out, using_line), 1);
}

static void expect_min_write_is_one(const char *max)
{
    backlight(max, max);
    start_main();
    tst_drag(240, 30, SLIDER_Y);
    CHECK_EQ_INT(brightness(), 1);
}

/* (H9) */
static void case_small_max_never_writes_zero_9(void)
{
    expect_min_write_is_one("9\n");
}

/* (H1) */
static void case_small_max_never_writes_zero_1(void)
{
    expect_min_write_is_one("1\n");
}

/* (B) */
static void case_initial_zero_not_written(void)
{
    backlight("255\n", "0\n");
    start_main();
    CHECK_EQ_INT(brightness(), 0);
}

/* (M) */
static void case_popup_blocks_drag(void)
{
    backlight("255\n", "204\n");
    start_main();
    tst_tap(MUSIC_BTN_X, MUSIC_BTN_Y);
    tst_drag(100, 400, SLIDER_Y);
    CHECK_EQ_INT(brightness(), 204);
}

static void expect_hidden(const char *reason)
{
    const char *out;

    tst_capture_begin();
    start_main();
    out = tst_capture_end();
    CHECK_EQ_INT(tst_count_lines(out, "control hidden"), 1);
    CHECK_EQ_INT(tst_count_lines(out, reason), 1);
}

static void expect_hidden_bad_max(const char *max)
{
    char reason[512];

    backlight(max, "100\n");
    tst_fmt(reason, sizeof(reason), "invalid max_brightness in %s/backlight", tst_backlight_root());
    expect_hidden(reason);
}

/* (C) */
static void case_hidden_no_device(void)
{
    char reason[512];

    tst_fmt(reason, sizeof(reason), "no backlight device under %s", tst_backlight_root());
    expect_hidden(reason);
}

/* (D) */
static void case_hidden_non_numeric_max(void)
{
    expect_hidden_bad_max("abc\n");
}

/* (E) */
static void case_hidden_zero_max(void)
{
    expect_hidden_bad_max("0\n");
}

/* (N) */
static void case_hidden_oversized_max(void)
{
    expect_hidden_bad_max("30000000\n");
}

/* (F) root 身份下 chmod 444 挡不住写入，改用指向 /dev/full 的符号链接制造写入失败。 */
static void case_write_failure_reported_once(void)
{
    char path[512];
    const char *out;
    int relinked;

    backlight("255\n", "204\n");
    tst_capture_begin();
    start_main();
    tst_path(path, sizeof(path), BL_DEV "/brightness");
    relinked = unlink(path) == 0 && symlink("/dev/full", path) == 0;
    tst_drag(240, 30, SLIDER_Y);
    tst_drag(240, 460, SLIDER_Y);
    out = tst_capture_end();

    CHECK(relinked);
    CHECK_EQ_INT(tst_count_lines(out, "brightness: write backlight failed"), 1);
}

static int count_close_write(int fd)
{
    char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
    int count = 0;
    ssize_t n;

    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        char *p = buf;

        while (p < buf + n) {
            const struct inotify_event *ev = (const struct inotify_event *)p;

            if (ev->mask & IN_CLOSE_WRITE)
                count++;
            p += sizeof(struct inotify_event) + ev->len;
        }
    }
    return count;
}

/* (G) 拖动中按换算值去重写入；PR #6 实测 11 次，上限 15。 */
static void case_writes_deduplicated(void)
{
    char path[512];
    int fd;
    int watched;
    int writes;

    backlight("10\n", "10\n");
    start_main();
    tst_path(path, sizeof(path), BL_DEV "/brightness");
    fd = inotify_init1(IN_NONBLOCK);
    /* inotify 会合并队尾未读的相同事件，只监听 IN_CLOSE_WRITE 时多次写入会并成 1 次；同时监听 IN_OPEN 使相邻事件交替、不被合并。 */
    watched = fd >= 0 && inotify_add_watch(fd, path, IN_OPEN | IN_CLOSE_WRITE) >= 0;
    tst_drag(240, 30, SLIDER_Y);
    tst_drag(240, 460, SLIDER_Y);
    writes = watched ? count_close_write(fd) : -1;
    if (fd >= 0)
        close(fd);

    printf("INFO writes=%d\n", writes);
    CHECK(watched);
    CHECK(writes > 0 && writes <= 15);
    CHECK_EQ_INT(brightness(), 10);
}

static const tst_case_t cases[] = {
    { "drag_writes_min_and_max", case_drag_writes_min_and_max },
    { "small_max_never_writes_zero_9", case_small_max_never_writes_zero_9 },
    { "small_max_never_writes_zero_1", case_small_max_never_writes_zero_1 },
    { "initial_zero_not_written", case_initial_zero_not_written },
    { "popup_blocks_drag", case_popup_blocks_drag },
    { "hidden_no_device", case_hidden_no_device },
    { "hidden_non_numeric_max", case_hidden_non_numeric_max },
    { "hidden_zero_max", case_hidden_zero_max },
    { "hidden_oversized_max", case_hidden_oversized_max },
    { "write_failure_reported_once", case_write_failure_reported_once },
    { "writes_deduplicated", case_writes_deduplicated },
};

int main(int argc, char **argv)
{
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
