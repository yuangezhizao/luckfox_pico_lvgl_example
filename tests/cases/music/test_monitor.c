/* mpv 监听线程：解析后释放 cJSON；读满缓冲区时补 NUL（spec §5.4 ⑤⑥）。 */
#include <pthread.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "check.h"
#include "music_env.h"

void *get_music_playback_time(void *arg);

static int fionread(int fd)
{
    int n = -1;

    return ioctl(fd, FIONREAD, &n) == 0 ? n : -1;
}

/* 线程 pthread_detach 且永不返回：轮询线程那一端读空后再等 100 ms，保证解析在进程退出（LSan 检查）之前完成。 */
static void feed_and_drain(const char *data, size_t len)
{
    pthread_t th;
    int peer = music_env_socketpair();
    int i;

    CHECK(pthread_create(&th, NULL, get_music_playback_time, NULL) == 0);
    usleep(20000);
    CHECK_EQ_INT(write(peer, data, len), (long)len);
    for (i = 0; i < 2000 && fionread(fd_mpv) != 0; i++)
        usleep(1000);
    usleep(100000);
    CHECK_EQ_INT(fionread(fd_mpv), 0);
}

/* 8 行事件一次写入（总长 < 512，避开 ⑥）；不带 id、data，不碰音乐屏控件；最后一行的对象仍被线程栈引用，所以至少 2 行。 */
static void no_leak(void)
{
    static const char line[] = "{\"event\":\"idle\"}\n";
    char data[8 * (sizeof(line) - 1)];
    int i;

    for (i = 0; i < 8; i++)
        memcpy(data + i * (sizeof(line) - 1), line, sizeof(line) - 1);
    feed_and_drain(data, sizeof(data));
}

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {{"no_leak", no_leak}};
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
