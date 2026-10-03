/* wifi_input_check()：按字节校验 SSID 1–32、密码 8–63，拒绝控制字符、非法 UTF-8 与 " 之后的 #（spec D11）。 */
#include <string.h>

#include "check.h"

const char *wifi_input_check(const char *ssid, const char *password);

static void expect(const char *ssid, const char *psk, int ok)
{
    const char *err = wifi_input_check(ssid, psk);

    printf("ssid=[%s] psk=[%s] -> %s\n", ssid, psk, err != NULL ? err : "OK");
    CHECK((err == NULL) == ok);
}

static void rules(void)
{
    char s32[33], s33[34], p63[64], p64[65];

    memset(s32, 'a', 32); s32[32] = '\0';
    memset(s33, 'a', 33); s33[33] = '\0';
    memset(p63, 'p', 63); p63[63] = '\0';
    memset(p64, 'p', 64); p64[64] = '\0';
    expect("", "password1", 0);
    expect(s33, "password1", 0);
    expect(s32, "password1", 1);
    expect("中文网络", "password1", 1);
    expect("home", "1234567", 0);
    expect("home", "12345678", 1);
    expect("home", p63, 1);
    expect("home", p64, 0);
    expect("home", "", 0);
    expect("ho\tme", "password1", 0);
    expect("home", "pass\x01word", 0);
    expect("home", "pass\x7fword", 0);
    expect("\xff\xfe", "password1", 0);
    expect("a\"b", "pa\"ss1234", 1);
    expect("a\\b", "pa\\ss1234", 1);
    expect("a#b", "pass#word1", 1);
    expect("a\"#b", "password1", 0);
    expect("home", "pa\"ss#word", 0);
    expect("a#b\"c", "a#b\"c1234", 1);
}

/* 非法序列覆盖续字节、截断、过长编码、代理区与 Unicode 上界。 */
static void utf8(void)
{
    static const char *const invalid[] = {
        "\x80", "\xc3(", "\xe2\x82", "\xf0\x90\x80",
        "\xc0\x80", "\xe0\x80\x80", "\xf0\x80\x80\x80",
        "\xed\xa0\x80", "\xed\xbf\xbf", "\xf4\x90\x80\x80"
    };
    static const char *const valid[] = {
        "\xc2\x80", "\xdf\xbf", "\xe0\xa0\x80", "\xed\x9f\xbf",
        "\xee\x80\x80", "\xef\xbf\xbf", "\xf0\x90\x80\x80", "\xf4\x8f\xbf\xbf"
    };
    char password[32];
    for (size_t i = 0; i < TST_COUNT(invalid); i++) {
        expect(invalid[i], "password1", 0);
        snprintf(password, sizeof(password), "password%s", invalid[i]);
        expect("home", password, 0);
    }
    for (size_t i = 0; i < TST_COUNT(valid); i++) {
        expect(valid[i], "password1", 1);
        snprintf(password, sizeof(password), "password%s", valid[i]);
        expect("home", password, 1);
    }
}

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {{"rules", rules}, {"utf8", utf8}};
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
