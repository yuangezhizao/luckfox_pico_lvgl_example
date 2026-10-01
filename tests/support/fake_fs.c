#define _XOPEN_SOURCE 700

#include "fake_fs.h"

#include <errno.h>
#include <ftw.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static char tmp_dir[256];
static char backlight_root[320];

static void die(const char *what)
{
    perror(what);
    exit(2);
}

static int remove_entry(const char *path, const struct stat *sb, int flag, struct FTW *ftwbuf)
{
    (void)sb;
    (void)flag;
    (void)ftwbuf;
    return remove(path);
}

static void remove_tmp_dir(void)
{
    nftw(tmp_dir, remove_entry, 16, FTW_DEPTH | FTW_PHYS);
}

void tst_fmt(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= size) {
        fprintf(stderr, "tst_fmt: output truncated (format \"%s\")\n", fmt);
        exit(2);
    }
}

const char *tst_tmp_dir(void)
{
    if (tmp_dir[0] == '\0') {
        const char *base = getenv("TMPDIR");

        tst_fmt(tmp_dir, sizeof(tmp_dir), "%s/luckfox-tests-XXXXXX", base != NULL && base[0] != '\0' ? base : "/tmp");
        if (mkdtemp(tmp_dir) == NULL)
            die("mkdtemp");
        atexit(remove_tmp_dir);
    }
    return tmp_dir;
}

const char *tst_backlight_root(void)
{
    if (backlight_root[0] == '\0')
        tst_path(backlight_root, sizeof(backlight_root), "sys/class/backlight");
    return backlight_root;
}

void tst_path(char *buf, size_t size, const char *rel)
{
    tst_fmt(buf, size, "%s/%s", tst_tmp_dir(), rel);
}

void tst_mkdirs(const char *rel)
{
    char path[512];
    char *p;

    tst_path(path, sizeof(path), rel);
    for (p = path + strlen(tst_tmp_dir()) + 1; ; p++) {
        if (*p == '/' || *p == '\0') {
            char saved = *p;

            *p = '\0';
            if (mkdir(path, 0755) != 0 && errno != EEXIST)
                die(path);
            if (saved == '\0')
                break;
            *p = saved;
        }
    }
}

void tst_write_file(const char *rel, const char *content)
{
    char path[512];
    FILE *fp;

    tst_path(path, sizeof(path), rel);
    fp = fopen(path, "w");
    if (fp == NULL)
        die(path);
    if (fputs(content, fp) == EOF || fclose(fp) != 0)
        die(path);
}

long tst_read_long(const char *rel)
{
    char path[512];
    long value = -1;
    FILE *fp;

    tst_path(path, sizeof(path), rel);
    fp = fopen(path, "r");
    if (fp == NULL)
        return -1;
    if (fscanf(fp, "%ld", &value) != 1)
        value = -1;
    fclose(fp);
    return value;
}
