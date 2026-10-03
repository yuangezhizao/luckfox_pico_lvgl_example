#include "fake_fs.h"
#define WPA_FILE_PATH tst_wpa_conf_path()
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
/* 普通配置测试透传到 libc；conf_write 的强定义覆盖这些包装以注入 I/O 失败。 */
__attribute__((weak)) int tst_conf_fstat(int fd, struct stat *st) { return fstat(fd, st); }
__attribute__((weak)) int tst_conf_fchmod(int fd, mode_t mode) { return fchmod(fd, mode); }
__attribute__((weak)) int tst_conf_fflush(FILE *stream) { return fflush(stream); }
__attribute__((weak)) int tst_conf_fsync(int fd) { return fsync(fd); }
__attribute__((weak)) int tst_conf_fclose(FILE *stream) { return fclose(stream); }
__attribute__((weak)) int tst_conf_ferror(FILE *stream) { return ferror(stream); }
#define fstat tst_conf_fstat
#define fchmod tst_conf_fchmod
#define fflush tst_conf_fflush
#define fsync tst_conf_fsync
#define fclose tst_conf_fclose
#define ferror tst_conf_ferror
#include <stdarg.h>
/* 普通调用透传；扫描用例可模拟格式化输出超过剩余容量或编码失败。 */
__attribute__((weak)) int tst_wifi_snprintf(char *out, size_t size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(out, size, fmt, ap);
    va_end(ap);
    return n;
}
#define snprintf tst_wifi_snprintf
#include "custom_wifi.c"

void tst_wifi_conf_load(const char *ssid, const char *psk)
{
    _wifi_conf_load(ssid, psk);
}

void tst_wifi_conf_get(char ssid[128], char psk[128])
{
    _wifi_conf_get(ssid, 128, psk, 128);
}

int tst_wifi_scan(void) { return _wifi_scanning_ssid(); }

void tst_wifi_status_update(void) { _wifi_status_update(); }
