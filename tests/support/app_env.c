#include "app_env.h"

#include <stdio.h>
#include <unistd.h>

#include "lvgl.h"
#include "gui_guider.h"
#include "events_init.h"

lv_ui guider_ui;
int QUIT_FLAG;
float SCALE;
int WIFI_ENABLE;
int MUSIC_ENABLE;

static int cur_res = TST_DESIGN_RES;
static uint32_t fb_mem[TST_RES_MAX * TST_RES_MAX];
static lv_color_t draw_mem[TST_RES_MAX * TST_RES_MAX];
static int ptr_x;
static int ptr_y;
static int ptr_pressed;
static void (*present_hook)(const uint32_t *fb, int res);

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *px)
{
    int x;
    int y;

    for (y = area->y1; y <= area->y2; y++)
        for (x = area->x1; x <= area->x2; x++)
            fb_mem[y * cur_res + x] = (px++)->full;
    if (present_hook != NULL && lv_disp_flush_is_last(drv))
        present_hook(fb_mem, cur_res);
    lv_disp_flush_ready(drv);
}

static void read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;
    data->point.x = ptr_x;
    data->point.y = ptr_y;
    data->state = ptr_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static int scaled(int v)
{
    return (int)(v * SCALE);
}

void tst_app_set_present_hook(void (*hook)(const uint32_t *fb, int res))
{
    present_hook = hook;
}

void tst_app_init(int res, int wifi_enable, int music_enable)
{
    static lv_disp_draw_buf_t draw_buf;
    static lv_disp_drv_t disp_drv;
    static lv_indev_drv_t indev_drv;

    cur_res = (res > 0 && res <= TST_RES_MAX) ? res : TST_DESIGN_RES;
    SCALE = (float)cur_res / TST_DESIGN_RES;
    QUIT_FLAG = 0;
    WIFI_ENABLE = wifi_enable;
    MUSIC_ENABLE = music_enable;

    lv_init();
    lv_disp_draw_buf_init(&draw_buf, draw_mem, NULL, cur_res * cur_res);
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &draw_buf;
    disp_drv.flush_cb = flush_cb;
    disp_drv.hor_res = cur_res;
    disp_drv.ver_res = cur_res;
    disp_drv.full_refresh = 1;
    lv_disp_drv_register(&disp_drv);

    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = read_cb;
    lv_indev_drv_register(&indev_drv);
}

void tst_app_setup_ui(void)
{
    setup_ui(&guider_ui);
    events_init(&guider_ui);
    /* 性能监视器在启动 300 ms 后还要一次重绘才显示数值，主屏时钟 1 s 刷新提供这次重绘。 */
    tst_run_ms(1500);
}

void tst_run_ms(int ms)
{
    int t;

    for (t = 0; t < ms; t += 5) {
        lv_timer_handler();
        usleep(5000);
    }
}

void tst_pointer_set(int x, int y, int pressed)
{
    ptr_x = x;
    ptr_y = y;
    ptr_pressed = pressed;
}

void tst_tap(int x, int y)
{
    tst_pointer_set(scaled(x), scaled(y), 1);
    tst_run_ms(100);
    tst_pointer_set(scaled(x), scaled(y), 0);
    tst_run_ms(300);
}

void tst_drag(int x0, int x1, int y)
{
    int i;

    tst_pointer_set(scaled(x0), scaled(y), 1);
    tst_run_ms(100);
    for (i = 1; i <= 20; i++) {
        tst_pointer_set(scaled(x0 + (x1 - x0) * i / 20), scaled(y), 1);
        tst_run_ms(20);
    }
    tst_pointer_set(scaled(x1), scaled(y), 0);
    tst_run_ms(100);
}

int tst_save_ppm(const char *path)
{
    FILE *fp = fopen(path, "wb");
    int i;

    if (fp == NULL) {
        perror(path);
        return -1;
    }
    fprintf(fp, "P6\n%d %d\n255\n", cur_res, cur_res);
    for (i = 0; i < cur_res * cur_res; i++) {
        unsigned char rgb[3] = { (fb_mem[i] >> 16) & 0xff, (fb_mem[i] >> 8) & 0xff, fb_mem[i] & 0xff };

        fwrite(rgb, 1, sizeof(rgb), fp);
    }
    return fclose(fp) == 0 ? 0 : -1;
}
