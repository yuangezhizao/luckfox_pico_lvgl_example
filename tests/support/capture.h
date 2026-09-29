/* 把 stdout/stderr 捕获进临时文件；捕获期间 CHECK 的输出也会被吞掉，断言须在 tst_capture_end() 之后。 */
#ifndef TST_CAPTURE_H
#define TST_CAPTURE_H

void tst_capture_begin(void);
const char *tst_capture_end(void);
int tst_count_lines(const char *text, const char *needle);

#endif
