/* 字体按屏幕缩放：720 屏没有 27 号字，18 号映射到 24 号；480 屏不变（spec Q45、N10）。 */
#include "app_env.h"
#include "check.h"
#include "custom.h"
#include "gui_guider.h"
#include "lvgl.h"

static const lv_font_t *style_font(lv_style_t *style)
{
    lv_style_value_t v;

    if (lv_style_get_prop(style, LV_STYLE_TEXT_FONT, &v) != LV_RES_OK)
        return NULL;
    return v.ptr;
}

static void check_scaled(int res, const lv_font_t *from, const lv_font_t *want)
{
    lv_obj_t *obj;
    lv_style_t style;

    tst_app_init(res, 0, 0);
    obj = lv_obj_create(lv_scr_act());
    luckfox_lv_obj_set_style_text_font(obj, from, LV_PART_SELECTED);
    CHECK(lv_obj_get_style_text_font(obj, LV_PART_SELECTED) == want);
    lv_style_init(&style);
    luckfox_lv_style_set_text_font(&style, from);
    CHECK(style_font(&style) == want);
}

/* 音乐列表选中项用 18 号：720 屏换算为 27，没有对应字号时原先不设置字体，选中项退回普通项的字体。 */
static void scale_720_18(void)
{
    check_scaled(720, &lv_font_montserratMedium_18, &lv_font_montserratMedium_24);
}

static void scale_480_18(void)
{
    check_scaled(480, &lv_font_montserratMedium_18, &lv_font_montserratMedium_18);
}

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {{"scale_720_18", scale_720_18}, {"scale_480_18", scale_480_18}};
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
