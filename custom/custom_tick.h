#ifndef CUSTOM_TICK_H
#define CUSTOM_TICK_H

#include <stdint.h>

/* LVGL 的 tick 源，由 lib/lv_conf.h 的 LV_TICK_CUSTOM_INCLUDE 引入。 */
uint32_t custom_tick_get(void);

#endif
