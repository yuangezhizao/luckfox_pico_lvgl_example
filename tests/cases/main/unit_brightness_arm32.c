/* 经 #include 访问 custom_brightness.c 的 static 函数；本文件以 ARM 交叉 gcc、-w 编译（spec D4）。 */
extern char tst_arm32_root[];
#define BACKLIGHT_SYSFS_DIR tst_arm32_root
#include "custom_brightness.c"

int tst_bl_open(void)
{
    return backlight_open();
}

int tst_bl_read_percent(void)
{
    return backlight_read_percent();
}

void tst_bl_write_percent(int pct)
{
    backlight_write_percent(pct);
}

void tst_bl_reset(void)
{
    backlight_last_written = -1;
    backlight_write_error_reported = false;
}
