/* 歌曲列表按钮：点一次展开、再点一次收起；点音乐页背景也收起；展开时按钮为选中状态并显示橙色线条图（spec Q50、N11）。 */
#include <unistd.h>
#include "app_env.h"
#include "check.h"
#include "gui_guider.h"
#include "lvgl.h"
#include "music_env.h"

static void enter_music_page(void)
{
    alarm(10);
    music_env_socketpair();
    music_env_file("song_01.mp3");
    tst_app_init(480, 0, 1);
    tst_app_setup_ui();
    lv_event_send(guider_ui.Main_Music_btn, LV_EVENT_CLICKED, NULL);
    tst_run_ms(400);
    CHECK(lv_scr_act() == guider_ui.Music_player);
}

static int list_shown(void)
{
    return !lv_obj_has_flag(guider_ui.Music_player_cont_1, LV_OBJ_FLAG_HIDDEN);
}

static int btn_checked(void)
{
    return lv_obj_has_state(guider_ui.Music_player_music_list_btn, LV_STATE_CHECKED);
}

/* 与板上一致：松开时 LV_OBJ_FLAG_CHECKABLE 先自动切换 CHECKED，随后才是 CLICKED。 */
static void click_list_btn(void)
{
    lv_event_send(guider_ui.Music_player_music_list_btn, LV_EVENT_RELEASED, NULL);
    lv_event_send(guider_ui.Music_player_music_list_btn, LV_EVENT_CLICKED, NULL);
}

static void toggle(void)
{
    enter_music_page();
    CHECK(!list_shown());
    CHECK(!btn_checked());
    click_list_btn();
    CHECK(list_shown());
    CHECK(btn_checked());
    click_list_btn();
    CHECK(!list_shown());
    CHECK(!btn_checked());
    click_list_btn();
    CHECK(list_shown());
    CHECK(btn_checked());
}

/* 点背景收起时按钮同步退出选中状态；再点按钮仍是展开且选中，不因 CHECKED 错位而相反。 */
static void background_close(void)
{
    enter_music_page();
    click_list_btn();
    CHECK(list_shown());
    lv_event_send(guider_ui.Music_player, LV_EVENT_CLICKED, NULL);
    CHECK(!list_shown());
    CHECK(!btn_checked());
    click_list_btn();
    CHECK(list_shown());
    CHECK(btn_checked());
}

static const uint8_t *px(const lv_img_dsc_t *img, uint32_t i)
{
    return img->data + i * LV_IMG_PX_SIZE_ALPHA_BYTE;
}

/* 选中状态用另一张图（lv_imgbtn 按状态设图）：与原图同尺寸、同格式；原图的白底不变，最深的线条为 #FFA300，
 * 其余像素是橙色与白色的混合（红色恒为 255）。原图白底是画进图里的，整图着色会变成橙色方块。32 位色深，字节序 B、G、R、A。 */
static void checked_image(void)
{
    lv_obj_t *btn;
    const lv_img_dsc_t *normal, *checked;
    uint32_t i, n, darkest = 0;
    unsigned lum, min_lum = 256;

    enter_music_page();
    btn = guider_ui.Music_player_music_list_btn;
    normal = lv_imgbtn_get_src_middle(btn, LV_IMGBTN_STATE_RELEASED);
    checked = lv_imgbtn_get_src_middle(btn, LV_IMGBTN_STATE_CHECKED_RELEASED);
    CHECK(normal != NULL && checked != NULL);
    CHECK(checked != normal);
    CHECK(lv_imgbtn_get_src_middle(btn, LV_IMGBTN_STATE_CHECKED_PRESSED) == checked);
    CHECK(lv_imgbtn_get_src_middle(btn, LV_IMGBTN_STATE_PRESSED) == normal);
    if (checked == normal || checked == NULL || normal == NULL)
        return;
    CHECK_EQ_INT(checked->header.cf, normal->header.cf);
    CHECK_EQ_INT(checked->header.w, normal->header.w);
    CHECK_EQ_INT(checked->header.h, normal->header.h);
    CHECK_EQ_INT(checked->data_size, normal->data_size);
    n = (uint32_t)normal->header.w * normal->header.h;
    for (i = 0; i < n; i++) {
        const uint8_t *o = px(normal, i), *c = px(checked, i);
        lum = (o[2] * 77u + o[1] * 151u + o[0] * 28u) >> 8;
        if (lum < min_lum) {
            min_lum = lum;
            darkest = i;
        }
        CHECK_EQ_INT(c[3], o[3]);
        CHECK_EQ_INT(c[2], 0xff);
        if (o[0] == 0xff && o[1] == 0xff && o[2] == 0xff)
            CHECK(c[0] == 0xff && c[1] == 0xff);
    }
    CHECK_EQ_INT(px(checked, darkest)[2], 0xff);
    CHECK_EQ_INT(px(checked, darkest)[1], 0xa3);
    CHECK_EQ_INT(px(checked, darkest)[0], 0x00);
}

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {
        {"toggle", toggle}, {"background_close", background_close}, {"checked_image", checked_image}};
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
