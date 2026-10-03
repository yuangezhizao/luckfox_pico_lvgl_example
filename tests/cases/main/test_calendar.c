/* Main_datetext_1_init_calendar()：不改写日期标签、高亮数组在函数返回后仍有效、畸形日期不建日历（spec §5.1 ⑭）。 */
#include <string.h>

#include "app_env.h"
#include "check.h"
#include "gui_guider.h"
#include "lvgl.h"

void Main_datetext_1_init_calendar(lv_obj_t *obj, char *s);

static lv_obj_t *date_label(const char *text)
{
    tst_app_init(480, 0, 0);
    tst_app_setup_ui();
    lv_label_set_text(guider_ui.Main_datetext_1, text);
    return guider_ui.Main_datetext_1;
}

static void highlight_survives(void)
{
    lv_obj_t *label = date_label("2023/07/15");
    lv_obj_t *cal;
    const lv_calendar_date_t *hl;

    Main_datetext_1_init_calendar(label, lv_label_get_text(label));
    cal = lv_obj_get_child(lv_layer_top(), 0);
    CHECK(cal != NULL);
    if (cal == NULL)
        return;
    CHECK(strcmp(lv_label_get_text(label), "2023/07/15") == 0);
    fflush(stdout); /* ASan 终止进程前保留标签断言输出。 */
    /* 旧实现的高亮数组是已返回函数的局部数组：detect_stack_use_after_return 打开时它位于 ASan 假栈，返回后读取即报错。 */
    hl = lv_calendar_get_highlighted_dates(cal);
    CHECK(hl != NULL);
    if (hl != NULL) {
        CHECK_EQ_INT(hl[0].year, 2023);
        CHECK_EQ_INT(hl[0].month, 7);
        CHECK_EQ_INT(hl[0].day, 15);
    }
}

static void malformed(const char *text)
{
    lv_obj_t *label = date_label(text);

    Main_datetext_1_init_calendar(label, lv_label_get_text(label));
    CHECK_EQ_INT(lv_obj_get_child_cnt(lv_layer_top()), 0);
    CHECK(!lv_obj_has_flag(lv_layer_top(), LV_OBJ_FLAG_CLICKABLE));
}

static void no_slash(void) { malformed("abc"); }
static void one_slash(void) { malformed("2023/07"); }

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {
        {"highlight_survives", highlight_survives}, {"no_slash", no_slash}, {"one_slash", one_slash},
    };
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
