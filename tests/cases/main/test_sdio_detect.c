/* luckfox_sdio_has_id()：在临时目录造 uevent 布局，只有 SDIO_ID=C8A1:C18D 的整行匹配才判定为 Luckfox Pico Ultra W。 */
#include "check.h"
#include "fake_fs.h"

int tst_sdio_has_id(const char *bus_dir);

static void put_uevent(const char *dev, const char *content)
{
    char rel[256];

    tst_fmt(rel, sizeof(rel), "bus/%s", dev);
    tst_mkdirs(rel);
    tst_fmt(rel, sizeof(rel), "bus/%s/uevent", dev);
    tst_write_file(rel, content);
}

static void expect_bus(int want)
{
    char bus[512];

    tst_path(bus, sizeof(bus), "bus");
    CHECK_EQ_INT(tst_sdio_has_id(bus), want);
}

static void case_absent(void)
{
    expect_bus(0);
}

static void case_empty(void)
{
    tst_mkdirs("bus");
    expect_bus(0);
}

static void case_func1_only(void)
{
    put_uevent("mmc1:e9ea:1", "SDIO_CLASS=07\nSDIO_ID=C8A1:C08D\n");
    expect_bus(0);
}

static void case_ultra_w(void)
{
    put_uevent("mmc1:e9ea:1", "SDIO_CLASS=07\nSDIO_ID=C8A1:C08D\n");
    put_uevent("mmc1:e9ea:2", "SDIO_CLASS=07\nSDIO_ID=C8A1:C18D\nSDIO_REVISION=0.0\n");
    expect_bus(1);
}

static void case_hidden(void)
{
    put_uevent(".mmc1:e9ea:2", "SDIO_ID=C8A1:C18D\n");
    expect_bus(0);
}

static void case_no_newline(void)
{
    put_uevent("mmc1:e9ea:2", "SDIO_ID=C8A1:C18D");
    expect_bus(1);
}

static void case_prefix(void)
{
    put_uevent("mmc1:e9ea:2", "SDIO_ID=C8A1:C18DX\n");
    expect_bus(0);
}

static const tst_case_t cases[] = {
    { "absent", case_absent },
    { "empty", case_empty },
    { "func1_only", case_func1_only },
    { "ultra_w", case_ultra_w },
    { "hidden", case_hidden },
    { "no_newline", case_no_newline },
    { "prefix", case_prefix },
};

int main(int argc, char **argv)
{
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
