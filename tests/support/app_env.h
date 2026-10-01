/* 测试进程内的例程运行环境：src/main.c 的 5 个全局量、内存帧缓冲显示与脚本化触摸，不打开任何设备节点。 */
#ifndef TST_APP_ENV_H
#define TST_APP_ENV_H

#include <stdint.h>

#define TST_DESIGN_RES 480
#define TST_RES_MAX    720

void tst_app_init(int res, int wifi_enable, int music_enable);
void tst_app_setup_ui(void);
void tst_run_ms(int ms);
void tst_pointer_set(int x, int y, int pressed);
void tst_tap(int x, int y);
void tst_drag(int x0, int x1, int y);
int tst_save_ppm(const char *path);
void tst_app_set_present_hook(void (*hook)(const uint32_t *fb, int res));

#endif
