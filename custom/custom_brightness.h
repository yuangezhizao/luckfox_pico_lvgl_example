#ifndef CUSTOM_BRIGHTNESS_H
#define CUSTOM_BRIGHTNESS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/* 在 parent 上创建亮度文字与滑条；找不到可用的背光设备时不创建任何控件。 */
void brightness_ui_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif

#endif /* CUSTOM_BRIGHTNESS_H */
