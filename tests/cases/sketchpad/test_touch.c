/* 触摸读取只跟随第一根按下的手指（spec §2.5、§5.1 F2）：回放板上录到的多指事件，另含合成用例与单点退回。 */
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "app_env.h"
#include "check.h"
#include "fake_fs.h"
#include "lvgl.h"

int custom_touch_init(const char *path);
void custom_touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data);
int tst_touch_fd(void);

typedef struct {
    int x, y, pressed;
} sample_t;

static lv_indev_drv_t drv;
static int wfd = -1;
static sample_t samples[2048];
static int nsamples;

/* 先以非阻塞方式打开读端（FIFO 在没有写端时也能打开），再打开写端。 */
static void open_device(void)
{
    char path[1024];

    tst_app_init(480, 0, 0);
    lv_indev_drv_init(&drv);
    drv.type = LV_INDEV_TYPE_POINTER;
    drv.disp = lv_disp_get_default();
    tst_path(path, sizeof(path), "event0");
    CHECK(mkfifo(path, 0600) == 0);
    CHECK_EQ_INT(custom_touch_init(path), 0);
    wfd = open(path, O_WRONLY | O_NONBLOCK);
    CHECK(wfd >= 0);
    nsamples = 0;
}

static void emit(int type, int code, int value)
{
    struct input_event ev;

    memset(&ev, 0, sizeof(ev));
    ev.type = (unsigned short)type;
    ev.code = (unsigned short)code;
    ev.value = value;
    CHECK_EQ_INT(write(wfd, &ev, sizeof(ev)), (long)sizeof(ev));
}

/* 结束一帧并像 LVGL 一样读一次。 */
static sample_t syn(void)
{
    lv_indev_data_t data;
    sample_t s;

    emit(EV_SYN, SYN_REPORT, 0);
    memset(&data, 0, sizeof(data));
    custom_touch_read(&drv, &data);
    s.x = data.point.x;
    s.y = data.point.y;
    s.pressed = data.state == LV_INDEV_STATE_PRESSED;
    if (nsamples < (int)TST_COUNT(samples))
        samples[nsamples++] = s;
    return s;
}

/* 解析 evdev_probe 的文本输出，逐帧回放；返回帧数。 */
static int replay(const char *name)
{
    static const struct { const char *name; int code; } abs_names[] = {
        {"ABS_X", ABS_X}, {"ABS_Y", ABS_Y}, {"ABS_MT_SLOT", ABS_MT_SLOT}, {"ABS_MT_TOUCH_MAJOR", ABS_MT_TOUCH_MAJOR},
        {"ABS_MT_WIDTH_MAJOR", ABS_MT_WIDTH_MAJOR}, {"ABS_MT_POSITION_X", ABS_MT_POSITION_X},
        {"ABS_MT_POSITION_Y", ABS_MT_POSITION_Y}, {"ABS_MT_TRACKING_ID", ABS_MT_TRACKING_ID},
    };
    char path[1024], line[256], tag[64];
    int frames = 0;
    FILE *fp;

    snprintf(path, sizeof(path), "%s/%s", TST_SKETCH_SRC_DIR, name);
    fp = fopen(path, "r");
    CHECK(fp != NULL);
    if (fp == NULL)
        return 0;
    while (fgets(line, sizeof(line), fp) != NULL) {
        int code, value;
        size_t i;

        if (line[0] == '#')
            continue;
        if (sscanf(line, "%*s %63s", tag) != 1)
            continue;
        if (strcmp(tag, "SYN") == 0) {
            syn();
            frames++;
        } else if (strcmp(tag, "KEY") == 0 && sscanf(line, "%*s KEY code %d value %d", &code, &value) == 2) {
            emit(EV_KEY, code, value);
        } else {
            for (i = 0; i < TST_COUNT(abs_names); i++)
                if (strcmp(tag, abs_names[i].name) == 0 && sscanf(line, "%*s %*s %d", &value) == 1)
                    emit(EV_ABS, abs_names[i].code, value);
        }
    }
    fclose(fp);
    return frames;
}

static int near(sample_t s, int x, int y, int r)
{
    return abs(s.x - x) <= r && abs(s.y - y) <= r;
}

/* 第 2 步：slot 0 在中央画线，slot 1 在 (388,207) 点一下再抬起。旧驱动先跳到 (388,207) 连线，抬起后停笔。 */
static void board_tap_while_drawing(void)
{
    int i, jumped = 0, released_mid = 0, last_x = 0;

    open_device();
    CHECK(replay("touch_step2.txt") > 100);
    for (i = 0; i < nsamples - 1; i++) {
        if (samples[i].pressed && near(samples[i], 388, 207, 20))
            jumped++;
        if (!samples[i].pressed)
            released_mid++;
        if (samples[i].pressed)
            last_x = samples[i].x;
    }
    printf("frames=%d jumped=%d released_mid=%d last_x=%d\n", nsamples, jumped, released_mid, last_x);
    CHECK_EQ_INT(jumped, 0);
    CHECK_EQ_INT(released_mid, 0);
    CHECK(last_x >= 245);
    CHECK(!samples[nsamples - 1].pressed);
}

/* 第 1 步：slot 0 先按住 (388,223)，slot 1 在 (215,203) 画线后抬起。笔一直在 slot 0，不跳到 slot 1。 */
static void board_second_finger_ignored(void)
{
    int i, jumped = 0, released_mid = 0;

    open_device();
    CHECK(replay("touch_step1.txt") > 10);
    for (i = 0; i < nsamples - 1; i++) {
        if (samples[i].pressed && samples[i].x < 300)
            jumped++;
        if (!samples[i].pressed)
            released_mid++;
    }
    printf("frames=%d jumped=%d released_mid=%d\n", nsamples, jumped, released_mid);
    CHECK_EQ_INT(jumped, 0);
    CHECK_EQ_INT(released_mid, 0);
    CHECK(!samples[nsamples - 1].pressed);
}

/* 主触点先抬起而另一指仍在：停笔，直到全部抬起、重新按下才继续，避免从旧笔迹连线到另一指。 */
static void primary_lift_waits_all_up(void)
{
    sample_t s;

    open_device();
    emit(EV_ABS, ABS_MT_SLOT, 0);
    emit(EV_ABS, ABS_MT_TRACKING_ID, 10);
    emit(EV_ABS, ABS_MT_POSITION_X, 100);
    emit(EV_ABS, ABS_MT_POSITION_Y, 100);
    s = syn();
    CHECK(s.pressed && near(s, 100, 100, 0));
    emit(EV_ABS, ABS_MT_SLOT, 1);
    emit(EV_ABS, ABS_MT_TRACKING_ID, 11);
    emit(EV_ABS, ABS_MT_POSITION_X, 400);
    emit(EV_ABS, ABS_MT_POSITION_Y, 400);
    s = syn();
    CHECK(s.pressed && near(s, 100, 100, 0));
    /* 主触点抬起：内核把模拟单点的 ABS_X/ABS_Y 换到另一指；松开坐标必须仍是主触点的，否则会在另一指处触发点击。 */
    emit(EV_ABS, ABS_MT_SLOT, 0);
    emit(EV_ABS, ABS_MT_TRACKING_ID, -1);
    emit(EV_ABS, ABS_X, 400);
    emit(EV_ABS, ABS_Y, 400);
    s = syn();
    CHECK(!s.pressed && near(s, 100, 100, 0));
    emit(EV_ABS, ABS_MT_SLOT, 1);
    emit(EV_ABS, ABS_MT_POSITION_X, 410);
    s = syn();
    CHECK(!s.pressed);
    /* slot 1 仍按着时又按下第三根手指：同样不接管。 */
    emit(EV_ABS, ABS_MT_SLOT, 2);
    emit(EV_ABS, ABS_MT_TRACKING_ID, 13);
    emit(EV_ABS, ABS_MT_POSITION_X, 200);
    emit(EV_ABS, ABS_MT_POSITION_Y, 200);
    s = syn();
    CHECK(!s.pressed);
    emit(EV_ABS, ABS_MT_TRACKING_ID, -1);
    emit(EV_ABS, ABS_MT_SLOT, 1);
    emit(EV_ABS, ABS_MT_TRACKING_ID, -1);
    s = syn();
    CHECK(!s.pressed);
    /* 新接触落在 slot 1，X 与该槽位上次相同（内核不重发），坐标取该槽位记录的值。 */
    emit(EV_ABS, ABS_MT_TRACKING_ID, 12);
    emit(EV_ABS, ABS_MT_POSITION_Y, 300);
    s = syn();
    CHECK(s.pressed && near(s, 410, 300, 0));
}

/* 没有 MT 事件的单点触摸屏：退回 ABS_X/ABS_Y 加 BTN_TOUCH；坐标钳在屏幕内。 */
static void single_touch_fallback(void)
{
    sample_t s;

    open_device();
    emit(EV_KEY, BTN_TOUCH, 1);
    emit(EV_ABS, ABS_X, 50);
    emit(EV_ABS, ABS_Y, 60);
    s = syn();
    CHECK(s.pressed && near(s, 50, 60, 0));
    emit(EV_ABS, ABS_X, 900);
    s = syn();
    CHECK(s.pressed && near(s, 479, 60, 0));
    emit(EV_KEY, BTN_TOUCH, 0);
    s = syn();
    CHECK(!s.pressed);
}

/* popen、system 起的子进程（如常驻的 udhcpc）不应继承触摸设备的 fd（R3）。 */
static void fd_cloexec(void)
{
    int flags;

    open_device();
    flags = fcntl(tst_touch_fd(), F_GETFD);
    CHECK(flags >= 0 && (flags & FD_CLOEXEC));
}

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {
        {"board_tap_while_drawing", board_tap_while_drawing},
        {"board_second_finger_ignored", board_second_finger_ignored},
        {"primary_lift_waits_all_up", primary_lift_waits_all_up},
        {"single_touch_fallback", single_touch_fallback},
        {"fd_cloexec", fd_cloexec},
    };
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
