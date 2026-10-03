/* 笔迹落在 (屏幕点 - 画布左上角)，不落在屏幕坐标原值（spec D10）。 */
#include "app_env.h"
#include "check.h"
#include "gui_guider.h"
#include "lvgl.h"

extern lv_obj_t *canva_obj;

static int pen_near(lv_coord_t x, lv_coord_t y)
{
    lv_coord_t dx, dy;

    for (dy = -1; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++)
            if (lv_canvas_get_px(canva_obj, x + dx, y + dy).full == lv_color_black().full)
                return 1;
    return 0;
}

static void canvas_coordinates(void)
{
    lv_color_t bg = lv_palette_lighten(LV_PALETTE_GREY, 3);
    lv_area_t a;
    lv_coord_t sx, sy;

    tst_app_init(480, 0, 0);
    setup_scr_Sketchpad(&guider_ui);
    lv_scr_load(guider_ui.Sketchpad);
    tst_run_ms(100);
    lv_obj_get_coords(canva_obj, &a);
    printf("canvas x1=%d y1=%d\n", (int)a.x1, (int)a.y1);
    CHECK(a.x1 != 0 || a.y1 != 0);
    sx = a.x1 + 100;
    sy = a.y1 + 100;
    tst_pointer_set(sx, sy, 1);
    tst_run_ms(50);
    tst_pointer_set(sx + 10, sy, 1);
    tst_run_ms(50);
    tst_pointer_set(sx + 20, sy, 1);
    tst_run_ms(50);
    tst_pointer_set(sx + 20, sy, 0);
    tst_run_ms(50);
    CHECK(pen_near(110, 100));
    CHECK(lv_canvas_get_px(canva_obj, sx + 10, sy).full == bg.full);
}

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {{"canvas_coordinates", canvas_coordinates}};
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
