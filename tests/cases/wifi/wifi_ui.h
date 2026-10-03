/* 从页面中查找红色提示标签，测试不依赖生产代码的内部变量。 */
#ifndef TST_WIFI_UI_H
#define TST_WIFI_UI_H

#include "gui_guider.h"
#include "lvgl.h"

static inline lv_obj_t *wifi_test_hint(void)
{
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(guider_ui.WIFI); i++) {
        lv_obj_t *obj = lv_obj_get_child(guider_ui.WIFI, (int32_t)i);
        if (lv_obj_check_type(obj, &lv_label_class) &&
            lv_obj_get_style_text_color(obj, LV_PART_MAIN).full == lv_color_hex(0xff0000).full)
            return obj;
    }
    return NULL;
}

#endif
