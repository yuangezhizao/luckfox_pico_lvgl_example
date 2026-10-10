/* 音乐列表显示名：超宽时保留头尾、中间用 ...，宽度不超过列表（spec §5.1 F4）。 */
#include <string.h>

#include "app_env.h"
#include "check.h"
#include "gui_guider.h"
#include "lvgl.h"
#include "music_env.h"

int music_display_name(const char *name, char *out, size_t size, const lv_font_t *font, lv_coord_t letter_space, lv_coord_t max_w);
void music_roller_apply(lv_obj_t *roller);

#define PREFIX "luckfox_roller_clip_test_long_common_prefix_abcdefghijklmnopq"

static lv_coord_t width_of(const char *s, const lv_font_t *font, lv_coord_t ls)
{
    return lv_txt_get_width(s, (uint32_t)strlen(s), font, ls, LV_TEXT_FLAG_NONE);
}

static int utf8_valid(const unsigned char *s)
{
    while (*s) {
        int n = *s < 0x80 ? 1 : (*s & 0xe0) == 0xc0 ? 2 : (*s & 0xf0) == 0xe0 ? 3 : (*s & 0xf8) == 0xf0 ? 4 : 0;
        if (n == 0)
            return 0;
        for (int i = 1; i < n; i++)
            if ((s[i] & 0xc0) != 0x80)
                return 0;
        s += n;
    }
    return 1;
}

static void short_unchanged(void)
{
    char out[300];

    tst_app_init(480, 0, 1);
    CHECK_EQ_INT(music_display_name("a.mp3", out, sizeof(out), &lv_font_montserratMedium_18, 0, 272), 0);
    CHECK(strcmp(out, "a.mp3") == 0);
}

static void long_keeps_head_tail(void)
{
    static const char *const names[] = {PREFIX "_01.mp3", PREFIX "_02.mp3", PREFIX "_03.mp3"};
    char out[3][300];

    tst_app_init(480, 0, 1);
    for (int i = 0; i < 3; i++) {
        CHECK_EQ_INT(music_display_name(names[i], out[i], sizeof(out[i]), &lv_font_montserratMedium_18, 0, 272), 0);
        printf("%s -> %s (%d px)\n", names[i], out[i], (int)width_of(out[i], &lv_font_montserratMedium_18, 0));
        CHECK(width_of(out[i], &lv_font_montserratMedium_18, 0) <= 272);
        CHECK(strstr(out[i], "...") != NULL);
        CHECK(strncmp(out[i], "luckfox", 7) == 0);
    }
    CHECK(strstr(out[0], "_01.mp3") != NULL && strstr(out[1], "_02.mp3") != NULL && strstr(out[2], "_03.mp3") != NULL);
    CHECK(strcmp(out[0], out[1]) != 0 && strcmp(out[1], out[2]) != 0);
}

/* 截断点落在多字节字符中间会产生非法 UTF-8。头尾用字体中有字形、宽度不为 0 的 3 字节符号（U+F001、U+F00C），
 * 让两侧的截断点都可能落在多字节字符上；中间的 ASCII 足够宽，保证必须截断。 */
static void utf8_boundary(void)
{
    char name[512] = "", out[512];
    lv_coord_t w;

    tst_app_init(480, 0, 1);
    for (int i = 0; i < 10; i++)
        strcat(name, "\xef\x80\x81");
    for (int i = 0; i < 30; i++)
        strcat(name, "Ab");
    for (int i = 0; i < 10; i++)
        strcat(name, "\xc3\xa9\xe4\xb8\xad");
    for (int i = 0; i < 10; i++)
        strcat(name, "\xef\x80\x8c");
    strcat(name, ".mp3");
    for (w = 60; w <= 272; w += 7) {
        CHECK_EQ_INT(music_display_name(name, out, sizeof(out), &lv_font_montserratMedium_18, 2, w), 0);
        if (!utf8_valid((const unsigned char *)out) || width_of(out, &lv_font_montserratMedium_18, 2) > w) {
            printf("bad at w=%d: %s\n", (int)w, out);
            CHECK(0);
        }
    }
}

static void check_roller_fits(int res)
{
    lv_obj_t *roller;
    const lv_font_t *font;
    lv_coord_t ls, max_w;
    char opts[1024], *line, *save = NULL;
    int n = 0;

    music_env_socketpair();
    music_env_file(PREFIX "_01.mp3");
    music_env_file(PREFIX "_02.mp3");
    music_env_file(PREFIX "_03.mp3");
    tst_app_init(res, 0, 1);
    setup_scr_Music_player(&guider_ui);
    roller = guider_ui.Music_player_roller_1;
    lv_obj_update_layout(roller);
    font = lv_obj_get_style_text_font(roller, LV_PART_SELECTED);
    ls = lv_obj_get_style_text_letter_space(roller, LV_PART_SELECTED);
    max_w = lv_obj_get_content_width(roller);
    snprintf(opts, sizeof(opts), "%s", lv_roller_get_options(roller));
    printf("res=%d max_w=%d options=[%s]\n", res, (int)max_w, opts);
    /* INFINITE 模式下选项串重复多遍，只看前 option_cnt 行。 */
    CHECK_EQ_INT(lv_roller_get_option_cnt(roller), 3);
    for (line = strtok_r(opts, "\n", &save); line != NULL && n < 3; line = strtok_r(NULL, "\n", &save), n++)
        CHECK(width_of(line, font, ls) <= max_w);
    CHECK_EQ_INT(n, 3);
    CHECK(strstr(lv_roller_get_options(roller), "_01.mp3") != NULL);
    CHECK(strstr(lv_roller_get_options(roller), "_03.mp3") != NULL);
    CHECK(lv_label_get_long_mode(guider_ui.Music_player_music_name) == LV_LABEL_LONG_SCROLL_CIRCULAR);
}

static void roller_fits_480(void) { check_roller_fits(480); }
static void roller_fits_720(void) { check_roller_fits(720); }

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {{"short_unchanged", short_unchanged}, {"long_keeps_head_tail", long_keeps_head_tail},
                                       {"utf8_boundary", utf8_boundary}, {"roller_fits_480", roller_fits_480}, {"roller_fits_720", roller_fits_720}};
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
