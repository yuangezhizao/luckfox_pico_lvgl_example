/* mpv 监听线程：解析后释放 cJSON；读满缓冲区时补 NUL（PR #8 spec §5.4 ⑤⑥）；跨两次 read 的行拼接后解析、不用 strtok，只在 UI 线程设置控件（spec §5.1 F5、R1）。 */
#include <errno.h>
#include <pthread.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "app_env.h"
#include "check.h"
#include "gui_guider.h"
#include "lvgl.h"
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

static void full_buffer_line(void)
{
    char data[600];

    memset(data, 'a', sizeof(data));
    feed_and_drain(data, sizeof(data));
}

/* 监听线程与 UI 线程（日历）共用 strtok 的静态游标会互相打乱；统计非主线程的调用。 */
static pthread_t main_thread;
static volatile int strtok_in_thread;
char *__real_strtok(char *str, const char *delim);
char *__wrap_strtok(char *str, const char *delim);
char *__wrap_strtok(char *str, const char *delim)
{
    if (!pthread_equal(pthread_self(), main_thread))
        strtok_in_thread++;
    return __real_strtok(str, delim);
}

/* 音乐页 + 监听线程；返回对端。 */
static int start_page_and_monitor(void)
{
    pthread_t th;
    int peer = music_env_socketpair();

    main_thread = pthread_self();
    music_env_file("a.mp3");
    tst_app_init(480, 0, 1);
    setup_scr_Music_player(&guider_ui);
    CHECK(pthread_create(&th, NULL, get_music_playback_time, NULL) == 0);
    usleep(20000);
    return peer;
}

static void write_all(int fd, const char *s)
{
    CHECK_EQ_INT(write(fd, s, strlen(s)), (long)strlen(s));
}

static void wait_drained(void)
{
    int i;

    for (i = 0; i < 2000 && fionread(fd_mpv) != 0; i++)
        usleep(1000);
    usleep(100000);
}

#define EV_DURATION "{\"event\":\"property-change\",\"id\":2,\"name\":\"duration\",\"data\":200.0}\n"
#define EV_TIME_A   "{\"event\":\"property-change\",\"id\":1,\"name\":\"playback"
#define EV_TIME_B   "-time\",\"data\":12.5}\n"

/* mpv 一次输出多条事件，一行可能被两次 read 拆开；旧实现两半都解析失败，进度事件丢失。 */
static void split_line(void)
{
    int peer = start_page_and_monitor();

    write_all(peer, EV_DURATION EV_TIME_A);
    usleep(50000);
    write_all(peer, EV_TIME_B);
    wait_drained();
    tst_run_ms(300);
    CHECK_EQ_INT(lv_bar_get_max_value(guider_ui.Music_player_progress_slider), 200);
    CHECK_EQ_INT(lv_slider_get_value(guider_ui.Music_player_progress_slider), 12);
}

static void no_strtok(void)
{
    int peer = start_page_and_monitor();

    write_all(peer, EV_DURATION EV_TIME_A EV_TIME_B);
    wait_drained();
    printf("strtok calls in monitor thread: %d\n", strtok_in_thread);
    CHECK_EQ_INT(strtok_in_thread, 0);
}

/* LVGL 不是线程安全的：监听线程只能记录数值，由 UI 线程的定时器设置控件。没运行 LVGL 定时器之前控件不应变化。 */
static void ui_thread_only(void)
{
    int peer = start_page_and_monitor();

    write_all(peer, EV_DURATION EV_TIME_A EV_TIME_B);
    wait_drained();
    CHECK_EQ_INT(lv_bar_get_max_value(guider_ui.Music_player_progress_slider), 100);
    CHECK_EQ_INT(lv_slider_get_value(guider_ui.Music_player_progress_slider), 0);
    tst_run_ms(300);
    CHECK_EQ_INT(lv_bar_get_max_value(guider_ui.Music_player_progress_slider), 200);
    CHECK_EQ_INT(lv_slider_get_value(guider_ui.Music_player_progress_slider), 12);
}

/* 用户正在拖动进度滑块时，mpv 的进度事件不覆盖滑块值。 */
extern int slider_pressed;
static void pressed_keeps_value(void)
{
    int peer = start_page_and_monitor();

    slider_pressed = 1;
    write_all(peer, EV_DURATION EV_TIME_A EV_TIME_B);
    wait_drained();
    tst_run_ms(300);
    CHECK_EQ_INT(lv_bar_get_max_value(guider_ui.Music_player_progress_slider), 200);
    CHECK_EQ_INT(lv_slider_get_value(guider_ui.Music_player_progress_slider), 0);
}

/* mpv 退出（套接字对端关闭）后：监听线程回收子进程、关闭并置空 fd_mpv、退出，不留僵尸也不空转（R2）。 */
extern pid_t pid;
static void peer_exit_reaps(void)
{
    pthread_t th;
    pid_t child;
    int peer = music_env_socketpair();
    int i;

    child = fork();
    if (child == 0)
        _exit(0);
    CHECK(child > 0);
    pid = child;
    CHECK(pthread_create(&th, NULL, get_music_playback_time, NULL) == 0);
    usleep(50000);
    close(peer);
    for (i = 0; i < 200 && fd_mpv != -1; i++)
        usleep(10000);
    CHECK_EQ_INT(fd_mpv, -1);
    errno = 0;
    CHECK_EQ_INT(waitpid(child, NULL, WNOHANG), -1);
    CHECK_EQ_INT(errno, ECHILD);
}

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {{"no_leak", no_leak}, {"full_buffer_line", full_buffer_line},
                                       {"split_line", split_line}, {"no_strtok", no_strtok},
                                       {"ui_thread_only", ui_thread_only}, {"pressed_keeps_value", pressed_keeps_value},
                                       {"peer_exit_reaps", peer_exit_reaps}};
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
