/* 退出清屏接口，不依赖 GUI 与硬件驱动头文件。 */
#ifndef CUSTOM_FB_H
#define CUSTOM_FB_H

#ifdef __cplusplus
extern "C" {
#endif

int custom_fb_clear(const char *path);

#ifdef __cplusplus
}
#endif
#endif
