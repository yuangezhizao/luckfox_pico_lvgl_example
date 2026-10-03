/* 画布缓冲区：不在栈上、可写满 w*h*sizeof(lv_color_t)、分配失败时不设缓冲也不崩溃（spec D10）。 */
#define _GNU_SOURCE
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "app_env.h"
#include "check.h"
#include "gui_guider.h"
#include "lvgl.h"

extern lv_obj_t *canva_obj;
extern bool tst_canvas_alloc_fail;
void Sketchpad_clear_btn_event_cb(lv_event_t *e);

static int on_stack(const void *p)
{
    pthread_attr_t attr;
    void *base = NULL;
    size_t size = 0;

    pthread_getattr_np(pthread_self(), &attr);
    pthread_attr_getstack(&attr, &base, &size);
    pthread_attr_destroy(&attr);   /* 不销毁时 glibc 为它分配的 cpuset 会被 LSan 报泄漏 */
    return (const char *)p >= (const char *)base && (const char *)p < (const char *)base + size;
}

static const lv_img_dsc_t *setup(void)
{
    tst_app_init(480, 0, 0);
    setup_scr_Sketchpad(&guider_ui);
    return lv_canvas_get_img(canva_obj);
}

static void buffer_not_on_stack(void)
{
    const lv_img_dsc_t *img = setup();

    CHECK(img->data != NULL);
    CHECK(!on_stack(img->data));
}

static void buffer_full_write(void)
{
    const lv_img_dsc_t *img = setup();

    CHECK_EQ_INT(img->header.w, 480);
    CHECK_EQ_INT(img->header.h, 360);
    memset((void *)img->data, 0x5a, (size_t)img->header.w * img->header.h * sizeof(lv_color_t));
}

/* 分配失败后渲染、画笔、清除都不碰空缓冲；画布为 0×0，先撑开对象尺寸让触摸落到画布上。 */
static void alloc_failure(void)
{
    alarm(5);
    const lv_img_dsc_t *img;
    lv_obj_t *btn;

    tst_canvas_alloc_fail = true;
    img = setup();
    CHECK(img->data == NULL);
    lv_scr_load(guider_ui.Sketchpad);
    tst_run_ms(100);
    lv_obj_set_size(canva_obj, 480, 360);
    tst_run_ms(20);
    tst_pointer_set(100, 100, 1);
    tst_run_ms(50);
    tst_pointer_set(120, 100, 1);
    tst_run_ms(50);
    tst_pointer_set(120, 100, 0);
    tst_run_ms(50);
    btn = lv_btn_create(lv_scr_act());
    lv_obj_add_event_cb(btn, Sketchpad_clear_btn_event_cb, LV_EVENT_ALL, NULL);
    lv_event_send(btn, LV_EVENT_CLICKED, NULL);
    lv_event_send(btn, LV_EVENT_RELEASED, NULL);
    tst_run_ms(50);
    CHECK(lv_canvas_get_img(canva_obj)->data == NULL);
}

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {
        {"buffer_not_on_stack", buffer_not_on_stack}, {"buffer_full_write", buffer_full_write}, {"alloc_failure", alloc_failure},
    };
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
