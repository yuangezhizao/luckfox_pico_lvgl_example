/* _wifi_scanning_ssid()：按 TAB 切分、解码转义、丢弃不可用条目、按保留项计 10 个上限（spec D15）。 */
#include <string.h>
#include <stdarg.h>

#include "app_env.h"
#include "check.h"
#include "gui_guider.h"
#include "lvgl.h"
#include "wifi_env.h"

int tst_wifi_scan(void);

#define HDR "bssid / frequency / signal level / flags / ssid\n"
#define ROW(ssid) "aa:bb:cc:dd:ee:ff\t2412\t-40\t[WPA2-PSK-CCMP][ESS]\t" ssid "\n"

static int format_call, fail_call, format_error;

/* 保留 snprintf 的截断语义，强制覆盖正常容量推算不可达的分支。 */
int tst_wifi_snprintf(char *out, size_t size, const char *fmt, ...)
{
    va_list ap;
    int n;
    va_start(ap, fmt);
    if (strcmp(fmt, "%s%s") == 0 && ++format_call == fail_call) {
        CHECK(size > 0 && size <= 1285);
        if (size > 0 && size <= 1285) {
            memset(out, 'x', size - 1);
            out[size - 1] = '\0';
        }
        n = format_error ? -1 : (int)size + 17;
    } else {
        n = vsnprintf(out, size, fmt, ap);
    }
    va_end(ap);
    return n;
}

static void run_scan(const char *scan)
{
    wifi_env_setup(NULL);
    tst_write_file("scan.txt", scan);
    tst_app_init(480, 1, 0);
    guider_ui.WIFI_wifi_list = lv_dropdown_create(lv_scr_act());
    tst_wifi_scan();
}

static void expect_options(const char *const *want, size_t n)
{
    char buf[2048];
    const char *opts = lv_dropdown_get_options(guider_ui.WIFI_wifi_list);
    char *p, *nl;
    size_t i;

    printf("options=[%s]\n", opts);
    CHECK_EQ_INT(lv_dropdown_get_option_cnt(guider_ui.WIFI_wifi_list), n);
    snprintf(buf, sizeof(buf), "%s", opts);
    p = buf;
    for (i = 0; i < n && p != NULL; i++) {
        nl = strchr(p, '\n');
        if (nl != NULL)
            *nl = '\0';
        CHECK(strcmp(p, want[i]) == 0);
        p = nl != NULL ? nl + 1 : NULL;
    }
}

static void parse(void)
{
    static const char *const want[] = {"my net", "emptyflags", "12345678901234567890", "a\"b", "a\\b", "\xe4\xb8\xad", "\xe4\xb8\xad", "..."};
    char scan[4096], longssid[129];

    memset(longssid, 'a', 128);
    longssid[128] = '\0';
    tst_fmt(scan, sizeof(scan), "%s",
            HDR ROW("my net") "aa:bb:cc:dd:ee:ff\t2412\t-40\t\temptyflags\n" ROW("12345678901234567890") ROW("a\\\"b") ROW("a\\\\b")
            ROW("\\xe4\\xb8\\xad") ROW("\\xE4\\xB8\\xAD") ROW("") ROW("\\e") ROW("a\\nb") ROW("a\\rb") ROW("a\\tb") ROW("\\x00hide") ROW("\\x01hide")
            ROW("pre\\x4") ROW("\\q") ROW("\\"));
    snprintf(scan + strlen(scan), sizeof(scan) - strlen(scan), "aa:bb:cc:dd:ee:ff\t2412\t-40\t[ESS]\t%s\n", longssid);
    run_scan(scan);
    expect_options(want, TST_COUNT(want));
}

static void parse_all_discarded(void)
{
    static const char *const want[] = {"scanning"};

    run_scan(HDR ROW("") ROW("\\x01hide") ROW("\\q") ROW("\\e"));
    expect_options(want, TST_COUNT(want));
}

static void parse_cap(void)
{
    static const char *const want[] = {"n01", "n02", "n03", "n04", "n05", "n06", "n07", "n08", "n09", "n10", "..."};

    run_scan(HDR ROW("n01") ROW("n02") ROW("n03") ROW("\\e") ROW("n04") ROW("n05") ROW("\\q") ROW("n06") ROW("n07") ROW("n08") ROW("n09") ROW("n10") ROW("n11") ROW("n12"));
    expect_options(want, TST_COUNT(want));
}

static void append_truncated(void)
{
    static const char *const want[] = {"n01", "..."};
    fail_call = 2;
    run_scan(HDR ROW("n01") ROW("n02") ROW("n03"));
    expect_options(want, TST_COUNT(want));
    CHECK_EQ_INT(format_call, 2);
}

static void append_error(void)
{
    static const char *const want[] = {"scanning"};
    fail_call = 1;
    format_error = 1;
    run_scan(HDR ROW("n01") ROW("n02"));
    expect_options(want, TST_COUNT(want));
    CHECK_EQ_INT(format_call, 1);
}

int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {{"parse", parse}, {"parse_all_discarded", parse_all_discarded}, {"parse_cap", parse_cap}, {"append_truncated", append_truncated}, {"append_error", append_error}};
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
