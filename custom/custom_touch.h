#ifndef CUSTOM_TOUCH_H
#define CUSTOM_TOUCH_H

#include "lvgl.h"

/* 打开触摸设备（O_NONBLOCK | O_CLOEXEC）；失败返回 -1，之后读到的始终是松开。 */
int custom_touch_init(const char *path);
/* LVGL 指针输入的 read_cb：只跟随第一根按下的手指。 */
void custom_touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data);

#endif
