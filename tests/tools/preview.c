/* preview 目标：SDL 开窗预览例程界面，鼠标即触摸；PREVIEW_RES=480|720，PREVIEW_BACKLIGHT=<max>:<当前值> 建假背光，点 OFF 退出。 */
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>

#include "app_env.h"
#include "fake_fs.h"

extern int QUIT_FLAG;

static SDL_Renderer *renderer;
static SDL_Texture *texture;

/* SDL 与 X11 库在进程退出时有未释放内存，与例程无关，预览不做泄漏检测。 */
const char *__asan_default_options(void)
{
    return "detect_leaks=0";
}

static void present(const uint32_t *fb, int res)
{
    SDL_UpdateTexture(texture, NULL, fb, res * (int)sizeof(uint32_t));
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

static void setup_backlight(const char *spec)
{
    char max[32];
    char cur[32];
    char line[40];

    if (spec == NULL || sscanf(spec, "%31[0-9]:%31[0-9]", max, cur) != 2)
        return;
    tst_mkdirs("sys/class/backlight/backlight");
    tst_fmt(line, sizeof(line), "%s\n", max);
    tst_write_file("sys/class/backlight/backlight/max_brightness", line);
    tst_fmt(line, sizeof(line), "%s\n", cur);
    tst_write_file("sys/class/backlight/backlight/brightness", line);
}

int main(void)
{
    const char *env = getenv("PREVIEW_RES");
    int res = env != NULL ? atoi(env) : TST_DESIGN_RES;
    int x = 0;
    int y = 0;
    int pressed = 0;
    int running = 1;
    SDL_Window *win;

    if (res != 480 && res != 720)
        res = TST_DESIGN_RES;
    setup_backlight(getenv("PREVIEW_BACKLIGHT"));
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    win = SDL_CreateWindow(res == 480 ? "Luckfox LVGL preview (480x480)" : "Luckfox LVGL preview (720x720)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, res, res, 0);
    renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    texture = renderer != NULL ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, res, res) : NULL;
    if (win == NULL || renderer == NULL || texture == NULL) {
        fprintf(stderr, "SDL window: %s\n", SDL_GetError());
        return 1;
    }
    tst_app_set_present_hook(present);
    tst_app_init(res, 1, 0);
    tst_app_setup_ui();

    while (running && !QUIT_FLAG) {
        SDL_Event ev;

        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = 0;
            } else if (ev.type == SDL_MOUSEMOTION) {
                x = ev.motion.x;
                y = ev.motion.y;
            } else if (ev.type == SDL_MOUSEBUTTONDOWN) {
                pressed = 1;
            } else if (ev.type == SDL_MOUSEBUTTONUP) {
                pressed = 0;
            }
        }
        tst_pointer_set(x, y, pressed);
        tst_run_ms(5);
    }
    SDL_Quit();
    return 0;
}
