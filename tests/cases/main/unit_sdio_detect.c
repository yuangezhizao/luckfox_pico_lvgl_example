/* 经 #include 访问 custom_main.c 的 static 函数；本文件随被测源码以 -w 编译（spec D4）。 */
#include "custom_main.c"

int tst_sdio_has_id(const char *bus_dir)
{
    return luckfox_sdio_has_id(bus_dir, LUCKFOX_ULTRA_W_SDIO_ID);
}
