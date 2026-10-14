/* 音乐列表选项串：总长超过 255 字节不溢出，内容逐项正确（PR #8 spec D3、D4）；roller 显示缩写名（spec §5.1 F4）；按文件名排序（spec Q33）。 */
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include "capture.h"
int music_scan_list(void);

#include "app_env.h"
#include "check.h"
#include "gui_guider.h"
#include "lvgl.h"
#include "music_env.h"

const char *music_roller_options(void);
void Music_player_list_roller_event_handler(lv_event_t *e);

static void options_over_255(void)
{
    char names[12][201], buf[256];
    int seen[12] = {0};
    lv_obj_t *roller;
    uint16_t i;

    music_env_socketpair();
    for (i = 0; i < 12; i++) {
        memset(names[i], 'a' + i, 196);
        memcpy(names[i] + 196, ".mp3", 5);
        music_env_file(names[i]);
    }
    tst_app_init(480, 0, 1);
    setup_scr_Music_player(&guider_ui);
    roller = guider_ui.Music_player_roller_1;
    CHECK_EQ_INT(lv_roller_get_option_cnt(roller), 12);
    for (i = 0; i < 12; i++) {
        lv_roller_set_selected(roller, i, LV_ANIM_OFF);
        lv_roller_get_selected_str(roller, buf, sizeof(buf));
        /* roller 显示的是按宽度缩写的名字（spec §5.1 F4），完整名字由 music_roller_options() 校验。 */
        CHECK(strlen(buf) < 200 && strstr(buf, "...") != NULL);
        if (buf[0] >= 'a' && buf[0] < 'a' + 12)
            seen[buf[0] - 'a'] = 1;
    }
    for (i = 0; i < 12; i++)
        CHECK(seen[i]);
    CHECK(strlen(music_roller_options()) == 12 * 200 + 11);
}

static void newline_name_skipped(void)
{
    const char *out, *opts, *nl;

    music_env_socketpair();
    music_env_file("a.mp3");
    music_env_file("b\nc.mp3");
    music_env_file("d.mp3");
    tst_capture_begin();
    music_scan_list();
    out = tst_capture_end();
    opts = music_roller_options();
    nl = strchr(opts, '\n');
    CHECK(nl != NULL && strchr(nl + 1, '\n') == NULL);
    CHECK(strchr(opts, 'b') == NULL);
    CHECK_EQ_INT(tst_count_lines(out, "skip music file"), 1);
}

static void missing_dir(void)
{
    music_env_socketpair();
    CHECK(music_scan_list() != 0);
    CHECK(strcmp(music_roller_options(), "") == 0);
}

/* 按 03、01、04、02 的顺序建文件：列表与 mpv 播放列表都按文件名升序（spec Q33、N6），第一首即 01。
 * 主机 readdir 的顺序不受控，断言只看结果有序。 */
static void sorted_by_name(void)
{
    static const char *const created[] = {"song_03.mp3", "song_01.mp3", "song_04.mp3", "song_02.mp3"};
    static char sent[8192];
    const char *p;
    size_t used = 0;
    ssize_t n;
    int peer, i;

    peer = music_env_socketpair();
    fcntl(peer, F_SETFL, O_NONBLOCK);
    for (i = 0; i < 4; i++)
        music_env_file(created[i]);
    tst_app_init(480, 0, 1);
    setup_scr_Music_player(&guider_ui);
    CHECK(strcmp(music_roller_options(), "song_01.mp3\nsong_02.mp3\nsong_03.mp3\nsong_04.mp3") == 0);
    CHECK_EQ_INT(lv_roller_get_selected(guider_ui.Music_player_roller_1), 0);
    while (used < sizeof(sent) - 1 && (n = read(peer, sent + used, sizeof(sent) - 1 - used)) > 0)
        used += (size_t)n;
    sent[used] = '\0';
    printf("sent=[%s]\n", sent);
    p = sent;
    for (i = 1; i <= 4; i++) {
        char want[32];

        snprintf(want, sizeof(want), "/music/song_%02d.mp3", i);
        p = strstr(p, want);
        CHECK(p != NULL);
        if (p == NULL)
            return;
    }
    /* 逐项选中：歌曲名与 playlist-pos 都按排序后的位置对应，节点 id 按排序后的顺序编号。 */
    lv_obj_add_event_cb(guider_ui.Music_player_roller_1, Music_player_list_roller_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
    for (i = 0; i < 4; i++) {
        char want_name[32], want_pos[32];

        lv_roller_set_selected(guider_ui.Music_player_roller_1, (uint16_t)i, LV_ANIM_OFF);
        lv_event_send(guider_ui.Music_player_roller_1, LV_EVENT_VALUE_CHANGED, NULL);
        snprintf(want_name, sizeof(want_name), "song_%02d.mp3", i + 1);
        CHECK(strcmp(lv_label_get_text(guider_ui.Music_player_music_name), want_name) == 0);
        used = 0;
        while (used < sizeof(sent) - 1 && (n = read(peer, sent + used, sizeof(sent) - 1 - used)) > 0)
            used += (size_t)n;
        sent[used] = '\0';
        snprintf(want_pos, sizeof(want_pos), "\"playlist-pos\", %d]", i);
        CHECK(strstr(sent, want_pos) != NULL);
    }
}

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {{"options_over_255", options_over_255}, {"newline_name_skipped", newline_name_skipped}, {"missing_dir", missing_dir},
                                       {"sorted_by_name", sorted_by_name}};
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
