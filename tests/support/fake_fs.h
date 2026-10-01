/* 以本进程专属临时目录为根的假文件工具；custom_brightness.c 经 -include 本文件获得 tst_backlight_root() 原型（spec D6）。 */
#ifndef TST_FAKE_FS_H
#define TST_FAKE_FS_H

#include <stddef.h>

const char *tst_tmp_dir(void);
const char *tst_backlight_root(void);
void tst_fmt(char *buf, size_t size, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
void tst_path(char *buf, size_t size, const char *rel);
void tst_mkdirs(const char *rel);
void tst_write_file(const char *rel, const char *content);
long tst_read_long(const char *rel);

#endif
