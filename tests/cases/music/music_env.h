/* 音乐页用例公共环境：fd_mpv 换成 socketpair，音乐文件建在 tst_music_dir()。 */
#ifndef MUSIC_ENV_H
#define MUSIC_ENV_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "fake_fs.h"

extern int32_t fd_mpv;

/* 返回对端；扫描与播放控制命令都写进 fd_mpv，不能落到 stdin。 */
static inline int music_env_socketpair(void)
{
    int sv[2];

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        perror("socketpair");
        abort();
    }
    fd_mpv = sv[0];
    return sv[1];
}

static inline void music_env_file(const char *name)
{
    char rel[512];

    tst_mkdirs("music");
    tst_fmt(rel, sizeof(rel), "music/%s", name);
    tst_write_file(rel, "");
}

#endif
