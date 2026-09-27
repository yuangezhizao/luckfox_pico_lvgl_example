# Luckfox Pico LVGL Example — Luckfox Pico Ultra W WiFi 判定修复实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 SDK 统一设备树固件（`model = "Luckfox Pico Ultra"`）上，按 SDIO ID `C8A1:C18D` 判定 Luckfox Pico Ultra W 板载 WiFi，恢复例程主界面的 WIFI 按键。

**Architecture:** `custom/custom_main.c` 新增只依赖 libc 的 `luckfox_sdio_has_id()`，遍历 `/sys/bus/sdio/devices/*/uevent` 精确匹配 `SDIO_ID=C8A1:C18D`；`luckfox_get_wifi_enable_info()` 的 `"Luckfox Pico Ultra"` 分支改为调用它，旧固件 `"Luckfox Pico Ultra W"` 行为不变，并修掉同函数的重复 `fclose`；`custom/custom_wifi.c` 的 `wifi_backend_release()` 判空，消除 WIFI 键恢复后可达的退出段错误。逻辑用临时 host harness 验证，交叉编译由本机 glibc 与 PR CI 两行验证，功能由作者 Luckfox Pico Ultra W 真机验证。

**Tech Stack:** C（GUI-Guider + LVGL 8.3）、CMake、`arm-linux-gnueabihf-`（本机与 CI glibc 行）、Rockchip `arm-rockchip830-linux-uclibcgnueabihf-`（CI uClibc 行）、host `gcc`（harness）。

**Spec:** [`docs/superpowers/specs/2026-09-25-lvgl-example-luckfox-pico-ultra-w-wifi-detect-design.md`](../specs/2026-09-25-lvgl-example-luckfox-pico-ultra-w-wifi-detect-design.md)

## Global Constraints

每个 Task 默认包含 spec 全文。硬约束：

- 设计以 spec 为准：FR1–FR7、NFR1–NFR3、D1–D7、§5 判定表、§6 判据
- 只改 `custom/custom_main.c`、`custom/custom_wifi.c`（仅 `wifi_backend_release()`）与 `AGENTS.md`；不动 `generated/`、`src/main.c`、`CMakeLists.txt`、README、SDK
- 判据字符串恰为 `SDIO_ID=C8A1:C18D`，目录恰为 `/sys/bus/sdio/devices`
- 新代码不引入 `strcpy`/`strcat`/`sprintf`/`gets`/`popen`/`system`；路径拼接 `snprintf` 并检查截断
- 不在本机或 CI 执行 ARM 二进制；板上只用 uClibc artifact
- 非 W 板与旧固件无法实测，只做代码走查并标「未实测」
- 提交序：`fix(custom)` → `docs(agents)` → `docs(superpowers)`（最后单个提交）；评审发现的问题直接合并进对应提交，改写历史后用 `--force-with-lease` 推送，不 force-push `dev`
- 分支仅 `cursor/ultra-w-wifi-detect-7dfe`；不另开 PR
- 已完成 Task 只保留落地文件与勾选步骤，不重复已入库的代码与提交说明（以 git 为准）；不入库的 harness 与复现程序保留全文以便复现

## File Structure

| 文件 | 责任 |
|---|---|
| `custom/custom_main.c` | `DEFINES` 区 `LUCKFOX_SDIO_BUS_DIR`/`LUCKFOX_ULTRA_W_SDIO_ID`；`STATIC FUNCTIONS` 区 `luckfox_sdio_has_id()`；`luckfox_get_wifi_enable_info()` 改判据与修 `fclose` |
| `custom/custom_wifi.c` | `wifi_backend_release()` 判空并置 `NULL` |
| `AGENTS.md` | `### 运行 / 验证 GUI（重要陷阱）` 末尾一段板型判据 gotcha |
| `/tmp/sdio-test/`（不入库） | host harness：`harness.c`、`run.sh`、从源文件抽出的 `helper.inc` |
| `/tmp/ll-segv/`（不入库） | host 复现程序：`main.c`，证明 `_lv_ll_remove(&ll, NULL)` 段错误 |
| `/tmp/dblclose/`（不入库） | host 复现程序：`main.c`，证明对同一 `FILE*` 两次 `fclose` 的后果 |
| `/tmp/lp-toolchain/`、`/tmp/nofix/`（不入库） | uClibc 工具链 sparse 检出与修复前对照程序构建目录（Task 7） |
| 本 plan / spec | Task、验证证据、偏离 |

---

### Task 1: 真机前置采集（判据门槛，已完成）

**Files:** 无仓库文件。作者在 Luckfox Pico Ultra W（刷 run 35115207257 固件）上执行，结果见 spec §2.3 与「验证证据」。

- [x] **Step 1:** 板上执行

```bash
cat /proc/device-tree/model; echo
ls /sys/bus/sdio/devices
grep -H SDIO_ /sys/bus/sdio/devices/*/uevent
which wpa_cli wpa_supplicant udhcpc
ls -l /etc/wpa_supplicant.conf
pidof wpa_supplicant
ls /var/run/wpa_supplicant
```

- [x] **Step 2:** 判定：function 2 为 `SDIO_ID=C8A1:C18D`，`wpa_supplicant` 在运行 → 继续 Task 2（无 `C8A1:C18D` 时的处理为停止并修订 spec D2）

### Task 2: 按 SDIO ID 判定 WiFi（`fix(custom)`，已完成）

**落地文件（以仓库为准，不在此重复代码与提交说明）：**

- Modify: `custom/custom_main.c`（两个宏、`luckfox_sdio_has_id()`、`luckfox_get_wifi_enable_info()` 的 `"Luckfox Pico Ultra"` 分支与 `fclose`）
- Modify: `custom/custom_wifi.c`（`wifi_backend_release()` 判空，见「与计划的偏离」）
- Test: `/tmp/sdio-test/`（不入库）

**Interfaces:** 产出 `static int luckfox_sdio_has_id(const char *bus_dir, const char *uevent_line)`（找到返回 1，否则 0）；宏 `LUCKFOX_SDIO_BUS_DIR`、`LUCKFOX_ULTRA_W_SDIO_ID`。

harness（`/tmp/sdio-test/harness.c`）：

```c
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "helper.inc"

#define ROOT "/tmp/sdio-test/root"

static int failures;

static void make_dir(const char *path)
{
    if (mkdir(path, 0755) != 0)
        perror(path);
}

static void put_uevent(const char *bus_dir, const char *dev, const char *content)
{
    char path[512];
    FILE *fp;

    snprintf(path, sizeof(path), "%s/%s", bus_dir, dev);
    make_dir(path);
    snprintf(path, sizeof(path), "%s/%s/uevent", bus_dir, dev);
    fp = fopen(path, "w");
    if (fp == NULL) {
        perror(path);
        return;
    }
    fputs(content, fp);
    fclose(fp);
}

static void expect(const char *name, const char *bus_dir, int want)
{
    int got = luckfox_sdio_has_id(bus_dir, LUCKFOX_ULTRA_W_SDIO_ID);

    printf("%s %s: got=%d want=%d\n", got == want ? "PASS" : "FAIL", name, got, want);
    if (got != want)
        failures++;
}

int main(void)
{
    expect("absent", ROOT "/absent", 0);

    make_dir(ROOT "/empty");
    expect("empty", ROOT "/empty", 0);

    make_dir(ROOT "/func1_only");
    put_uevent(ROOT "/func1_only", "mmc1:e9ea:1", "SDIO_CLASS=07\nSDIO_ID=C8A1:C08D\n");
    expect("func1_only", ROOT "/func1_only", 0);

    make_dir(ROOT "/ultra_w");
    put_uevent(ROOT "/ultra_w", "mmc1:e9ea:1", "SDIO_CLASS=07\nSDIO_ID=C8A1:C08D\n");
    put_uevent(ROOT "/ultra_w", "mmc1:e9ea:2", "SDIO_CLASS=07\nSDIO_ID=C8A1:C18D\nSDIO_REVISION=0.0\n");
    expect("ultra_w", ROOT "/ultra_w", 1);

    make_dir(ROOT "/hidden");
    put_uevent(ROOT "/hidden", ".mmc1:e9ea:2", "SDIO_ID=C8A1:C18D\n");
    expect("hidden", ROOT "/hidden", 0);

    make_dir(ROOT "/no_newline");
    put_uevent(ROOT "/no_newline", "mmc1:e9ea:2", "SDIO_ID=C8A1:C18D");
    expect("no_newline", ROOT "/no_newline", 1);

    make_dir(ROOT "/prefix");
    put_uevent(ROOT "/prefix", "mmc1:e9ea:2", "SDIO_ID=C8A1:C18DX\n");
    expect("prefix", ROOT "/prefix", 0);

    return failures == 0 ? 0 : 1;
}
```

`/tmp/sdio-test/run.sh`（从仓库源文件抽宏与函数，测的是入库代码）：

```bash
#!/usr/bin/env bash
set -euo pipefail
cd /tmp/sdio-test
rm -rf root && mkdir -p root
{ grep -E '^#define LUCKFOX_(SDIO_BUS_DIR|ULTRA_W_SDIO_ID) ' /workspace/custom/custom_main.c || true
  sed -n '/^static int luckfox_sdio_has_id(/,/^}/p' /workspace/custom/custom_main.c; } > helper.inc
gcc -std=gnu99 -Wall -Wextra -Werror -o harness harness.c
./harness
```

复现程序（`/tmp/ll-segv/main.c`，为 `lv_mem_*` 打桩，只编 vendored `lv_ll.c`）：

```c
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include "lvgl/src/misc/lv_ll.h"

void * lv_mem_alloc(size_t size) { return malloc(size); }
void lv_mem_free(void * data) { free(data); }
void * lv_mem_realloc(void * p, size_t n) { return realloc(p, n); }

static void on_segv(int sig) { (void)sig; const char m[] = "CAUGHT SIGSEGV in _lv_ll_remove(&ll, NULL)\n"; write(1, m, sizeof(m) - 1); _exit(139); }

int main(void)
{
    lv_ll_t ll;
    _lv_ll_init(&ll, 16);
    _lv_ll_ins_head(&ll);
    _lv_ll_ins_head(&ll);
    printf("list has 2 nodes: head=%p tail=%p\n", _lv_ll_get_head(&ll), _lv_ll_get_tail(&ll));
    fflush(stdout);
    signal(SIGSEGV, on_segv);
    _lv_ll_remove(&ll, NULL);
    printf("NO CRASH\n");
    return 0;
}
```

```bash
cd /tmp/ll-segv && gcc -I/workspace/lib -I/workspace/lib/lvgl -o ll-segv main.c /workspace/lib/lvgl/src/misc/lv_ll.c && ./ll-segv; echo "exit=$?"
```

双重 `fclose` 复现程序（`/tmp/dblclose/main.c`，host glibc）：

```c
#include <stdio.h>
int main(void)
{
    FILE *file = fopen("/proc/self/status", "r");
    if (file == NULL)
        return 2;
    fclose(file);
    puts("first fclose done");
    fflush(stdout);
    fclose(file);
    puts("second fclose returned");
    return 0;
}
```

```bash
cd /tmp/dblclose && gcc -o dblclose main.c && ./dblclose; echo "exit=$?"
```

- [x] **Step 1:** 写 harness 与 `run.sh`
- [x] **Step 2:** RED：`bash /tmp/sdio-test/run.sh` 编译失败（`luckfox_sdio_has_id` implicit declaration、`LUCKFOX_ULTRA_W_SDIO_ID` undeclared）
- [x] **Step 3:** 加宏
- [x] **Step 4:** 加 `luckfox_sdio_has_id()`
- [x] **Step 5:** GREEN：7 行 PASS，退出码 0
- [x] **Step 6:** 接入判据并去掉读取失败分支的重复 `fclose`
- [x] **Step 7:** 判定表走查（结论见「验证证据」）
- [x] **Step 8:** 本机 glibc 交叉编译与告警门禁

```bash
set -euo pipefail
cd /workspace
unset LUCKFOX_SDK_PATH || true
export GLIBC_COMPILER=/usr/bin/arm-linux-gnueabihf-
rm -rf build install && mkdir -p build && cd build
cmake .. >/dev/null
make -j"$(nproc)" 2>&1 | tee /tmp/build.log >/dev/null
make install >/dev/null
readelf -h ../install/luckfox_lvgl_demo/luckfox_lvgl_demo | grep -E 'ELF32|ARM'
grep -E 'custom_(main|wifi)\.c:.*warning' /tmp/build.log
```

- [x] **Step 9:** 范围检查：`custom/custom_main.c`、`custom/custom_wifi.c`；新增行无 `strcpy`/`strcat`/`sprintf(`/`popen`/`system(`
- [x] **Step 10:** 提交 `fix(custom)`（`custom/custom_main.c`、`custom/custom_wifi.c`）
- [x] **Step 11:** ASan/UBSan 下重跑 harness（先执行 `run.sh` 生成 `helper.inc`）

```bash
set -euo pipefail
cd /tmp/sdio-test
gcc -std=gnu99 -Wall -Wextra -Werror -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all -o harness-san harness.c
rm -rf root && mkdir -p root
ASAN_OPTIONS=detect_leaks=1 ./harness-san
```

### Task 3: `AGENTS.md` 板型判据 gotcha（`docs(agents)`，已完成）

**落地文件（以仓库为准）：** Modify: `AGENTS.md`（文件末尾空行 + 单行段落，末句为「仅旧固件 `model` 为 `Luckfox Pico Ultra W` 时直接视为带 WiFi」，见「与计划的偏离」；`### GitHub Actions（交叉编译门禁）` 中「权威验证仍是物理 …」一句型号写全称）

- [x] **Step 1:** 追加段落
- [x] **Step 2:** 范围检查：只有 `AGENTS.md`，+3/−1 行；`grep -c 'SDIO_ID=C8A1:C18D' AGENTS.md` 为 1
- [x] **Step 3:** 提交 `docs(agents)`

### Task 4: 推送与 CI（已完成）

- [x] **Step 1:** 推送 `cursor/ultra-w-wifi-detect-7dfe`（`--force-with-lease`）
- [x] **Step 2:** PR 的 `pull_request` run：`build-image` dest-lock 复用 dev 签名镜像，glibc 与 uClibc 两行均 success
- [x] **Step 3:** run URL 与 artifact 记入「验证证据」

### Task 5: 真机功能验证（已完成）

**Files:** 无仓库文件。由作者执行，结果见「验证证据」。

**Interfaces:** 消费 run 36249595774 的 uClibc artifact（`luckfox_lvgl_demo_uclibc-buildroot_on-ubuntu-24.04`，包内 `luckfox_lvgl_demo`；代码树与最终提交相同）。

- [x] **Step 1:** 下载 run 36249595774 的 uClibc artifact 并解压，记录 run ID 与 artifact ID；`adb push luckfox_lvgl_demo /root/`，板上 `chmod +x /root/luckfox_lvgl_demo`，确认无其他 GUI 进程占用 `/dev/dri/card0`（如 `pidof luckfox_lvgl_demo` 为空）
- [x] **Step 2:** 路径 A：`/root/luckfox_lvgl_demo; echo "exit=$?"`，主界面出现 WIFI 键，5 键从左到右为 WIFI/PAD/Music/GIF/OFF
- [x] **Step 3:** 路径 A（续）：进入 WIFI 页不崩溃，扫描/连接结果如实记录（`wpa_supplicant` 未运行时记「按键已恢复、WiFi 页功能受固件限制」）；返回主界面后点 OFF，正常退出、屏幕被清、`exit=0`
- [x] **Step 4:** 路径 B：再次启动，**不进 WIFI 页**直接点 OFF，正常退出、屏幕被清、`exit=0`（无 `Segmentation fault`、非 139）
- [x] **Step 5:** 非 W 板、旧固件两项在「验证证据」保持「未实测」

### Task 6: 回填证据与文档提交（已完成）

- [x] **Step 1:** 回填「验证证据」与「与计划的偏离及原因」
- [x] **Step 2:** 提交 `docs(superpowers)` 为 PR 最后一个提交并 `git push --force-with-lease origin cursor/ultra-w-wifi-detect-7dfe`
- [x] **Step 3:** Task 5 真机结果回填「验证证据」并勾选 Task 5，amend `docs(superpowers)` 后 `--force-with-lease` 推送

### Task 7: 修复前对照程序（可选，已完成）

**Files:** 无仓库改动。对照程序只去掉 `wifi_backend_release()` 判空（spec FR7），其余与包含本修复的提交相同，仅用于真机对照，不合并。

**Interfaces:** 消费本 PR 最终代码树与 merge-base `31e3506` 上的 `custom/custom_wifi.c`；产出 `luckfox_lvgl_demo_uclibc_without_timer_guard`（本次构建 sha256 见「验证证据」，可按 Step 1 原样复现）。

- [x] **Step 1:** 构建（工具链与 CI uClibc 行同源，`yuangezhizao/luckfox-pico@dev`，实测 `d4cd3f7`）

```bash
set -euo pipefail
rm -rf /tmp/lp-toolchain
git clone --depth 1 --filter=blob:none --sparse -b dev https://github.com/yuangezhizao/luckfox-pico.git /tmp/lp-toolchain
git -C /tmp/lp-toolchain sparse-checkout set tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf
rm -rf /tmp/nofix && mkdir -p /tmp/nofix/src
git -C /workspace archive HEAD | tar -x -C /tmp/nofix/src
git -C /workspace show 31e3506:custom/custom_wifi.c > /tmp/nofix/src/custom/custom_wifi.c
unset GLIBC_COMPILER || true
cd /tmp/nofix/src && mkdir -p build && cd build
LUCKFOX_SDK_PATH=/tmp/lp-toolchain cmake .. && make -j"$(nproc)"
```

`git -C /workspace archive HEAD` 须在包含本修复的提交上执行。链接时会把工具链内 `crt1.o` 等文件的绝对路径写入二进制，sha256 与工具链路径绑定：要复现「验证证据」中的 sha256，工具链须位于 `/tmp/lp-toolchain`，源码目录不影响结果。

- [x] **Step 2:** 校验

```bash
cd /tmp/nofix/src/build
arm-linux-gnueabihf-readelf -h luckfox_lvgl_demo | grep -E 'Class|Machine'
arm-linux-gnueabihf-readelf -d luckfox_lvgl_demo | grep -E 'NEEDED|RPATH'
arm-linux-gnueabihf-objdump -d --no-show-raw-insn --disassemble=wifi_backend_release luckfox_lvgl_demo
sha256sum luckfox_lvgl_demo
```

- [x] **Step 3:** 交付作者：以 `luckfox_lvgl_demo_uclibc_without_timer_guard` 及同名 `.sha256`、`.zip` 放入 `/workspace/control-artifacts/`（写入 `.git/info/exclude`，不入库），作者从 Cloud Agent workspace 下载。Cursor Artifacts（`/opt/cursor/artifacts/`）网页端对该类型只显示「Preview not available」、无下载入口，不能用作交付渠道
- [x] **Step 4:** 真机路径 B：`/root/luckfox_lvgl_demo_uclibc_without_timer_guard; echo "exit=$?"`，启动后不进 WIFI 页直接 OFF，预期 `Segmentation fault`、`exit=139`、屏幕不清、无 `No space left on device`
- [x] **Step 5:** 真机路径 A：再次启动，先进 WIFI 页再 OFF，预期正常退出、清屏、`exit=0`

---

## Execution Handoff

无剩余施工。Task 1–6 均已完成（非 W 板、旧固件见验证证据「未实测」）；Task 7 修复前对照（可选）已完成。

## 完成情况总结

| 交付物 | 状态 |
|---|---|
| Task 1 真机前置采集（`SDIO_ID=C8A1:C18D`） | ✅ |
| Task 2 `fix(custom)` 判据、`fclose`、`wifi_backend_release()` 判空 | ✅ |
| Task 3 `docs(agents)` 板型判据 gotcha 与型号全称 | ✅ |
| Task 4 推送与 PR CI | ✅ |
| Task 5 Luckfox Pico Ultra W 真机功能（WIFI 键、WIFI 页、退出路径 A/B） | ✅ |
| Task 6 回填证据与 `docs(superpowers)` | ✅ |
| Task 7 修复前对照程序（可选） | ✅ |
| 非 W 板、旧固件 | 未实测 |

**整体结论：** 规格已实施。统一设备树固件上 Luckfox Pico Ultra W 按 `SDIO_ID=C8A1:C18D` 恢复 WIFI 键，WIFI 页可扫描并经 `wpa_cli` 下发配置，退出路径 A/B 均清屏且 `exit=0`；PR CI 两行通过。非 W 板与旧固件无硬件/镜像，仅代码走查，不记作通过。修复前对照程序（Task 7）在同一板子上路径 B 段错误（`exit=139`、不清屏）、路径 A 正常，证实 FR7 判空必要。

---

## 验证证据

通过标准见 spec §6。

| 项 | 期望 | 实测 |
|---|---|---|
| 改前基线（glibc 交叉编译） | 构建成功、ELF32 ARM；记录既存告警 | 成功、ELF32 ARM；`custom_main.c` 既存告警 2 条：`implicit declaration of function 'open'`、`-Wformat-truncation`；全量既存告警 4 条（另 2 条在 `lib/lv_conf.h`、`custom/custom_musicplayer.c`） |
| 镜像核实（artifact 10458251515） | DTB `model`、WiFi 工具与加载链 | `SHA256SUMS` 校验 OK；`boot.img` 两份 FDT 根 `model` 均为 19 字节 `Luckfox Pico Ultra\0`、含 `sdio-pwrseq`；rootfs 有 `/usr/bin/wpa_cli`、`/usr/bin/wpa_supplicant`、`/sbin/udhcpc`、`/etc/wpa_supplicant.conf`，无 `/usr/bin/wifi_bt_init.sh`；oem `insmod_wifi.sh` 第 109–111 行（`#aic8800` 分支）条件为 `model` 含 W 或 `uevent` 含 `C8A1\:C18D`；加载链 `S21appinit` → `RkLunch.sh` → `insmod_ko.sh` → `insmod_wifi.sh` |
| 真机前置采集（Task 1） | `SDIO_ID=C8A1:C18D` 存在 | 通过。`model`=`Luckfox Pico Ultra`；`/sys/bus/sdio/devices` 为 `mmc1:7a8a:1`、`mmc1:7a8a:2`；`:1` 为 `SDIO_CLASS=07`/`SDIO_ID=C8A1:C08D`/`SDIO_REVISION=0.0`，`:2` 为 `SDIO_CLASS=07`/`SDIO_ID=C8A1:C18D`/`SDIO_REVISION=0.0`；`/usr/bin/wpa_cli`、`/usr/bin/wpa_supplicant`、`/sbin/udhcpc` 存在；`/etc/wpa_supplicant.conf` 145 字节；`pidof wpa_supplicant`=`590`；`/var/run/wpa_supplicant/wlan0` 存在 |
| harness 先失败 | 编译报未声明 | `harness.c:37:15: error: implicit declaration of function 'luckfox_sdio_has_id'`、`'LUCKFOX_ULTRA_W_SDIO_ID' undeclared`，`cc1: all warnings being treated as errors` |
| harness 通过 | 7 行 PASS，退出码 0 | `absent`/`empty`/`func1_only`/`ultra_w`/`hidden`/`no_newline`/`prefix` 全 PASS，退出码 0。未覆盖：路径截断（`bus_dir` 21 字节 + `d_name` ≤255 字节 + 8 字节远小于 512，实际输入不可达）、设备目录无 `uevent`（与其他 `fopen` 失败共用同一 `continue`）、sysfs 符号链接（代码不看 `d_type`，`fopen` 透明跟随，由 Task 5 真机覆盖） |
| harness（ASan/UBSan） | 7 行 PASS，无 sanitizer 报告，退出码 0 | host `gcc 13.3.0`：7 行 PASS，无 AddressSanitizer / UndefinedBehaviorSanitizer / LeakSanitizer 输出，退出码 0（`-fno-sanitize-recover=all` 下任一 UB 即中止） |
| 判定表走查 | 5 种输入与 spec §5 一致 | 一致：`Luckfox Pico Ultra W`→1 不检测 SDIO；`Luckfox Pico Ultra`+命中→1；`Luckfox Pico Ultra`+无匹配/空/不存在→0；其他→`exit(EXIT_FAILURE)`；读取失败→`perror`、保持 0、`fclose` 一次 |
| 双重 `fclose`（host glibc 2.39） | 后果属实 | `first fclose done` 后 `free(): double free detected in tcache 2`、`Aborted`，`exit=134`；板上该分支仅在 `fgets` 失败时进入，正常启动不可达，uClibc 未测 |
| 退出段错误复现 | `lv_timer_del(NULL)` 路径崩溃属实 | Task 2 复现程序输出 `list has 2 nodes: head=0x… tail=0x…`、`CAUGHT SIGSEGV in _lv_ll_remove(&ll, NULL)`、`exit=139`（139 由信号处理函数 `_exit(139)` 给出） |
| 本机 glibc 交叉编译 | 成功、ELF32 ARM、无新增告警 | 成功、ELF32 ARM；全量告警 4 条，除 `custom_main.c` 行号后移（113→116、367→409）外与基线一致，`custom_wifi.c` 0 条 |
| PR CI 两行 | glibc / uClibc 均 success，ABI 门禁通过 | run [36295249206](https://github.com/yuangezhizao/luckfox_pico_lvgl_example/actions/runs/36295249206)（代码树与最终提交相同）：`🐳 构建 CI 镜像`、`🛠️ glibc · Ubuntu`、`🛠️ uClibc · Buildroot` 均 success；artifact `luckfox_lvgl_demo_uclibc-buildroot_on-ubuntu-24.04`（ID 10923687433，627076 bytes）、`luckfox_lvgl_demo_glibc-ubuntu_on-ubuntu-24.04`（ID 10922884352，1942569 bytes） |
| 真机运行对象 | 代码树与最终提交相同的 uClibc artifact | run [36249595774](https://github.com/yuangezhizao/luckfox_pico_lvgl_example/actions/runs/36249595774)（代码树与最终提交相同）的 `luckfox_lvgl_demo_uclibc-buildroot_on-ubuntu-24.04`（ID 10908737492），`adb push` 3353724 字节；启动输出 `Mode 0: 480x480 @ 60Hz` |
| 修复前对照程序构建（Task 7） | ELF32 ARM、uClibc ABI | ELF32 ARM；`NEEDED` 为 `libdrm.so.2`、`libcjson.so.1`、`libc.so.0`；`RPATH=/usr/lib`；3353672 字节；sha256 `bf757a3add5f50c627f1275f0c35cc8c097cc98653889f740dcec04b04a04967`；工具链 fork `d4cd3f7` |
| 对照程序与修复版差异 | 仅 `wifi_backend_release()` 判空 | 同法本地构建修复版对照反汇编：修复版为 `ldr r3,[r3]` → `cmp r3, #0` → `beq` → `bl lv_timer_del` → `str #0`；对照版为 `ldr r3,[r3]` → `bl lv_timer_del`。本地修复版 3353672 字节，与 CI artifact 3353724 字节仅差 CI 工具链路径字符串（`/__w/…/crt1.o`） |
| 真机主界面 WIFI 键 | 出现且 5 键布局正常 | 通过：作者目测主界面 5 键从左到右为 WIFI/PAD/Music/GIF/OFF，均完整显示、无遮挡重叠，与 `generated/setup_scr_Main.c` 在 `WIFI_ENABLE == 1` 时不重排的 GUI-Guider 坐标（x=5/100/195/290/385、y=370、90×80）一致；路径 A 经 WIFI 键进入 WIFI 页 |
| 真机 WIFI 页 | 不崩溃；扫描/连接按实际记录 | 不崩溃。扫描列出 `yuangezhizao`、`CU_eE9b`、`Xiaomi_A9E8`、`Tenda_222FD0`；两次连接均经 `wpa_cli v2.6`（`Selected interface 'wlan0'`、`set_network`/`save_config` 回 `OK`）后 `reconfigure` 回 `OK` 并启动 `udhcpc -i wlan0`（`udhcpc: started, v1.36.1`），日志内只见 `broadcasting discover`、未见租约，DHCP 结果不属本修复判据 |
| 真机退出路径 A（进过 WIFI 页后 OFF） | 正常退出、`exit=0` | 通过：屏幕被清、`exit=0`。`cat: write error: No space left on device` 是清屏 `cat /dev/zero > /dev/fb0` 写满 framebuffer 后的 ENOSPC，属预期输出；由 `custom_wifi.c` 以 `system("udhcpc -i wlan0 &")` 启动的 `udhcpc` 在例程退出后仍在后台广播（既存行为，不在本 PR 范围） |
| 真机退出路径 B（不进 WIFI 页直接 OFF） | 正常退出、`exit=0` | 通过：`Mode 0: 480x480 @ 60Hz` 后直接 OFF，屏幕被清、`exit=0`，无 `Segmentation fault` |
| 修复前对照真机（Task 7 路径 B） | 段错误、`exit=139`、不清屏 | 通过：`adb push` 3353672 字节；`Mode 0: 480x480 @ 60Hz` 后直接 OFF，`Segmentation fault (core dumped)`、`exit=139`，屏幕停在最后一帧不清屏，无 `No space left on device` |
| 修复前对照真机（Task 7 路径 A） | 正常退出、清屏 | 通过：先进 WIFI 页再 OFF，`cat: write error: No space left on device`（清屏写满 framebuffer）、`exit=0`，屏幕被清；日志无 SSID 扫描输出，未崩溃即说明 `wifi_update_timer` 已由进入 WIFI 页创建 |
| 非 W 板 | 无 WIFI 键 | 未实测（无硬件） |
| 旧固件（`model = "Luckfox Pico Ultra W"`） | 有 WIFI 键 | 未实测（无镜像） |

## 与计划的偏离及原因

- `AGENTS.md` 段落末句写明旧固件例外（「仅旧固件 `model` 为 `Luckfox Pico Ultra W` 时直接视为带 WiFi」），未采用笼统禁止按 `model` 判定的写法：后者与保留的旧固件分支（spec FR3/D4）矛盾，读者可能据此误删该分支。
- 范围扩大到 `custom/custom_wifi.c` 的 `wifi_backend_release()`：WIFI 键恢复后，启动后不进 WIFI 页直接 OFF 会对从未创建的 `wifi_update_timer` 调用 `lv_timer_del(NULL)` 而段错误（spec §2.1、D7），该路径由本修复变为可达，故判空并置 `NULL`，与判据修复同属 `fix(custom)`；真机验证相应分路径 A/B（Task 5）。
