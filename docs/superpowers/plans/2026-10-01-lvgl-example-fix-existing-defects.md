# Luckfox Pico LVGL Example — 既存缺陷统一修复实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 按 spec §6 的提交序列，在 1 个前置 `test(tests)` 提交之后用 30 个 `fix` 提交修复 spec §3.1、§3.2 的 30 处缺陷，每处先写出在修复前失败的测试，测试与修复同一个提交，最后以 spec 与本 plan 的 `docs(superpowers)` 提交收尾。

**Architecture:** 测试全部放在已有的主机原生工程 `tests/`（gcc + ASan/UBSan，每个 CASE 一个进程），按页面落在 `tests/cases/{main,wifi,sketchpad,music,exit}/`；需要访问 `static` 函数时用 `unit_<对象>.c` `#include` 被测 `.c`；被测路径（`WPA_FILE_PATH`、`MUSIC_DIR_PATH`、`MPV_SOCKET_PATH`）以 `#ifndef` 默认值加测试侧运行期表达式覆盖到各进程的 `tst_tmp_dir()`；编译层缺陷由新增的 `luckfox_add_compile_check()` 以普通 `-I` 加 `-Werror=` 编译判定；32 位 tick 以 `arm-linux-gnueabihf-gcc -U_TIME_BITS -U_FILE_OFFSET_BITS` 编译、`qemu-arm` 运行。

**Tech Stack:** C（gnu99）、CMake ≥ 3.16 与 CTest、主机 gcc 13 + ASan/UBSan/LSan、LVGL 8.3.10、cJSON、`arm-linux-gnueabihf-gcc` 13.3 + `qemu-arm`、uClibc gcc 8.3（`yuangezhizao/luckfox-pico@dev` 工具链）、Python 3（假 `mpv`）、GitHub Actions。

**Spec:** [`docs/superpowers/specs/2026-10-01-lvgl-example-fix-existing-defects-design.md`](../specs/2026-10-01-lvgl-example-fix-existing-defects-design.md)

## Global Constraints

每个 Task 默认包含 spec 全文。硬约束：

- 基线为当前分支与 `origin/dev` 的分叉点；分支 `cursor/fix-existing-defects-spec-6be3`（PR #8）。提交序列严格按 spec §6：前置 1 + 主屏 5 + WiFi 9 + 画板 2 + 音乐 13 + 退出 1 + 最后 1 个 `docs(superpowers)`，共 32 个；本 plan 的 Task 0–30 与之一一对应，Task 31 收尾。
- 一个缺陷一个 `fix` 提交，测试与修复同一提交；不提交会失败的测试（spec D1）。修复前失败与修复后通过的输出分别临时存放在 `$R/<编号>-red.txt` 与 `$R/<编号>-green.txt`（`R=$(mktemp -d)` 创建临时目录），在「验证证据」表回填摘要。
- 行为变更只限 spec §5.2 列出的几项；不做 §3.3 的 7 项暂缓；不清理既有告警，本 PR 新出现的告警只登记在「与计划的偏离及原因」（spec §7.5）。
- 新逻辑放 `custom/`；`generated/` 只改 spec §5.6 列出的 4 个文件的调用处或一两行（D2）；现有公开接口仅 `music_scan_list` 改调用参数，三个 WiFi 无参函数在 `custom.h` 中的声明补 `(void)` 完整原型，`custom_wifi.c` 中的定义保持 `()` 不变（D20）。
- 被测路径宏只当 `const char *` 表达式使用，经 `snprintf("%s…")` 拼接，不做字面量拼接、不 `sizeof`（spec §2.6）。
- 用到固定路径的用例一律改到各自的 `tst_tmp_dir()`；同一可执行文件的多个 CASE 各是一个进程（spec §2.6）。
- 测试代码以 `-Wall -Wextra -Werror` 编译，不用 `strcpy`/`strcat`/`sprintf`；被测源码与 `unit_*.c` 以 `-w` 编译。
- `.mutants` 的 `file` 只能在 `custom/` 下；`generated/` 与 `lib/lv_conf.h` 的改动只由 ctest 验收（spec §2.6）。
- roller 选中用例用 `alarm(5)` 终止死循环；其余死循环类用例默认靠 `luckfox_add_test()` 固定的 `TIMEOUT 120` 判失败；画板 `alloc_failure` 用例另设 `alarm(5)`，以 `SIGALRM` 在约 5 秒时终止断言死循环。
- 提交用 `git cz` 格式 `type(scope): emoji 主题`：前置提交 `test(tests): ✅ …`，修复提交 `fix(<页面>): 🐛 页面 [序号/总数] 主题`（如 `fix(wifi): 🐛 WiFi [3/9] 写配置时只替换 ssid= 与 psk= 行`）（scope 取 `main`、`wifi`、`sketchpad`、`music`、`exit`），body 用 `-` 列 why（根因、关键决策与被否决的替代），末行 `详见 spec §X`；不写实现罗列与过程来源；注释与文档单句不硬折行。
- 提交序列属于 `cursor/fix-existing-defects-spec-6be3`；spec 与 plan 仅由 Task 31 的最后一个 `docs(superpowers)` 提交交付（spec §6）。

## File Structure

| 文件 | 职责 | Task |
|---|---|---|
| `tests/CMakeLists.txt` | `luckfox_add_compile_check()`；`custom_wifi.c`、`custom_musicplayer.c`、`custom_fb.c` 的路径与上限覆盖；`add_subdirectory(cases/{sketchpad,music,exit})`；UNIT 目标加 `tests/support` 包含路径 | 0、6、7、15、17、27、30 |
| `tests/tools/compile_check.cmake` | 逐个 `-c -o /dev/null` 编译源文件，任一失败即退出非 0 | 0 |
| `tests/support/fake_fs.{h,c}` | `tst_wpa_conf_path()`、`tst_music_dir()`、`tst_mpv_socket_path()`、`tst_fake_exec()`、`tst_fb_clear_max_bytes` | 6、17、27、30 |
| `tests/cases/main/` | `compile_fcntl_custom.c`、`test_tick.c`、`{test,unit}_tick_arm32.c`、`{test,unit}_clock.c`、`test_calendar.c`、`test_sysinfo.c`、`tick.mutants`、`clock.mutants`、`sysinfo.mutants` | 1–5 |
| `tests/cases/wifi/` | `wifi_env.h`（假命令与配置）、`unit_wifi.c`、`test_{validate,load,conf_write,conf_get,scan,popen,timer,dhcp}.c`、对应 `.mutants` | 6–14 |
| `tests/cases/sketchpad/` | `CMakeLists.txt`、`{test,unit}_canvas.c`、`test_stroke.c`、`canvas.mutants`、`stroke.mutants` | 15–16 |
| `tests/cases/music/` | `CMakeLists.txt`、`music_env.h`、`fake_mpv.py`（可执行）、`test_{list,mpv_cmd,roller,empty_list,mpv_send,monitor,player}.c`、对应 `.mutants` | 17–29 |
| `tests/cases/exit/` | `CMakeLists.txt`、`test_fb_clear.c`、`fb_clear.mutants` | 30 |
| `tests/README.md`、`AGENTS.md` | 测试工具、ARM32、路径覆盖、音乐与退出页登记；常规 UI 测试的初始化说明；最终告警基线 | 0、2、6、7、17、27、30 |
| `custom/custom_tick.h`（新增）、`custom/custom.h`、`lib/lv_conf.h`、`custom/custom_brightness.c` | ⑮ 声明与头文件 | 1 |
| `custom/custom_main.c` | tick、正午、`popen` 判空、删同步 `mpv` | 2、3、5、28 |
| `generated/widgets_init.c` | 日历 | 4 |
| `custom/custom_wifi.c`、`generated/setup_scr_WIFI.c` | 校验与提示、配置读写、扫描、`popen`、定时器、`udhcpc` | 6–14 |
| `custom/custom_sketchpad.c`、`generated/setup_scr_Sketchpad.c` | 画布缓冲区与坐标 | 15–16 |
| `custom/custom_musicplayer.c`、`generated/setup_scr_Music_player.c` | 音乐页 13 处 | 17–29 |
| `custom/custom_fb.{h,c}`（新增）、`src/main.c` | OFF 退出清屏 | 30 |

## 缺陷、Task 与失败测试对照

ctest 名与 spec §5 逐一对应；`main.tick.monotonic_wrap` 为 32 位（标签 `arm32`），`*.compile.*` 由 `luckfox_add_compile_check()` 注册。

| Task | 缺陷 | ctest |
|---|---|---|
| 1 | ⑮ | `main.compile.no_implicit_decl` |
| 2 | ⑬ | `main.tick.monotonic_wrap`、`main.tick.step_back_host` |
| 3 | ⑫ | `main.clock.meridiem` |
| 4 | ⑭ | `main.calendar.highlight_survives`、`main.calendar.no_slash`、`main.calendar.one_slash` |
| 5 | ⑯ | `main.sysinfo.popen_failure` |
| 6 | ⑳ | `wifi.validate.rules`、`wifi.load.invalid_no_wpa_call`、`wifi.load.dropdown_overlong`、`wifi.validate.utf8`、`wifi.load.dropdown_utf8`、`wifi.load.edit_hides_hint`、`wifi.compile.prototypes` |
| 7 | ㉑ | `wifi.conf_write.same_dir_atomic`、`wifi.conf_write.io_failures` |
| 8 | C3 | `wifi.conf_write.only_ssid_psk_lines` |
| 9 | C5 | `wifi.conf_write.brace_in_value`、`wifi.conf_write.crlf_block_end` |
| 10 | ㉕ | `wifi.conf_get.long_and_quoted` |
| 11 | ㉒ | `wifi.scan.parse`、`wifi.scan.parse_all_discarded`、`wifi.scan.parse_cap`、`wifi.scan.append_truncated`、`wifi.scan.append_error` |
| 12 | ㉓ + C4 | `wifi.popen.failure` |
| 13 | ㉔（定时器） | `wifi.timer.inactive_screen` |
| 14 | ㉔（`udhcpc`） | `wifi.dhcp.single_instance` |
| 15 | ⑱ | `sketchpad.canvas.buffer_not_on_stack`、`sketchpad.canvas.buffer_full_write`、`sketchpad.canvas.alloc_failure` |
| 16 | ⑲ | `sketchpad.stroke.canvas_coordinates` |
| 17 | ② | `music.list.options_over_255`、`music.list_alloc.options_alloc_failure` |
| 18 | C2 | `music.list.newline_name_skipped` |
| 19 | ③ | `music.mpv_cmd.escaping_and_length` |
| 20 | ④ | `music.roller.select_long_name` |
| 21 | ⑧ | `music.list.missing_dir` |
| 22 | C1 | `music.empty_list.no_crash`、`music.empty_list.alloc_failure_no_crash` |
| 23 | ⑦ | `music.mpv_send.peer_closed` |
| 24 | ⑤ | `music.monitor.no_leak` |
| 25 | ⑥ | `music.monitor.full_buffer_line` |
| 26 | ⑨ | `music.compile.return_type` |
| 27 | ① | `music.player.exec_failure`、`music.mpv_send.negative_fd_no_send` |
| 28 | ⑩ | `music.player.slow_socket`、`music.player.timeout_reaps_child`、`music.player.mpv_missing_fast`、`music.player.stale_socket` |
| 29 | ⑪ | `music.player.stdin_kept` |
| 30 | ⑰ | `exit.fb_clear.enospc_silent`、`exit.fb_clear.cap`、`exit.fb_clear.open_failure`、`exit.fb_clear.zero_write_silent`、`exit.fb_clear.eintr_retries` |

## 变体登记

最终树有 78 条用例（21 条基线 + 57 条新增）与 62 个变体（12 个原有 + 50 个新增）。变体按页面计：主屏 16、WiFi 22、画板 2、音乐 18、退出 4。下表逐个登记 `tests/cases/*/*.mutants` 的 id；对应测试正则以各清单的 `tests:` 为准。

| 清单 | 变体 id |
|---|---|
| `tests/cases/exit/fb_clear.mutants` | `EXIT-17a`、`EXIT-17b`、`EXIT-17c`、`EXIT-17d` |
| `tests/cases/main/brightness.mutants` | `M1`、`M2`、`M3`、`M4`、`M5`、`M6`、`M7`、`M10`、`M11` |
| `tests/cases/main/brightness_arm32.mutants` | `M12` |
| `tests/cases/main/clock.mutants` | `MAIN-12a`、`MAIN-12b` |
| `tests/cases/main/sdio_detect.mutants` | `S1` |
| `tests/cases/main/sysinfo.mutants` | `MAIN-16` |
| `tests/cases/main/tick.mutants` | `MAIN-13a`、`MAIN-13b` |
| `tests/cases/music/connect.mutants` | `MUSIC-10a`、`MUSIC-10b`、`MUSIC-12`、`MUSIC-10c` |
| `tests/cases/music/empty_list.mutants` | `MUSIC-C1` |
| `tests/cases/music/list.mutants` | `MUSIC-2`、`MUSIC-13` |
| `tests/cases/music/monitor.mutants` | `MUSIC-5`、`MUSIC-6` |
| `tests/cases/music/mpv_cmd.mutants` | `MUSIC-3` |
| `tests/cases/music/mpv_send.mutants` | `MUSIC-7`、`MUSIC-14` |
| `tests/cases/music/newline.mutants` | `MUSIC-C2` |
| `tests/cases/music/opendir.mutants` | `MUSIC-8` |
| `tests/cases/music/player.mutants` | `MUSIC-1` |
| `tests/cases/music/return.mutants` | `MUSIC-9` |
| `tests/cases/music/roller.mutants` | `MUSIC-4` |
| `tests/cases/music/stdin.mutants` | `MUSIC-11` |
| `tests/cases/sketchpad/canvas.mutants` | `SKETCH-18` |
| `tests/cases/sketchpad/stroke.mutants` | `SKETCH-19` |
| `tests/cases/wifi/backend_release.mutants` | `W1` |
| `tests/cases/wifi/conf_get.mutants` | `WIFI-25` |
| `tests/cases/wifi/conf_match.mutants` | `WIFI-C3`、`WIFI-C5a`、`WIFI-C5b`、`WIFI-C5c` |
| `tests/cases/wifi/conf_write.mutants` | `WIFI-21a`、`WIFI-21b` |
| `tests/cases/wifi/dhcp.mutants` | `WIFI-24d` |
| `tests/cases/wifi/popen.mutants` | `WIFI-23`、`WIFI-C4` |
| `tests/cases/wifi/scan.mutants` | `WIFI-22a`、`WIFI-22b`、`WIFI-22c` |
| `tests/cases/wifi/timer.mutants` | `WIFI-24t` |
| `tests/cases/wifi/validate.mutants` | `WIFI-20a`、`WIFI-20b`、`WIFI-20c`、`WIFI-20utf8`、`WIFI-20cont`、`WIFI-20hide`、`WIFI-20length` |

## 通用命令

以下命令块在各 Task 中按名称引用，均在仓库根目录执行；先设 `BASE_REV=$(git merge-base origin/dev HEAD)` 定位基线。

**[H] 主机全量（含 32 位）**

```bash
cmake -S tests -B build-tests -DLUCKFOX_TESTS_ARM32=ON >/dev/null && cmake --build build-tests -j"$(nproc)" && ctest --test-dir build-tests --output-on-failure -j"$(nproc)"
```

**[H1] 单个对象**（`$RE` 为 ctest 正则；每次执行创建临时目录 `$R`，`$ID` 取 `<编号>-red` 或 `<编号>-green`，输出临时存放在 `$R/$ID.txt`，摘要填入「验证证据」表）

```bash
R=$(mktemp -d)
cmake -S tests -B build-tests -DLUCKFOX_TESTS_ARM32=ON >/dev/null && cmake --build build-tests -j"$(nproc)" 2>&1 | tail -20; ctest --test-dir build-tests --output-on-failure -R "$RE" 2>&1 | tee "$R/$ID.txt"
```

**[M] 改坏检验**（`$IDS` 为本行列出的新增或回归变体 id；本 PR 新增的 id 一律带页面前缀 `MAIN-`、`WIFI-`、`SKETCH-`、`MUSIC-`、`EXIT-`，与既有 `.mutants` 的 `M1`–`M7`、`M10`–`M12`、`S1`、`W1` 全仓不重名：`mutate.sh` 遇重名 id 在解析阶段即以退出码 2 结束）

```bash
MUTATE_WORK=/tmp/luckfox-mutate-fix tests/tools/mutate.sh $IDS
```

**[X] 两条交叉编译**（构建目录在仓库外；uClibc 工具链按 `AGENTS.md` 稀疏检出到 `/tmp/luckfox-pico`）

```bash
[ -d /tmp/luckfox-pico/tools/linux/toolchain ] || { git clone -q --depth 1 --filter=blob:none --sparse -b dev https://github.com/yuangezhizao/luckfox-pico.git /tmp/luckfox-pico && git -C /tmp/luckfox-pico sparse-checkout set tools/linux/toolchain; }
env -u LUCKFOX_SDK_PATH GLIBC_COMPILER=/usr/bin/arm-linux-gnueabihf- cmake -S . -B /tmp/build-glibc >/dev/null && cmake --build /tmp/build-glibc --clean-first -j"$(nproc)" > /tmp/build-glibc.log 2>&1; echo "glibc rc=$?"
env -u GLIBC_COMPILER LUCKFOX_SDK_PATH=/tmp/luckfox-pico cmake -S . -B /tmp/build-uclibc >/dev/null && cmake --build /tmp/build-uclibc --clean-first -j"$(nproc)" > /tmp/build-uclibc.log 2>&1; echo "uclibc rc=$?"
grep -c 'warning:' /tmp/build-glibc.log /tmp/build-uclibc.log
```

Expected（[X]）：两条 `rc=0`；告警数与 `AGENTS.md` 的当前基线 glibc 119、uClibc 118 比较，不得新增告警；各历史检查点在其对应 Task 的提交上执行 [X] 并记录告警数。

---

### Task 0: `luckfox_add_compile_check()`（`test(tests)`）

**Files:** Create `tests/tools/compile_check.cmake`；Modify `tests/CMakeLists.txt`、`tests/README.md`。

**Interfaces:**
- Produces: `luckfox_add_compile_check(<用例> FLAGS <选项>... SOURCES <文件>...)`，注册 ctest `<类别>.compile.<用例>`（类别取所在目录名），标签 `<类别>;compile`，`TIMEOUT 120`；Task 1（⑮）与 Task 26（⑨）使用。

- [x] **Step 1: 写 `tests/tools/compile_check.cmake`**

落地：[tests/tools/compile_check.cmake](../../../tests/tools/compile_check.cmake)、[tests/CMakeLists.txt](../../../tests/CMakeLists.txt)。

- [x] **Step 2: 在 `tests/CMakeLists.txt` 的 `luckfox_add_test()` 之后加函数**

落地：[tests/tools/compile_check.cmake](../../../tests/tools/compile_check.cmake)、[tests/CMakeLists.txt](../../../tests/CMakeLists.txt)。

- [x] **Step 3: 自检（临时注册，不提交）**：在 `tests/cases/main/CMakeLists.txt` 末尾临时加两行。

```cmake
luckfox_add_compile_check(selftest_ok FLAGS -Werror=implicit-function-declaration SOURCES ${LUCKFOX_ROOT}/custom/custom_brightness.c)
luckfox_add_compile_check(selftest_bad FLAGS -Werror=implicit-function-declaration SOURCES ${LUCKFOX_ROOT}/lib/lvgl/src/hal/lv_hal_tick.c)
```

Run: `RE='compile\.selftest'; ID=0-selftest` 后执行 [H1]；再 `ctest --test-dir build-tests -R compile.selftest_bad -V | grep -c -- '-isystem'`。
Expected：`main.compile.selftest_ok` 通过；`main.compile.selftest_bad` 失败，输出含 `implicit declaration of function 'custom_tick_get'` 与 `FAIL …lv_hal_tick.c`；`-isystem` 计数 0。之后删去这两行。

- [x] **Step 4: 全量**：执行 [H]。Expected：21 条全过（新函数尚无使用方）。

- [x] **Step 5: 提交** `test(tests): ✅ 新增 luckfox_add_compile_check() 编译检查注册函数`；body：`- 编译层缺陷（缺 return、隐式声明）以 -Werror= 编译失败作为失败测试，需要能注册成 ctest 的编译检查`、`- 直接读 LUCKFOX_INCLUDE_DIRS 生成普通 -I：继承 luckfox_app 的 SYSTEM 包含目录会变成 -isystem，GCC 不报系统头里展开的宏，lv_conf.h 的 custom_tick_get 会漏检`、`详见 spec D19`。

---

## 主屏与时间（Task 1–5）

### Task 1: ⑮ 隐式声明（`fix(main)`）

**Files:** Create `custom/custom_tick.h`、`tests/cases/main/compile_fcntl_custom.c`；Modify `custom/custom.h`、`custom/custom_brightness.c`、`lib/lv_conf.h`、`tests/cases/main/CMakeLists.txt`。

**Interfaces:**
- Consumes: `luckfox_add_compile_check()`（Task 0）。
- Produces: `custom/custom_tick.h` 声明 `uint32_t custom_tick_get(void);`；`custom/custom.h` 包含 `<fcntl.h>` 与 `"custom_tick.h"`，此后各 `unit_` 文件可同时包含 `<fcntl.h>`。

- [x] **Step 1: 写失败测试**：`tests/cases/main/compile_fcntl_custom.c`

落地：[tests/cases/main/](../../../tests/cases/main/)、[custom/custom_tick.h](../../../custom/custom_tick.h)、[custom/custom.h](../../../custom/custom.h)、[custom/custom_brightness.c](../../../custom/custom_brightness.c)、[lib/lv_conf.h](../../../lib/lv_conf.h)。

`tests/cases/main/CMakeLists.txt` 追加：

落地：[tests/CMakeLists.txt](../../../tests/CMakeLists.txt)、[tests/cases/main/CMakeLists.txt](../../../tests/cases/main/CMakeLists.txt)。

- [x] **Step 2: 确认先红**：`RE='^main\.compile\.no_implicit_decl$'; ID=15-red` 后执行 [H1]。
Expected：失败；三个源文件均为 `FAIL`：`custom_main.c` 报 `implicit declaration of function 'open'`（`custom_main.c:117`），`lv_hal_tick.c` 报 `implicit declaration of function 'custom_tick_get'`，`compile_fcntl_custom.c` 报 `redefinition of 'struct flock'`。

- [x] **Step 3: 修复**

`custom/custom_tick.h`：

落地：[tests/cases/main/](../../../tests/cases/main/)、[custom/custom_tick.h](../../../custom/custom_tick.h)、[custom/custom.h](../../../custom/custom.h)、[custom/custom_brightness.c](../../../custom/custom_brightness.c)、[lib/lv_conf.h](../../../lib/lv_conf.h)。

`lib/lv_conf.h` 中 `#define LV_TICK_CUSTOM_INCLUDE <stdint.h>` 改为 `#define LV_TICK_CUSTOM_INCLUDE "custom_tick.h"`（其余不动）；`custom/custom.h:18` 的 `#include <linux/fcntl.h>` 改为 `#include <fcntl.h>`；`custom/custom.h:58` 的 `uint32_t custom_tick_get(void);` 改为 `#include "custom_tick.h"`；`custom/custom_brightness.c:10` 的注释改为 `/* sysfs 用 stdio 读写。 */`。

- [x] **Step 4: 确认转绿**：`ID=15-green` 执行 [H1]，三个源文件均 `PASS`；再执行 [H]，全部通过；执行 [X]，两条 `rc=0`（uClibc 下 `custom/*.c` 包含 `<fcntl.h>` 可编译）。

- [x] **Step 5: 提交** `fix(main): 🐛 主屏 [1/5] 补全 custom_tick_get 与 open 的声明`；body：`- lv_conf.h 的 LV_TICK_CUSTOM_INCLUDE 指向 <stdint.h>，lv_hal_tick.c 看不到 custom_tick_get 的声明；custom_main.c 用 open 却没有 <fcntl.h>`、`- custom.h 改含 <fcntl.h> 而不是在 custom_main.c 另加：两者同时包含会重复定义 struct flock；否决给 open 写局部 extern（绕开冲突而不解决）`、`- 声明只留在新增的 custom_tick.h 一处`、`详见 spec D8`。

### Task 2: ⑬ tick 改单调时钟（`fix(main)`）

**Files:** Create `tests/cases/main/test_tick.c`、`tests/cases/main/test_tick_arm32.c`、`tests/cases/main/tick.mutants`、`tests/cases/main/unit_tick_arm32.c`；Modify `custom/custom_main.c`、`tests/README.md`、`tests/cases/main/CMakeLists.txt`。

**Interfaces:**
- Consumes: `custom/custom_tick.h`（Task 1）。
- Produces: `uint32_t custom_tick_get(void)` 基于 `CLOCK_MONOTONIC`；ctest `main.tick.step_back_host`、`main.tick.monotonic_wrap`（标签 `main;arm32`）。

- [x] **Step 1: 写主机用例 `tests/cases/main/test_tick.c`**（只桩墙钟，`CLOCK_MONOTONIC` 保持真实；等待 20 ms 后同时断言 tick 前进，防止恒为 0 的实现漏检）

落地：[tests/cases/main/](../../../tests/cases/main/)、[custom/custom_main.c](../../../custom/custom_main.c)。

- [x] **Step 2: 写 32 位用例**：`unit_tick_arm32.c` 只有一行 `#include "custom_main.c"`（`-ffunction-sections` 加 `--gc-sections` 丢弃 `custom_tick_get` 以外的函数及其 LVGL、DRM 依赖）；`test_tick_arm32.c`：

落地：[tests/cases/main/](../../../tests/cases/main/)、[custom/custom_main.c](../../../custom/custom_main.c)。

`tests/cases/main/CMakeLists.txt`：`luckfox_add_test(tick CASES step_back_host)`；在 `if(LUCKFOX_ARM32_ENABLED)` 块内仿照 `main_brightness_arm32` 追加（两个编译命令都带 `-U_TIME_BITS -U_FILE_OFFSET_BITS`）：

落地：[tests/CMakeLists.txt](../../../tests/CMakeLists.txt)、[tests/cases/main/CMakeLists.txt](../../../tests/cases/main/CMakeLists.txt)。

- [x] **Step 3: 确认先红**：`RE='^main\.tick\.'; ID=13-red` 执行 [H1]。
Expected：`main.tick.step_back_host` 失败（`(uint32_t)(b - a) < 1000u`，旧实现基于墙钟，差值回绕为约 4.29×10^9）；`main.tick.monotonic_wrap` 中 `sizeof(time_t) == 4` 与 Δ=1 s、亚秒、板上量级 Δ=1 s 的断言 `PASS`，跨 2148 s、3000 s、50 天、板上量级 Δ=1 小时的断言 `FAIL`。若 32 位目标在链接阶段报 `custom_main.c` 其他函数的未定义引用，停下汇报（`--gc-sections` 假设不成立）。

- [x] **Step 4: 修复** `custom/custom_main.c` 的 `custom_tick_get()` 整体替换（文件顶部补 `#include <stdbool.h>`、`#include <time.h>`，已有则不加）：

落地：[tests/cases/main/](../../../tests/cases/main/)、[custom/custom_main.c](../../../custom/custom_main.c)。

- [x] **Step 5: 确认转绿与改坏**：`ID=13-green` 执行 [H1]，两条通过；`tests/cases/main/tick.mutants`：

落地：[tests/cases/main/](../../../tests/cases/main/)。

`IDS='MAIN-13a MAIN-13b'` 执行 [M]。Expected：`MAIN-13a` 被 `main.tick.monotonic_wrap` 抓住，`MAIN-13b` 被两条都抓住，退出 0。执行 [X]：uClibc `rc=0`（`clock_gettime` 无需 `-lrt`；若报未定义引用，在顶层 `CMakeLists.txt` 的 `target_link_libraries` 补 `rt` 并登记偏离）。

- [x] **Step 6: 提交** `fix(main): 🐛 主屏 [2/5] custom_tick_get 改用单调时钟与 64 位毫秒`；body：`- 32 位 time_t 下 tv_sec * 1000000 在 long 中溢出，板上 tick 每约 71.6 分钟回跳约 4.29×10^6 ms`、`- 改 CLOCK_MONOTONIC：基于墙钟时 NTP 校时会让 tick 突变；否决只补 uint64_t 转换（仍基于墙钟）`、`- 起点用独立 bool 标志，不拿 start_ms == 0 当哨兵`、`- 32 位测试加 -U_TIME_BITS -U_FILE_OFFSET_BITS：Ubuntu armhf gcc 默认 64 位 time_t，不加则旧实现不溢出`、`详见 spec D8、§2.4`。

### Task 3: ⑫ 正午显示 AM（`fix(main)`）

**Files:** Create `tests/cases/main/clock.mutants`、`tests/cases/main/test_clock.c`、`tests/cases/main/unit_clock.c`；Modify `custom/custom_main.c`、`tests/cases/main/CMakeLists.txt`。

**Interfaces:**
- Produces: `unit_clock.c` 导出 `void tst_time_update(void)`（调用 `static _time_update()`）；ctest `main.clock.meridiem`。

- [x] **Step 1: 写失败测试**：`unit_clock.c`

落地：[tests/cases/main/](../../../tests/cases/main/)、[custom/custom_main.c](../../../custom/custom_main.c)。

`test_clock.c`：

落地：[tests/cases/main/](../../../tests/cases/main/)、[custom/custom_main.c](../../../custom/custom_main.c)。

`time()` 只在本可执行文件里被覆盖；`_time_update()` 会调用 `lv_label_set_text_fmt()` 更新日期标签，因此须先 `tst_app_init(480, 0, 0); tst_app_setup_ui();`，否则会因空控件访问触发 UBSan，无法检查小时与 AM/PM。若 `Main_digital_clock_1_hour_value` 的类型不是 `int`，按 `custom_main.c` 的定义改 `extern`。CMake：`luckfox_add_test(clock UNIT unit_clock.c CASES meridiem)`。

- [x] **Step 2: 确认先红**：`RE='^main\.clock\.'; ID=12-red` 执行 [H1]。Expected：0 点 `got 0, want 12`，12 点 `strcmp(…, "PM") == 0` 失败；11、13、23 点通过。

- [x] **Step 3: 修复**：`_time_update()` 中从 `Main_digital_clock_1_hour_value = tm_info->tm_hour;` 到 `if/else` 结束的整段换成：

落地：[tests/cases/main/](../../../tests/cases/main/)、[custom/custom_main.c](../../../custom/custom_main.c)。

`Main_digital_clock_1_meridiem` 定义为 `char …[] = "AM"`（`custom_main.c:42`），`sizeof` 为 3；`clock_count_12()` 与 vendored LVGL 不改。

- [x] **Step 4: 转绿与改坏**：`ID=12-green` 执行 [H1]；`clock.mutants`：

落地：[tests/cases/main/](../../../tests/cases/main/)。

`IDS='MAIN-12a MAIN-12b'` 执行 [M]，均 `KILLED`。

- [x] **Step 5: 提交** `fix(main): 🐛 主屏 [3/5] 修正正午与零点的 12 小时制显示`；body：`- 以 tm_hour > 12 判下午，12:00–12:59 显示 AM、0:00–0:59 显示 0`、`- 小时取 (tm_hour + 11) % 12 + 1，meridiem 改 snprintf；每秒走字的 clock_count_12() 本身正确且每 5 s 重新校准，vendored LVGL 不改`、`详见 spec §5.1`。

### Task 4: ⑭ 日历高亮数组与日期解析（`fix(main)`）

**Files:** Create `tests/cases/main/test_calendar.c`；Modify `generated/widgets_init.c`、`tests/cases/main/CMakeLists.txt`。

**Interfaces:**
- Consumes: `tst_app_init()`、`tst_app_setup_ui()`（`tests/support/app_env.h`）。
- Produces: ctest `main.calendar.highlight_survives`、`main.calendar.no_slash`、`main.calendar.one_slash`。

- [x] **Step 1: 写失败测试** `test_calendar.c`

落地：[tests/cases/main/](../../../tests/cases/main/)、[generated/widgets_init.c](../../../generated/widgets_init.c)。

三个用例都是独立进程且不先创建日历（静态的 `Main_datetext_1_calendar` 保持 NULL，否则函数在 `widgets_init.c:108` 直接返回）。若 `tst_app_setup_ui()` 后 `lv_layer_top()` 已有子对象，`malformed` 改为比较调用前后的子对象数并在「偏离」登记。CMake：`luckfox_add_test(calendar CASES highlight_survives no_slash one_slash)`，并加 `set_tests_properties(main.calendar.highlight_survives PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_stack_use_after_return=1")`：gcc 默认以 `-fsanitize-address-use-after-return=runtime` 插桩，运行期打开后局部数组分配在假栈上，函数返回后读取必报 `stack-use-after-return`，不依赖 libasan 的默认值；指针是否落在线程栈范围内区分不了修复前后（假栈不在线程栈内）。

- [x] **Step 2: 确认先红**：`RE='^main\.calendar\.'; ID=14-red` 执行 [H1]。Expected：`highlight_survives` 先打印标签断言 `FAIL`（标签变为 `2023`），随后读高亮数组时 ASan 报 `stack-use-after-return`（READ，地址位于 `Main_datetext_1_init_calendar` 的已返回栈帧）并终止进程；`no_slash`、`one_slash` 在 `atoi(NULL)` 处由 UBSan 报错或 SEGV，进程非 0 退出。

- [x] **Step 3: 修复** `generated/widgets_init.c`：`#include <stdlib.h>` 后补 `#include <stdio.h>` 与 `#include <string.h>`（已有则不加）；`Main_datetext_1_init_calendar` 的 `if` 块开头改为：

落地：[tests/cases/main/](../../../tests/cases/main/)、[generated/widgets_init.c](../../../generated/widgets_init.c)。

删去原来位于 `lv_obj_set_size(...)` 之后的三次 `strtok` 与局部 `lv_calendar_date_t highlighted_days[1];`，其余语句（`set_size`、`set_showed_date`、`highlighted_days[0]` 赋值、对齐、事件、箭头头部）保持原顺序。

- [x] **Step 4: 转绿**：`ID=14-green` 执行 [H1]，三条通过（`generated/` 不配 `.mutants`）。

- [x] **Step 5: 提交** `fix(main): 🐛 主屏 [4/5] 日历高亮数组改静态并在建日历前解析日期`；body：`- highlighted_days 是局部数组，LVGL 只保存其指针，函数返回后日历读到失效栈；strtok 直接改写日期标签文本`、`- 三次 strtok 前移到置顶层可点击与 lv_calendar_create 之前，任一为 NULL 直接返回，畸形文本不调用 atoi，也不留下半成品日历和一直可点击的顶层`、`详见 spec §5.1`。

### Task 5: ⑯ 主屏 `popen` 判空（`fix(main)`）

**Files:** Create `tests/cases/main/sysinfo.mutants`、`tests/cases/main/test_sysinfo.c`；Modify `custom/custom_main.c`、`tests/cases/main/CMakeLists.txt`。

- [x] **Step 1: 写失败测试** `test_sysinfo.c`

落地：[tests/cases/main/](../../../tests/cases/main/)、[custom/custom_main.c](../../../custom/custom_main.c)。

CMake：`luckfox_add_test(sysinfo CASES popen_failure)`。

- [x] **Step 2: 确认先红**：`RE='^main\.sysinfo\.'; ID=16-red` 执行 [H1]。Expected：`fgets(…, NULL)` 处由 UBSan 报 `null pointer passed as argument 3` 并以非 0 退出；未被非空参数检查提前截获时，ASan 报 `SEGV on unknown address`。

- [x] **Step 3: 修复**：`luckfox_get_system_info()` 的判空分支改为

落地：[tests/cases/main/](../../../tests/cases/main/)、[custom/custom_main.c](../../../custom/custom_main.c)。

- [x] **Step 4: 转绿与改坏**：`ID=16-green` 执行 [H1]；`sysinfo.mutants`：

落地：[tests/cases/main/](../../../tests/cases/main/)。

`IDS=MAIN-16` 执行 [M]，`KILLED`。

- [x] **Step 5: 提交** `fix(main): 🐛 主屏 [5/5] 系统信息 popen 失败时不再读空指针`；body：`- popen 失败后仍 fgets(fp=NULL)；只在 fork 失败时可达，属防御性修复`、`- 失败按非 Ubuntu 处理（返回 0），与读不到 Ubuntu 的分支一致`、`详见 spec D18`。

### 主屏回归检查点

- [x] 执行 [H]：全部通过，`ctest -N -L arm32` 列出 `main.brightness_arm32.conversion` 与 `main.tick.monotonic_wrap`。
- [x] 执行 [X]：两条 `rc=0`，告警数记入「验证证据」。
- [x] `IDS='MAIN-13a MAIN-13b MAIN-12a MAIN-12b MAIN-16'` 执行 [M]：5 个 `KILLED`、退出 0。
- [x] 推送：`git push -u origin cursor/fix-existing-defects-spec-6be3`。

## WiFi（Task 6–14）

WiFi 用例的公共前提：`setup_scr_WIFI(&guider_ui)` 会调用 `wifi_backend_init()` 与 `wifi_app_init()`，后者读 `WPA_FILE_PATH` 并回填文本框；`_wifi_conf_load()` 会操作 `WIFI_wifi_log_img`、`WIFI_loaded_wifi_label`，所以调用它之前必须先 `tst_app_init(480, 1, 0)` 并 `setup_scr_WIFI(&guider_ui)`。Load 按钮不依赖生成代码里的按钮字段名：测试自建一个按钮，挂上 `WIFI_load_btn_event_handler` 后发 `LV_EVENT_RELEASED`。

### Task 6: ⑳ SSID/密码校验与提示标签（`fix(wifi)`）

**Files:** Create `tests/cases/wifi/compile_prototypes.c`、`tests/cases/wifi/test_load.c`、`tests/cases/wifi/test_validate.c`、`tests/cases/wifi/validate.mutants`、`tests/cases/wifi/wifi_env.h`、`tests/cases/wifi/wifi_ui.h`；Modify `custom/custom.h`、`custom/custom_wifi.c`、`generated/setup_scr_WIFI.c`、`tests/CMakeLists.txt`、`tests/README.md`、`tests/cases/wifi/CMakeLists.txt`、`tests/support/fake_fs.c`、`tests/support/fake_fs.h`。

**Interfaces:**
- Produces: `const char *wifi_input_check(const char *ssid, const char *password)`（合法返回 NULL，否则返回纯 ASCII 提示文案）；内部静态 `lv_obj_t *wifi_hint_label`；`static void wifi_hint_show(const char *text)`（C4 复用）；`tst_wpa_conf_path()`、`tst_fake_exec()`；`wifi_env.h` 的 `wifi_env_setup()`、`wifi_env_read()`、`wifi_env_reset_logs()`。

- [x] **Step 1: 测试支持**：`tests/support/fake_fs.h` 增加声明，`fake_fs.c` 按 `tst_backlight_root()` 的写法实现（文件顶部补 `#include <sys/stat.h>`）：

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)、[custom/custom_wifi.c](../../../custom/custom_wifi.c)、[generated/setup_scr_WIFI.c](../../../generated/setup_scr_WIFI.c)。

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)、[custom/custom_wifi.c](../../../custom/custom_wifi.c)、[generated/setup_scr_WIFI.c](../../../generated/setup_scr_WIFI.c)。

`tests/CMakeLists.txt` 在背光覆盖之后加：

落地：[tests/CMakeLists.txt](../../../tests/CMakeLists.txt)、[tests/cases/wifi/CMakeLists.txt](../../../tests/cases/wifi/CMakeLists.txt)。

`tests/cases/wifi/wifi_env.h`：

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)、[custom/custom_wifi.c](../../../custom/custom_wifi.c)、[generated/setup_scr_WIFI.c](../../../generated/setup_scr_WIFI.c)。

- [x] **Step 2: 写失败测试** `test_validate.c`

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)、[custom/custom_wifi.c](../../../custom/custom_wifi.c)、[generated/setup_scr_WIFI.c](../../../generated/setup_scr_WIFI.c)。

`test_load.c`（提示标签通过页面子控件的类型与红色样式查找，不引用生产代码内部变量；没有标签时返回 NULL）：

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)、[custom/custom_wifi.c](../../../custom/custom_wifi.c)、[generated/setup_scr_WIFI.c](../../../generated/setup_scr_WIFI.c)。

`tests/cases/wifi/wifi_ui.h`：

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)、[custom/custom_wifi.c](../../../custom/custom_wifi.c)、[generated/setup_scr_WIFI.c](../../../generated/setup_scr_WIFI.c)。

`tests/cases/wifi/compile_prototypes.c`：

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)、[custom/custom_wifi.c](../../../custom/custom_wifi.c)、[generated/setup_scr_WIFI.c](../../../generated/setup_scr_WIFI.c)。

CMake 注册：

落地：[tests/CMakeLists.txt](../../../tests/CMakeLists.txt)、[tests/cases/wifi/CMakeLists.txt](../../../tests/cases/wifi/CMakeLists.txt)。

`wifi.load.dropdown_overlong` 通过真实下拉框事件分别选择 33 字节 ASCII 与 33 字节中文名称，断言输入保持为空、SSID 提示可见，随后以合法密码点击 Load，确认命令日志、wpa_cli 输入和配置均未变化；`WIFI-20length` 将长度守卫改为 `if (0)`，由该用例捕获。

回填与输入校验：以下守卫缺失现象针对单独移除守卫的变体；整项生产改动撤销的现象见验证证据。`ctest --test-dir build-tests --output-on-failure -R '^wifi.load.dropdown_utf8$'` 在回填守卫缺失时内容与提示断言失败；完整原型补齐前 `^wifi.compile.prototypes$` 三条静态断言失败。UTF-8 校验和编辑隐藏提示由 `WIFI-20utf8`、`WIFI-20cont`、`WIFI-20hide` 缺陷变体验证；`wifi.validate.utf8` 覆盖 10 种非法、8 种合法序列，分别放入 SSID 与密码，共 36 次判定。


- [x] **Step 3: 确认先红**：`RE='^wifi\.(validate|load)\.'; ID=20-red` 执行 [H1]。Expected：`wifi_validate` 链接失败 `undefined reference to 'wifi_input_check'`（新接口，spec §5.2 ⑳ 按 §5.5 的先例接受）；暂时把 `validate` 那行注释掉再跑一次，`wifi.load.invalid_no_wpa_call` 失败：`log[0] == '\0'` 与 `in[0] == '\0'` 为 `FAIL`（旧代码仍 `popen("wpa_cli")` 写入 `set_network`），提示标签断言 `FAIL`。旧的无条件 `#define WPA_FILE_PATH` 会盖过测试的覆盖，旧代码读的是 `/etc/wpa_supplicant.conf`，配置断言此时通过。然后恢复 `validate` 那行。

- [x] **Step 4: 修复** `custom/custom_wifi.c`：
  - 在 `STATIC VARIABLES` 区域声明 `static lv_obj_t *wifi_hint_label;`；`custom/custom.h` 声明完整原型 `const char *wifi_input_check(const char *ssid, const char *password);`，消除新增接口缺前置原型的 `-Wmissing-prototypes` 告警，并将 `wifi_app_init`、`wifi_backend_init`、`wifi_backend_release` 在 `custom.h` 中的声明由 `()` 补为 `(void)` 完整原型，`custom_wifi.c` 中的定义保持 `()` 不变，调用方式和行为不变；本提交 [X] 告警为 glibc 127、uClibc 126，相对前一提交各减少 3 条。

  - `#define WPA_FILE_PATH "/etc/wpa_supplicant.conf"` 改为 `#ifndef WPA_FILE_PATH` / `#define …` / `#endif`。
  - 在 `_wifi_conf_load` 之前新增：

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)、[custom/custom_wifi.c](../../../custom/custom_wifi.c)、[generated/setup_scr_WIFI.c](../../../generated/setup_scr_WIFI.c)。

  - `WIFI_load_btn_event_handler` 的 `RELEASED` 分支改为：

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)、[custom/custom_wifi.c](../../../custom/custom_wifi.c)、[generated/setup_scr_WIFI.c](../../../generated/setup_scr_WIFI.c)。

  - 下拉框回填之前校验原始字节，非法 UTF-8 或超过 32 字节时拒绝回填并提示，超长提示复用 `wifi_input_check`，避免 LVGL 过滤或截断后把残余子串作为网络名：

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)、[custom/custom_wifi.c](../../../custom/custom_wifi.c)、[generated/setup_scr_WIFI.c](../../../generated/setup_scr_WIFI.c)。

  - `wifi_app_init()` 末尾追加（字体符号以 `grep -rn 'lv_font_.*_16\b' generated/guider_fonts/*.c | head -1` 得到的 16 号字体为准，下行以 `lv_font_montserratMedium_16` 示意）：

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)、[custom/custom_wifi.c](../../../custom/custom_wifi.c)、[generated/setup_scr_WIFI.c](../../../generated/setup_scr_WIFI.c)。

  - `generated/setup_scr_WIFI.c:104`：`lv_textarea_set_max_length(ui->WIFI_psw_ta, 32);` 改为 `63`。

- [x] **Step 5: 转绿与改坏**：`ID=20-green` 执行 [H1]，校验、回填、编辑隐藏提示与完整原型用例均通过；`validate.mutants`：

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)。

`IDS='WIFI-20a WIFI-20b WIFI-20c WIFI-20utf8 WIFI-20cont WIFI-20hide WIFI-20length'` 执行 [M]，均 `KILLED`。

- [x] **Step 6: 提交** `fix(wifi): 🐛 WiFi [1/9] 提交前校验 SSID 与密码并显示提示`；body：`- SSID、密码不校验就交给 wpa_cli，save_config 会让 wpa_supplicant 自己落盘，校验必须先于任何 wpa_cli 调用`、`- 按字节计长度：max_length 按字符计，且密码模式回填不受其限制`、`- " 与 \\ 合法，只拒绝 # 之前出现过 " 的值与非法 UTF-8；否决拒绝全部 " 与 \\（误伤合法口令）`、`- 提示用新增的红色标签，不复用 WIFI_loaded_wifi_label：5 秒定时器只取消隐藏不改文本，会残留成 SSID`、`- WPA_FILE_PATH 改 #ifndef 默认值，供测试指向临时目录`、`详见 spec D11、D12、D14`。

### Task 7: ㉑ 配置原子写入（`fix(wifi)`）

**Files:** Create `tests/cases/wifi/conf_write.mutants`、`tests/cases/wifi/test_conf_write.c`、`tests/cases/wifi/unit_wifi.c`；Modify `custom/custom_wifi.c`、`tests/CMakeLists.txt`、`tests/README.md`、`tests/cases/wifi/CMakeLists.txt`。

**Interfaces:**
- Produces: `unit_wifi.c` 导出 `void tst_wifi_conf_load(const char *ssid, const char *psk)`；后续 Task 在同一文件追加 `tst_wifi_conf_get`、`tst_wifi_scan`、`tst_wifi_status_update`。

- [x] **Step 1: 写失败测试**：`unit_wifi.c`

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)、[custom/custom_wifi.c](../../../custom/custom_wifi.c)。

`test_conf_write.c`（本可执行文件的 `rename` 被覆盖：记录参数、可注入失败，否则转调 `renameat`）：

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)、[custom/custom_wifi.c](../../../custom/custom_wifi.c)。

CMake：`luckfox_add_test(conf_write UNIT unit_wifi.c CASES same_dir_atomic io_failures)`（C3、C5 往同一行追加 CASE）。`unit_wifi.c` 包含 `fake_fs.h`，而 UNIT 目标只继承 `luckfox_app` 的包含目录，原样编译报 `fatal error: fake_fs.h: No such file or directory`：`tests/CMakeLists.txt` 的 `luckfox_add_test()` 在 `target_compile_options(${exe}_unit PRIVATE -w)` 之后加 `target_include_directories(${exe}_unit PRIVATE ${PROJECT_SOURCE_DIR}/support)`，之后各 Task 往 `unit_wifi.c` 追加的用例同样依赖它。

- [x] **Step 2: 确认先红**：`RE='^wifi\.conf_write\.'; ID=21-red` 执行 [H1]。Expected：`dirname` 比较 `FAIL`（源为相对路径 `temp_wpa_supplicant.conf`），模式断言 `FAIL`（`fopen("w")` 受 umask 影响得 0644），注入失败后读配置 `FAIL`（旧代码已先 `remove`，配置文件不存在）。`io_failures` 的 8 种失败注入均未阻止旧实现替换文件，内容及 rename 断言合计 16 条 `FAIL`。

- [x] **Step 3: 修复** `_wifi_conf_load`（文件顶部补 `#include <sys/stat.h>`、`#include <unistd.h>`，已有则不加）：从 `FILE *file = fopen(WPA_FILE_PATH, "r");` 之前加临时路径拼接，替换临时文件的创建与收尾：

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)、[custom/custom_wifi.c](../../../custom/custom_wifi.c)。

权限设置失败时关闭两个流并清理临时文件；读写流错误、刷盘及关闭失败均中止替换。`io_failures` 通过 UNIT 的 libc 包装注入这些错误，测试环境继续使用真实 libc。

循环之后的 `fclose(file); fclose(temp_file); remove(WPA_FILE_PATH); rename(...)` 换成：

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)、[custom/custom_wifi.c](../../../custom/custom_wifi.c)。

`snprintf` 放在 `popen("wpa_cli")` 之后、读配置之前：路径过长时不写任何文件。

- [x] **Step 4: 转绿与改坏**：`ID=21-green` 执行 [H1]；`conf_write.mutants`：

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)。

`IDS='WIFI-21a WIFI-21b'` 执行 [M]，均 `KILLED`。

- [x] **Step 5: 提交** `fix(wifi): 🐛 WiFi [2/9] 配置改为同目录临时文件加 rename 原子替换`；body：`- 先 remove 再 rename，rename 失败时配置丢失；临时文件建在当前目录，可能跨文件系统而无法原子替换`、`- 临时文件用专用后缀 .luckfox.new，不用 wpa_supplicant 自己 save_config 的 .tmp`、`- fchmod 成原文件模式后 fflush + fsync 再 rename`、`详见 spec D14`。

### Task 8: C3 只改 `ssid=`、`psk=` 行（`fix(wifi)`）

**Files:** Create `tests/cases/wifi/conf_match.mutants`；Modify `custom/custom_wifi.c`、`tests/cases/wifi/CMakeLists.txt`、`tests/cases/wifi/test_conf_write.c`。

**Interfaces:**
- Produces: `static const char *wifi_conf_skip_ws(const char *s)`、`static int wifi_conf_is_key(const char *line, const char *key)`（读写共用，C5、㉕ 继续使用）。

- [x] **Step 1: 写失败测试**：`test_conf_write.c` 增加用例并加进 `cases[]`，CMake 的 `CASES` 追加 `only_ssid_psk_lines`：

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)、[custom/custom_wifi.c](../../../custom/custom_wifi.c)。

- [x] **Step 2: 确认先红**：`RE='^wifi\.conf_write\.only_ssid_psk_lines$'; ID=C3-red` 执行 [H1]。Expected：`FAIL`；打印的配置里 `scan_ssid=1`、`bssid=…` 被改成 `ssid="newnet"`，`wpa_psk=keep` 被改成 `psk="newpass12"`。

- [x] **Step 3: 修复**：在 `_wifi_conf_load` 之前新增

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)、[custom/custom_wifi.c](../../../custom/custom_wifi.c)。

`_wifi_conf_load` 与 `_wifi_conf_get` 中的 `strstr(line, "ssid=")` 改为 `wifi_conf_is_key(line, "ssid")`，`strstr(line, "psk=")` 改为 `wifi_conf_is_key(line, "psk")`。

- [x] **Step 4: 转绿与改坏**：`ID=C3-green` 执行 [H1]，`wifi.conf_write.*` 三条通过；`conf_match.mutants`：

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)。

`IDS=WIFI-C3` 执行 [M]，`KILLED`。

- [x] **Step 5: 提交** `fix(wifi): 🐛 WiFi [3/9] 写配置时只替换 ssid= 与 psk= 行`；body：`- strstr 子串匹配让 scan_ssid=1、bssid= 命中 ssid=，wpa_psk= 等命中 psk=，提交时会写坏用户配置`、`- 去掉行首空白后按 <key>= 前缀匹配，读写共用同一个判定`、`详见 spec §3.2 C3、D13`。

### Task 9: C5 块边界判定（`fix(wifi)`）

**Files:** Modify `custom/custom_wifi.c`、`tests/cases/wifi/CMakeLists.txt`、`tests/cases/wifi/conf_match.mutants`、`tests/cases/wifi/test_conf_write.c`、`tests/cases/wifi/unit_wifi.c`。

**Interfaces:**
- Produces: `unit_wifi.c` 的 `void tst_wifi_conf_get(char ssid[128], char psk[128])`（本 Task 先以两参数调用 `_wifi_conf_get`，Task 10 改为四参数，签名不变）。

- [x] **Step 1: 写失败测试**：`unit_wifi.c` 追加

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)、[custom/custom_wifi.c](../../../custom/custom_wifi.c)。

`test_conf_write.c` 增加（`CASES` 追加 `brace_in_value crlf_block_end`）：

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)、[custom/custom_wifi.c](../../../custom/custom_wifi.c)。

- [x] **Step 2: 确认先红**：`RE='^wifi\.conf_write\.brace_in_value$'; ID=C5-red` 执行 [H1]。Expected：第一次读回 `ssid=[]`（含 `}` 的行被当成块结束）与 `psk=[]`（含 `network={` 的行被当成块开始），第二次写入后 `ssid` 仍不是 `plain`（旧写入把 `ssid="my}net"` 行当块结束而原样保留）。

新增 `wifi.conf_write.crlf_block_end`：CRLF 块结束后块外配置不被改写。`WIFI-C5c` 删除块结束判定对 `\r` 的接受，由此用例捕获。

- [x] **Step 3: 修复**：在 `wifi_conf_is_key` 之后新增

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)、[custom/custom_wifi.c](../../../custom/custom_wifi.c)。

`_wifi_conf_load` 与 `_wifi_conf_get` 中 `strstr(line, "network={")` 改为 `wifi_conf_is_block_start(line)`，`strstr(line, "}")` 改为 `wifi_conf_is_block_end(line)`。

- [x] **Step 4: 转绿与改坏**：`ID=C5-green` 执行 [H1]；`conf_match.mutants` 追加：

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)。

`IDS='WIFI-C3 WIFI-C5a WIFI-C5b WIFI-C5c'` 执行 [M]，均 `KILLED`。

- [x] **Step 5: 提交** `fix(wifi): 🐛 WiFi [4/9] network 块的起止只认整行的 network={ 与 }`；body：`- strstr(line, "}") 让含 } 的 SSID/口令行在匹配 ssid= 之前就被当成块结束，写回时原样保留、读取时读不出；network={ 同理会被值里的子串命中`、`- 与 C3 共用行首去空白的判定，读写两侧一致`、`详见 spec §3.2 C5、D13`。

### Task 10: ㉕ 读配置限宽并取首尾引号之间（`fix(wifi)`）

**Files:** Create `tests/cases/wifi/conf_get.mutants`、`tests/cases/wifi/test_conf_get.c`；Modify `custom/custom_wifi.c`、`tests/cases/wifi/CMakeLists.txt`、`tests/cases/wifi/unit_wifi.c`。

**Interfaces:**
- Produces: `static void _wifi_conf_get(char *ssid, size_t ssid_size, char *passwd, size_t passwd_size)`；`static int wifi_conf_quoted(const char *line, char *out, size_t size)`。

- [x] **Step 1: 写失败测试** `test_conf_get.c`

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)、[custom/custom_wifi.c](../../../custom/custom_wifi.c)。

CMake：`luckfox_add_test(conf_get UNIT unit_wifi.c CASES long_and_quoted)`（不建 UI：`_wifi_conf_get` 只读文件）。

`unit_wifi.c` 已包含 Task 7 的 I/O 故障注入包装：六个 `tst_conf_*` 使用 `__attribute__((weak))` 的 libc 透传定义，使新可执行文件正常链接；`test_conf_write.c` 的强定义仍覆盖这些包装以保留失败注入。落地见 `tests/cases/wifi/unit_wifi.c`。

- [x] **Step 2: 确认先红**：按「验证证据」的方法导出本 Task 并回退生产改动后，在导出目录执行 `sed -i 's/_wifi_conf_get(ssid, 128, psk, 128)/_wifi_conf_get(ssid, psk)/' tests/cases/wifi/unit_wifi.c`，使测试包装调用旧的两参数接口；再以 `RE='^wifi\.conf_get\.'; ID=25-red` 执行 [H1]。Expected：ASan `stack-buffer-overflow`（`sscanf` 将 200 字节值及结尾空字符共 201 字节写入 128 字节的 `ssid`）。

- [x] **Step 3: 修复**：新增

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)、[custom/custom_wifi.c](../../../custom/custom_wifi.c)。

`_wifi_conf_get` 签名改为 `static void _wifi_conf_get(char* ssid, size_t ssid_size, char* passwd, size_t passwd_size)`，块内读取改为：

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)、[custom/custom_wifi.c](../../../custom/custom_wifi.c)。

`wifi_app_init()` 调用改为 `_wifi_conf_get(ssid, MAX_CONF_LEN, passwd, MAX_CONF_LEN);`；`unit_wifi.c` 的 `tst_wifi_conf_get` 改为 `_wifi_conf_get(ssid, 128, psk, 128);`。

- [x] **Step 4: 转绿与改坏**：`ID=25-green` 执行 [H1]，并跑 `RE='^wifi\.'` 确认 `conf_write` 仍过；`conf_get.mutants`：

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)。

`IDS=WIFI-25` 执行 [M]，`KILLED`（ASan 越界写）。

- [x] **Step 5: 提交** `fix(wifi): 🐛 WiFi [5/9] 读配置限定宽度并取首尾引号之间的值`；body：`- sscanf(" ssid=\"%[^\"]\"") 无宽度限制，长值溢出 128 字节缓冲区；遇到第一个 " 就停，含 " 的 SSID/口令回读丢尾`、`- 取第一个到最后一个 " 之间，_wifi_conf_get 增加缓冲区大小参数，调用方只有 wifi_app_init()`、`详见 spec D13`。

### Task 11: ㉒ 扫描结果按 TAB 解析并解码（`fix(wifi)`）

**Files:** Create `tests/cases/wifi/scan.mutants`、`tests/cases/wifi/test_scan.c`；Modify `custom/custom_wifi.c`、`tests/cases/wifi/CMakeLists.txt`、`tests/cases/wifi/unit_wifi.c`。

**Interfaces:**
- Produces: `unit_wifi.c` 的 `int tst_wifi_scan(void)`（调用 `_wifi_scanning_ssid()`）；`static int wifi_ssid_decode(const char *in, char *out, size_t out_size)`。

- [x] **Step 1: 写失败测试**：`unit_wifi.c` 追加 `int tst_wifi_scan(void) { return _wifi_scanning_ssid(); }`；`test_scan.c`：

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)、[custom/custom_wifi.c](../../../custom/custom_wifi.c)。

CMake：`luckfox_add_test(scan UNIT unit_wifi.c CASES parse parse_all_discarded parse_cap append_truncated append_error)`。

在 `unit_wifi.c` 的 `#include "custom_wifi.c"` 前加入可覆盖的格式化包装：

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)、[custom/custom_wifi.c](../../../custom/custom_wifi.c)。

扫描拼接边界：`ctest --test-dir build-tests --output-on-failure -R '^wifi.scan.append_'` 注入截断返回值与 -1，修复前两条均被 ASan 栈越界检查截获；修复后分别保留完整的首项与省略号、显示 `scanning`。`IDS=WIFI-22c` 执行 [M] 确认截断守卫不可删除。


- [x] **Step 2: 确认先红**：`RE='^wifi\.scan\.'; ID=22-red` 执行 [H1]（`unit_wifi.c` 能编译依赖 Task 7 给 UNIT 目标加的 `tests/support` 包含路径）。Expected：parse 系列三条均 `FAIL`：`parse` 选项数不是 8、内容对不上（空格截断、`flags` 为空错位、未解码）；`parse_all_discarded` 选项不是唯一的 `scanning`；`parse_cap` 选项数不是 11。

- [x] **Step 3: 修复**：新增

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)、[custom/custom_wifi.c](../../../custom/custom_wifi.c)。

`_wifi_scanning_ssid()` 中从 `// Parse each line` 的 `while` 到 `ssid_string` 拼接结束整段换成（`networks[MAX_LINE_LEN]` 不动，spec §3.3）：

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)、[custom/custom_wifi.c](../../../custom/custom_wifi.c)。

其后 `printf`、`scanning` 分支与 `strcat(ssid_string, "\n...")` 保持；每次拼接先预留四字节后缀容量，负返回值或截断时撤回整条并停止，`used` 只累加成功写入的长度。

- [x] **Step 4: 转绿与改坏**：`ID=22-green` 执行 [H1]；`scan.mutants`：

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)。

`IDS='WIFI-22a WIFI-22b WIFI-22c'` 执行 [M]，均 `KILLED`。

- [x] **Step 5: 提交** `fix(wifi): 🐛 WiFi [6/9] 扫描结果按 TAB 切分并解码 SSID 转义`；body：`- strtok("\t ") 把带空格的 SSID 截断，连续 TAB 合并让 flags 为空的条目错位；strlen < 16 过滤让长 SSID 不显示`、`- 解码 printf_encode 的 \\"、\\\\、\\e、\\n、\\r、\\t、\\xNN：不解码时用户选到的名字与 AP 不符`、`- 空、含控制字符、残缺或未知转义、超过 127 字节的条目整条丢弃：lv_dropdown 以 \\n 分隔，换行会拆项，其余放不进 networks[].ssid 或还原不出真实名称；上限按保留项计`、`详见 spec D15`。

### Task 12: ㉓ + C4 WiFi `popen` 失败处理（`fix(wifi)`）

**Files:** Create `tests/cases/wifi/popen.mutants`、`tests/cases/wifi/test_popen.c`；Modify `custom/custom_wifi.c`、`tests/cases/wifi/CMakeLists.txt`、`tests/cases/wifi/unit_wifi.c`。

- [x] **Step 1: 写失败测试**：`unit_wifi.c` 追加 `void tst_wifi_status_update(void) { _wifi_status_update(); }`；`test_popen.c`：

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)、[custom/custom_wifi.c](../../../custom/custom_wifi.c)。

CMake：`luckfox_add_test(popen UNIT unit_wifi.c CASES failure)`。

- [x] **Step 2: 确认先红**：`RE='^wifi\.popen\.'; ID=23-C4-red` 执行 [H1]。Expected：`tst_wifi_status_update()` 中 `pclose(NULL)` 被 UBSan 非空参数检查截获；临时删去该调用再跑，进程在 `_wifi_conf_load` 以退出码 1 结束（`exit(EXIT_FAILURE)`），无 `RESULT` 行；之后恢复。

- [x] **Step 3: 修复**：`_wifi_status_update` 判空分支删去 `pclose(fp);`；`_wifi_conf_load` 判空分支改为

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)、[custom/custom_wifi.c](../../../custom/custom_wifi.c)。

- [x] **Step 4: 转绿与改坏**：`ID=23-C4-green` 执行 [H1]；`popen.mutants`：

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)。

`IDS='WIFI-23 WIFI-C4'` 执行 [M]，均 `KILLED`。

- [x] **Step 5: 提交** `fix(wifi): 🐛 WiFi [7/9] wpa_cli 无法启动时不再 pclose(NULL) 或退出进程`；body：`- _wifi_status_update 在 popen 失败分支里 pclose(NULL)；_wifi_conf_load 失败直接 exit(EXIT_FAILURE) 让整个界面退出`、`- 改为提示并返回，提示复用校验用的红色标签`、`详见 spec D18、§3.2 C4`。

### Task 13: ㉔ WIFI 定时器只在 WIFI 页刷新（`fix(wifi)`）

**Files:** Create `tests/cases/wifi/test_timer.c`、`tests/cases/wifi/timer.mutants`；Modify `custom/custom_wifi.c`、`tests/cases/wifi/CMakeLists.txt`。

- [x] **Step 1: 写失败测试** `test_timer.c`（计数后转调真实 `popen`，不依赖 NULL 处理）

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)、[custom/custom_wifi.c](../../../custom/custom_wifi.c)。

CMake：`luckfox_add_test(timer CASES inactive_screen)`。

- [x] **Step 2: 确认先红**：`RE='^wifi\.timer\.'; ID=24t-red` 执行 [H1]。Expected：`popen_calls == 0` 为 `FAIL`（`got 1`）。

- [x] **Step 3: 修复**

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)、[custom/custom_wifi.c](../../../custom/custom_wifi.c)。

- [x] **Step 4: 转绿与改坏**：`ID=24t-green` 执行 [H1]；`timer.mutants`：

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)。

`IDS=WIFI-24t` 执行 [M]，`KILLED`。

- [x] **Step 5: 提交** `fix(wifi): 🐛 WiFi [8/9] 离开 WIFI 页后定时器不再 popen wpa_cli`；body：`- 离开 WIFI 页后 5 s 定时器仍每次 popen("wpa_cli status")`、`- 回调开头判当前屏，不删除定时器：*_del 标志让 setup_scr_WIFI 只执行一次，删除后再也不会重建；否决 pause/resume 与 SCREEN_LOADED 立即刷新（多一处屏幕事件接线）`、`- 代价：回到 WIFI 页后最多 5 s 内状态仍是旧的`、`详见 spec D16`。

### Task 14: ㉔ 每次 Load 前清掉 wlan0 的 `udhcpc`（`fix(wifi)`）

**Files:** Create `tests/cases/wifi/dhcp.mutants`、`tests/cases/wifi/test_dhcp.c`；Modify `custom/custom_wifi.c`、`tests/cases/wifi/CMakeLists.txt`。

- [x] **Step 1: 写失败测试** `test_dhcp.c`（覆盖 `system` 记录命令后转调真实实现；`PATH` 里是假 `pkill`、`udhcpc`、`wpa_cli`）

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)、[custom/custom_wifi.c](../../../custom/custom_wifi.c)。

CMake：`luckfox_add_test(dhcp UNIT unit_wifi.c CASES single_instance)`。

- [x] **Step 2: 确认先红**：`RE='^wifi\.dhcp\.'; ID=24d-red` 执行 [H1]。Expected：`pkills` `got 0, want 2`，`ordered` `FAIL`。

- [x] **Step 3: 修复**：`_wifi_conf_load` 末尾改为

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)、[custom/custom_wifi.c](../../../custom/custom_wifi.c)。

- [x] **Step 4: 转绿与改坏**：`ID=24d-green` 执行 [H1]；`dhcp.mutants`：

落地：[tests/cases/wifi/](../../../tests/cases/wifi/)。

`IDS=WIFI-24d` 执行 [M]，`KILLED`。

- [x] **Step 5: 提交** `fix(wifi): 🐛 WiFi [9/9] 重启 udhcpc 前先结束 wlan0 上的旧实例`；body：`- 每按一次 Load 多起一个 udhcpc -i wlan0`、`- pkill -f '[u]dhcpc -i wlan0' 只匹配 wlan0 的实例；否决裸 pkill udhcpc（误杀 eth0 等其他接口）`、`- 与启动命令分两次 system()：合在一条 sh -c 里时 shell 自己的命令行也匹配而被杀（退出码 143）`、`- 不加 -n（会改变重试行为）；板上是否有 pkill 见真机清单，没有时改用 -p pidfile`、`详见 spec D17`。

### WiFi 回归检查点

- [x] 执行 [H]：全部通过；并行运行下各 WiFi 用例的配置与日志都在各自临时目录（`grep -c '/etc/wpa_supplicant.conf' build-tests/Testing/Temporary/LastTest.log` 为 0）。
- [x] 执行 [X]：两条 `rc=0`。
- [x] `IDS='WIFI-20a WIFI-20b WIFI-20c WIFI-20utf8 WIFI-20cont WIFI-20hide WIFI-20length WIFI-21a WIFI-21b WIFI-C3 WIFI-C5a WIFI-C5b WIFI-C5c WIFI-25 WIFI-22a WIFI-22b WIFI-22c WIFI-23 WIFI-C4 WIFI-24t WIFI-24d'` 执行 [M]：21 个 `KILLED`、退出 0。
- [x] 推送。

## 画板（Task 15–16）

### Task 15: ⑱ 画布缓冲区改堆分配（`fix(sketchpad)`）

**Files:** Create `tests/cases/sketchpad/CMakeLists.txt`、`tests/cases/sketchpad/canvas.mutants`、`tests/cases/sketchpad/test_canvas.c`、`tests/cases/sketchpad/unit_canvas.c`；Modify `custom/custom.h`、`custom/custom_sketchpad.c`、`generated/setup_scr_Sketchpad.c`、`tests/CMakeLists.txt`。

**Interfaces:**
- Produces: `lv_res_t lv_sketchpad_set_size(lv_obj_t * obj, lv_coord_t w, lv_coord_t h)`（声明在 `custom/custom.h`，紧随 `lv_sketchpad_create`）；`#ifndef SKETCHPAD_BUF_ALLOC` 注入点；ctest `sketchpad.canvas.{buffer_not_on_stack,buffer_full_write,alloc_failure}`。

- [x] **Step 0: 前提核对**：`grep -n 'define LV_MEM_CUSTOM\|define LV_MEM_SIZE' lib/lv_conf.h`。Expected：`LV_MEM_CUSTOM 1`（走系统 `malloc`），或 `LV_MEM_SIZE` 不小于 1.6 MB（720 屏 1.5 MB 缓冲）；否则停下汇报（`lv_mem_alloc` 放不下画布）。

- [x] **Step 1: 写失败测试**：`unit_canvas.c`

落地：[tests/cases/sketchpad/](../../../tests/cases/sketchpad/)、[custom/custom_sketchpad.c](../../../custom/custom_sketchpad.c)、[generated/setup_scr_Sketchpad.c](../../../generated/setup_scr_Sketchpad.c)。

`test_canvas.c`：

落地：[tests/cases/sketchpad/](../../../tests/cases/sketchpad/)、[custom/custom_sketchpad.c](../../../custom/custom_sketchpad.c)、[generated/setup_scr_Sketchpad.c](../../../generated/setup_scr_Sketchpad.c)。

`tests/cases/sketchpad/CMakeLists.txt`：`luckfox_add_test(canvas UNIT unit_canvas.c CASES buffer_not_on_stack buffer_full_write alloc_failure)`；`tests/CMakeLists.txt` 在 `add_subdirectory(cases/wifi)` 之后加 `add_subdirectory(cases/sketchpad)`。

- [x] **Step 2: 确认先红**：`RE='^sketchpad\.canvas\.'; ID=18-red` 执行 [H1]。Expected：`buffer_not_on_stack` 的 `!on_stack` 为 `FAIL`（主断言）；`alloc_failure` 的 `img->data == NULL` 为 `FAIL`（旧代码不经分配，直接用栈 VLA）；`buffer_full_write` 写的是已返回函数的栈区，可能崩溃也可能通过，不作为修复前失败的依据。

- [x] **Step 3: 修复** `custom/custom_sketchpad.c`：
  - 结构体末尾加 `void * buf;   /* lv_sketchpad_set_size() 分配的画布缓冲区，析构时释放 */`；构造函数加 `sketchpad->buf = NULL;`。
  - `#define LAST_VALUE` 之后加：

落地：[tests/cases/sketchpad/](../../../tests/cases/sketchpad/)、[custom/custom_sketchpad.c](../../../custom/custom_sketchpad.c)、[generated/setup_scr_Sketchpad.c](../../../generated/setup_scr_Sketchpad.c)。

  - 析构函数改为：

落地：[tests/cases/sketchpad/](../../../tests/cases/sketchpad/)、[custom/custom_sketchpad.c](../../../custom/custom_sketchpad.c)、[generated/setup_scr_Sketchpad.c](../../../generated/setup_scr_Sketchpad.c)。

  - 事件函数 `LV_EVENT_PRESSING` 分支开头加 `if (sketchpad->buf == NULL)  return;`；`Sketchpad_clear_btn_event_cb` 在填充画布之前加 `if (lv_canvas_get_img(canva_obj)->data == NULL) return;`（分配失败后画笔与清除都不碰空缓冲）。
  - `lv_sketchpad_create` 之后新增：

落地：[tests/cases/sketchpad/](../../../tests/cases/sketchpad/)、[custom/custom_sketchpad.c](../../../custom/custom_sketchpad.c)、[generated/setup_scr_Sketchpad.c](../../../generated/setup_scr_Sketchpad.c)。

  - `custom/custom.h` 在 `lv_obj_t * lv_sketchpad_create(lv_obj_t * parent);` 之后加 `lv_res_t lv_sketchpad_set_size(lv_obj_t * obj, lv_coord_t w, lv_coord_t h);`。
  - `generated/setup_scr_Sketchpad.c` 的 VLA 两行与 `lv_canvas_fill_bg` 改为：

落地：[tests/cases/sketchpad/](../../../tests/cases/sketchpad/)、[custom/custom_sketchpad.c](../../../custom/custom_sketchpad.c)、[generated/setup_scr_Sketchpad.c](../../../generated/setup_scr_Sketchpad.c)。

- [x] **Step 4: 转绿与改坏**：`ID=18-green` 执行 [H1]，三条通过、无 LSan 报告；`alloc_failure` 在失败后的渲染、画笔与清除都不崩溃：未调用 `lv_canvas_set_buffer` 时 `dsc` 全零，0×0 画布的绘制与填充是空操作，画笔与清除另由判空直接返回（判空去掉后行为相同，不配变体）；若崩溃，停下汇报；`canvas.mutants`：

落地：[tests/cases/sketchpad/](../../../tests/cases/sketchpad/)。

`IDS=SKETCH-18` 执行 [M]，`KILLED`（`alloc_failure` 将空缓冲传入 `lv_canvas_set_buffer`，触发 `LV_ASSERT_NULL` 的无限循环，由用例开头的 `alarm(5)` 在约 5 秒时以 `SIGALRM` 终止并判失败）。

- [x] **Step 5: 提交** `fix(sketchpad): 🐛 画板 [1/2] 画布缓冲区改为堆上一次性分配`；body：`- 缓冲区是 setup_scr_Sketchpad 里的栈 VLA，函数返回后画布指向失效栈；又把字节数当 lv_color_t 元素数，多分配 4 倍`、`- lv_mem_alloc 按 LV_CANVAS_BUF_SIZE_TRUE_COLOR 字节分配，指针存进对象、析构时先 invalidate 再释放；否决 static 数组（480 屏 691 KB、720 屏 1.5 MB 常驻 BSS）`、`- 分配失败返回错误、不设缓冲，画笔与清除跳过空缓冲`、`详见 spec D10`。

### Task 16: ⑲ 笔迹坐标换算到画布（`fix(sketchpad)`）

**Files:** Create `tests/cases/sketchpad/stroke.mutants`、`tests/cases/sketchpad/test_stroke.c`；Modify `custom/custom_sketchpad.c`、`tests/cases/sketchpad/CMakeLists.txt`。

- [x] **Step 1: 写失败测试** `test_stroke.c`

落地：[tests/cases/sketchpad/](../../../tests/cases/sketchpad/)、[custom/custom_sketchpad.c](../../../custom/custom_sketchpad.c)。

480 屏上画布 480×360、居中再上移 80，左上角为 `(0, -20)`，旧实现的落点与正确落点相差 20 px。CMake：`luckfox_add_test(stroke CASES canvas_coordinates)`。

- [x] **Step 2: 确认先红**：`RE='^sketchpad\.stroke\.'; ID=19-red` 执行 [H1]。Expected：`canvas x1=0 y1=-20`；`pen_near(110, 100)` 与底色断言均 `FAIL`。

- [x] **Step 3: 修复**：`PRESSING` 分支中 `lv_indev_get_point(indev, &point);` 之后加

落地：[tests/cases/sketchpad/](../../../tests/cases/sketchpad/)、[custom/custom_sketchpad.c](../../../custom/custom_sketchpad.c)。

- [x] **Step 4: 转绿与改坏**：`ID=19-green` 执行 [H1]；`stroke.mutants`：

落地：[tests/cases/sketchpad/](../../../tests/cases/sketchpad/)。

`IDS=SKETCH-19` 执行 [M]，`KILLED`。

- [x] **Step 5: 提交** `fix(sketchpad): 🐛 画板 [2/2] 笔迹坐标换算为画布坐标`；body：`- lv_indev_get_point 返回屏幕坐标，画布居中后上移 80，笔迹与手指偏移`、`- 减去 lv_obj_get_coords() 的左上角`、`详见 spec D10`。

### 画板回归检查点

- [x] 执行 [H]：全部通过。执行 [X]：两条 `rc=0`。`IDS='SKETCH-18 SKETCH-19'` 执行 [M]：2 个 `KILLED`。
- [x] 推送。

## 音乐页（Task 17–29）

所有调用 `music_scan_list`、`setup_scr_Music_player` 或 `_music_*` 的用例先用 `music_env_socketpair()` 把 `fd_mpv` 换成 `socketpair` 的一端：扫描会写 `loadfile`，`music_app_init()`（`setup_scr_Music_player.c:463` 调用）与选中回调会写播放控制命令；`fd_mpv` 初值 -1 到 Task 27 才引入，此前默认的 0 是 stdin。音乐页的按钮同样用测试自建按钮挂上对应 handler 后发事件。

### Task 17: ② 选项串按实际长度分配（`fix(music)`）

**Files:** Create `tests/cases/music/CMakeLists.txt`、`tests/cases/music/list.mutants`、`tests/cases/music/music_env.h`、`tests/cases/music/test_list.c`、`tests/cases/music/test_list_alloc.c`、`tests/cases/music/unit_list_alloc.c`；Modify `custom/custom.h`、`custom/custom_musicplayer.c`、`generated/setup_scr_Music_player.c`、`tests/CMakeLists.txt`、`tests/README.md`、`tests/support/fake_fs.c`、`tests/support/fake_fs.h`。

**Interfaces:**
- Produces: `int music_scan_list(void)`、`const char *music_roller_options(void)`（永不返回 NULL）；`static void music_list_clear(void)`、`static int music_build_roller_options(int count)`；`tst_music_dir()`；`music_env.h` 的 `music_env_socketpair()`、`music_env_file()`。

- [x] **Step 1: 测试支持与可覆盖的路径宏**：`custom/custom_musicplayer.c:23` 的 `#define MUSIC_DIR_PATH "/music"` 先改为 `#ifndef MUSIC_DIR_PATH` 默认值（源文件里的无条件 `#define` 会盖过测试的 `-D`；路径覆盖保证可执行用例只扫描本进程临时目录，避免访问真实 `/music`）；`fake_fs` 增加 `const char *tst_music_dir(void);`（返回 `<临时目录>/music`，不创建目录，写法同 `tst_wpa_conf_path()`）；`tests/CMakeLists.txt` 加覆盖并在 `cases/sketchpad` 之后 `add_subdirectory(cases/music)`：

落地：[tests/CMakeLists.txt](../../../tests/CMakeLists.txt)、[tests/cases/music/CMakeLists.txt](../../../tests/cases/music/CMakeLists.txt)。

`tests/cases/music/music_env.h`：

落地：[tests/cases/music/](../../../tests/cases/music/)、[custom/custom_musicplayer.c](../../../custom/custom_musicplayer.c)、[generated/setup_scr_Music_player.c](../../../generated/setup_scr_Music_player.c)。

- [x] **Step 2: 写失败测试** `test_list.c`（直接引用公开接口；旧实现缺少 `music_roller_options`，Expected：链接时报 `undefined reference to music_roller_options`）

落地：[tests/cases/music/](../../../tests/cases/music/)、[custom/custom_musicplayer.c](../../../custom/custom_musicplayer.c)、[generated/setup_scr_Music_player.c](../../../generated/setup_scr_Music_player.c)。

文件名 200 字节，不触发 Task 19 才修的 `cmd[256]` 溢出。`tests/cases/music/CMakeLists.txt`：`luckfox_add_test(list CASES options_over_255)`（C2、⑧ 追加 CASE）。

- [x] **Step 3: 确认先红**：使用 Step 2 的测试与 CMake 注册，在旧实现上运行 `cmake --build build-tests --target music_list -j"$(nproc)"`，保存构建红灯输出。Expected：构建 `music_list` 时链接失败，报 `undefined reference to music_roller_options`；尚未生成可运行的测试程序，不进入 ctest 或 ASan 检查。

- [x] **Step 4: 修复** `custom/custom_musicplayer.c`：
  - 删除无引用的 `free_music_node_list` 和 `MAX_CMD_LEN`，在节点插入函数之后新增：

落地：[tests/cases/music/](../../../tests/cases/music/)、[custom/custom_musicplayer.c](../../../custom/custom_musicplayer.c)、[generated/setup_scr_Music_player.c](../../../generated/setup_scr_Music_player.c)。

  - `music_scan_list` 改为 `int music_scan_list(void)`：开头 `init_music_node_list(&head);` 换成 `music_list_clear();`，删除不再使用的 `init_music_node_list` 与局部变量 `i`，避免新增告警；`closedir(dir);` 之后的「Create roller str」整段换成

落地：[tests/cases/music/](../../../tests/cases/music/)、[custom/custom_musicplayer.c](../../../custom/custom_musicplayer.c)、[generated/setup_scr_Music_player.c](../../../generated/setup_scr_Music_player.c)。

  - `custom/custom.h:81` 改为 `int music_scan_list(void);` 并加 `const char *music_roller_options(void);`。
  - `generated/setup_scr_Music_player.c:413-415` 改为

落地：[tests/cases/music/](../../../tests/cases/music/)、[custom/custom_musicplayer.c](../../../custom/custom_musicplayer.c)、[generated/setup_scr_Music_player.c](../../../generated/setup_scr_Music_player.c)。

- [x] **Step 5: 转绿与改坏**：`ID=2-green` 执行 [H1]；`list.mutants`：

落地：[tests/cases/music/](../../../tests/cases/music/)。

`IDS=MUSIC-2` 执行 [M]，`KILLED`（`snprintf` 截断，长度断言失败）。

- [x] **Step 6: 提交** `fix(music): 🐛 音乐 [1/13] 音乐列表选项串按实际长度动态分配`；body：`- 选项串是 setup_scr_Music_player 里的 roller_str[256]，由 strcat 拼接，文件多或名字长即栈溢出`、`- 所有权留在 custom/：music_roller_options() 永不返回 NULL（lv_roller_set_options(NULL) 在本仓断言配置下死循环），先构造新串再换指针，分配失败清空选项和节点；否决调用方传缓冲区（要把大小算法暴露给生成代码）与截断文件名（改写用户数据）`、`- 每次扫描先释放旧链表，重复扫描不泄漏；MUSIC_DIR_PATH 改 #ifndef 默认值供测试覆盖`、`详见 spec D3、D4、D20`。

选项分配失败处理：`music_scan_list` 已重建链表，选项串分配失败时须同时清空节点和旧选项，返回 -1。独立用例，以源码单元覆盖 malloc，仅让第二次分配（节点后的选项串）失败；先建立旧列表，再换成新文件，验证空选项、空链表、滚轮空文本和恢复扫描；本 Task 不调用需要空节点守卫的初始化或列表回调。CMake 注册：`luckfox_add_test(list_alloc UNIT unit_list_alloc.c CASES options_alloc_failure)`。

`tests/cases/music/unit_list_alloc.c`：

落地：[tests/cases/music/](../../../tests/cases/music/)、[custom/custom_musicplayer.c](../../../custom/custom_musicplayer.c)、[generated/setup_scr_Music_player.c](../../../generated/setup_scr_Music_player.c)。

`tests/cases/music/test_list_alloc.c`：

落地：[tests/cases/music/](../../../tests/cases/music/)、[custom/custom_musicplayer.c](../../../custom/custom_musicplayer.c)、[generated/setup_scr_Music_player.c](../../../generated/setup_scr_Music_player.c)。

`list.mutants` 追加：

落地：[tests/cases/music/](../../../tests/cases/music/)。

`ctest --test-dir build-tests --output-on-failure -R '^music.list_alloc.'`：`MUSIC-13` 单独删除清空逻辑时 4 条断言失败，完整实现通过；整项生产改动撤销时先因接口不兼容编译失败；`IDS=MUSIC-13` 执行 [M]，Expected KILLED。

### Task 18: C2 跳过含 `\n` 的文件名（`fix(music)`）

**Files:** Create `tests/cases/music/newline.mutants`；Modify `custom/custom_musicplayer.c`、`tests/cases/music/CMakeLists.txt`、`tests/cases/music/test_list.c`。

- [x] **Step 1: 写失败测试**：`test_list.c` 增加（`#include "capture.h"`，声明 `int music_scan_list(void);`，`CASES` 追加 `newline_name_skipped`）

落地：[tests/cases/music/](../../../tests/cases/music/)、[custom/custom_musicplayer.c](../../../custom/custom_musicplayer.c)。

- [x] **Step 2: 确认先红**：`RE='^music\.list\.newline_name_skipped$'; ID=C2-red` 执行 [H1]。Expected：选项串含 3 个 `\n` 分隔项，三条断言 `FAIL`。

- [x] **Step 3: 修复**：`music_scan_list` 循环里 `if (entry->d_type == DT_REG) {` 之后加

落地：[tests/cases/music/](../../../tests/cases/music/)、[custom/custom_musicplayer.c](../../../custom/custom_musicplayer.c)。

- [x] **Step 4: 转绿与改坏**：`ID=C2-green` 执行 [H1]；`newline.mutants`：

落地：[tests/cases/music/](../../../tests/cases/music/)。

`IDS=MUSIC-C2` 执行 [M]，`KILLED`。

- [x] **Step 5: 提交** `fix(music): 🐛 音乐 [2/13] 扫描时跳过文件名含换行的音乐文件`；body：`- lv_roller 以 \n 分隔选项，含 \n 的文件名占两项，此后选中下标与链表 id 全部错位`、`- 跳过并打印一行，不改写文件名`、`详见 spec §3.2 C2`。

### Task 19: ③ `loadfile` 命令改用 cJSON 构造（`fix(music)`）

**Files:** Create `tests/cases/music/mpv_cmd.mutants`、`tests/cases/music/test_mpv_cmd.c`；Modify `custom/custom_musicplayer.c`、`tests/cases/music/CMakeLists.txt`。

**Interfaces:**
- Produces: `static int mpv_send(const char *buf, size_t len)`（定义在 `_music_pause` 之前；Task 23 改 `send`，Task 27 加 `fd_mpv < 0` 判断）；`static void music_mpv_loadfile(const char *filename)`。

- [x] **Step 1: 写失败测试** `test_mpv_cmd.c`

落地：[tests/cases/music/](../../../tests/cases/music/)、[custom/custom_musicplayer.c](../../../custom/custom_musicplayer.c)。

CMake：`luckfox_add_test(mpv_cmd CASES escaping_and_length)`。

- [x] **Step 2: 确认先红**：`RE='^music\.mpv_cmd\.'; ID=3-red` 执行 [H1]。Expected：ASan `stack-buffer-overflow`，栈含 `sprintf`（`vsprintf`）与 `music_scan_list`（`cmd[256]`；② 已修，失败只来自这里）。

- [x] **Step 3: 修复**：在 `_music_pause` 之前新增

落地：[tests/cases/music/](../../../tests/cases/music/)、[custom/custom_musicplayer.c](../../../custom/custom_musicplayer.c)。

`music_scan_list` 中 `char cmd[256]; sprintf(cmd, …loadfile…); write(fd_mpv, cmd, strlen(cmd));` 三行换成 `music_mpv_loadfile(entry->d_name);`。

- [x] **Step 4: 转绿与改坏**：`ID=3-green` 执行 [H1]，并跑 `RE='^music\.'` 确认 `list` 仍过；`mpv_cmd.mutants`：

落地：[tests/cases/music/](../../../tests/cases/music/)。

`IDS=MUSIC-3` 执行 [M]，`KILLED`。

- [x] **Step 5: 提交** `fix(music): 🐛 音乐 [3/13] mpv 的 loadfile 命令改用 cJSON 构造`；body：`- sprintf 进 cmd[256]，长文件名栈溢出；文件名里的 " 可改写 JSON 命令`、`- 用已有的 cJSON 构造并转义，经新增的 mpv_send() 发出（后续收口全部写 mpv 的路径）`、`详见 spec D3、D6`。

### Task 20: ④ roller 选中长文件名死循环（`fix(music)`）

**Files:** Create `tests/cases/music/roller.mutants`、`tests/cases/music/test_roller.c`；Modify `custom/custom_musicplayer.c`、`tests/cases/music/CMakeLists.txt`。

- [x] **Step 1: 写失败测试** `test_roller.c`

落地：[tests/cases/music/](../../../tests/cases/music/)、[custom/custom_musicplayer.c](../../../custom/custom_musicplayer.c)、[generated/setup_scr_Music_player.c](../../../generated/setup_scr_Music_player.c)。

CMake：`luckfox_add_test(roller CASES select_long_name)`。

- [x] **Step 2: 确认先红**：`RE='^music\.roller\.'; ID=4-red` 执行 [H1]。Expected：约 5 s 后 `SIGALRM`（`buf[32]` 截断后 `strcmp` 永不相等，在环形链表上死循环）。

- [x] **Step 3: 修复**：`VALUE_CHANGED` 分支整体替换

落地：[tests/cases/music/](../../../tests/cases/music/)、[custom/custom_musicplayer.c](../../../custom/custom_musicplayer.c)、[generated/setup_scr_Music_player.c](../../../generated/setup_scr_Music_player.c)。

- [x] **Step 4: 转绿与改坏**：`ID=4-green` 执行 [H1]；`roller.mutants`：

落地：[tests/cases/music/](../../../tests/cases/music/)。

`IDS=MUSIC-4` 执行 [M]，`KILLED`。

- [x] **Step 5: 提交** `fix(music): 🐛 音乐 [4/13] roller 按下标而非截断后的文件名定位歌曲`；body：`- lv_roller_get_selected_str 截到 buf[32]，≥32 字节的文件名 strcmp 永不相等，在环形链表上死循环`、`- 改用 lv_roller_get_selected() 按 id 查找，判空与越界；否决加大 buf（仍可能被更长的名字截断）`、`详见 spec D5`。

### Task 21: ⑧ `opendir` 判空（`fix(music)`）

**Files:** Create `tests/cases/music/opendir.mutants`；Modify `custom/custom_musicplayer.c`、`tests/cases/music/CMakeLists.txt`、`tests/cases/music/test_list.c`。

- [x] **Step 1: 写失败测试**：`test_list.c` 增加（`CASES` 追加 `missing_dir`；不创建 `music/` 目录）

落地：[tests/cases/music/](../../../tests/cases/music/)、[custom/custom_musicplayer.c](../../../custom/custom_musicplayer.c)。

- [x] **Step 2: 确认先红**：`RE='^music\.list\.missing_dir$'; ID=8-red` 执行 [H1]。Expected：`readdir(NULL)` 处由 UBSan 非空参数检查截获。

- [x] **Step 3: 修复**：`dir = opendir(MUSIC_DIR_PATH);` 之后加

落地：[tests/cases/music/](../../../tests/cases/music/)、[custom/custom_musicplayer.c](../../../custom/custom_musicplayer.c)。

- [x] **Step 4: 转绿与改坏**：`ID=8-green` 执行 [H1]；`opendir.mutants`：

落地：[tests/cases/music/](../../../tests/cases/music/)。

`IDS=MUSIC-8` 执行 [M]，`KILLED`。

- [x] **Step 5: 提交** `fix(music): 🐛 音乐 [5/13] 音乐目录打不开时不再 readdir(NULL)`；body：`- /music 缺失时 opendir 返回 NULL 后仍 readdir`、`- 打印并返回 -1，选项串置为空串`、`详见 spec D18`。

### Task 22: C1 空列表空指针（`fix(music)`）

**Files:** Create `tests/cases/music/empty_list.mutants`、`tests/cases/music/test_empty_list.c`；Modify `custom/custom_musicplayer.c`、`tests/cases/music/CMakeLists.txt`。

- [x] **Step 1: 写失败测试** `test_empty_list.c`

落地：[tests/cases/music/](../../../tests/cases/music/)、[custom/custom_musicplayer.c](../../../custom/custom_musicplayer.c)。

CMake：`luckfox_add_test(empty_list UNIT unit_list_alloc.c CASES no_crash alloc_failure_no_crash)`。`alloc_failure_no_crash` 复用 Task 17 的分配注入，验证选项串分配失败后 `music_app_init()` 返回 -1、上一曲、下一曲及 roller 回调不崩溃；缺少初始化守卫时由 UBSan 报空节点成员访问。

- [x] **Step 2: 确认先红**：`RE='^music\.empty_list\.'; ID=C1-red` 执行 [H1]。Expected：`music_app_init` → `_music_set_pos` 读 `playing_music_node->filename` 处由 UBSan 截获空节点成员访问。

- [x] **Step 3: 修复**：`music_app_init()` 开头加 `if (playing_music_node == NULL)\n        return -1;`；Next、Prev 两个 handler 的 `RELEASED` 分支开头各加

落地：[tests/cases/music/](../../../tests/cases/music/)、[custom/custom_musicplayer.c](../../../custom/custom_musicplayer.c)。

roller 回调的空链表判断已在 Task 20 加入。

- [x] **Step 4: 转绿与改坏**：`ID=C1-green` 执行 [H1]；`empty_list.mutants`：

落地：[tests/cases/music/](../../../tests/cases/music/)。

`IDS=MUSIC-C1` 执行 [M]，`KILLED`。

- [x] **Step 5: 提交** `fix(music): 🐛 音乐 [6/13] 音乐列表为空时不再解引用空节点`；body：`- /music 为空或缺失时 playing_music_node 为 NULL，进入音乐页（music_app_init）与上一曲、下一曲都会解引用`、`- 与 opendir 判空同属一条路径，不一起修则判空后仍崩`、`详见 spec §3.2 C1`。

### Task 23: ⑦ 写 mpv 套接字不触发 SIGPIPE（`fix(music)`）

**Files:** Create `tests/cases/music/mpv_send.mutants`、`tests/cases/music/test_mpv_send.c`；Modify `custom/custom_musicplayer.c`、`tests/cases/music/CMakeLists.txt`。

- [x] **Step 1: 写失败测试** `test_mpv_send.c`

落地：[tests/cases/music/](../../../tests/cases/music/)、[custom/custom_musicplayer.c](../../../custom/custom_musicplayer.c)。

CMake：`luckfox_add_test(mpv_send CASES peer_closed)`。

- [x] **Step 2: 确认先红**：`RE='^music\.mpv_send\.'; ID=7-red` 执行 [H1]。Expected：进程被 SIGPIPE 终止（ctest 报 `Exception` / `Child aborted` 一类，无 `RESULT` 行）。

- [x] **Step 3: 修复**：`mpv_send` 改为

落地：[tests/cases/music/](../../../tests/cases/music/)、[custom/custom_musicplayer.c](../../../custom/custom_musicplayer.c)。

`_music_pause`、`_music_set_pos`、`_music_set_volume`、`_music_set_progress`、`_music_set_mode` 中的 `write(fd_mpv, cmd, strlen(cmd));` 与 `get_music_playback_time` 开头的两处 `write(fd_mpv, …)` 全部改为 `mpv_send(…, strlen(…))`。核对：`grep -c 'write(fd_mpv' custom/custom_musicplayer.c` 为 0。

- [x] **Step 4: 转绿与改坏**：`ID=7-green` 执行 [H1]；`mpv_send.mutants`：

落地：[tests/cases/music/](../../../tests/cases/music/)。

`IDS=MUSIC-7` 执行 [M]，`KILLED`。

- [x] **Step 5: 提交** `fix(music): 🐛 音乐 [7/13] 写 mpv 套接字改用 MSG_NOSIGNAL`；body：`- mpv 退出后 write(fd_mpv) 触发 SIGPIPE，整个界面被杀`、`- 全部写 mpv 的路径收口到 mpv_send()，以 send(MSG_NOSIGNAL) 发送、失败只打印；否决 signal(SIGPIPE, SIG_IGN)（进程级，影响 WiFi 的 popen）`、`详见 spec D6`。

### Task 24: ⑤ 监听线程释放 cJSON（`fix(music)`）

**Files:** Create `tests/cases/music/monitor.mutants`、`tests/cases/music/test_monitor.c`；Modify `custom/custom_musicplayer.c`、`tests/cases/music/CMakeLists.txt`。

**Interfaces:**
- Produces: `test_monitor.c` 的 `feed_and_drain(const char *data, size_t len)`（Task 25 复用）。

- [x] **Step 1: 写失败测试** `test_monitor.c`

落地：[tests/cases/music/](../../../tests/cases/music/)、[custom/custom_musicplayer.c](../../../custom/custom_musicplayer.c)。

CMake：`luckfox_add_test(monitor CASES no_leak)`（Task 25 追加）。

- [x] **Step 2: 确认先红**：`RE='^music\.monitor\.'; ID=5-red` 执行 [H1]。Expected：断言全部 `PASS`，进程退出时 LeakSanitizer 报 `detected memory leaks`（`cJSON_Parse` 分配，至少 7 个对象），退出码非 0。

- [x] **Step 3: 修复**：`get_music_playback_time` 中 `if (root != NULL) { … }` 块的末尾（`if (cJSON_HasObjectItem…)` 块之后）加 `cJSON_Delete(root);`。

- [x] **Step 4: 转绿与改坏**：`ID=5-green` 执行 [H1]，无 LSan 报告；`monitor.mutants`：

落地：[tests/cases/music/](../../../tests/cases/music/)。

`IDS=MUSIC-5` 执行 [M]，`KILLED`。

- [x] **Step 5: 提交** `fix(music): 🐛 音乐 [8/13] mpv 监听线程解析后释放 cJSON 对象`；body：`- 每收到一行 mpv 事件就 cJSON_Parse 一次且从不释放，播放期间每秒数次，内存持续增长`、`详见 spec §5.4`。

### Task 25: ⑥ `read` 读满后补 NUL（`fix(music)`）

**Files:** Modify `custom/custom_musicplayer.c`、`tests/cases/music/CMakeLists.txt`、`tests/cases/music/monitor.mutants`、`tests/cases/music/test_monitor.c`。

- [x] **Step 1: 写失败测试**：`test_monitor.c` 增加（`CASES` 追加 `full_buffer_line`）

落地：[tests/cases/music/](../../../tests/cases/music/)、[custom/custom_musicplayer.c](../../../custom/custom_musicplayer.c)。

- [x] **Step 2: 确认先红**：`RE='^music\.monitor\.full_buffer_line$'; ID=6-red` 执行 [H1]。Expected：ASan `stack-buffer-overflow` READ，栈含 `strtok` 与 `get_music_playback_time`（`buf[512]` 读满后无 NUL）。

- [x] **Step 3: 修复**：`if (read(fd_mpv, buf, sizeof(buf)) > 0)` 改为

落地：[tests/cases/music/](../../../tests/cases/music/)、[custom/custom_musicplayer.c](../../../custom/custom_musicplayer.c)。

（原 `if` 块内的其余语句与右括号不动。）

- [x] **Step 4: 转绿与改坏**：`ID=6-green` 执行 [H1]，两条通过；`monitor.mutants` 追加：

落地：[tests/cases/music/](../../../tests/cases/music/)。

`IDS='MUSIC-5 MUSIC-6'` 执行 [M]，均 `KILLED`。

- [x] **Step 5: 提交** `fix(music): 🐛 音乐 [9/13] mpv 监听线程读满缓冲区时补结尾 NUL`；body：`- read 读满 512 字节时 buf 没有结尾 NUL，随后 strtok 越界读`、`- 读 sizeof(buf) - 1 并按返回值补 NUL`、`详见 spec §5.4`。

### Task 26: ⑨ 补齐缺失的 `return`（`fix(music)`）

**Files:** Create `tests/cases/music/return.mutants`；Modify `custom/custom_musicplayer.c`、`tests/cases/music/CMakeLists.txt`。

- [x] **Step 1: 写失败测试**：`tests/cases/music/CMakeLists.txt` 追加

落地：[tests/CMakeLists.txt](../../../tests/CMakeLists.txt)、[tests/cases/music/CMakeLists.txt](../../../tests/cases/music/CMakeLists.txt)。

- [x] **Step 2: 确认先红**：`RE='^music\.compile\.return_type$'; ID=9-red` 执行 [H1]。Expected：`FAIL …custom_musicplayer.c`，3 处 `control reaches end of non-void function`（`music_player_thread_init`、`music_app_init`、`music_app_quit`）。

- [x] **Step 3: 修复**：`music_player_thread_init` 父进程分支 `pthread_create` 成功之后加 `return 0;`；`music_app_init` 末尾加 `return 0;`；`music_app_quit` 末尾加 `return 0;`。

- [x] **Step 4: 转绿与改坏**：`ID=9-green` 执行 [H1]；`return.mutants`：

落地：[tests/cases/music/](../../../tests/cases/music/)。

`IDS=MUSIC-9` 执行 [M]，`KILLED`。

- [x] **Step 5: 提交** `fix(music): 🐛 音乐 [10/13] 补齐音乐页三个 int 函数的返回值`；body：`- music_player_thread_init、music_app_init、music_app_quit 声明返回 int 却落到函数末尾，调用方读到未定义值`、`- 以 -Werror=return-type 的编译检查作为失败测试（-fsyntax-only 不产生该诊断，须 -c）`、`详见 spec D19`。

### Task 27: ① `vfork` 子进程 exec 失败后 `_exit(127)`（`fix(music)`）

**Files:** Create `tests/cases/music/player.mutants`、`tests/cases/music/test_player.c`；Modify `AGENTS.md`、`custom/custom_musicplayer.c`、`tests/CMakeLists.txt`、`tests/README.md`、`tests/cases/music/CMakeLists.txt`、`tests/cases/music/mpv_send.mutants`、`tests/cases/music/test_mpv_send.c`、`tests/support/fake_fs.c`、`tests/support/fake_fs.h`。

**Interfaces:**
- Produces: `#ifndef MPV_SOCKET_PATH`（默认 `"/tmp/mpvsocket"` 加 `_Static_assert`）；`int32_t fd_mpv = -1`；`tst_mpv_socket_path()`；`test_player.c` 的 `path_only_bin()`（Task 28、29 复用）；`now_ms()` 随 Task 28 首次使用时加入。

- [x] **Step 1: 测试支持**：`fake_fs` 增加 `const char *tst_mpv_socket_path(void);`（返回 `<临时目录>/mpv.sock`，写法同 `tst_wpa_conf_path()`）；`tests/CMakeLists.txt` 中 `custom_musicplayer.c` 的 `COMPILE_DEFINITIONS` 改为 `"MUSIC_DIR_PATH=tst_music_dir();MPV_SOCKET_PATH=tst_mpv_socket_path()"`。

- [x] **Step 2: 写失败测试** `test_player.c`

落地：[tests/cases/music/](../../../tests/cases/music/)、[custom/custom_musicplayer.c](../../../custom/custom_musicplayer.c)。

CMake：`luckfox_add_test(player CASES exec_failure)`。

新增 `music.mpv_send.negative_fd_no_send`：fd 为负时触发发送入口，断言包装的 send 调用数为 0。`MUSIC-14` 删除 `fd_mpv < 0` 直接返回的守卫，由此用例捕获。

- [x] **Step 3: 确认先红**：`RE='^music\.player\.'; ID=1-red` 执行 [H1]。Expected：非正常结束（子进程 `return` 后在共享栈上继续执行测试代码，表现为 SEGV、ASan 报告、栈保护失败或重复的 `RESULT` 输出之一），记录实际现象。

- [x] **Step 4: 修复** `custom/custom_musicplayer.c`：
  - 头部宏区加：

落地：[tests/cases/music/](../../../tests/cases/music/)、[custom/custom_musicplayer.c](../../../custom/custom_musicplayer.c)。

  - `int32_t fd_mpv;` 改为 `int32_t fd_mpv = -1;`；`mpv_send` 开头加 `if (fd_mpv < 0)\n        return -1;`。
  - `music_player_thread_init` 的 `vfork` 之前与子进程分支改为（父进程分支里 `strcpy(addr.sun_path, "/tmp/mpvsocket");` 改为 `memcpy(addr.sun_path, sock_path, strlen(sock_path) + 1);`，其余暂不动）：

落地：[tests/cases/music/](../../../tests/cases/music/)、[custom/custom_musicplayer.c](../../../custom/custom_musicplayer.c)。

  - `tests/README.md` 中「测试不调用 `custom_init()`：它用 `vfork()` 启动 `mpv`，主机无 `mpv` 时子进程 `return` 会破坏父进程栈。」改为「常规 UI 测试不调用 `custom_init()`：它会拉起 `mpv` 并做硬件相关初始化；`mpv` 的启动与连接由 `cases/music/` 的用例以假 `mpv` 单独测试。」
  - `AGENTS.md`「测试」节的「测试不调用 `custom_init()`（无 `mpv` 时其 `vfork` 子进程 `return` 会破坏父进程栈）。」改为「常规 UI 测试不调用 `custom_init()`（它会拉起 `mpv` 并做硬件相关初始化；`mpv` 的启动与连接由 `cases/music/` 的用例以假 `mpv` 单独测试）。」

- [x] **Step 5: 转绿与改坏**：`ID=1-green` 执行 [H1]，`exec_failure` 约 1 s 后返回 -1（连接段仍是旧的 `sleep(1)`，Task 28 改）；`player.mutants`：

落地：[tests/cases/music/](../../../tests/cases/music/)。

`IDS='MUSIC-1 MUSIC-14'` 执行 [M]，`KILLED`。另验证默认值的编译期检查：仓库外副本把默认值改成 109 字节的路径后 `gcc -fsyntax-only` 编译 `custom_musicplayer.c`（普通 `-I`），Expected `static assertion failed`；主机 gcc、`arm-linux-gnueabihf-gcc` 与 [X] 的 uClibc gcc 8.3 对默认值均编译通过。

- [x] **Step 6: 提交** `fix(music): 🐛 音乐 [11/13] mpv 启动失败时子进程 _exit 而不是 return`；body：`- vfork 子进程与父进程共用栈，execlp 失败后 return 会破坏父进程栈；板上自带 mpv 不触发，主机无 mpv 时必现`、`- 套接字路径改为 #ifndef MPV_SOCKET_PATH：默认值编译期 _Static_assert，覆盖值（测试的运行期表达式）在 vfork 前做长度检查；否决限定字面量并拼接（同一可执行文件的多个用例只能共用一条路径，ctest -j 下串扰）与读环境变量（改变板上行为）`、`- fd_mpv 初值 -1，mpv_send 对未连接直接返回`、`- 常规 UI 测试不调用 custom_init()，以避免硬件初始化；mpv 启动与连接由 cases/music/ 的假 mpv 用例单独验证`、`详见 spec D7、§5.4`。

### Task 28: ⑩ 轮询连接、超时回收、删同步 `mpv`（`fix(music)`）

**Files:** Create `tests/cases/music/connect.mutants`、`tests/cases/music/fake_mpv.py`；Modify `custom/custom_main.c`、`custom/custom_musicplayer.c`、`tests/cases/music/CMakeLists.txt`、`tests/cases/music/test_player.c`。

- [x] **Step 1: 写假 `mpv`** `tests/cases/music/fake_mpv.py`，`chmod +x` 并以 `git add --chmod=+x` 入库：

落地：[tests/cases/music/fake_mpv.py](../../../tests/cases/music/fake_mpv.py)。

- [x] **Step 2: 写失败测试**：`test_player.c` 增加（`CASES` 追加 `slow_socket timeout_reaps_child mpv_missing_fast stale_socket`，补 `<sys/un.h>`、`<string.h>`；测试仅以 `target_link_options(music_player PRIVATE -Wl,--wrap=clock_gettime)` 注入 2.5 倍速单调时钟，生产上限 5000 毫秒对应约 2000 毫秒实时时间，`now_ms()` 读取真实时钟，保留 1900–2900 毫秒与回收断言；CMake 在 `luckfox_add_test(player …)` 之后加 `target_compile_definitions(music_player PRIVATE TST_MUSIC_SRC_DIR="${CMAKE_CURRENT_SOURCE_DIR}")`）

落地：[tests/cases/music/](../../../tests/cases/music/)、[custom/custom_musicplayer.c](../../../custom/custom_musicplayer.c)、[custom/custom_main.c](../../../custom/custom_main.c)。

`music.player.timeout_reaps_child` 的实时时间须在 1900–2900 ms：5000 / 2.5 = 2000 ms；下限 1900 ms 容纳 100 ms 计时误差，上限 2900 ms 留出 900 ms 调度与回收余量，仍低于 MUSIC-10c 的 8000 / 2.5 = 3200 ms。`MUSIC-10c` 将生产连接预算从 5000 ms 改为 8000 ms，由此用例的上限断言捕获。

- [x] **Step 3: 确认先红**：`RE='^music\.player\.'; ID=10-red` 执行 [H1]。Expected：`exec_failure` 通过；`slow_socket` 返回 -1（`sleep(1)` 后单次 `connect` 失败）；`timeout_reaps_child` 约 1000 ms 返回且 `kill(pid, 0)` 成功（子进程未回收）；`mpv_missing_fast` 约 1000 ms。

- [x] **Step 4: 修复**：
  - 文件顶部补 `#include <sys/wait.h>`、`#include <time.h>`；宏区加 `#define MPV_CONNECT_TIMEOUT_MS 5000` 与 `#define MPV_CONNECT_POLL_MS 50`。
  - `music_player_thread_init` 之前新增：

落地：[tests/cases/music/](../../../tests/cases/music/)、[custom/custom_musicplayer.c](../../../custom/custom_musicplayer.c)、[custom/custom_main.c](../../../custom/custom_main.c)。

  - `vfork` 之前先 `unlink(sock_path)`，旧监听器即使存活也不能被当作新 mpv；`stale_socket` 在该路径预置监听器，并让假 mpv 在 unlink 前延迟 500 毫秒，断言旧监听器没有收到连接。
  - 父进程分支（`else if (pid > 0)` 到 `return 0;`）整体换成：

落地：[tests/cases/music/](../../../tests/cases/music/)、[custom/custom_musicplayer.c](../../../custom/custom_musicplayer.c)、[custom/custom_main.c](../../../custom/custom_main.c)。

  `close(0)` 原样保留到 Task 29 删除；`addr` 的赋值随旧连接段一并删除（连接改用 `mpv_wait_connect` 的局部 `sa`）。`socket()` 与 `pthread_create` 失败两条清理路径难以在测试里稳定触发，靠代码审查确认。
  - `custom/custom_main.c` 的 `custom_init()` 删去 `system("mpv 2>&1 >/dev/null");`。

- [x] **Step 5: 转绿与改坏**：`ID=10-green` 执行 [H1]，五条通过（`stdin_kept` 属于 Task 29），`slow_socket` 约 1.3–1.5 s、`timeout_reaps_child` 约 2.0 s、`mpv_missing_fast` 约 0–100 ms（首轮 waitpid 已可回收时立即返回）；再以 `ctest --test-dir build-tests -R '^music\.player\.' -j8 --repeat until-fail:5` 确认并行下稳定；`connect.mutants`：

落地：[tests/cases/music/](../../../tests/cases/music/)。

`IDS='MUSIC-1 MUSIC-10a MUSIC-10b MUSIC-10c MUSIC-12'` 执行 [M]，均 `KILLED`。

- [x] **Step 6: 提交** `fix(music): 🐛 音乐 [12/13] mpv 连接改为限时轮询并回收失败的子进程`；body：`- 固定 sleep(1) 后只 connect 一次：每次启动白等 1 s，慢板子仍可能连不上；连接前的 sleep(0.1) 实为 sleep(0)；custom_init() 里同步跑一次无参 mpv 的结果从未使用`、`- 每 50 ms 先 waitpid(WNOHANG) 再 connect，上限 5 s：mpv 缺失（127）或中途退出立即放弃，不靠耗尽重试；超时、socket()、pthread_create 失败都先 SIGKILL 再 waitpid，不留占着音频设备的 mpv 与僵尸进程`、`- 轮询期间用局部 fd，连上才赋给 fd_mpv`、`- 板上 mpv 启动耗时见真机清单，用于核对 5 s 上限`、`详见 spec D7、§2.5`。

### Task 29: ⑪ 删除 `close(0)`（`fix(music)`）

**Files:** Create `tests/cases/music/stdin.mutants`；Modify `custom/custom_musicplayer.c`、`tests/cases/music/CMakeLists.txt`、`tests/cases/music/test_player.c`。

- [x] **Step 1: 写失败测试**：`test_player.c` 增加（`#include <fcntl.h>`、`#include <sys/stat.h>`；`CASES` 追加 `stdin_kept`）

落地：[tests/cases/music/](../../../tests/cases/music/)、[custom/custom_musicplayer.c](../../../custom/custom_musicplayer.c)。

- [x] **Step 2: 确认先红**：`RE='^music\.player\.stdin_kept$'; ID=11-red` 执行 [H1]。Expected：`fd_type(0)` 变为 `S_IFSOCK`、`fd_mpv != 0` 为 `FAIL`。

- [x] **Step 3: 修复**：删除父进程分支开头的 `close(0);`。

- [x] **Step 4: 转绿与改坏**：`ID=11-green` 执行 [H1]，`music.player.*` 六条通过；`stdin.mutants`：

落地：[tests/cases/music/](../../../tests/cases/music/)。

`IDS=MUSIC-11` 执行 [M]，`KILLED`。

- [x] **Step 5: 提交** `fix(music): 🐛 音乐 [13/13] 启动 mpv 后不再关闭标准输入`；body：`- close(0) 让随后的 socket() 拿到 fd 0，mpv 套接字顶替了标准输入，其他读 stdin 的代码会读到 mpv 的数据`、`详见 spec §2.1 ⑪`。

### 音乐页回归检查点

- [x] 执行 [H]：全部通过；`ls /tmp | grep -c luckfox-tests-` 与运行前相同（无新增残留）；`pgrep -f fake_mpv.py` 无输出。
- [x] 执行 [X]：两条 `rc=0`；uClibc 下 `_Static_assert` 与 `MSG_NOSIGNAL` 可编译。
- [x] `IDS='MUSIC-2 MUSIC-C2 MUSIC-3 MUSIC-4 MUSIC-8 MUSIC-C1 MUSIC-7 MUSIC-5 MUSIC-6 MUSIC-9 MUSIC-1 MUSIC-10a MUSIC-10b MUSIC-10c MUSIC-11 MUSIC-12 MUSIC-13 MUSIC-14'` 执行 [M]：18 个 `KILLED`、退出 0。
- [x] 推送。

## 退出（Task 30）

### Task 30: ⑰ OFF 退出清屏改为 `custom_fb_clear()`（`fix(exit)`）

**Files:** Create `custom/custom_fb.c`、`custom/custom_fb.h`、`tests/cases/exit/CMakeLists.txt`、`tests/cases/exit/fb_clear.mutants`、`tests/cases/exit/test_fb_clear.c`；Modify `AGENTS.md`、`custom/custom.h`、`src/main.c`、`tests/CMakeLists.txt`、`tests/README.md`、`tests/support/fake_fs.c`、`tests/support/fake_fs.h`。

**Interfaces:**
- Produces: `int custom_fb_clear(const char *path)`（声明在轻量头 `custom/custom_fb.h`，`custom/custom.h` 紧随 `void custom_init();` 包含它）；`#ifndef FB_CLEAR_MAX_BYTES`（默认 64 MiB 总量）；`tests/support` 的 `unsigned long long tst_fb_clear_max_bytes`（默认 `64ULL << 20`）。

- [x] **Step 1: 测试支持**：`fake_fs.h` 加 `extern unsigned long long tst_fb_clear_max_bytes;`，`fake_fs.c` 定义 `unsigned long long tst_fb_clear_max_bytes = 64ULL << 20;`（放在 `tst_support` 里，链接 `luckfox_app` 的每个可执行文件都能解析）；`tests/CMakeLists.txt`：

落地：[tests/CMakeLists.txt](../../../tests/CMakeLists.txt)、[tests/cases/exit/CMakeLists.txt](../../../tests/cases/exit/CMakeLists.txt)。

并在 `cases/music` 之后 `add_subdirectory(cases/exit)`。

- [x] **Step 2: 写失败测试** `test_fb_clear.c`

落地：[tests/cases/exit/](../../../tests/cases/exit/)、[custom/custom_fb.c](../../../custom/custom_fb.c)、[custom/custom_fb.h](../../../custom/custom_fb.h)、[src/main.c](../../../src/main.c)。

`tests/cases/exit/CMakeLists.txt`：`luckfox_add_test(fb_clear CASES enospc_silent cap open_failure zero_write_silent eintr_retries)`。

新增 `exit.fb_clear.zero_write_silent`、`exit.fb_clear.eintr_retries`：前者注入 write 返回 0 并检查静默成功，后者注入首次 EINTR 并检查重试后写满设定长度。`EXIT-17c` 删除 write 返回 0 时退出循环的分支，由前者捕获；`EXIT-17d` 删除 EINTR 重试分支，由后者捕获。

- [x] **Step 3: 确认先红并记录旧行为**：`RE='^exit\.fb_clear\.'; ID=30-red` 执行 [H1]。Expected：`exit_fb_clear` 链接失败 `undefined reference to 'custom_fb_clear'`。旧命令的输出另存：`sh -c 'cat /dev/zero > /dev/full' 2>&1 | tee "$R/30-red-cat.txt"`，Expected `cat: write error: No space left on device`。

- [x] **Step 4: 修复**：`custom/custom_fb.c`

落地：[tests/cases/exit/](../../../tests/cases/exit/)、[custom/custom_fb.c](../../../custom/custom_fb.c)、[custom/custom_fb.h](../../../custom/custom_fb.h)、[src/main.c](../../../src/main.c)。

`custom/custom_fb.h` 加带包含守卫与 C++ 链接守卫的 `int custom_fb_clear(const char *path);`，`custom/custom.h` 紧随 `void custom_init();` 加 `#include "custom_fb.h"`；`src/main.c` 的 `system("cat /dev/zero > /dev/fb0");` 改为 `custom_fb_clear("/dev/fb0");`（`src/main.c` 未包含 `custom.h` 时补 `#include "custom.h"`）。顶层 `CMakeLists.txt` 以 `custom/*.c` glob 收入新文件，不需改动。

- [x] **Step 5: 转绿与改坏**：`ID=30-green` 执行 [H1]，五条通过；`fb_clear.mutants`：

落地：[tests/cases/exit/](../../../tests/cases/exit/)。

`IDS='EXIT-17a EXIT-17b EXIT-17c EXIT-17d'` 执行 [M]，均 `KILLED`。

- [x] **Step 6: 更新告警基线**：执行 [X]（两条构建均用 `cmake --build <构建目录> --clean-first -j"$(nproc)"` 清理重编），取两条构建日志的 `grep -c 'warning:'`（当前实测 glibc 119、uClibc 118）；`AGENTS.md`「Lint（代码检查）」节记录这两条最终告警基线，其余文字不动。
- [x] **Step 7: 提交** `fix(exit): 🐛 退出 [1/1] OFF 退出清屏改为 C 实现并静默处理写满`；body：`- system("cat /dev/zero > /dev/fb0") 每次退出都打印 cat: write error: No space left on device`、`- 4096 字节小块循环写，ENOSPC 与 write 返回 0 视为写满、EINTR 重试，总量上限防止写普通文件时不停；否决先取 FBIOGET_FSCREENINFO 的 smem_len（多一个 ioctl 依赖，/dev/full 与普通文件测不了）`、`详见 spec D9`。

### 退出回归检查点

- [x] 执行 [H]：全部通过。执行 [X]（同 Task 30 Step 6 清理重编）：两条 `rc=0`（`src/main.c` 链接到 `custom_fb_clear`）。`IDS='EXIT-17a EXIT-17b EXIT-17c EXIT-17d'` 执行 [M]。推送。

---

### Task 31: 全量验证、文档提交收尾与 CI

- [x] **Step 1: 全量改坏检验**：`R=$(mktemp -d); tests/tools/mutate.sh 2>&1 | tee "$R/mutate-all.txt"`。Expected：原有 12 个与本 PR 新增 50 个变体全部 `KILLED`，`62 run, 0 skipped`，退出 0。
- [x] **Step 2: 全量测试（root 与非 root）**：在确认不存在的全新构建目录执行 [H]，`100% tests passed`，输出中 `runtime error`、`ERROR: AddressSanitizer`、`ERROR: LeakSanitizer` 命中 0；`ctest --test-dir build-tests -N | tail -1` 的总数与本 plan 新增用例（21 条基线 + 本 PR 新增）一致；`chmod -R a+rwX build-tests && su nobody -s /bin/sh -c 'cd /workspace && ctest --test-dir build-tests -j"$(nproc)"'` 同样全过（容器无 `su` 或 `nobody` 时记「未执行」及原因），之后恢复属主与权限。
- [x] **Step 3: 两条交叉编译与冒烟**：执行 [X]；uClibc 产物按 `AGENTS.md` 以 `qemu-arm` 冒烟，Expected 含 `cannot open /dev/dri/card0`、`exit=1`。
- [x] **Step 4: 回填**：「验证证据」逐 Task 填修复前现象与修复后结果（执行时从临时目录 `$R` 中提取摘要），「与计划的偏离及原因」填实际偏离（含告警基线变化，不得新增告警），各 Task 勾选；已入库的完整代码正文改为指向仓库文件的一行「落地」。
- [x] **Step 5: 历史结构与独立验证**：`git rev-list --count "$BASE_REV"..HEAD` 为 32；`git log --stat "$BASE_REV"..HEAD -- docs/superpowers` 仅最后一个 docs 提交。1 个 test 与 30 个 fix 提交逐个导出到空目录并运行当时全量测试，31/31 通过；各提交全量条数见验证证据。以下脚本在仓库根目录执行，源码和构建目录均在仓库外：

```bash
set -e
BASE_REV=$(git merge-base origin/dev HEAD)
VALIDATION_ROOT=$(mktemp -d)
git rev-list --reverse "$BASE_REV"..HEAD~1 | while read -r REV; do
    SOURCE_DIR="$VALIDATION_ROOT/$REV/src"
    TEST_BUILD="$VALIDATION_ROOT/$REV/build"
    mkdir -p "$SOURCE_DIR"
    git archive "$REV" | tar -x -C "$SOURCE_DIR"
    cmake -S "$SOURCE_DIR/tests" -B "$TEST_BUILD" -DLUCKFOX_TESTS_ARM32=ON
    cmake --build "$TEST_BUILD" -j8
    ctest --test-dir "$TEST_BUILD" --output-on-failure -j8
done
```

- [x] **Step 6: CI**：PR #8 的 `pull_request` [run 37125871849](https://github.com/yuangezhizao/luckfox_pico_lvgl_example/actions/runs/37125871849) 除 `docs/superpowers` 外代码树与最终提交相同，运行的 head 由 `gh run view 37125871849 --json headSha --jq .headSha` 查询。`build-image`（dest-lock 复用 dev 镜像）、glibc、uClibc、`native-tests` 四个 job 均 success；`native-tests` 为 78/78，`main.brightness_arm32.conversion` 与 `main.tick.monotonic_wrap` 均 Passed，sanitizer 与 LSan ptrace 错误 0；各 job 耗时见「验证证据」。

### Task 32: 真机验证清单（物理 Luckfox Pico Ultra W，合并前完成）

CI 成功只证明交叉编译与测试门禁通过，不能代替真机点亮。逐项记录结果到「验证证据」：

- [x] 准备：板上执行 `which pkill`（预期来自 procps-ng）、`ps | grep -i dhcp`（同时看 `udhcpc` 与 `dhcpcd`）、`wpa_supplicant -v`（预期 2.6）。没有 `pkill` 时停下汇报，按 spec D17 改用 `-p` pidfile 方案。
- [x] 主屏：日历翻月；正午（12:xx）显示 PM、零点显示 12 AM。tick 溢出以 32 位模拟测试证明，不做 72 分钟长跑。
- [x] WiFi：输入含 `"`、`\`、`}` 的 SSID 与口令并 Load，`cat /etc/wpa_supplicant.conf` 与回读一致，文件模式未变；少于 8 位的密码、`"` 之后含 `#` 的值被拒绝且红色提示可见，改动文本框后提示消失；SSID 含空格的网络出现在扫描下拉框且不被截断；离开 WIFI 页后 `ps` 中不再周期出现 `wpa_cli status`；连按两次 Load 后 `ps` 只有一个 `udhcpc -i wlan0`，`eth0` 的实例不受影响。
- [x] 画板：笔迹落点与手指一致。
- [x] 音乐页：放入 ≥100 字节的文件名、总长超过 255 字节的列表、文件名含 `"` 的文件，选中与播放正常；`/music` 为空与缺失时进入音乐页、上一曲、下一曲不崩溃；`kill` 掉 `mpv` 后按下一曲，界面进程不退出。
- [x] mpv 启动耗时：冷启动后测 `mpv` 启动到 `/tmp/mpvsocket` 出现的耗时，并对比先跑一次无参 `mpv` 预热页缓存的情形，核对 5 s 上限是否足够。
- [x] OFF 退出：终端无 `cat: write error` 噪音，屏幕清黑。

执行结果见「验证证据」的 Task 32 各行与「真机遗留问题」；勾选表示已执行，不表示全部通过。

## 验证证据

各 Task 的全量用例数来自该提交的独立构建与测试；最终树检查点为 78 条用例（21 条基线与 57 条新增），历史检查点按所标 Task 导出源码后执行相应命令，CI 行仅记录所列 run 的结果。最终告警基线以 `AGENTS.md` 为准。

第 N 个 fix 提交与 Task N 对应（1 ≤ N ≤ 30）；前置 test 提交为第 1 个提交。历史行先用 `REV=$(git log --reverse --format=%H "$BASE_REV"..HEAD | sed -n "$((N + 1))p")` 定位，再执行 `SOURCE_DIR=$(mktemp -d); git archive "$REV" | tar -x -C "$SOURCE_DIR"`，在该目录执行行内命令；[M] 使用仓库外独立的 `MUTATE_WORK`，[X] 使用该导出树与独立构建目录。标为 Task 31 的行使用最终代码树。修复前现象在独立导出目录执行 `git diff "$REV^" "$REV" -- custom generated src lib | (cd "$SOURCE_DIR" && git apply --reverse)` 撤掉该项生产改动后，按行内测试命令核对；保留该提交的测试。

命令变量 `TEST_BUILD`、`CTEST_LOG` 分别指向本次全新构建目录与测试日志；`UCLIBC_CC`、`UCLIBC_BIN` 分别为 [X] 的 uClibc 编译器与产物。非 root 运行前按 Step 2 开放构建目录权限，结束后恢复属主与权限，并将 `TEST_BUILD` 传入 `su` 的环境。

执行时按 Task 回填：修复前现象与修复后结果（分别从对应 [H1] 临时目录的 `$R/<编号>-red.txt` 与 `$R/<编号>-green.txt` 提取摘要）、改坏检验结果、检查点的 ctest 条数与交叉编译告警数、CI run 与真机结果。

| 项 | 判据 | 实测 |
|---|---|---|
| Task 0（前置 test 提交） | 基线测试与改坏工具可用 | [H]：21/21；`tests/tools/mutate.sh`：12 KILLED、12 run、0 skipped；[X]：两条 rc=0，告警 glibc 132、uClibc 131 |
| Task 1（⑮ 隐式声明） | `RE='^main\.compile\.no_implicit_decl$'`：三个源文件先 FAIL 后 PASS；[H] 全过；[X] 两条 rc=0 | `ctest --test-dir build-tests --output-on-failure -R '^main.compile.no_implicit_decl$'`：修复前三个源文件分别报 `open`、`custom_tick_get` 隐式声明与 `struct flock` 重复定义，与 Expected 一致；修复后均 PASS；[H]：22/22；[X] 两条 rc=0，告警 glibc 130、uClibc 129；本 Task 无 [M] |
| Task 2（⑬ tick 单调时钟） | `RE='^main\.tick\.'`：主机与 32 位先红后绿；`IDS='MAIN-13a MAIN-13b'` [M] 全 KILLED；[H] 全过；[X] 两条 rc=0 | `ctest --test-dir build-tests --output-on-failure -R '^main.tick.'`：修复前主机回拨断言失败，32 位四个跨度断言失败；修复后两条通过；MAIN-13a、MAIN-13b 均被主机与 32 位用例捕获，2 run、0 skipped；[H]：24/24；[X] 告警 130、129，uClibc 无需额外链接 `rt` |
| Task 3（⑫ 正午与零点显示） | `RE='^main\.clock\.'`：先红后绿；`IDS='MAIN-12a MAIN-12b'` [M] 全 KILLED；[H] 全过 | `ctest --test-dir build-tests --output-on-failure -R '^main.clock.'`：修复前零点小时得 0（应为 12）、正午 PM 断言失败，11、13、23 点通过；修复后五个时刻全部通过；两个变体均 KILLED，2 run、0 skipped；[H]：25/25；本 Task 不要求 [X] |
| Task 4（⑭ 日历高亮与日期解析） | `RE='^main\.calendar\.'`：先红后绿；[H] 全过 | `ctest --test-dir build-tests --output-on-failure -R '^main.calendar.'`：修复前标签断言 FAIL 后 ASan 报 `stack-use-after-return`，两个畸形日期均在 `atoi(NULL)` 处被 UBSan 截获；修复后三条通过；[H]：28/28；`generated/` 不配 [M]，本 Task 不要求 [X] |
| Task 5（⑯ 主屏 popen 判空） | `RE='^main\.sysinfo\.'`：先红后绿；`IDS=MAIN-16` [M] KILLED | `ctest --test-dir build-tests --output-on-failure -R '^main.sysinfo.'`：修复前在 `fgets(…, NULL)` 处被 UBSan 非空参数检查截获；修复后返回 0，测试通过；MAIN-16 KILLED，1 run、0 skipped |
| 主屏回归检查点（第 5 个 fix 提交） | [H] 全过且含两条 `arm32`；[X] 两条 rc=0；`IDS='MAIN-13a MAIN-13b MAIN-12a MAIN-12b MAIN-16'` [M] 全 KILLED | [H] 29/29，`ctest -N -L arm32` 列出 `main.brightness_arm32.conversion` 与 `main.tick.monotonic_wrap`；[X] 两条 rc=0，告警 glibc 130、uClibc 129（基线 132、131）；[M] 5 个 KILLED，5 run、0 skipped，退出 0 |
| Task 6（⑳ SSID/密码校验） | `RE='^wifi\.(validate\|load)\.'`：先红后绿；`IDS='WIFI-20a WIFI-20b WIFI-20c WIFI-20utf8 WIFI-20cont WIFI-20hide WIFI-20length'` [M] 全 KILLED；[H] 全过 | `cmake --build build-tests -j"$(nproc)"`：`wifi_input_check` 未定义；暂时屏蔽 `validate` 注册时 `ctest --test-dir build-tests --output-on-failure -R '^wifi\.load\.'`：命令日志、标准输入与提示标签 3 条断言 FAIL，配置不变 PASS；`ctest --test-dir build-tests -V -R '^wifi\.(validate\|load)\.'`：6/6 通过，rules 19 条判定；`IDS='WIFI-20a WIFI-20b WIFI-20c WIFI-20utf8 WIFI-20cont WIFI-20hide WIFI-20length'` [M]：7 KILLED、0 skipped；[H]：36/36，通过的 arm32 2 条 |
| Task 7（㉑ 配置原子写入） | `RE='^wifi\.conf_write\.'`：先红后绿；`IDS='WIFI-21a WIFI-21b'` [M] 全 KILLED；[H] 全过 | `ctest --test-dir build-tests --output-on-failure -R '^wifi\.conf_write\.'`：修复前 `same_dir_atomic` 3 条 FAIL、`io_failures` 16 条 FAIL；修复后 2/2，通过 8 种 I/O 失败注入；`IDS='WIFI-21a WIFI-21b'` [M]：2 KILLED、0 skipped；[H]：38/38，含 arm32 2 条 |
| Task 8（C3 配置键精确匹配） | `RE='^wifi\.conf_write\.'`：先红后绿；`IDS=WIFI-C3` [M] KILLED；[H] 全过 | `ctest --test-dir build-tests --output-on-failure -R '^wifi\.conf_write\.only_ssid_psk_lines$'`：修复前完整配置比较 1 条 FAIL，`scan_ssid`、`bssid`、`wpa_psk` 被误替换；`ctest --test-dir build-tests --output-on-failure -R '^wifi\.conf_write\.'`：修复后 3/3；`IDS=WIFI-C3` [M]：1 KILLED、0 skipped；[H]：39/39，含 arm32 2 条 |
| Task 9（C5 块边界判定） | `RE='^wifi\.conf_write\.brace_in_value$'`：先红后绿；`IDS='WIFI-C3 WIFI-C5a WIFI-C5b WIFI-C5c'` [M] 全 KILLED；[H] 全过 | `ctest --test-dir build-tests --output-on-failure -R '^wifi\.conf_write\.brace_in_value$'`：修复前两次读回 `ssid=[] psk=[]`、4 条 FAIL，修复后 1/1；`IDS='WIFI-C3 WIFI-C5a WIFI-C5b WIFI-C5c'` [M]：4 KILLED、0 skipped；`ctest --test-dir build-tests -R '^wifi.conf_write.crlf_block_end$'`：1/1；[H]：41/41，含 arm32 2 条 |
| Task 10（㉕ 配置读取限宽） | `RE='^wifi\.conf_get\.'`：先红后绿；`IDS=WIFI-25` [M] KILLED；[H] 全过 | 回退生产改动后执行 `sed -i 's/_wifi_conf_get(ssid, 128, psk, 128)/_wifi_conf_get(ssid, psk)/' tests/cases/wifi/unit_wifi.c`，再按 [H1] 构建；`ctest --test-dir build-tests --output-on-failure -R '^wifi\.conf_get\.'`：修复前 ASan `stack-buffer-overflow`，`sscanf` 写 201 字节到 128 字节 SSID；修复后 1/1；`ctest --test-dir build-tests --output-on-failure -R '^wifi\.'`：15/15；`IDS=WIFI-25` [M]：1 KILLED、0 skipped；[H]：42/42，含 arm32 2 条 |
| Task 11（㉒ 扫描解析与解码） | `RE='^wifi\.scan\.'`：先红后绿；`IDS='WIFI-22a WIFI-22b WIFI-22c'` [M] 全 KILLED；[H] 全过 | `ctest --test-dir build-tests --output-on-failure -R '^wifi\.scan\.'`：修复前 parse 系列 3/3 FAIL，选项数分别 9/8、5/1、12/11；修复后 5/5；`IDS='WIFI-22a WIFI-22b WIFI-22c'` [M]：3 KILLED、0 skipped；[H]：47/47，含 arm32 2 条 |
| Task 12（㉓ + C4 popen 失败） | `RE='^wifi\.popen\.'`：两阶段先红后绿；`IDS='WIFI-23 WIFI-C4'` [M] 全 KILLED；[H] 全过 | `ctest --test-dir build-tests --output-on-failure -R '^wifi\.popen\.'`：修复前 status 在 `pclose(NULL)` 被 UBSan 截获，临时跳过 status 后 load 退出且无 `RESULT`；`./build-tests/cases/wifi/wifi_popen failure`：临时跳过 status 后退出码 1；修复后 1/1；`IDS='WIFI-23 WIFI-C4'` [M]：2 KILLED、0 skipped；[H]：48/48，含 arm32 2 条 |
| Task 13（㉔ WIFI 定时器页面守卫） | `RE='^wifi\.timer\.'`：先红后绿；`IDS=WIFI-24t` [M] KILLED；[H] 全过 | `ctest --test-dir build-tests --output-on-failure -R '^wifi\.timer\.'`：修复前 `popen_calls` 得 1、应为 0，恢复页后 `>= 1` 通过；修复后 1/1；`IDS=WIFI-24t` [M]：1 KILLED、0 skipped；[H]：49/49，含 arm32 2 条 |
| Task 14（㉔ DHCP 单实例） | `RE='^wifi\.dhcp\.'`：先红后绿；`IDS=WIFI-24d` [M] KILLED；[H] 全过 | `ctest --test-dir build-tests --output-on-failure -R '^wifi\.dhcp\.'`：修复前 `pkills` 得 0、应为 2，`ordered` FAIL，`starts` 得 2 通过；修复后 1/1；`IDS=WIFI-24d` [M]：1 KILLED、0 skipped；[H]：50/50，含 arm32 2 条 |
| WiFi 回归检查点（第 14 个 fix 提交） | [H] 全过且含两条 `arm32`；配置路径隔离；[X] 两条 rc=0；21 个 WiFi 变体全部 KILLED | [H]：50/50；`ctest --test-dir build-tests -N -L arm32`：2 条；`grep -c '/etc/wpa_supplicant.conf' build-tests/Testing/Temporary/LastTest.log`：0；[X]：glibc、uClibc 两条 rc=0，告警 127、126；`IDS='WIFI-20a WIFI-20b WIFI-20c WIFI-20utf8 WIFI-20cont WIFI-20hide WIFI-20length WIFI-21a WIFI-21b WIFI-C3 WIFI-C5a WIFI-C5b WIFI-C5c WIFI-25 WIFI-22a WIFI-22b WIFI-22c WIFI-23 WIFI-C4 WIFI-24t WIFI-24d'` [M]：21 KILLED、21 run、0 skipped、退出 0 |
| Task 15（⑱ 画布堆分配） | 画布三项先红后绿；SKETCH-18 KILLED；[H] 全过 | `ctest --test-dir build-tests --output-on-failure -R '^sketchpad\.canvas\.'`：修复前栈地址断言与两条空指针断言 FAIL，2/3 失败；修复后 3/3，无 LSan 报告；`IDS=SKETCH-18` [M]：1 KILLED（约 5 秒，SIGALRM）、1 run、0 skipped；[H]：53/53，含 arm32 2 条 |
| Task 16（⑲ 画布坐标换算） | 坐标用例先红后绿；SKETCH-19 KILLED；[H] 全过 | `ctest --test-dir build-tests --output-on-failure -R '^sketchpad\.stroke\.'`：修复前左上角 (0, -20)，笔迹与底色 2 条断言 FAIL；修复后 1/1；`IDS=SKETCH-19` [M]：1 KILLED、1 run、0 skipped；[H]：54/54，含 arm32 2 条 |
| 画板回归检查点（第 16 个 fix 提交） | [H] 全过；[X] 两条 rc=0；SKETCH-18、SKETCH-19 全 KILLED | [H]：54/54；`ctest --test-dir build-tests -N -L arm32`：2 条；[X]：glibc、uClibc 两条 rc=0，告警 126、125；`IDS='SKETCH-18 SKETCH-19'` [M]：2 KILLED、2 run、0 skipped、退出 0 |
| WiFi 回填校验检查点（第 6 个 fix 提交） | 原始 SSID 非法 UTF-8 不回填且提示 | `cmake --build build-tests --target wifi_load -j8` 后 `ctest --test-dir build-tests --output-on-failure -R '^wifi.load.dropdown_utf8$'`：修复前因提示控件不存在触发 UBSan 空指针成员访问，1/1 失败；修复后 1/1 |
| WiFi 扫描拼接边界检查点（第 11 个 fix 提交） | 格式化截断与负返回值不得导致剩余长度下溢 | `ctest --test-dir build-tests --output-on-failure -R '^wifi.scan.append_'`：修复前 2/2 失败，各 3 条断言失败，选项数分别 5/2、4/1，格式化调用次数分别 0/2、0/1；修复后 2/2；`IDS=WIFI-22c` [M]：1 KILLED |
| WiFi UTF-8 边界与提示隐藏检查点（第 6 个 fix 提交） | UTF-8 边界与编辑隐藏提示可捕获缺陷变体 | `ctest --test-dir build-tests --output-on-failure -R '^wifi.validate.utf8$'`：1/1，36 次判定；`ctest --test-dir build-tests --output-on-failure -R '^wifi.load.edit_hides_hint$'`：1/1；`IDS='WIFI-20utf8 WIFI-20cont WIFI-20hide WIFI-20length'` [M]：4 KILLED |
| WiFi 原型与错误提示检查点（第 6 个 fix 提交） | 完整原型与非法输入提示 | `ctest --test-dir build-tests --output-on-failure -R '^wifi.compile.prototypes$'`：修复前三条静态断言失败且 `wifi_input_check` 未声明，修复后 1/1；`cmake --build build-tests --target wifi_load -j8` 后 `ctest --test-dir build-tests --output-on-failure -R '^wifi.load.invalid_no_wpa_call$'`：修复前命令日志、标准输入与提示 3 条断言失败，修复后 1/1；`IDS=WIFI-20c` [M]：1 KILLED |
| WiFi 校验与扫描检查点（第 30 个 fix 提交） | 全量、WiFi 目录全部变体与两条交叉编译 | [H]：78/78；`ctest --test-dir build-tests -N -L arm32`：2 条；`IDS=$(sed -n 's/^id: //p' tests/cases/wifi/*.mutants)` [M]：22 KILLED、22 run、0 skipped、退出 0；[X]：两条 rc=0，告警 glibc 119、uClibc 118 |
| 画板分配失败检查点（第 16 个 fix 提交） | 分配失败变体五秒终止；全量与交叉编译通过 | `ctest --test-dir build-tests --output-on-failure -R '^sketchpad.canvas.'`：3/3；`IDS='SKETCH-18 SKETCH-19'` [M]：2 KILLED、2 run、0 skipped，SKETCH-18 以 SIGALRM 在 5.01 秒失败；[H]：54/54，含 arm32 2 条；[X]：两条 rc=0，告警 glibc 126、uClibc 125，新增 0 |
| Task 17 | 先红后绿；[H] 全过；本 Task 的 [M] | `cmake --build build-tests --target music_list -j8`：旧实现缺少 music_roller_options，链接失败；`ctest --test-dir build-tests --output-on-failure -R '^music\.list\.'`：实现接口后 1/1；`IDS='MUSIC-2 MUSIC-13'` [M]：2 KILLED、0 skipped；`ctest --test-dir build-tests -R '^music.list_alloc.'`：1/1；[H]：56/56，含 arm32 2 条 |
| Task 18 | 先红后绿；[H] 全过；本 Task 的 [M] | `ctest --test-dir build-tests --output-on-failure -R '^music\.list\.newline_name_skipped$'`：修复前 3 条断言 FAIL，修复后 1/1；`IDS=MUSIC-C2` [M]：1 KILLED、0 skipped；[H]：57/57，含 arm32 2 条 |
| Task 19 | 先红后绿；[H] 全过；本 Task 的 [M] | `ctest --test-dir build-tests --output-on-failure -R '^music\.mpv_cmd\.'`：修复前 sprintf 栈溢出，`ctest --test-dir build-tests --output-on-failure -R '^music\.'`：修复后 4/4；`IDS=MUSIC-3` [M]：1 KILLED、0 skipped；[H]：58/58，含 arm32 2 条 |
| Task 20 | 先红后绿；[H] 全过；本 Task 的 [M] | `ctest --test-dir build-tests --output-on-failure -R '^music\.roller\.'`：使用按文件名匹配的回调时 SIGALRM 5.01 秒，修复后 1/1；`IDS=MUSIC-4` [M]：1 KILLED、0 skipped；[H]：59/59，含 arm32 2 条 |
| Task 21 | 先红后绿；[H] 全过；本 Task 的 [M] | `ctest --test-dir build-tests --output-on-failure -R '^music\.list\.missing_dir$'`：修复前 UBSan 截获 readdir 空参数，修复后 1/1；`IDS=MUSIC-8` [M]：1 KILLED、0 skipped；[H]：60/60，含 arm32 2 条 |
| Task 22 | 先红后绿；[H] 全过；本 Task 的 [M] | `ctest --test-dir build-tests --output-on-failure -R '^music\.empty_list\.'`：修复前 UBSan 截获空节点成员访问，修复后 2/2；`IDS=MUSIC-C1` [M]：1 KILLED、0 skipped；[H]：62/62，含 arm32 2 条 |
| Task 23 | 先红后绿；[H] 全过；本 Task 的 [M] | `ctest --test-dir build-tests --output-on-failure -R '^music\.mpv_send\.'`：修复前 SIGPIPE 终止且无 RESULT，修复后 1/1；`IDS=MUSIC-7` [M]：1 KILLED、0 skipped；[H]：63/63，含 arm32 2 条；`grep -c 'write(fd_mpv' custom/custom_musicplayer.c`：0 |
| Task 24 | 先红后绿；[H] 全过；本 Task 的 [M] | `ctest --test-dir build-tests --output-on-failure -R '^music\.monitor\.'`：修复前断言通过但 LSan 报 1128 字节、32 次分配泄漏，修复后 1/1；`IDS=MUSIC-5` [M]：1 KILLED、0 skipped；[H]：64/64，含 arm32 2 条 |
| Task 25 | 先红后绿；[H] 全过；本 Task 的 [M] | `ctest --test-dir build-tests --output-on-failure -R '^music\.monitor\.full_buffer_line$'`：修复前 strtok 越界读 513 字节；`ctest --test-dir build-tests --output-on-failure -R '^music\.monitor\.'`：修复后 2/2；`IDS='MUSIC-5 MUSIC-6'` [M]：2 KILLED、0 skipped；[H]：65/65，含 arm32 2 条 |
| Task 26 | 先红后绿；[H] 全过；本 Task 的 [M] | `ctest --test-dir build-tests --output-on-failure -R '^music\.compile\.return_type$'`：修复前 3 处 control reaches end 错误，修复后 1/1；`IDS=MUSIC-9` [M]：1 KILLED、0 skipped；[H]：66/66，含 arm32 2 条 |
| Task 27 | 先红后绿；[H] 全过；本 Task 的 [M] | `ctest --test-dir build-tests --output-on-failure -R '^music\.player\.'`：修复前返回 0 断言失败且父进程 ASan SEGV，修复后 1/1、约 1 秒；`IDS='MUSIC-1 MUSIC-14'` [M]：2 KILLED、0 skipped；`ctest --test-dir build-tests -R '^music.mpv_send.negative_fd_no_send$'`：1/1；[H]：68/68，含 arm32 2 条；[X]：两条 rc=0，告警 119、118，新增 0 |
| Task 28 | 先红后绿；[H] 全过；本 Task 的 [M] | `ctest --test-dir build-tests --output-on-failure -R '^music\.player\.'`：修复前 3/5 失败，慢连接返回 -1、超时约 1000 毫秒且子进程未回收、缺失 mpv 约 1000 毫秒，`exec_failure` 与 `stale_socket` 通过；修复后 5/5；`ctest --test-dir build-tests -R '^music\.player\.' -j8 --repeat until-fail:5`：25/25；`IDS='MUSIC-1 MUSIC-10a MUSIC-10b MUSIC-10c MUSIC-12'` [M]：5 KILLED、0 skipped；[H]：72/72，含 arm32 2 条 |
| Task 29 | 先红后绿；[H] 全过；本 Task 的 [M] | `ctest --test-dir build-tests --output-on-failure -R '^music\.player\.stdin_kept$'`：修复前 fd 0 类型由 8192 变为 49152、3 条断言 FAIL；`ctest --test-dir build-tests --output-on-failure -R '^music\.player\.'`：修复后 6/6；`IDS=MUSIC-11` [M]：1 KILLED、0 skipped；[H]：73/73，含 arm32 2 条 |
| 音乐页回归检查点（第 29 个 fix 提交） | [H] 含 arm32；[X] 两条成功；18 个音乐变体全部 KILLED | [H]：73/73；`ctest --test-dir build-tests -N -L arm32`：2 条；`pgrep -af '[f]ake_mpv.py'`：0 个进程；[X]：两条 rc=0，告警 glibc 119、uClibc 118，新增 0；`IDS='MUSIC-2 MUSIC-C2 MUSIC-3 MUSIC-4 MUSIC-8 MUSIC-C1 MUSIC-7 MUSIC-5 MUSIC-6 MUSIC-9 MUSIC-1 MUSIC-10a MUSIC-10b MUSIC-10c MUSIC-11 MUSIC-12 MUSIC-13 MUSIC-14'` [M]：18 KILLED、18 run、0 skipped、退出 0 |
| Task 30（⑰ OFF 退出清屏） | 先红后绿；[H] 全过；[X] 无新增告警；四个变体 KILLED | `cmake --build build-tests -j"$(nproc)"`：修复前 custom_fb_clear 未定义导致链接失败；`sh -c 'cat /dev/zero > /dev/full'`：No space left on device；`ctest --test-dir build-tests --output-on-failure -R '^exit\.fb_clear\.'`：修复后 5/5；[H]：78/78，含 arm32 2 条；[X]（Step 6 清理重编）：两条 rc=0，告警 glibc 119、uClibc 118，新增 0；`IDS='EXIT-17a EXIT-17b EXIT-17c EXIT-17d'` [M]：4 KILLED、4 run、0 skipped、退出 0 |
| 退出回归检查点（第 30 个 fix 提交） | [H] 含 arm32；[X] 两条成功；四个退出变体 KILLED | [H]：78/78；`ctest --test-dir build-tests -N -L arm32`：2 条；[X]（Task 30 Step 6 清理重编）：两条 rc=0，告警 glibc 119、uClibc 118，新增 0；`IDS='EXIT-17a EXIT-17b EXIT-17c EXIT-17d'` [M]：4 KILLED、4 run、0 skipped、退出 0 |
| 音乐页连接预算检查点（第 28 个 fix 提交） | 功能验证 | `ctest --test-dir build-tests --output-on-failure -R '^music.player.(slow_socket\|timeout_reaps_child)$'`：修复前 2/2 失败，慢连接返回 -1，超时约 1000 毫秒且子进程未回收；`ctest --test-dir build-tests --output-on-failure -R '^music.player.'`：修复后 5/5，超时约 2 秒实时时间、生产时钟预算 5000 毫秒 |
| 音乐页旧监听路径检查点（第 28 个 fix 提交） | 旧监听路径回归与 unlink 变体验证 | `ctest --test-dir build-tests --output-on-failure -R '^music.player.stale_socket$'`：撤掉本提交生产改动时 1/1 通过，修复后 1/1 通过；`ctest --test-dir build-tests --output-on-failure -R '^music.player.'`：修复后 5/5；`IDS=MUSIC-12` [M]：删除 `unlink` 后旧监听器接到连接、断言 FAIL，1 KILLED、0 skipped |
| 音乐页回调与接口检查点（第 20 个 fix 提交） | 功能验证 | `ctest --test-dir build-tests --output-on-failure -R '^music.roller.'`：修复前按文件名匹配的回调 SIGALRM 5.01 秒，修复后 1/1；`ctest --test-dir build-tests --output-on-failure -R '^music\.list\.'`：2/2；[H]：59/59 |
| 音乐页分配失败检查点（第 17 个 fix 提交） | 功能验证 | `cmake --build build-tests --target music_list_alloc -j8`：修复前 `music_roller_options` 缺少声明、`music_scan_list` 调用参数不足，编译失败；`ctest --test-dir build-tests --output-on-failure -R '^music.list_alloc.'`：修复后 1/1；`IDS=MUSIC-13` [M]：1 KILLED、0 skipped |
| 音乐页综合检查点（第 30 个 fix 提交） | 功能验证 | [H]：78/78；`ctest --test-dir build-tests -N -L arm32`：2 条；`ctest --test-dir build-tests -R '^music.player.' -j8 --repeat until-fail:5`：30/30；`IDS=$(sed -n 's/^id: //p' tests/cases/music/*.mutants)` [M]：18 KILLED、18 run、0 skipped、退出 0；[X]：两条 rc=0，告警 glibc 119、uClibc 118，新增 0 |
| Task 31 全量改坏检验 | 原有 12 个与新增 50 个全部捕获 | `tests/tools/mutate.sh`：62/62 KILLED、62 run、0 skipped、退出 0 |
| Task 31 全量测试 | root 与非 root；ARM32 强制启用；sanitizer 无错误 | [H]（全新构建目录，`-DLUCKFOX_TESTS_ARM32=ON`）：78/78；`su nobody -s /bin/sh -c 'cd /workspace && ctest --test-dir "$TEST_BUILD" --output-on-failure -j"$(nproc)"'`：78/78；`ctest --test-dir "$TEST_BUILD" -N -L arm32`：2 条，`main.brightness_arm32.conversion`、`main.tick.monotonic_wrap`；`grep -Ec -e 'runtime error' -e 'ERROR: AddressSanitizer' -e 'ERROR: LeakSanitizer' "$CTEST_LOG"`：root 与 nobody 均 0 |
| Task 31 交叉编译 | 两条 rc=0，无新增告警 | [X]：glibc rc=0、119 条告警；uClibc rc=0、118 条告警；较退出回归检查点新增 0 |
| Task 31 uClibc 冒烟 | 动态库可加载，硬件入口按预期退出 | `qemu-arm -L "$("$UCLIBC_CC" -print-sysroot)" -E LD_LIBRARY_PATH="$PWD/lib/uclibc/libdrm:$PWD/lib/uclibc/libcjson" "$UCLIBC_BIN"; echo "exit=$?"`：包含 `cannot open /dev/dri/card0`、exit=1 |
| Task 31 历史结构 | 1 个 test、30 个 fix、最后 1 个 docs | `git rev-list --count "$BASE_REV"..HEAD`：32；`git log --format=%s "$BASE_REV"..HEAD`：1 个 test、30 个 fix、1 个 docs；`git log --format=%s "$BASE_REV"..HEAD -- docs/superpowers`：1 个 docs |
| Task 31 最终树验证 | ARM32、双 ABI 与全量变体通过 | [H]（全新构建目录，`-DLUCKFOX_TESTS_ARM32=ON`）：78/78，含 arm32 2 条，sanitizer 错误 0；[X]：两条 rc=0，告警 glibc 119、uClibc 118，新增 0；`tests/tools/mutate.sh`：62/62 KILLED、62 run、0 skipped、退出 0 |
| Task 31 逐提交单独验证 | 空目录导出、ARM32 强制启用、每个 test/fix 提交全量测试 | 执行 Task 31 Step 5 的 `git rev-list` 导出与验证脚本：31/31 提交通过；全量条数依次为 21→22→24→25→28→29→36→38→39→41→42→47→48→49→50→53→54→56→57→58→59→60→62→63→64→65→66→68→72→73→78；sanitizer 错误 0 |
| WiFi 超长扫描项（第 6 个 fix 提交） | 33 字节 ASCII 与中文均拒绝回填，Load 无副作用 | `cmake --build build-tests --target wifi_load -j8` 后 `ctest --test-dir build-tests --output-on-failure -R '^wifi.load.dropdown_overlong$'`：修复前因提示控件不存在触发 UBSan 空指针成员访问，1/1 失败；修复后 1/1；`IDS=WIFI-20length` [M]：1 KILLED、0 skipped |
| 音乐分配失败交互（第 22 个 fix 提交） | 空节点初始化、上一曲、下一曲和 roller 安全返回 | `ctest --test-dir build-tests --output-on-failure -R '^music.empty_list.alloc_failure_no_crash$'`：初始化守卫缺失时 1/1 UBSan 失败，完整实现 1/1 通过 |
| Task 31 CI | 四个 job success，原生测试包含两条 ARM32 用例 | `gh run view 37125871849`：[run 37125871849](https://github.com/yuangezhizao/luckfox_pico_lvgl_example/actions/runs/37125871849)（运行的 head 由 `gh run view 37125871849 --json headSha --jq .headSha` 查询，除 `docs/superpowers` 外代码树与最终提交相同）；`gh run view 37125871849 --json jobs`：build-image success 19 秒、glibc success 33 秒、uClibc success 36 秒、native-tests success 66 秒；`gh run view 37125871849 --job 111210986075 --log`：78/78，`main.brightness_arm32.conversion` 与 `main.tick.monotonic_wrap` 均 Passed，sanitizer 与 LSan ptrace 错误 0 |
| Task 32 测试产物 | ARM 32 位 uClibc，与板上二进制一致 | ADB 核对 ELF 与 `sha256sum`：[CI run 37115102018](https://github.com/yuangezhizao/luckfox_pico_lvgl_example/actions/runs/37115102018) 构建的 ARM 32 位 uClibc 二进制，SHA256 `f44fe0ab32cc7deac7780f75cc5d774fe4ab3a1409d058b880b2f05495a6fdc9`；该 run 在 PR 开发后期构建，运行代码与最终提交一致，仅 tests/ 与文档不同 |
| Task 32 板上环境 | 记录目标板、固件与运行依赖 | 2026-10-03，Luckfox Pico Ultra W，经 ADB；`which pkill`、`pkill --version`：`/bin/pkill`，procps-ng 3.3.17；`ps`、`busybox`：dhcpcd + BusyBox udhcpc 1.36.1，测试前 wlan0 两个 udhcpc、eth0 一个；`wpa_supplicant -v`：v2.6；固件 Buildroot 2023.02.6（`-g824b817f8`），`uname -a`：Linux 5.10.160 armv7l；`cat /proc/device-tree/model`：`Luckfox Pico Ultra`（共用设备树，不能据此否定 W 板型）；uClibc 1.0.31 |
| Task 32 准备 | pkill 可用，核对 DHCP 与 wpa_supplicant | `which pkill`、`ps` 筛选 dhcp、`wpa_supplicant -v`：pkill 可用，dhcpcd 与 udhcpc 均运行，wpa_supplicant v2.6；通过 |
| Task 32 主屏 | 翻月、正午 PM、零点 12 AM | 日历翻月、临时改时后观察：翻月正常，显示 `12:13 PM`、`12:00 AM`；通过；时间进度与 ntpd 已恢复，未做 72 分钟长跑 |
| Task 32 WiFi | 特殊字符配置、权限、输入校验、SSID、离页轮询与 DHCP 实例 | 输入特殊字符后 Load，`cat /etc/wpa_supplicant.conf`：写入正确，重启后 SSID 回读一致，密码逐字回读未验证；文件权限保持 644；两种非法输入被拒且配置哈希不变，编辑后提示消失；手动 `wpa_cli -i wlan0 scan` 后 `Redmi K20 Pro` 完整显示；离页 `ps` 采样 200 次无 status（可能漏过短命进程）；两次 Load 后 wlan0 一个 udhcpc，eth0 PID 397 保留；多数子项通过 |
| Task 32 画板 | 笔迹落点与手指一致 | 进入 PAD，中央画十字、右下画短线与轻点：中央正常，右下区域笔迹吸附到右侧靠下位置；轻点不出点、移动后才出短线；不通过，根因未确认 |
| Task 32 音乐页 | 长名称与列表、双引号文件选播、空／缺失目录与 mpv 退出处理 | 放入 3 个各 107 字节名称（列表 339 字节）及 `quote"test.mp3` 后选播：mpv 加载 `01_…mp3` 与 `quote"test.mp3`，进度前进；`kill` mpv 后下一曲仍响应，移走目录后提示且不崩溃；长名称裁切使三项无法区分，首次进度约 50%；不通过；空目录重新加载未测，空／缺失时上下曲不可达，无扬声器，声音未测 |
| Task 32 mpv 耗时 | 3 次清缓存与 3 次预热，核对 5 秒预算 | 清缓存后启动 mpv、记录套接字出现耗时：780/780/780 ms；先运行无参 `mpv` 预热：520/510/500 ms，套接字均出现；通过，最大值距 5 秒上限 4.22 秒；采用清缓存模拟，未测整板重启，套接字出现不等于连接成功 |
| Task 32 OFF 退出 | 屏幕清黑、进程退出、日志无写错误 | 点击 OFF 后观察屏幕、`ps` 与终端日志：屏幕清黑，应用与 mpv 退出，无 `cat: write error`；通过；残留套接字已清理 |

## 与计划的偏离及原因

- Task 2：32 位换算的 Δ=1 s 对照通过、Δ=1 小时对照失败；主机用例以 20 ms 睡眠后差值大于 0 且小于 1000u 判定单调递增，避免恒为 0 的实现通过。
- Task 3、4：时制测试必须初始化 UI，避免日期更新先访问空控件；日历用例在断言后刷新 stdout，确保 ASan 终止前输出可见。
- Task 5、12、21、22：空指针缺陷由 UBSan 的非空参数或成员访问检查截获，失败形式为 sanitizer 错误。
- Task 7：配置写入包含 8 种 I/O 故障注入；fstat、fchmod、读写、刷盘或关闭失败均不得 rename，以满足 D14 的模式保留与原子替换约束。
- Task 9：WIFI-C5a 在块结束判定入口返回 `strstr(line, "}") != NULL`，完整恢复子串误判；仅改变首字符检查而保留后续整行约束不能模拟该缺陷。
- Task 10：共用 unit_wifi.c 的六个 I/O 包装使用 libc 弱定义透传，由写入测试的强定义覆盖，其他可执行文件可以链接且故障注入保持有效。
- Task 12、18、30：变体锚点避开 C 字符串的反斜杠 n，因为 mutate.sh 会将其解码为换行；WIFI-23 匹配 popen 判空入口，MUSIC-C2 删除跳过分支的 continue，EXIT-17b 匹配错误输出前缀。
- Task 15：SKETCH-18 触发 LV_ASSERT_NULL 后进入 while(1)，alloc_failure 用例用 alarm(5) 将失败限定为约 5 秒 SIGALRM；正常实现必须通过全部断言。
- WiFi 输入与扫描边界：文本框会过滤非法 UTF-8 或截断超长名称，因此下拉框在回填前拒绝非法 UTF-8 与超过 32 字节的选择并提示，保留原输入；扫描拼接遇 snprintf 负返回值或截断时撤回整条，避免 used 超界。
- WiFi 校验与提示测试：提示标签通过页面子控件查找，生产指针保持文件内 static；popen 失败由调用次数、连接状态和 UBSan 判定。
- Task 17、22：选项串分配失败会使选项与节点错位，因此二者同时清空；Task 17 只验证清理、滚轮空文本和扫描恢复，Task 22 负责分配失败后的初始化及交互守卫。LVGL 无限滚轮把一项空文本展开成七页，测试检查页间换行、真实选项数和选中文本。
- Task 27、28：now_ms 仅在 Task 28 的计时用例中定义，避免未使用函数告警；mpv 缺失时首轮 waitpid 已可回收则立即返回，允许约 0–100 毫秒。
- Task 28：连接预算为 5000 毫秒，测试单调时钟加速 2.5 倍，真实计时与回收断言保持 1900–2900 毫秒；vfork 前移除旧监听路径，避免连接到另一个存活实例。
- Task 30：清屏接口使用轻量 custom_fb.h，避免引入 GUI 依赖与 LVGL 的 -Wundef 告警；两条交叉编译均从空目录构建。
- 音乐长文件名回调：环形链表上的死循环由用例 alarm(5) 以 SIGALRM 判失败，CTest 默认超时仍为 120 秒。
- 告警基线：glibc 119、uClibc 118；声明补全、移除画布栈 VLA、完整 WiFi 原型、删除未用代码、消除 loadfile 溢出与补齐返回值均减少既存告警。
- 变体清单：共 62 个，其中原有 12 个、本 PR 新增 50 个；主屏 16、WiFi 22、画板 2、音乐 18、退出 4。

### 真机遗留问题

以下现象转到后续 PR 处理，复现依据见 [Task 32 真机测试评论](https://github.com/yuangezhizao/luckfox_pico_lvgl_example/pull/8#issuecomment-5968843362)：

- Scan 按钮只读取扫描结果、不主动扫描：开启热点后连点 Scan 10 次一直显示 `scanning`，手动 `wpa_cli -i wlan0 scan` 后才能发现热点。
- 画板中央正常，右下区域落点吸附到右侧靠下位置；根因未确认。
- 画板轻点不出点，移动后才出现短线。
- 音乐列表长名称前缀被裁切，无法区分三项；首次进入进度滑块约 50%。
- 应用启动时无音乐，运行中加入文件后仍提示找不到音乐，重启后可进入。
