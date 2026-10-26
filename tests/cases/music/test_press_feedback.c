/* 音乐页按钮的按下反馈：上一首、下一首、播放、播放模式按下时叠 30% 黑色，返回按钮叠 50% 白色；列表按钮不着色
 * （spec §3.1、§4）。真正按下后取渲染出的帧缓冲比较像素，「黑叠黑」这类看不出变化的情况也能发现。 */
#include <string.h>
#include <unistd.h>
#include "app_env.h"
#include "check.h"
#include "gui_guider.h"
#include "lvgl.h"
#include "music_env.h"

LV_IMG_DECLARE(_btn_play_alpha_84x84);

#define RING 4

static uint32_t frame[TST_RES_MAX * TST_RES_MAX];
static uint32_t before[TST_RES_MAX * TST_RES_MAX];
static uint32_t pressed[TST_RES_MAX * TST_RES_MAX];
static uint32_t after[TST_RES_MAX * TST_RES_MAX];
static int res;

static void on_present(const uint32_t *fb, int r)
{
    memcpy(frame, fb, sizeof(uint32_t) * (size_t)r * (size_t)r);
}

static void enter(int r)
{
    alarm(20);
    res = r;
    music_env_socketpair();
    music_env_file("song_01.mp3");
    music_env_file("song_02.mp3");
    tst_app_init(r, 0, 1);
    tst_app_set_present_hook(on_present);
    tst_app_setup_ui();
    lv_event_send(guider_ui.Main_Music_btn, LV_EVENT_CLICKED, NULL);
    tst_run_ms(400);
    CHECK(lv_scr_act() == guider_ui.Music_player);
}

static void snap(uint32_t *buf)
{
    tst_run_ms(100);
    memcpy(buf, frame, sizeof(uint32_t) * (size_t)res * (size_t)res);
}

static void press(lv_obj_t *btn)
{
    tst_pointer_set((btn->coords.x1 + btn->coords.x2) / 2, (btn->coords.y1 + btn->coords.y2) / 2, 1);
    snap(pressed);
}

/* 打断按下：松开时只发 LV_EVENT_PRESS_LOST，不发 RELEASED、CLICKED，不会切歌、换页或切换选中状态。 */
static void cancel_press(lv_obj_t *btn)
{
    lv_indev_wait_release(lv_indev_get_next(NULL));
    tst_pointer_set((btn->coords.x1 + btn->coords.x2) / 2, (btn->coords.y1 + btn->coords.y2) / 2, 0);
    snap(after);
}

static int ch(uint32_t px, int shift)
{
    return (int)((px >> shift) & 0xff);
}

static int sum(uint32_t px)
{
    return ch(px, 16) + ch(px, 8) + ch(px, 0);
}

typedef struct {
    int area;
    int changed;
    int not_darker;   /* 变化的像素中，有通道变亮或总和没变小的个数 */
    int not_lighter;  /* 变化的像素中，有通道变暗或总和没变大的个数 */
    int min_r_white;  /* 按下前为纯白且变化的像素，按下后 R 的最小值；没有时为 256 */
} diff_t;

static diff_t diff_area(const uint32_t *a, const uint32_t *b, const lv_area_t *r)
{
    diff_t d = {0, 0, 0, 0, 256};
    int x, y, s;

    for (y = r->y1; y <= r->y2; y++) {
        for (x = r->x1; x <= r->x2; x++) {
            uint32_t pa = a[y * res + x] & 0xffffff, pb = b[y * res + x] & 0xffffff;

            d.area++;
            if (pa == pb)
                continue;
            d.changed++;
            for (s = 0; s <= 16; s += 8) {
                if (ch(pb, s) > ch(pa, s)) {
                    d.not_darker++;
                    break;
                }
            }
            if (s > 16 && sum(pb) >= sum(pa))
                d.not_darker++;
            for (s = 0; s <= 16; s += 8) {
                if (ch(pb, s) < ch(pa, s)) {
                    d.not_lighter++;
                    break;
                }
            }
            if (s > 16 && sum(pb) <= sum(pa))
                d.not_lighter++;
            if (pa == 0xffffff && ch(pb, 16) < d.min_r_white)
                d.min_r_white = ch(pb, 16);
        }
    }
    return d;
}

/* 按钮四周 RING 像素宽的环带（裁到屏幕内）中变化的像素数：有位移或变宽时不为 0。 */
static int ring_changed(const uint32_t *a, const uint32_t *b, const lv_area_t *r)
{
    int x, y, n = 0;

    for (y = LV_MAX(r->y1 - RING, 0); y <= LV_MIN(r->y2 + RING, res - 1); y++) {
        for (x = LV_MAX(r->x1 - RING, 0); x <= LV_MIN(r->x2 + RING, res - 1); x++) {
            if (x >= r->x1 && x <= r->x2 && y >= r->y1 && y <= r->y2)
                continue;
            if ((a[y * res + x] & 0xffffff) != (b[y * res + x] & 0xffffff))
                n++;
        }
    }
    return n;
}

/* 按下后变暗：至少一半像素变化且都变暗，四周不变，打断按下后恢复；white 为真时另查白色圆底为原值的约 70%
 * （(255 − LV_OPA_30) × 255 / 255 ≈ 179），real_release 为真时另查正常松开后恢复。 */
static void check_darken(const char *name, lv_obj_t *btn, int white, int real_release)
{
    lv_area_t r = btn->coords;
    diff_t d;

    printf("-- %s (%d,%d)-(%d,%d)\n", name, r.x1, r.y1, r.x2, r.y2);
    snap(before);
    press(btn);
    d = diff_area(before, pressed, &r);
    printf("   area=%d changed=%d not_darker=%d min_r_white=%d\n", d.area, d.changed, d.not_darker, d.min_r_white);
    CHECK(d.changed * 2 >= d.area);
    CHECK_EQ_INT(d.not_darker, 0);
    CHECK_EQ_INT(ring_changed(before, pressed, &r), 0);
    if (white) {
        CHECK(d.min_r_white >= 172);
        CHECK(d.min_r_white <= 186);
    }
    cancel_press(btn);
    CHECK_EQ_INT(diff_area(before, after, &r).changed, 0);
    if (real_release) {
        press(btn);
        tst_pointer_set((r.x1 + r.x2) / 2, (r.y1 + r.y2) / 2, 0);
        tst_run_ms(300);
        snap(after);
        CHECK_EQ_INT(diff_area(before, after, &r).changed, 0);
    }
}

static void darken(int r)
{
    enter(r);
    check_darken("pre", guider_ui.Music_player_pre_btn, 1, 1);
    check_darken("next", guider_ui.Music_player_next_btn, 1, 1);
    /* 暂停时：按下不再换图，蓝紫圆与白色三角形一起叠 30% 黑色（spec §5.1、Q14）。 */
    CHECK(!lv_obj_has_state(guider_ui.Music_player_start_btn, LV_STATE_CHECKED));
    check_darken("start(paused)", guider_ui.Music_player_start_btn, 1, 0);
    /* 播放中：选中状态显示暂停图，按下时同一张图叠 30% 黑色。 */
    lv_obj_add_state(guider_ui.Music_player_start_btn, LV_STATE_CHECKED);
    check_darken("start(checked)", guider_ui.Music_player_start_btn, 0, 0);
}

static void darken_480(void)
{
    darken(480);
}

static void darken_720(void)
{
    darken(720);
}

/* 按下后变亮：变化的像素不少于 min_changed 且都变亮，四周不变，打断按下后恢复；按下前最深的像素应为
 * darkest_rgb，叠 50% 白色后其 R 约为 R + (255 − R) × LV_OPA_50 / 255。 */
static void check_lighten(const char *name, lv_obj_t *btn, int min_changed, uint32_t darkest_rgb)
{
    lv_area_t a = btn->coords;
    diff_t d;
    int x, y, r0, want, darkest = -1;

    printf("-- %s (%d,%d)-(%d,%d)\n", name, a.x1, a.y1, a.x2, a.y2);
    snap(before);
    press(btn);
    d = diff_area(before, pressed, &a);
    for (y = a.y1; y <= a.y2; y++)
        for (x = a.x1; x <= a.x2; x++)
            if (darkest < 0 || sum(before[y * res + x]) < sum(before[darkest]))
                darkest = y * res + x;
    r0 = ch(before[darkest], 16);
    want = r0 + (255 - r0) * LV_OPA_50 / 255;
    printf("   area=%d changed=%d not_lighter=%d darkest=%06x -> %06x want_r=%d\n", d.area, d.changed, d.not_lighter,
           (unsigned)(before[darkest] & 0xffffff), (unsigned)(pressed[darkest] & 0xffffff), want);
    CHECK(d.changed >= min_changed);
    CHECK_EQ_INT(d.not_lighter, 0);
    CHECK_EQ_INT(before[darkest] & 0xffffff, darkest_rgb);
    CHECK(ch(pressed[darkest], 16) >= want - 8);
    CHECK(ch(pressed[darkest], 16) <= want + 8);
    CHECK_EQ_INT(ring_changed(before, pressed, &a), 0);
    cancel_press(btn);
    CHECK_EQ_INT(diff_area(before, after, &a).changed, 0);
}

/* 深色图标按下叠 50% 白色：返回按钮是透明底上的黑色线条（000000 → 约 7f7f7f）；播放模式是深紫圆底，
 * 叠黑色只暗约 24 级、真机上看不清（spec §5.1），循环与随机两张图都查。 */
static void lighten(int r)
{
    lv_obj_t *mode;

    enter(r);
    check_lighten("back", guider_ui.Music_player_back_btn, 150, 0x000000);
    mode = guider_ui.Music_player_play_mode_btn;
    check_lighten("mode(loop)", mode, (int)(lv_area_get_size(&mode->coords) / 2), 0x504d6c);
    lv_obj_add_state(mode, LV_STATE_CHECKED);
    check_lighten("mode(random)", mode, (int)(lv_area_get_size(&mode->coords) / 2), 0x504d6c);
    CHECK(lv_obj_has_state(mode, LV_STATE_CHECKED));
    CHECK(lv_scr_act() == guider_ui.Music_player);
}

static void lighten_480(void)
{
    lighten(480);
}

static void lighten_720(void)
{
    lighten(720);
}

/* 暂停时按下播放键：生成代码把按下图设为另一张浅色底的播放图，custom/ 改回与松开时相同的图，
 * 两种状态都只着色、不换图（spec §5.1、Q14）。 */
static void play_paused(void)
{
    lv_obj_t *btn;

    enter(480);
    btn = guider_ui.Music_player_start_btn;
    CHECK(!lv_obj_has_state(btn, LV_STATE_CHECKED));
    press(btn);
    CHECK(lv_obj_has_state(btn, LV_STATE_PRESSED));
    CHECK(lv_imgbtn_get_src_middle(btn, LV_IMGBTN_STATE_RELEASED) == &_btn_play_alpha_84x84);
    CHECK(lv_imgbtn_get_src_middle(btn, LV_IMGBTN_STATE_PRESSED) == &_btn_play_alpha_84x84);
    CHECK_EQ_INT(lv_obj_get_style_img_recolor_opa(btn, LV_PART_MAIN), LV_OPA_30);
    CHECK_EQ_INT(lv_color_to32(lv_obj_get_style_img_recolor(btn, LV_PART_MAIN)) & 0xffffff, 0);
    cancel_press(btn);
}

/* 列表按钮整图不透明，着色会成灰色方块，不加按下反馈（spec Q1）。 */
static void list_unchanged(void)
{
    lv_obj_t *btn;

    enter(480);
    btn = guider_ui.Music_player_music_list_btn;
    snap(before);
    press(btn);
    CHECK(lv_obj_has_state(btn, LV_STATE_PRESSED));
    CHECK_EQ_INT(diff_area(before, pressed, &btn->coords).changed, 0);
    cancel_press(btn);
}

/* 音乐页只建一次：返回主屏再进入，按下反馈仍在。 */
static void reenter(void)
{
    lv_obj_t *btn;
    diff_t d;

    enter(480);
    lv_event_send(guider_ui.Music_player_back_btn, LV_EVENT_CLICKED, NULL);
    tst_run_ms(400);
    CHECK(lv_scr_act() == guider_ui.Main);
    lv_event_send(guider_ui.Main_Music_btn, LV_EVENT_CLICKED, NULL);
    tst_run_ms(400);
    CHECK(lv_scr_act() == guider_ui.Music_player);
    btn = guider_ui.Music_player_pre_btn;
    snap(before);
    press(btn);
    d = diff_area(before, pressed, &btn->coords);
    CHECK(d.changed * 2 >= d.area);
    CHECK_EQ_INT(d.not_darker, 0);
    cancel_press(btn);
}

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {
        {"darken_480", darken_480}, {"darken_720", darken_720}, {"lighten_480", lighten_480}, {"lighten_720", lighten_720},
        {"play_paused", play_paused}, {"list_unchanged", list_unchanged}, {"reenter", reenter}};
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
