/* music_player_thread_init()：mpv 启动、连接、超时与回收（spec D7）。 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <sys/un.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "check.h"
#include "music_env.h"

int music_player_thread_init(void);

/* PATH 只含本进程的 bin 目录：其中没有 mpv 时 execlp 必然失败。 */
static void path_only_bin(void)
{
    char bin[1024];

    tst_mkdirs("bin");
    tst_path(bin, sizeof(bin), "bin");
    setenv("PATH", bin, 1);
}

static void exec_failure(void)
{
    path_only_bin();
    CHECK_EQ_INT(music_player_thread_init(), -1);
}

/* 生产代码看到 2.5 倍速的单调时钟：5 秒预算仅需约 2 秒实时时间；测试计时仍用真实时钟。 */
int __real_clock_gettime(clockid_t clock_id, struct timespec *ts);
int __wrap_clock_gettime(clockid_t clock_id, struct timespec *ts);
int __wrap_clock_gettime(clockid_t clock_id, struct timespec *ts)
{
    int rc = __real_clock_gettime(clock_id, ts);
    if (rc == 0 && clock_id == CLOCK_MONOTONIC) {
        long long ns = ((long long)ts->tv_sec * 1000000000 + ts->tv_nsec) / 2 * 5;
        ts->tv_sec = (time_t)(ns / 1000000000);
        ts->tv_nsec = (long)(ns % 1000000000);
    }
    return rc;
}

static long long now_ms(void)
{
    struct timespec ts;

    __real_clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* bin/mpv 链接到假 mpv；PATH 只含 bin，真 mpv 不会被找到。 */
static void install_fake_mpv(void)
{
    char dst[1024], pidfile[1024];

    path_only_bin();
    tst_path(dst, sizeof(dst), "bin/mpv");
    CHECK(symlink(TST_MUSIC_SRC_DIR "/fake_mpv.py", dst) == 0);
    tst_path(pidfile, sizeof(pidfile), "mpv.pid");
    setenv("MPV_PIDFILE", pidfile, 1);
}

/* 单调时钟加速 2.5 倍，1300 毫秒实时时间对应 3250 毫秒生产时钟，低于 5 秒连接预算。 */
static void slow_socket(void)
{
    long long t0;
    int rc;

    install_fake_mpv();
    setenv("MPV_DELAY_MS", "1300", 1);
    t0 = now_ms();
    rc = music_player_thread_init();
    printf("elapsed_ms=%lld\n", now_ms() - t0);
    CHECK_EQ_INT(rc, 0);
    CHECK(fd_mpv >= 0);
}

/* 回收须在本进程退出前检查：退出后 PDEATHSIG 也会让 kill(pid, 0) 得到 ESRCH。 */
static void timeout_reaps_child(void)
{
    long long t0, elapsed;
    long pid;
    int rc;

    install_fake_mpv();
    setenv("MPV_NEVER", "1", 1);
    t0 = now_ms();
    rc = music_player_thread_init();
    elapsed = now_ms() - t0;
    pid = tst_read_long("mpv.pid");
    printf("elapsed_ms=%lld pid=%ld\n", elapsed, pid);
    CHECK_EQ_INT(rc, -1);
    /* 5000 / 2.5 = 2000 ms；下限 1900 ms 容纳 100 ms 计时误差，上限 2900 ms 留出 900 ms 调度与回收余量，仍低于 8000 / 2.5 = 3200 ms。 */
    CHECK(elapsed >= 1900 && elapsed <= 2900);
    errno = 0;
    CHECK(pid > 0 && kill((pid_t)pid, 0) == -1 && errno == ESRCH);
}

static void mpv_missing_fast(void)
{
    long long t0, elapsed;
    int rc;

    path_only_bin();
    t0 = now_ms();
    rc = music_player_thread_init();
    elapsed = now_ms() - t0;
    printf("elapsed_ms=%lld\n", elapsed);
    CHECK_EQ_INT(rc, -1);
    CHECK(elapsed < 500);
}

/* 旧监听器仍活着；新 mpv 尚未建立端点时不得误连旧实例。 */
static void stale_socket(void)
{
    struct sockaddr_un sa = {0};
    int old = socket(AF_UNIX, SOCK_STREAM, 0);
    int accepted;

    CHECK(old >= 0);
    sa.sun_family = AF_UNIX;
    snprintf(sa.sun_path, sizeof(sa.sun_path), "%s", tst_mpv_socket_path());
    CHECK(bind(old, (struct sockaddr *)&sa, sizeof(sa)) == 0);
    CHECK(listen(old, 1) == 0);
    CHECK(fcntl(old, F_SETFL, O_NONBLOCK) == 0);
    install_fake_mpv();
    setenv("MPV_BEFORE_UNLINK_MS", "500", 1);
    CHECK_EQ_INT(music_player_thread_init(), 0);
    errno = 0;
    accepted = accept(old, NULL, NULL);
    CHECK(accepted == -1 && (errno == EAGAIN || errno == EWOULDBLOCK));
    if (accepted >= 0)
        close(accepted);
    close(old);
}

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {{"exec_failure", exec_failure}, {"slow_socket", slow_socket}, {"timeout_reaps_child", timeout_reaps_child}, {"mpv_missing_fast", mpv_missing_fast}, {"stale_socket", stale_socket}};
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
