/* roller 选中 100 字节文件名：按下标找到节点并显示（spec D5）。 */
#include <string.h>
#include <unistd.h>

#include "app_env.h"
#include "check.h"
#include "gui_guider.h"
#include "lvgl.h"
#include "music_env.h"

void Music_player_list_roller_event_handler(lv_event_t *e);

static void select_long_name(void)
{
    alarm(5);
    char longname[101], buf[256];
    lv_obj_t *roller;
    uint16_t i, cnt;

    memset(longname, 'L', 96);
    memcpy(longname + 96, ".mp3", 5);
    music_env_socketpair();
    music_env_file("a.mp3");
    music_env_file(longname);
    tst_app_init(480, 0, 1);
    setup_scr_Music_player(&guider_ui);
    roller = guider_ui.Music_player_roller_1;
    cnt = lv_roller_get_option_cnt(roller);
    for (i = 0; i < cnt; i++) {
        lv_roller_set_selected(roller, i, LV_ANIM_OFF);
        lv_roller_get_selected_str(roller, buf, sizeof(buf));
        /* roller 显示缩写名（spec §5.1 F4）：以 L 开头的那项就是长文件名。 */
        if (buf[0] == 'L')
            break;
    }
    CHECK(i < cnt);
    lv_obj_add_event_cb(roller, Music_player_list_roller_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_event_send(roller, LV_EVENT_VALUE_CHANGED, NULL);
    CHECK(strcmp(lv_label_get_text(guider_ui.Music_player_music_name), longname) == 0);
}

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {{"select_long_name", select_long_name}};
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
