/* 32 位 long 上亮度换算不溢出（目标板 long 为 32 位，主机为 64 位测不出）；以 qemu-arm 运行。 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "check.h"

char tst_arm32_root[256];

int tst_bl_open(void);
int tst_bl_read_percent(void);
void tst_bl_write_percent(int pct);
void tst_bl_reset(void);

static void attr_path(char *buf, size_t size, const char *attr)
{
    int n = snprintf(buf, size, "%s/backlight/%s", tst_arm32_root, attr);

    if (n < 0 || (size_t)n >= size)
        exit(2);
}

static void put(const char *attr, const char *value)
{
    char path[320];
    FILE *fp;

    attr_path(path, sizeof(path), attr);
    fp = fopen(path, "w");
    if (fp == NULL || fputs(value, fp) == EOF || fclose(fp) != 0)
        exit(2);
}

static long get_brightness(void)
{
    char path[320];
    long value = -1;
    FILE *fp;

    attr_path(path, sizeof(path), "brightness");
    fp = fopen(path, "r");
    if (fp != NULL) {
        if (fscanf(fp, "%ld", &value) != 1)
            value = -1;
        fclose(fp);
    }
    return value;
}

int main(void)
{
    char base[] = "/tmp/luckfox-arm32-XXXXXX";
    char path[320];
    int rc;

    if (mkdtemp(base) == NULL)
        return 2;
    if (snprintf(tst_arm32_root, sizeof(tst_arm32_root), "%s/class", base) >= (int)sizeof(tst_arm32_root))
        return 2;
    mkdir(tst_arm32_root, 0755);
    attr_path(path, sizeof(path), "");
    mkdir(path, 0755);

    CHECK_EQ_INT(sizeof(long), 4);

    put("max_brightness", "21474836\n");
    put("brightness", "21474836\n");
    tst_bl_reset();
    CHECK_EQ_INT(tst_bl_open(), 0);
    CHECK_EQ_INT(tst_bl_read_percent(), 100);
    put("brightness", "0\n");
    tst_bl_reset();
    tst_bl_read_percent();
    tst_bl_write_percent(100);
    CHECK_EQ_INT(get_brightness(), 21474836);

    put("max_brightness", "255\n");
    put("brightness", "204\n");
    tst_bl_reset();
    CHECK_EQ_INT(tst_bl_open(), 0);
    CHECK_EQ_INT(tst_bl_read_percent(), 80);
    tst_bl_write_percent(10);
    CHECK_EQ_INT(get_brightness(), 26);

    rc = tst_finish();
    attr_path(path, sizeof(path), "max_brightness");
    unlink(path);
    attr_path(path, sizeof(path), "brightness");
    unlink(path);
    attr_path(path, sizeof(path), "");
    rmdir(path);
    rmdir(tst_arm32_root);
    rmdir(base);
    return rc;
}
