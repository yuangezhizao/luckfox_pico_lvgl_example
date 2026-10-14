/*
 * 触摸屏读取，替代 lv_drivers 的 evdev_read（spec §2.5、§5.1 F2）。
 *
 * lv_drivers 的 evdev 不区分多点触控槽位：任一槽位的 ABS_MT_POSITION_* 都覆盖指针坐标，
 * 任一槽位的 ABS_MT_TRACKING_ID -1 都把指针置为松开。画画时手掌或第二根手指碰到屏幕，
 * 笔迹就跳过去连线，第二个接触点抬起后又停笔。
 *
 * 这里按 MT 协议 B 记录每个槽位的状态与坐标，只跟随第一根按下的手指（主触点）：
 * 其他接触点的按下、移动、抬起都忽略；主触点抬起即停笔，屏幕上仍有其他接触点时，
 * 要等全部抬起、重新按下才继续，避免从旧位置连线到另一根手指。
 * 没有出现过 MT 事件的设备退回 ABS_X/ABS_Y 加 BTN_TOUCH。
 */
#include <fcntl.h>
#include <linux/input.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "custom_touch.h"

#ifndef ABS_MT_SLOT
#define ABS_MT_SLOT 0x2f
#endif

#define TOUCH_MAX_SLOTS 10

static int touch_fd = -1;
static bool touch_mt;              /* 读到过 ABS_MT_SLOT 或 ABS_MT_TRACKING_ID */
static int touch_slot;             /* 当前事件所属槽位；超出范围时为 -1 */
static int touch_primary = -1;     /* 主触点所在槽位；-1 表示没有 */
static bool touch_wait_all_up;     /* 主触点抬起时还有其他接触点：等全部抬起 */
static bool slot_active[TOUCH_MAX_SLOTS];
static lv_coord_t slot_x[TOUCH_MAX_SLOTS];
static lv_coord_t slot_y[TOUCH_MAX_SLOTS];
static lv_coord_t pen_x, pen_y;    /* 多点：主触点最后的位置，松开时也报告它 */
static lv_coord_t st_x, st_y;      /* 单点：ABS_X、ABS_Y（多点设备上是内核按最早接触点模拟的，不用） */
static bool st_pressed;            /* 单点：BTN_TOUCH */

int custom_touch_init(const char *path)
{
    struct input_absinfo slot;

    if (touch_fd >= 0)
        close(touch_fd);
    memset(slot_active, 0, sizeof(slot_active));
    touch_mt = false;
    touch_primary = -1;
    touch_wait_all_up = false;
    st_pressed = false;
    touch_fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (touch_fd < 0) {
        perror("custom_touch_init");
        return -1;
    }
    /* 内核只在槽位变化时才发 ABS_MT_SLOT，打开时要取当前槽位；不是 evdev 设备时按 0。 */
    touch_slot = ioctl(touch_fd, EVIOCGABS(ABS_MT_SLOT), &slot) == 0 ? slot.value : 0;
    if (touch_slot < 0 || touch_slot >= TOUCH_MAX_SLOTS)
        touch_slot = -1;
    return 0;
}

static bool touch_any_active(void)
{
    int i;

    for (i = 0; i < TOUCH_MAX_SLOTS; i++)
        if (slot_active[i])
            return true;
    return false;
}

static void touch_tracking_id(int id)
{
    if (touch_slot < 0)
        return;
    if (id >= 0) {
        slot_active[touch_slot] = true;
        if (touch_primary < 0 && !touch_wait_all_up)
            touch_primary = touch_slot;
        return;
    }
    slot_active[touch_slot] = false;
    if (touch_slot == touch_primary) {
        touch_primary = -1;
        touch_wait_all_up = true;
    }
    if (!touch_any_active())
        touch_wait_all_up = false;
}

static void touch_handle(const struct input_event *in)
{
    if (in->type == EV_KEY && in->code == BTN_TOUCH) {
        st_pressed = in->value != 0;
        return;
    }
    if (in->type != EV_ABS)
        return;
    switch (in->code) {
    case ABS_X:
        st_x = (lv_coord_t)in->value;
        break;
    case ABS_Y:
        st_y = (lv_coord_t)in->value;
        break;
    case ABS_MT_SLOT:
        touch_mt = true;
        touch_slot = (in->value >= 0 && in->value < TOUCH_MAX_SLOTS) ? in->value : -1;
        break;
    case ABS_MT_TRACKING_ID:
        touch_mt = true;
        touch_tracking_id(in->value);
        break;
    case ABS_MT_POSITION_X:
        if (touch_slot >= 0)
            slot_x[touch_slot] = (lv_coord_t)in->value;
        break;
    case ABS_MT_POSITION_Y:
        if (touch_slot >= 0)
            slot_y[touch_slot] = (lv_coord_t)in->value;
        break;
    default:
        break;
    }
}

static lv_coord_t touch_clamp(lv_coord_t v, lv_coord_t max)
{
    if (v < 0)
        return 0;
    return v >= max ? max - 1 : v;
}

void custom_touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    lv_disp_t *disp = drv->disp != NULL ? drv->disp : lv_disp_get_default();
    struct input_event in;
    bool pressed;
    lv_coord_t x, y;

    if (touch_fd >= 0)
        while (read(touch_fd, &in, sizeof(in)) == (ssize_t)sizeof(in))
            touch_handle(&in);
    if (touch_mt) {
        pressed = touch_primary >= 0;
        if (pressed) {
            pen_x = slot_x[touch_primary];
            pen_y = slot_y[touch_primary];
        }
        x = pen_x;
        y = pen_y;
    } else {
        pressed = st_pressed;
        x = st_x;
        y = st_y;
    }
    data->point.x = touch_clamp(x, lv_disp_get_hor_res(disp));
    data->point.y = touch_clamp(y, lv_disp_get_ver_res(disp));
    data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
