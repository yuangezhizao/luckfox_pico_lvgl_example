#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "custom_fb.h"

#ifndef FB_CLEAR_MAX_BYTES
#define FB_CLEAR_MAX_BYTES (64ULL * 1024 * 1024)
#endif
/* 块必须远小于帧缓冲：内核 fb_write 在 count 大于帧缓冲总长时先写满再返回 EFBIG。 */
#define FB_CLEAR_CHUNK 4096

/* 循环写零直到帧缓冲写满（ENOSPC 或 write 返回 0）或达到总量上限；不带 O_CREAT。 */
int custom_fb_clear(const char *path)
{
    static const char zeros[FB_CLEAR_CHUNK];
    unsigned long long total = 0;
    int fd = open(path, O_WRONLY | O_CLOEXEC);

    if (fd < 0) {
        fprintf(stderr, "custom_fb_clear: open %s: %s\n", path, strerror(errno));
        return -1;
    }
    while (total < FB_CLEAR_MAX_BYTES) {
        ssize_t n = write(fd, zeros, sizeof(zeros));

        if (n > 0) {
            total += (unsigned long long)n;
            continue;
        }
        if (n == 0)
            break;
        if (errno == EINTR)
            continue;
        if (errno == ENOSPC)
            break;
        fprintf(stderr, "custom_fb_clear: write %s: %s\n", path, strerror(errno));
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}
