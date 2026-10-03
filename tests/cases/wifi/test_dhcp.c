/* 连按两次 Load：每次启动 udhcpc 前都有一次独立的、限定 wlan0 的 pkill（spec D17）。 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

#include "app_env.h"
#include "check.h"
#include "gui_guider.h"
#include "wifi_env.h"

void tst_wifi_conf_load(const char *ssid, const char *psk);

static char cmds[16][256];
static int ncmds;

int system(const char *command)
{
    static int (*real_system)(const char *);

    if (real_system == NULL)
        real_system = (int (*)(const char *))dlsym(RTLD_NEXT, "system");
    if (command != NULL && ncmds < 16)
        snprintf(cmds[ncmds++], sizeof(cmds[0]), "%s", command);
    return real_system(command);
}

static void single_instance(void)
{
    int i, pkills = 0, starts = 0, ordered = 1, pending = 0;

    wifi_env_setup(WIFI_ENV_CONF);
    tst_app_init(480, 1, 0);
    setup_scr_WIFI(&guider_ui);
    tst_wifi_conf_load("home", "homepass1");
    tst_wifi_conf_load("home", "homepass1");
    for (i = 0; i < ncmds; i++) {
        printf("system[%d]=%s\n", i, cmds[i]);
        if (strcmp(cmds[i], "pkill -f '[u]dhcpc -i wlan0'") == 0) {
            pkills++;
            pending = 1;
        } else if (strstr(cmds[i], "udhcpc -i wlan0") != NULL) {
            starts++;
            if (!pending || strstr(cmds[i], "pkill") != NULL)
                ordered = 0;
            pending = 0;
        }
    }
    CHECK_EQ_INT(pkills, 2);
    CHECK_EQ_INT(starts, 2);
    CHECK(ordered);
}

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {{"single_instance", single_instance}};
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
