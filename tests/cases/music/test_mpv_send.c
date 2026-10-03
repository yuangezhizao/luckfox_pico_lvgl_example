/* mpv 已退出（对端关闭）后按播放/暂停，进程存活（spec D6）。 */
#include <fcntl.h>
#include <unistd.h>

#include "app_env.h"
#include "check.h"
#include "lvgl.h"
#include "music_env.h"

void Music_player_start_btn_event_handler(lv_event_t *e);

static int send_calls;
ssize_t __real_send(int fd, const void *buf, size_t len, int flags);
ssize_t __wrap_send(int fd, const void *buf, size_t len, int flags);
ssize_t __wrap_send(int fd, const void *buf, size_t len, int flags)
{
    ++send_calls;
    return __real_send(fd, buf, len, flags);
}

/* 未连接时按播放/暂停不应调用 send，即使系统调用本身只返回 EBADF。 */
static void negative_fd_no_send(void)
{
    lv_obj_t *btn;

    tst_app_init(480, 0, 1);
    fd_mpv = -1;
    btn = lv_btn_create(lv_scr_act());
    lv_obj_add_event_cb(btn, Music_player_start_btn_event_handler, LV_EVENT_RELEASED, NULL);
    send_calls = 0;
    lv_event_send(btn, LV_EVENT_RELEASED, NULL);
    CHECK_EQ_INT(send_calls, 0);
    CHECK_EQ_INT(fd_mpv, -1);
}

static void peer_closed(void)
{
    lv_obj_t *btn;
    int peer = music_env_socketpair();

    tst_app_init(480, 0, 1);
    close(peer);
    btn = lv_btn_create(lv_scr_act());
    lv_obj_add_event_cb(btn, Music_player_start_btn_event_handler, LV_EVENT_RELEASED, NULL);
    lv_event_send(btn, LV_EVENT_RELEASED, NULL);
    CHECK(fcntl(fd_mpv, F_GETFD) != -1);
}

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {{"negative_fd_no_send", negative_fd_no_send}, {"peer_closed", peer_closed}};
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
