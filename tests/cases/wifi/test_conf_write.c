/* _wifi_conf_load() 写配置：临时文件同目录、不叫 <conf>.tmp、保留原模式、替换失败时原配置不丢（spec D14）。 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "app_env.h"
#include "check.h"
#include "gui_guider.h"
#include "wifi_env.h"

void tst_wifi_conf_load(const char *ssid, const char *psk);

static char g_from[1024], g_to[1024];
static int g_fail;

int rename(const char *from, const char *to)
{
    snprintf(g_from, sizeof(g_from), "%s", from);
    snprintf(g_to, sizeof(g_to), "%s", to);
    if (g_fail) {
        errno = EIO;
        return -1;
    }
    return renameat(AT_FDCWD, from, AT_FDCWD, to);
}

/* 只由 UNIT 包装调用，测试环境本身继续使用真实 libc。 */
enum { IO_NONE, IO_STAT, IO_MODE, IO_READ, IO_WRITE, IO_FLUSH, IO_SYNC, IO_CLOSE_IN, IO_CLOSE_OUT };
static int g_io_fail, g_errors, g_closes;

int tst_conf_fstat(int fd, struct stat *st)
{
    if (g_io_fail == IO_STAT) { errno = EIO; return -1; }
    return fstat(fd, st);
}

int tst_conf_fchmod(int fd, mode_t mode)
{
    if (g_io_fail == IO_MODE) { errno = EIO; return -1; }
    return fchmod(fd, mode);
}

int tst_conf_ferror(FILE *stream)
{
    ++g_errors;
    if ((g_io_fail == IO_READ && g_errors == 1) || (g_io_fail == IO_WRITE && g_errors == 2))
        return 1;
    return ferror(stream);
}

int tst_conf_fflush(FILE *stream)
{
    if (g_io_fail == IO_FLUSH) { errno = EIO; return EOF; }
    return fflush(stream);
}

int tst_conf_fsync(int fd)
{
    if (g_io_fail == IO_SYNC) { errno = EIO; return -1; }
    return fsync(fd);
}

int tst_conf_fclose(FILE *stream)
{
    int rc = fclose(stream);
    ++g_closes;
    if ((g_io_fail == IO_CLOSE_IN && g_closes == 1) || (g_io_fail == IO_CLOSE_OUT && g_closes == 2)) {
        errno = EIO;
        return EOF;
    }
    return rc;
}

static void load_screen(const char *conf)
{
    wifi_env_setup(conf);
    tst_app_init(480, 1, 0);
    setup_scr_WIFI(&guider_ui);
}

static void same_dir_atomic(void)
{
    char a[1024], b[1024];
    struct stat st;
    char *conf;

    load_screen(WIFI_ENV_CONF);
    CHECK(chmod(tst_wpa_conf_path(), 0600) == 0);
    tst_wifi_conf_load("newnet", "newpass12");
    snprintf(a, sizeof(a), "%s", g_from);
    snprintf(b, sizeof(b), "%s", g_to);
    CHECK(strcmp(dirname(a), dirname(b)) == 0);
    snprintf(a, sizeof(a), "%s", g_from);
    CHECK(strcmp(basename(a), "wpa_supplicant.conf.tmp") != 0);
    CHECK(stat(tst_wpa_conf_path(), &st) == 0 && (st.st_mode & 07777) == 0600);
    conf = wifi_env_read("wpa_supplicant.conf");
    CHECK(strstr(conf, "ssid=\"newnet\"") != NULL);
    free(conf);
    g_fail = 1;
    tst_wifi_conf_load("other", "otherpass1");
    conf = wifi_env_read("wpa_supplicant.conf");
    CHECK(strstr(conf, "ssid=\"newnet\"") != NULL && strstr(conf, "psk=\"newpass12\"") != NULL);
    free(conf);
}

/* 任一读写或权限设置失败均不能替换原配置，也不能遗留临时文件。 */
static void io_failures(void)
{
    char temp[1024];
    load_screen(WIFI_ENV_CONF);
    CHECK(chmod(tst_wpa_conf_path(), 0600) == 0);
    snprintf(temp, sizeof(temp), "%s.luckfox.new", tst_wpa_conf_path());
    for (int kind = IO_STAT; kind <= IO_CLOSE_OUT; ++kind) {
        char *conf;
        printf("注入错误 %d\n", kind);
        g_io_fail = kind;
        g_errors = g_closes = 0;
        g_from[0] = '\0';
        tst_wifi_conf_load("other", "otherpass1");
        conf = wifi_env_read("wpa_supplicant.conf");
        CHECK(strcmp(conf, WIFI_ENV_CONF) == 0);
        CHECK(g_from[0] == '\0');
        CHECK(access(temp, F_OK) == -1 && errno == ENOENT);
        free(conf);
    }
}

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {{"same_dir_atomic", same_dir_atomic}, {"io_failures", io_failures}};
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
