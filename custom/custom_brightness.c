#include "custom_brightness.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* custom.h 引入了 <linux/fcntl.h>，与 <fcntl.h> 同时包含会重复定义 struct flock，因此 sysfs 用 stdio 读写。 */
#include "custom.h"

#ifndef BACKLIGHT_SYSFS_DIR
#define BACKLIGHT_SYSFS_DIR "/sys/class/backlight"
#endif

/* Luckfox Pico Ultra W 实测背光低于 raw 25（满量程 255）即看不清；10% 写入 26，是最低的可见档位。 */
#define BRIGHTNESS_MIN_PCT      10
#define BRIGHTNESS_MAX_PCT      100
#define BRIGHTNESS_ACCENT_COLOR 0xff9600
#define BRIGHTNESS_TRACK_COLOR  0xe0e0e0
#define BRIGHTNESS_TEXT_COLOR   0x5a5959

/* 真实背光的 max_brightness 远小于此值，超过即视为异常；目标板 long 为 32 位，换算中间值一律用 long long。 */
#define BRIGHTNESS_MAX_RAW      (INT_MAX / 100)

enum {
    BACKLIGHT_OK = 0,
    BACKLIGHT_NO_DEVICE,
    BACKLIGHT_BAD_MAX,
};

static char backlight_dir[256];
static long backlight_max;
static long backlight_last_written = -1;
static bool backlight_write_error_reported;

static int backlight_attr_path(char *buf, size_t size, const char *attr)
{
    int n = snprintf(buf, size, "%s/%s", backlight_dir, attr);

    return (n < 0 || (size_t)n >= size) ? -1 : 0;
}

static int backlight_read_attr(const char *attr, long *value)
{
    char path[320];
    char buf[32];
    char *end;
    long parsed;
    FILE *fp;

    if (backlight_attr_path(path, sizeof(path), attr) != 0)
        return -1;

    fp = fopen(path, "r");
    if (fp == NULL)
        return -1;
    if (fgets(buf, sizeof(buf), fp) == NULL) {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    errno = 0;
    parsed = strtol(buf, &end, 10);
    if (errno != 0 || end == buf || (*end != '\0' && *end != '\n'))
        return -1;

    *value = parsed;
    return 0;
}

static int backlight_open(void)
{
    struct dirent *entry;
    DIR *dp;
    int n = -1;

    dp = opendir(BACKLIGHT_SYSFS_DIR);
    if (dp == NULL)
        return BACKLIGHT_NO_DEVICE;

    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;
        n = snprintf(backlight_dir, sizeof(backlight_dir), "%s/%s", BACKLIGHT_SYSFS_DIR, entry->d_name);
        break;
    }
    closedir(dp);

    if (n < 0 || (size_t)n >= sizeof(backlight_dir))
        return BACKLIGHT_NO_DEVICE;
    if (backlight_read_attr("max_brightness", &backlight_max) != 0 || backlight_max <= 0 || backlight_max > BRIGHTNESS_MAX_RAW)
        return BACKLIGHT_BAD_MAX;

    return BACKLIGHT_OK;
}

static int backlight_read_percent(void)
{
    long raw;
    long pct;

    if (backlight_read_attr("brightness", &raw) != 0 || raw < 0)
        return BRIGHTNESS_MAX_PCT;
    if (raw > backlight_max)
        raw = backlight_max;

    backlight_last_written = raw;
    pct = (long)(((long long)raw * 100 + backlight_max / 2) / backlight_max);
    if (pct < BRIGHTNESS_MIN_PCT)
        pct = BRIGHTNESS_MIN_PCT;
    if (pct > BRIGHTNESS_MAX_PCT)
        pct = BRIGHTNESS_MAX_PCT;
    return (int)pct;
}

static void backlight_write_percent(int pct)
{
    char path[320];
    long raw = (long)(((long long)pct * backlight_max + 50) / 100);
    bool ok = false;
    FILE *fp;

    /* max_brightness 很小（下限 10% 时为 <= 4）时换算会舍入为 0，而写 0 会让背光全灭，绝不能写入。 */
    if (raw < 1)
        raw = 1;
    if (raw == backlight_last_written)
        return;
    if (backlight_attr_path(path, sizeof(path), "brightness") != 0)
        return;

    /* stdio 刷新缓冲时才真正写入 sysfs，因此以 fclose() 的结果判定是否成功。 */
    fp = fopen(path, "w");
    if (fp != NULL) {
        ok = fprintf(fp, "%ld", raw) > 0;
        ok = (fclose(fp) == 0) && ok;
    }

    if (ok) {
        backlight_last_written = raw;
    } else if (!backlight_write_error_reported) {
        perror("brightness: write backlight failed");
        backlight_write_error_reported = true;
    }
}

static void brightness_slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    lv_obj_t *value_label = lv_event_get_user_data(e);
    int pct = (int)lv_slider_get_value(slider);

    backlight_write_percent(pct);
    lv_label_set_text_fmt(value_label, "%d%%", pct);
}

static void brightness_style_label(lv_obj_t *label)
{
    lv_obj_set_style_text_color(label, lv_color_hex(BRIGHTNESS_TEXT_COLOR), LV_PART_MAIN|LV_STATE_DEFAULT);
    luckfox_lv_obj_set_style_text_font(label, &lv_font_montserratMedium_16, LV_PART_MAIN|LV_STATE_DEFAULT);
}

void brightness_ui_create(lv_obj_t *parent)
{
    lv_obj_t *title_label;
    lv_obj_t *value_label;
    lv_obj_t *slider;
    int pct;

    switch (backlight_open()) {
    case BACKLIGHT_OK:
        printf("brightness: using %s (max %ld)\n", backlight_dir, backlight_max);
        break;
    case BACKLIGHT_BAD_MAX:
        printf("brightness: invalid max_brightness in %s, control hidden\n", backlight_dir);
        return;
    default:
        printf("brightness: no backlight device under %s, control hidden\n", BACKLIGHT_SYSFS_DIR);
        return;
    }
    pct = backlight_read_percent();

    title_label = lv_label_create(parent);
    lv_label_set_text(title_label, "Brightness");
    luckfox_lv_obj_set_pos(title_label, 60, 245);
    brightness_style_label(title_label);

    value_label = lv_label_create(parent);
    lv_label_set_text_fmt(value_label, "%d%%", pct);
    luckfox_lv_obj_set_pos(value_label, 360, 245);
    luckfox_lv_obj_set_width(value_label, 60);
    lv_obj_set_style_text_align(value_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN|LV_STATE_DEFAULT);
    brightness_style_label(value_label);

    slider = lv_slider_create(parent);
    lv_slider_set_range(slider, BRIGHTNESS_MIN_PCT, BRIGHTNESS_MAX_PCT);
    lv_slider_set_value(slider, pct, LV_ANIM_OFF);
    luckfox_lv_obj_set_pos(slider, 60, 280);
    luckfox_lv_obj_set_size(slider, 360, 20);

    lv_obj_set_style_bg_opa(slider, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(slider, lv_color_hex(BRIGHTNESS_TRACK_COLOR), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_MAIN|LV_STATE_DEFAULT);

    lv_obj_set_style_bg_opa(slider, 255, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(slider, lv_color_hex(BRIGHTNESS_ACCENT_COLOR), LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_INDICATOR|LV_STATE_DEFAULT);

    lv_obj_set_style_bg_opa(slider, 255, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0xffffff), LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(slider, lv_color_hex(BRIGHTNESS_ACCENT_COLOR), LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(slider, 3, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(slider, 6, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB|LV_STATE_DEFAULT);

    lv_obj_add_event_cb(slider, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, value_label);

    /* 先创建的同级对象（如「Can't find music!」弹窗）必须绘制在这些控件之上并优先接收触摸。 */
    lv_obj_move_background(slider);
    lv_obj_move_background(value_label);
    lv_obj_move_background(title_label);
}
