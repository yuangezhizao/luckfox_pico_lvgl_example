/* WIFI 定时器：不在 WIFI 页时不 popen，回到 WIFI 页后恢复（spec D16）。 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>

#include "app_env.h"
#include "check.h"
#include "gui_guider.h"
#include "lvgl.h"
#include "wifi_env.h"

static int popen_calls;

FILE *popen(const char *command, const char *type)
{
    static FILE *(*real_popen)(const char *, const char *);

    if (real_popen == NULL)
        real_popen = (FILE *(*)(const char *, const char *))dlsym(RTLD_NEXT, "popen");
    popen_calls++;
    return real_popen(command, type);
}

static void inactive_screen(void)
{
    wifi_env_setup(WIFI_ENV_CONF);
    tst_app_init(480, 1, 0);
    tst_app_setup_ui();
    if (guider_ui.WIFI == NULL)
        setup_scr_WIFI(&guider_ui);
    popen_calls = 0;
    tst_run_ms(6000);
    CHECK_EQ_INT(popen_calls, 0);
    lv_scr_load(guider_ui.WIFI);
    tst_run_ms(6000);
    CHECK(popen_calls >= 1);
}

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {{"inactive_screen", inactive_screen}};
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
