/* WiFi 用例公共环境：配置写到 tst_wpa_conf_path()，PATH 前置假 wpa_cli、udhcpc、pkill，命令行记进 <临时目录>/cmd.log。 */
#ifndef WIFI_ENV_H
#define WIFI_ENV_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "fake_fs.h"

static inline void wifi_env_setup(const char *conf)
{
    char script[2048];
    const char *dir = tst_tmp_dir();

    if (conf != NULL)
        tst_write_file("wpa_supplicant.conf", conf);
    tst_fmt(script, sizeof(script),
            "#!/bin/sh\necho \"wpa_cli $*\" >> '%s/cmd.log'\ncase \"$*\" in\n*scan_results*) cat '%s/scan.txt' 2>/dev/null ;;\n'') cat >> '%s/wpa_cli.stdin' ;;\nesac\nexit 0\n",
            dir, dir, dir);
    tst_fake_exec("wpa_cli", script);
    tst_fmt(script, sizeof(script), "#!/bin/sh\necho \"udhcpc $*\" >> '%s/cmd.log'\nexit 0\n", dir);
    tst_fake_exec("udhcpc", script);
    tst_fmt(script, sizeof(script), "#!/bin/sh\necho \"pkill $*\" >> '%s/cmd.log'\nexit 1\n", dir);
    tst_fake_exec("pkill", script);
}

/* 读 <临时目录>/<rel> 全文，不存在时返回空串；调用方 free。 */
static inline char *wifi_env_read(const char *rel)
{
    char path[1024];
    char *buf = calloc(1, 65536);
    FILE *fp;

    tst_path(path, sizeof(path), rel);
    fp = fopen(path, "r");
    if (fp != NULL) {
        buf[fread(buf, 1, 65535, fp)] = '\0';
        fclose(fp);
    }
    return buf;
}

static inline void wifi_env_reset_logs(void)
{
    char path[1024];

    tst_path(path, sizeof(path), "cmd.log");
    unlink(path);
    tst_path(path, sizeof(path), "wpa_cli.stdin");
    unlink(path);
}

static const char WIFI_ENV_CONF[] =
    "ctrl_interface=/var/run/wpa_supplicant\nnetwork={\n        ssid=\"home\"\n        psk=\"homepass1\"\n}\n";

#endif
