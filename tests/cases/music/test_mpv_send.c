/* mpv 已退出（对端关闭）后按播放/暂停，进程存活（spec D6）。 */
#include <fcntl.h>
#include <unistd.h>

#include "app_env.h"
#include "check.h"
#include "lvgl.h"
#include "music_env.h"

void Music_player_start_btn_event_handler(lv_event_t *e);

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
    static const tst_case_t cases[] = {{"peer_closed", peer_closed}};
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
