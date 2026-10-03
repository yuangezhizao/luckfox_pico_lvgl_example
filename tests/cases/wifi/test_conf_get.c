/* _wifi_conf_get()：超长值不溢出，含 " 的值完整读回（spec D13）。 */
#include <string.h>

#include "check.h"
#include "fake_fs.h"

void tst_wifi_conf_get(char ssid[128], char psk[128]);

static void long_and_quoted(void)
{
    char conf[1024], longv[201];
    char ssid[128] = "", psk[128] = "";

    memset(longv, 'a', 200);
    longv[200] = '\0';
    tst_fmt(conf, sizeof(conf), "network={\n        ssid=\"%s\"\n        psk=\"pa\"ss1234\"\n}\n", longv);
    tst_write_file("wpa_supplicant.conf", conf);
    tst_wifi_conf_get(ssid, psk);
    CHECK(strlen(ssid) < sizeof(ssid));
    CHECK(strcmp(psk, "pa\"ss1234") == 0);

    tst_write_file("wpa_supplicant.conf", "network={\n        ssid=\"a\"b\"\n        psk=\"pa\"ss1234\"\n}\n");
    memset(ssid, 0, sizeof(ssid));
    memset(psk, 0, sizeof(psk));
    tst_wifi_conf_get(ssid, psk);
    CHECK(strcmp(ssid, "a\"b") == 0);
    CHECK(strcmp(psk, "pa\"ss1234") == 0);
}

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {{"long_and_quoted", long_and_quoted}};
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
