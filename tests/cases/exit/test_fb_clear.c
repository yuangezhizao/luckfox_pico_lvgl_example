/* custom_fb_clear()：写到 ENOSPC 静默返回 0，达到总量上限即返回，打不开返回 -1 并打印一行（spec D9）。 */
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

#include "capture.h"
#include "check.h"
#include "fake_fs.h"

int custom_fb_clear(const char *path);

/* 链接期只包装本可执行文件的 write；未启用注入时透传，捕获输出仍使用 libc。 */
static int inject_write, write_calls;
ssize_t __real_write(int fd, const void *buf, size_t count);
ssize_t __wrap_write(int fd, const void *buf, size_t count);
ssize_t __wrap_write(int fd, const void *buf, size_t count)
{
    if (!inject_write)
        return __real_write(fd, buf, count);
    ++write_calls;
    if (write_calls == 1) {
        errno = inject_write == 1 ? EIO : EINTR;
        return inject_write == 1 ? 0 : -1;
    }
    return __real_write(fd, buf, count);
}

static void zero_write_silent(void)
{
    const char *out;
    int rc;

    tst_capture_begin();
    inject_write = 1;
    rc = custom_fb_clear("/dev/full");
    inject_write = 0;
    out = tst_capture_end();
    CHECK_EQ_INT(rc, 0);
    CHECK_EQ_INT(write_calls, 1);
    CHECK_EQ_INT(strlen(out), 0);
}

static void eintr_retries(void)
{
    char path[1024];
    struct stat st;
    const char *out;
    int rc;

    tst_fb_clear_max_bytes = 4096;
    tst_write_file("fb", "");
    tst_path(path, sizeof(path), "fb");
    tst_capture_begin();
    inject_write = 2;
    rc = custom_fb_clear(path);
    inject_write = 0;
    out = tst_capture_end();
    CHECK_EQ_INT(rc, 0);
    CHECK_EQ_INT(write_calls, 2);
    CHECK_EQ_INT(strlen(out), 0);
    CHECK(stat(path, &st) == 0 && st.st_size == 4096);
}

static void enospc_silent(void)
{
    const char *out;
    int rc;

    tst_capture_begin();
    rc = custom_fb_clear("/dev/full");
    out = tst_capture_end();
    CHECK_EQ_INT(rc, 0);
    CHECK_EQ_INT(strlen(out), 0);
}

static void cap(void)
{
    char path[1024];
    struct stat st;

    tst_fb_clear_max_bytes = 1ULL << 20;
    tst_write_file("fb", "");
    tst_path(path, sizeof(path), "fb");
    CHECK_EQ_INT(custom_fb_clear(path), 0);
    CHECK(stat(path, &st) == 0 && st.st_size == (off_t)(1 << 20));
}

static void open_failure(void)
{
    char path[1024];
    const char *out;
    int rc;

    tst_path(path, sizeof(path), "no-such-fb");
    tst_capture_begin();
    rc = custom_fb_clear(path);
    out = tst_capture_end();
    CHECK_EQ_INT(rc, -1);
    CHECK_EQ_INT(tst_count_lines(out, "custom_fb_clear"), 1);
}

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {{"zero_write_silent", zero_write_silent}, {"eintr_retries", eintr_retries}, {"enospc_silent", enospc_silent}, {"cap", cap}, {"open_failure", open_failure}};
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
