#include "fake_fs.h"
#define WPA_FILE_PATH tst_wpa_conf_path()
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
int tst_conf_fstat(int fd, struct stat *st);
int tst_conf_fchmod(int fd, mode_t mode);
int tst_conf_fflush(FILE *stream);
int tst_conf_fsync(int fd);
int tst_conf_fclose(FILE *stream);
int tst_conf_ferror(FILE *stream);
#define fstat tst_conf_fstat
#define fchmod tst_conf_fchmod
#define fflush tst_conf_fflush
#define fsync tst_conf_fsync
#define fclose tst_conf_fclose
#define ferror tst_conf_ferror
#include "custom_wifi.c"

void tst_wifi_conf_load(const char *ssid, const char *psk)
{
    _wifi_conf_load(ssid, psk);
}

void tst_wifi_conf_get(char ssid[128], char psk[128])
{
    _wifi_conf_get(ssid, psk);
}
