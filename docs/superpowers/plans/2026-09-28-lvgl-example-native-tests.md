# Luckfox Pico LVGL Example — 原生测试基础设施实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 恢复主程序编译告警，新增按页面分类的 `tests/` 原生 CMake 工程并纳入 PR #5、#6 的既有测试，把 `qemu-user` 补进两份 Dockerfile，接入 CI。

**Architecture:** `tests/` 是独立的主机 CMake 工程：被测源码（`lib/lvgl/src`、`generated/`、`custom/`）以 `-w` 编成静态库 `luckfox_app`，`tests/support/` 是 OBJECT 库（5 个全局量、headless 显示与脚本化触摸、输出捕获、临时目录），用例按页面放在 `tests/cases/<页面>/`，经 `luckfox_add_test()` 注册为 `<类别>.<对象>.<用例>` 的 CTest；全部以 ASan/UBSan 编译。32 位换算测试以 ARM 交叉 gcc 编译、`qemu-arm` 运行。

**Tech Stack:** C（gnu99）、CMake ≥ 3.16 与 CTest、主机 gcc 13 + ASan/UBSan、LVGL 8.3.10、`arm-linux-gnueabihf-gcc` + `qemu-user`、SDL2（仅 `preview`）、GitHub Actions。

**Spec:** [`docs/superpowers/specs/2026-09-28-lvgl-example-native-tests-design.md`](../specs/2026-09-28-lvgl-example-native-tests-design.md)

已完成的 Task 中，代码与文档以仓库为准，每个 Task 只留一行「落地」（文件、要点、提交标题）以及改坏命令、验证命令与 Expected。

## Global Constraints

每个 Task 默认包含 spec 全文。硬约束：

- 除 Task 1 的 `CMakeLists.txt` 外不改 `src/`、`custom/`、`generated/`、`lib/`；`.cursor/Dockerfile` 与 `.cursor/Dockerfile.luckfox_pico` 只补装 `qemu-user`（spec NFR1、D8）。
- 测试不调用 `custom_init()`（spec §2.5）。
- 本 PR 只提交在当前 `dev` 代码上通过的测试（spec D12）。
- 测试代码不引入 `strcpy`/`strcat`/`sprintf`/`gets`；路径格式化用 `snprintf`/`tst_fmt()` 并检查截断（spec NFR3）。
- 测试不依赖 root、`strace`、`su` 或真实设备节点（spec NFR4）。
- 目录与命名：用例在 `tests/cases/<页面>/test_<对象>.c`，访问 `static` 函数时配 `unit_<对象>.c`；CTest 名 `<类别>.<对象>.<用例>`，交叉编译的测试同样适用；不加序号或日期前缀；改坏清单按被测对象一份 `<对象>.mutants`（spec §5.2）。
- 测试代码与 `support/` 以 `-Wall -Wextra -Werror` 编译；被测源码与 `unit_*.c` 以 `-w` 编译（spec §5.3、D4）。
- 注释与文档单句不硬折行。
- 提交格式沿用仓库惯例 `type(scope): emoji 主题`（git cz），subject 单一主题，body 用 `-` 列 why（根因、关键决策与被否决的替代、权衡、⚠️合并后须知）；提交序与标题按 spec §8；本 plan 与 spec 最终合为 PR 最后一个 `docs(superpowers)` 提交，仅为此目的的重排直接 `git push --force-with-lease`，无需再询问作者。
- 草稿 PR 在执行前已创建；每个 Task 提交后推送到 `cursor/native-tests-cbb9`，PR 上的 CI 随之运行。
- 截图存 Project Context `media/native-tests/`，脚本与证据存 `internal/native-tests/`（Project Context 根：`/cursor/stores/bc-5aabc2a6-3a8e-4afe-93fb-808fe19d5b10/`）。

## File Structure

| 文件 | 职责 | Task |
|---|---|---|
| `CMakeLists.txt` | 告警段上移、`-std=gnu99` | 1 |
| `.gitignore` | 忽略 `build-tests/` | 2 |
| `tests/CMakeLists.txt` | 工程入口：编译选项、`luckfox_app`、`tst_support`、`luckfox_add_test()`、32 位探测、手动目标 | 2、3、5、6 |
| `tests/README.md` | 运行方法、类别表、命名与新增步骤、32 位与手动目标说明 | 2、5、6、12、13 |
| `tests/support/check.h` | 头文件自带实现的断言与用例分派 | 2 |
| `tests/support/fake_fs.{h,c}` | 本进程临时目录、路径格式化、假文件、`tst_backlight_root()` | 2 |
| `tests/support/capture.{h,c}` | stdout/stderr 捕获与按行计数 | 2 |
| `tests/support/app_env.{h,c}` | 5 个全局量、headless 显示/指针、`tst_run_ms`/`tst_tap`/`tst_drag`、PPM、呈现钩子 | 2 |
| `tests/cases/main/CMakeLists.txt` | 注册主屏类用例与 32 位测试 `main.brightness_arm32.conversion` | 2、4、5、12 |
| `tests/cases/main/{test,unit}_sdio_detect.c` | SDIO 判定 | 2 |
| `tests/cases/main/test_brightness.c` | 亮度断言 11 项 | 4 |
| `tests/cases/main/{test,unit}_brightness_arm32.c` | 32 位换算 | 5 |
| `tests/cases/wifi/CMakeLists.txt`、`test_backend_release.c` | `wifi_backend_release()` 判空 | 3 |
| `tests/tools/{screenshots.c,preview.c,mutate.sh}` | 手动目标 | 6、13 |
| `tests/cases/main/{brightness,sdio_detect}.mutants`、`tests/cases/wifi/backend_release.mutants` | 改坏变体（M1–M7、M10、M11；S1；W1） | 6 |
| `tests/cases/main/brightness_arm32.mutants` | 32 位换算的改坏变体 M12 | 6、12 |
| `.cursor/Dockerfile`、`.cursor/Dockerfile.luckfox_pico` | 补装 `qemu-user` | 12 |
| `.github/workflows/build-luckfox-lvgl-demo.yml` | `native-tests` job | 7、12 |
| `AGENTS.md` | Cloud Agent 环境、GitHub Actions、Lint、测试、运行/验证 GUI 五节 | 8、12 |
| `docs/superpowers/plans/2026-09-2{5,7}-*.md` 与对应 spec | 精简已入库测试正文 | 9、12 |
| `/tmp/luckfox-red/red.sh`（不入库） | Task 2–5 的单点改坏检验脚本 | 2 |

---

### Task 1: 恢复编译告警（`build(cmake)`）

**Files:** Modify `CMakeLists.txt:74-88`。

- [x] **Step 1: 构建改动前基线**（仓库外，同一路径前后各建一次，保证产物可逐字节比较；「改动前」取 `origin/dev` 的树，因为分支已含改动）

```bash
set -euo pipefail
W=/tmp/c1cmp; rm -rf $W; mkdir -p $W/src
git -C /workspace archive origin/dev | tar -x -C $W/src
[ -d /tmp/luckfox-pico/tools/linux/toolchain ] || { git clone -q --depth 1 --filter=blob:none --sparse -b dev https://github.com/yuangezhizao/luckfox-pico.git /tmp/luckfox-pico && git -C /tmp/luckfox-pico sparse-checkout set tools/linux/toolchain; }
build() {  # $1=标签
  (cd $W/src && rm -rf build-g build-u \
   && mkdir build-g && cd build-g && env -u LUCKFOX_SDK_PATH GLIBC_COMPILER=/usr/bin/arm-linux-gnueabihf- cmake .. >/dev/null && make -j"$(nproc)" > $W/make-glibc-$1.log 2>&1 && cp luckfox_lvgl_demo $W/glibc-$1 \
   && cd .. && mkdir build-u && cd build-u && env -u GLIBC_COMPILER LUCKFOX_SDK_PATH=/tmp/luckfox-pico cmake .. >/dev/null && make -j"$(nproc)" > $W/make-uclibc-$1.log 2>&1 && cp luckfox_lvgl_demo $W/uclibc-$1)
}
build before
grep -c 'warning:' $W/make-glibc-before.log $W/make-uclibc-before.log
```

Expected：glibc 4、uClibc 2。

- [x] **Step 2: 落地**：`CMakeLists.txt` 以仓库为准——`# warnning` 与其下整段 `add_compile_options(…)` 剪切到 `add_executable(` 之前，段末 `-std=c99` 改为 `-std=gnu99`；`# debug` 与 `add_compile_options(-fPIC -Wall -O3 -g0)` 留在 `add_executable()` 之后。Step 3 对比通过后提交 `build(cmake): 🔧 恢复主程序编译告警`。

- [x] **Step 3: 构建改动后并对比**

```bash
set -euo pipefail
W=/tmp/c1cmp
cp /workspace/CMakeLists.txt $W/src/CMakeLists.txt
build after   # build() 定义见 Step 1，须在同一 shell 中运行
grep -c 'warning:' $W/make-glibc-after.log $W/make-uclibc-after.log
grep -c ' error:' $W/make-glibc-after.log $W/make-uclibc-after.log || true
grep '^C_FLAGS' $W/src/build-g/CMakeFiles/luckfox_lvgl_demo.dir/flags.make | grep -c -- '-std=gnu99'
grep '^C_FLAGS' $W/src/build-g/lib/lvgl/CMakeFiles/lvgl.dir/flags.make
cmp $W/glibc-before $W/glibc-after && echo GLIBC_IDENTICAL
{ cmp -l $W/uclibc-before $W/uclibc-after || true; } | wc -l   # cmp 有差异时返回 1，须防 set -e 中止
for s in .text .rodata .data .bss .init_array; do echo "$s $(arm-linux-gnueabihf-objdump -s -j $s $W/uclibc-before | tail -n +4 | md5sum | cut -c1-12) $(arm-linux-gnueabihf-objdump -s -j $s $W/uclibc-after | tail -n +4 | md5sum | cut -c1-12)"; done
diff <(arm-linux-gnueabihf-readelf -SW $W/uclibc-before) <(arm-linux-gnueabihf-readelf -SW $W/uclibc-after) && echo SECTION_HEADERS_IDENTICAL
diff <(arm-linux-gnueabihf-readelf -sW $W/uclibc-before) <(arm-linux-gnueabihf-readelf -sW $W/uclibc-after) | head || true
```

Expected：告警 glibc 132、uClibc 131；error 0；主程序 `C_FLAGS` 含 `-std=gnu99` 计数 1；LVGL `C_FLAGS =` 为空；`GLIBC_IDENTICAL`；uClibc 差异 23 字节，五个段两列哈希相同，`SECTION_HEADERS_IDENTICAL`，符号表差异只有 `start_ms.NNNNN` 这类局部静态变量编号后缀。结果与 spec §5.1 不一致时停下汇报。

- [x] **Step 4: 存证据**：`cp $W/make-*-{before,after}.log` 与上一步输出（存为 `binary-compare.txt`）到 Project Context `internal/native-tests/warnings-baseline/`。

---

### Task 2: 测试骨架与 SDIO 判定测试（`test(tests)`）

**Files:** Create `tests/CMakeLists.txt`、`tests/README.md`、`tests/support/{check.h,fake_fs.h,fake_fs.c,capture.h,capture.c,app_env.h,app_env.c}`、`tests/cases/main/{CMakeLists.txt,test_sdio_detect.c,unit_sdio_detect.c}`；Modify `.gitignore`（追加 `build-tests/`）；Create（仓库外）`/tmp/luckfox-red/red.sh`。

- [x] **Step 1: 落地**：上列文件以仓库为准，提交 `test(tests): ✅ 新增原生测试工程并纳入 SDIO 板型判定测试`。要点：
  - `check.h` 头文件自带实现，32 位测试不链接 `support/` 也能直接包含；`tst_run_case()` 按命令行参数选用例，同一可执行文件每次只跑一个用例，用例之间不共享 LVGL 与模块静态状态。
  - `fake_fs`：每个进程一个 `mkdtemp` 临时目录并在 `atexit` 中递归清理，被 ASan 终止的进程不跑 `atexit`，其临时目录会残留；路径一律经 `tst_fmt()`/`tst_path()` 以 `snprintf` 格式化并检查截断；`tst_read_long()` 失败返回 −1；被测 `custom_brightness.c` 经 `-include fake_fs.h` 获得 `tst_backlight_root()` 原型（否则隐式声明把指针截成 `int`），并以宏 `BACKLIGHT_SYSFS_DIR` 指向它。
  - `capture`：`dup2` 把 stdout/stderr 重定向到临时文件；捕获期间用 `__sanitizer_set_report_fd` 把 sanitizer 报告指回原 stderr，否则崩溃时报告会留在被丢弃的捕获文件里；`CHECK` 的打印在捕获期间同样被捕获，断言须放在 `tst_capture_end()` 之后。
  - `app_env`：定义 `custom/`、`generated/` 以 `extern` 引用、而仅 `src/main.c` 才定义的 5 个全局量；`tst_pointer_set()` 用实际像素，`tst_tap()`、`tst_drag()` 用 480 设计坐标；`tst_run_ms()` 以 5 ms 间隔调用 `lv_timer_handler()`，按真实时间推进；`tst_app_set_present_hook()` 供截图与 SDL 预览取帧；不调用 `custom_init()`（spec §2.5）。
  - `tests/CMakeLists.txt`：被测源码递归 glob 编成静态库 `luckfox_app`，以 `-w` 编译（告警基线由交叉编译记录）；全部目标带 `-fsanitize=address,undefined -fno-sanitize-recover=all`；`tst_support` 做成 OBJECT 库，因为被测源码引用 `support/` 定义的全局量、`support/` 又引用被测源码，两个静态库互相引用会受链接顺序影响；`luckfox_add_test()` 按所在目录名定类别，注册 `<类别>.<对象>.<用例>`，有 `CASES` 时每个用例一个进程；缓存变量 `LUCKFOX_ROOT`（改坏检验用它指向源码副本）与 `LUCKFOX_TESTS_ARM32`。
  - `unit_sdio_detect.c` `#include` 被测的 `custom_main.c` 并导出 `tst_sdio_has_id()`，随被测源码以 `-w` 编译；`test_sdio_detect.c` 在临时目录造 7 种 uevent 布局（absent/empty/func1_only/ultra_w/hidden/no_newline/prefix），仍以 `-Wall -Wextra -Werror` 编译。
  - `tests/README.md`：运行方法、依赖、页面目录与类别表、命名规则、新增测试步骤、注意事项。

- [x] **Step 2: 确认先红**：`main_sdio_detect` 先注册为不带 `UNIT` 的 `luckfox_add_test(sdio_detect CASES …)`。

Run: `cmake -S tests -B build-tests && cmake --build build-tests -j"$(nproc)" 2>&1 | grep -E 'undefined reference|Error' | head`
Expected：`undefined reference to 'tst_sdio_has_id'`，`main_sdio_detect` 链接失败（`support/` 与 `luckfox_app` 均已编过）。

- [x] **Step 3: 加 `UNIT unit_sdio_detect.c` 后确认通过**

Run: `cmake --build build-tests -j"$(nproc)" && ctest --test-dir build-tests --output-on-failure -j"$(nproc)"`
Expected：`100% tests passed, 0 tests failed out of 7`，无 sanitizer 输出；`ls /tmp | grep -c luckfox-tests-` 为 0（临时目录已清理）。

- [x] **Step 4: 写仓库外改坏脚本 `/tmp/luckfox-red/red.sh`**（Task 2–5 共用，不入库）

```bash
#!/usr/bin/env bash
# 在仓库外副本上做一处改坏并确认指定测试失败；用法：red.sh <custom/下的路径> <原文> <替换为> <ctest 正则>
set -euo pipefail
R=/workspace
W=/tmp/luckfox-red
file=$1; old=$2; new=$3; regex=$4
[[ $file == custom/* ]] || { echo "只支持 custom/ 下的文件"; exit 2; }
if [[ ! -d $W/src ]]; then
  mkdir -p "$W/src"
  for d in lib generated include src; do ln -s "$R/$d" "$W/src/$d"; done
fi
rm -rf "$W/src/custom"
cp -R "$R/custom" "$W/src/custom"
python3 - "$W/src/$file" "$old" "$new" <<'PY'
import sys
path, old, new = sys.argv[1], sys.argv[2], sys.argv[3]
text = open(path, encoding="utf-8").read()
if text.count(old) != 1:
    sys.exit("anchor must match exactly once, found %d" % text.count(old))
open(path, "w", encoding="utf-8").write(text.replace(old, new))
PY
cmake -S "$R/tests" -B "$W/build" -DLUCKFOX_ROOT="$W/src" >/dev/null
cmake --build "$W/build" -j"$(nproc)" >/dev/null
if ctest --test-dir "$W/build" -R "$regex" --output-on-failure; then
  echo "RED CHECK: SURVIVED"; exit 1
fi
echo "RED CHECK: KILLED"
```

`cp -R` 不保留时间戳，确保上一次被改坏又还原的文件会重编。

- [x] **Step 5: 确认测试能抓住改坏**（整行匹配改为前缀匹配）

```bash
chmod +x /tmp/luckfox-red/red.sh
/tmp/luckfox-red/red.sh custom/custom_main.c '            if (strcmp(line, uevent_line) == 0) {' '            if (strncmp(line, uevent_line, strlen(uevent_line)) == 0) {' '^main\.sdio_detect\.'
```

Expected：`main.sdio_detect.prefix` 失败（`got 1, want 0`），其余 6 条通过，末行 `RED CHECK: KILLED`。

---

### Task 3: `wifi_backend_release()` 判空回归测试（`test(tests)`）

**Files:** Create `tests/cases/wifi/CMakeLists.txt`、`tests/cases/wifi/test_backend_release.c`；Modify `tests/CMakeLists.txt`（末尾加 `add_subdirectory(cases/wifi)`）。

- [x] **Step 1: 落地**：上列文件以仓库为准，提交 `test(tests): ✅ 纳入 wifi_backend_release() 判空回归测试`。要点：被测 `void wifi_backend_release(void)` 与全局 `lv_timer_t *wifi_update_timer`（`custom/custom_wifi.c`）没有对外头文件，测试手写 `extern` 声明；未创建定时器即调用、创建后连调两次，均须正常返回且 `wifi_update_timer` 为 NULL；两条用例各一个进程（共用 LVGL 静态状态）。

- [x] **Step 2: 确认改坏时失败**（换回 PR #5 之前 `31e3506` 的无判空实现）

```bash
/tmp/luckfox-red/red.sh custom/custom_wifi.c $'    if (wifi_update_timer != NULL) {\n        lv_timer_del(wifi_update_timer);\n        wifi_update_timer = NULL;\n    }\n' $'    lv_timer_del(wifi_update_timer);\n' '^wifi\.backend_release\.'
```

Expected：`without_timer` 以 SEGV 失败（ASan `SEGV on unknown address`，栈含 `_lv_ll_remove`、`lv_timer_del`）；`twice` 以 `heap-use-after-free` 失败；末行 `RED CHECK: KILLED`。

- [x] **Step 3: 确认当前代码通过**

Run: `cmake -S tests -B build-tests && cmake --build build-tests -j"$(nproc)" && ctest --test-dir build-tests --output-on-failure -j"$(nproc)"`
Expected：9 条全过。

---

### Task 4: 亮度断言（`test(tests)`）

**Files:** Create `tests/cases/main/test_brightness.c`；Modify `tests/cases/main/CMakeLists.txt`。

- [x] **Step 1: 落地**：上列文件以仓库为准，提交 `test(tests): ✅ 纳入亮度滑条断言`。要点：11 个用例，每个用例一个进程；写入失败用指向 `/dev/full` 的符号链接、写入次数用 inotify，均不依赖 root 或 strace；断言只看最终写入值与次数上界，不看中间帧。写入次数（`writes_deduplicated`）的 inotify 必须同时监听 `IN_OPEN`：内核会合并队列末尾相邻的相同事件，只监听 `IN_CLOSE_WRITE` 时多次写入合并成 1 次，删掉去重逻辑也不失败。

- [x] **Step 2: 确认改坏时失败**（下限改 0，对应 PR #6 M1）

```bash
/tmp/luckfox-red/red.sh custom/custom_brightness.c '#define BRIGHTNESS_MIN_PCT      10' '#define BRIGHTNESS_MIN_PCT      0' '^main\.brightness\.'
```

Expected：`main.brightness.drag_writes_min_and_max` 失败（`left: got 1, want 26`），末行 `RED CHECK: KILLED`。

- [x] **Step 3: 确认当前代码通过**

Run: `cmake --build build-tests -j"$(nproc)" && ctest --test-dir build-tests --output-on-failure -j"$(nproc)"`
Expected：20 条全过；`main.brightness.writes_deduplicated` 输出 `INFO writes=N`，记录 N（PR #6 为 11）。

- [x] **Step 4: 非 root 重跑**（spec §6 第 2 条）

```bash
id -u nobody >/dev/null && chmod -R a+rwX /workspace/build-tests && su nobody -s /bin/sh -c 'cd /workspace && ctest --test-dir build-tests -j"$(nproc)"'
```

Expected：同样全过（`/dev/full` 与 inotify 不依赖权限）。容器内无 `su` 或 `nobody` 时记「未执行」及原因。

---

### Task 5: 32 位换算测试（`test(tests)`）

**Files:** Create `tests/cases/main/test_brightness_arm32.c`、`tests/cases/main/unit_brightness_arm32.c`；Modify `tests/CMakeLists.txt`、`tests/cases/main/CMakeLists.txt`、`tests/README.md`。

- [x] **Step 1: 工具**：`arm-linux-gnueabihf-gcc` 与 `qemu-user` 均由两份 Dockerfile 提供（Task 12）；其他环境 `sudo apt-get update && sudo apt-get install -y gcc-arm-linux-gnueabihf qemu-user`。

- [x] **Step 2: 落地**：上列文件以仓库为准，提交 `test(tests): ✅ 纳入 32 位亮度换算测试`。要点：
  - unit 把 `BACKLIGHT_SYSFS_DIR` 定义为测试文件里的 `char tst_arm32_root[256]` 后 `#include "custom_brightness.c"`，导出 `tst_bl_open()`（`BACKLIGHT_OK` 为 0）、`tst_bl_read_percent()`、`tst_bl_write_percent()`、`tst_bl_reset()`；测试 `max=21474836` 初值 100、写 100% 得 21474836，`max=255` 初值 80、写 10% 得 26，共 7 项 `CHECK`（含 `sizeof(long) == 4`）；目标板的 `long` 为 32 位、主机为 64 位，换算溢出只能在 32 位下测出。
  - `tests/CMakeLists.txt` 以 `find_program` 探测 `arm-linux-gnueabihf-gcc` 与 `qemu-arm`，得 `LUCKFOX_ARM32_ENABLED`、`LUCKFOX_ARM32_CC`、`LUCKFOX_QEMU_ARM`、`LUCKFOX_ARM32_INCLUDE_FLAGS`；`AUTO` 缺工具跳过、`ON` 缺工具 configure 报错、`OFF` 不注册。
  - `tests/cases/main/CMakeLists.txt` 用 `add_custom_command` 分别以 `-w`（被测）和 `-Wall -Wextra -Werror`（测试）编译，`-ffunction-sections` 加 `-Wl,--gc-sections` 丢弃未引用的 `brightness_ui_create()` 及其 LVGL 依赖，所以不必交叉编译 LVGL；以 `qemu-arm -L /usr/arm-linux-gnueabihf` 运行，注册为 `main.brightness_arm32.conversion`，标签 `main;arm32`。
  - `tests/README.md`「32 位测试」一节：工具来源、三种开关、它不经 `luckfox_add_test()` 而测试名仍按三级规则。

- [x] **Step 3: 确认改坏时失败**（中间值改回 `long` 运算）

```bash
/tmp/luckfox-red/red.sh custom/custom_brightness.c '    pct = (long)(((long long)raw * 100 + backlight_max / 2) / backlight_max);' '    pct = (raw * 100 + backlight_max / 2) / backlight_max;' '^main\.brightness_arm32\.'
```

Expected：`FAIL … tst_bl_read_percent(): got 10, want 100`（与 PR #6 修复前一致），末行 `RED CHECK: KILLED`。

- [x] **Step 4: 确认通过与三种开关**

```bash
cmake -S tests -B build-tests -DLUCKFOX_TESTS_ARM32=ON && cmake --build build-tests -j"$(nproc)" && ctest --test-dir build-tests --output-on-failure -j"$(nproc)"
ctest --test-dir build-tests -N -L arm32 | tail -1
cmake -S tests -B /tmp/arm32-auto -DLUCKFOX_TESTS_ARM32=AUTO -DLUCKFOX_QEMU_ARM=/nonexistent/qemu-arm 2>&1 | grep '跳过 32 位测试'
cmake -S tests -B /tmp/arm32-on -DLUCKFOX_TESTS_ARM32=ON -DLUCKFOX_QEMU_ARM=/nonexistent/qemu-arm 2>&1 | grep -c '未找到'
```

Expected：21 条全过，输出含 `PASS sizeof(long) == 4`；`Total Tests: 1`；`AUTO` 缺 `qemu-arm` 时打印跳过；`ON` 缺 `qemu-arm` 时 configure 报错（计数 ≥1）。

---

### Task 6: 手动目标与改坏检验（`test(tests)`）

**Files:** Create `tests/tools/{screenshots.c,preview.c,mutate.sh}`、`tests/cases/main/{brightness,brightness_arm32,sdio_detect}.mutants`、`tests/cases/wifi/backend_release.mutants`；Modify `tests/CMakeLists.txt`、`tests/README.md`。

- [x] **Step 1: 落地**：上列文件以仓库为准，提交 `test(tests): ✅ 新增截图、SDL 预览与改坏检验手动目标`。要点：
  - `screenshots`：480、720 各输出初始 80%、拖到最左、音乐弹窗遮盖、无背光设备四类截图（PPM，装了 `ffmpeg` 时另存 PNG），假背光建在临时目录。
  - `preview`：SDL2 窗口显示 `tst_app_set_present_hook()` 取得的帧，鼠标即触摸，点 OFF 退出；`PREVIEW_RES` 选分辨率，`PREVIEW_BACKLIGHT=<max>:<brightness>` 造假背光。指针状态每轮事件才更新，自动化点击需按住约 150 ms，否则短点击会丢。
  - 两个目标均不进 `ALL`、不注册 ctest；SDL2 先按 config 模式找，找不到退回 pkg-config（CMake 没有 FindSDL2 模块），都找不到则不生成 `preview`。
  - 改坏清单共 12 个变体：`brightness.mutants` 为亮度 M1–M7、M10、M11（编号沿用 PR #6），`brightness_arm32.mutants` 为 M12（亮度换算中间值改回 `long`），`sdio_detect.mutants` 为 S1（整行匹配改前缀匹配），`backend_release.mutants` 为 W1（换回 PR #5 之前 `31e3506` 的无判空实现）；格式见 `tests/README.md`「改坏检验」。
  - `mutate.sh`（`chmod +x`）把 `custom/` 复制到 `/tmp/luckfox-mutate/src`、其余目录链接回仓库，逐条应用变体并以 `-DLUCKFOX_ROOT` 配置测试工程，不改动工作区；退出码 0 表示全部 `KILLED`、1 表示有 `SURVIVED`、2 表示工具或清单错误（基线不绿、清单解析失败、`old` 不是恰好匹配一次、id 不存在、一个变体都没跑）：清单出错时若仍退出 0，会把「检验没跑」误报成「全部通过」。
  - `tests/README.md` 增「手动目标」「改坏检验」两节。

- [x] **Step 2: 出截图并核对**

```bash
cmake -S tests -B build-tests && cmake --build build-tests --target screenshots -j"$(nproc)" && ls build-tests/screenshots/*.png | wc -l
```

Expected：8 张 PNG（480/720 各 `initial`、`left`、`popup`、`no-device`）；目视与 Project Context `artifacts/screenshots/brightness-*-{480,720}.png` 布局一致（初始 80%、最左 10%、弹窗盖住滑条、无设备时无滑条）。8 张图复制到 Project Context `media/native-tests/screenshots/`。

- [x] **Step 3: 开窗预览**（VM 桌面，tmux 会话 `native-preview`）

```bash
cmake --build build-tests --target preview -j"$(nproc)"
PREVIEW_RES=480 PREVIEW_BACKLIGHT=255:204 DISPLAY=:1 ./build-tests/preview
```

Expected：窗口显示主屏，鼠标拖动滑条百分比变化，点 OFF 约 1 s 后窗口关闭、退出码 0。截一张图存 `media/native-tests/preview-480.png`。

- [x] **Step 4: 跑改坏检验**

Run: `tests/tools/mutate.sh 2>&1 | tee /tmp/mutate.txt`
Expected：12 个变体均 `KILLED`，退出码 0，与 PR #6 的对应关系为 M1、M7 → `drag_writes_min_and_max`；M2 → `writes_deduplicated`；M3 → `popup_blocks_drag`；M4 → `initial_zero_not_written`；M5 → `hidden_zero_max`；M6 → `write_failure_reported_once`；M10 → `small_max_never_writes_zero_1`；M11 → `hidden_oversized_max`；M12 → `main.brightness_arm32.conversion`；S1 → `sdio_detect.prefix`；W1 → `backend_release.*`。`/tmp/mutate.txt` 存 Project Context `internal/native-tests/mutate.txt`。

---

### Task 7: CI job `native-tests`（`ci(github-actions)`）

**Files:** Modify `.github/workflows/build-luckfox-lvgl-demo.yml`（在文件末尾追加 job）。

- [x] **Step 1: 落地**：`native-tests` job 以仓库为准（与 `build-demo` 同级缩进），提交 `ci(github-actions): 👷 新增原生测试 job`。要点：消费 `needs.build-image.outputs.image`，在 CI 镜像容器内以 root 运行，`schedule`（月度重建镜像）时跳过；`qemu-user` 与交叉 gcc 都在镜像中（Task 12），job 不安装软件包；配置时传 `-DLUCKFOX_TESTS_ARM32=ON`，缺工具即报错，不会静默跳过 32 位测试。

- [x] **Step 2: 校验 YAML**

Run: `python3 -c "import yaml,sys; d=yaml.safe_load(open('.github/workflows/build-luckfox-lvgl-demo.yml')); print(list(d['jobs']))"`
Expected：`['build-image', 'build-demo', 'native-tests']`。

- [x] **Step 3: 本机按 CI 步骤重放**（容器内以 root）

Run: `rm -rf build-tests && cmake -S tests -B build-tests -DLUCKFOX_TESTS_ARM32=ON && cmake --build build-tests -j"$(nproc)" && ctest --test-dir build-tests --output-on-failure -j"$(nproc)"`
Expected：21 条全过。

---

### Task 8: `AGENTS.md`（`docs(agents)`）

**Files:** Modify `AGENTS.md` 的 `### Cloud Agent 环境`、`### GitHub Actions`、`### Lint（代码检查）`、`### 测试`、`### 运行 / 验证 GUI（重要陷阱）`。

- [x] **Step 1: 落地**：`AGENTS.md` 以仓库为准，提交 `docs(agents): 📝 更新告警、测试与原生渲染说明`。要点：
  - Cloud Agent 环境：当前活动的 `.cursor/Dockerfile` 安装内容含 `qemu-user`。
  - GitHub Actions：镜像标签取 `.cursor/Dockerfile` 内容哈希，修改该文件的 PR 在合并前 `build-image` 必然失败，合并后 dev push 重建镜像才恢复。
  - Lint：告警段在 `add_executable()` 之前（`add_compile_options()` 只作用于其后创建的 target）、`-std=gnu99`（`-std=c99` 会关掉 POSIX/GNU 扩展声明而编译失败）；glibc 132 条、uClibc 131 条为既存基线，只记录不作门禁；`-fPIC -Wall -O3 -g0` 一行仍在其后、未生效。
  - 测试：替换「本仓库没有自动化测试套件。」——`tests/` 的构建与运行命令、32 位测试需 `qemu-user` 与交叉 gcc 且缺任一自动跳过、两者由两份 Dockerfile 提供、CI 以 `ON` 强制运行、不调用 `custom_init()` 的原因、指向 `tests/README.md`。
  - 运行 / 验证 GUI：uClibc 冒烟所用 `qemu-user` 已在 `.cursor/Dockerfile` 中；以「没有硬件时可以选做 native 渲染验证」开头的整段改为手动目标 `screenshots`、`preview`，5 个全局量由 `tests/support/app_env.c` 提供的原因，权威验证仍是真机。

- [x] **Step 2: 核对限额**

Run: `wc -l -c AGENTS.md`
Expected：不超过 ~150 行、32 KiB（改前 52 行、7634 字节）。

---

### Task 9: 精简 PR #5、#6 的 plan/spec（`docs(superpowers)`）

**Files:** Modify `docs/superpowers/plans/2026-09-25-lvgl-example-luckfox-pico-ultra-w-wifi-detect.md`、`docs/superpowers/plans/2026-09-27-lvgl-example-brightness-slider.md`、`docs/superpowers/specs/2026-09-25-lvgl-example-luckfox-pico-ultra-w-wifi-detect-design.md`、`docs/superpowers/specs/2026-09-27-lvgl-example-brightness-slider-design.md`。

删与留的规则见 spec §5.9；保留部分（含 PR #6 的 uClibc 步骤）逐字不动，替换处不写过程说明。

- [x] **Step 1: 落地**：四份文档以仓库为准，提交 `docs(superpowers): 📝 PR #5、#6 plan 中已入库的测试改为指向 tests/`。要点：
  - PR #5 plan：File Structure 表 `/tmp/sdio-test/`、`/tmp/ll-segv/` 两行改指 `tests/`；Task 2 的 `Test:` 行、SDIO harness 与 `run.sh`、`_lv_ll_remove` 复现程序与其构建块各换为指向 `tests/` 的一句要点；Step 11 的 ASan 构建块换为「ASan/UBSan 由 `tests/` 工程默认开启」。
  - PR #6 plan：File Structure 表 `/tmp/brightness-preview/{…}` 一行改指 `tests/`；Task 1 Step 1–4（harness、`build.sh`、截图与开窗命令）、Task 5 Interfaces 与 Step 2、Step 4 的代码块、Task 13 的 `u32test.c` 与 `build.sh` 各换为指向 `tests/` 的一句要点，Expected 行保留；Task 1 Step 2 注明严格编译关卡不在 `tests/` 中（见下表最后一行）。
  - 两份 spec：`grep -n '不入库\|临时 harness'` 命中的「临时 harness（不入库）」改为「harness（以仓库 `tests/` 为准）」，其余不动。
  - Global Constraints、Architecture 与若干 Step 中引用已删 `/tmp/…` harness、`./build.sh && ./check.sh`、`check.sh` 断言的句子，PR #6 plan 中与新 Interfaces 矛盾的断言说明（「F 只读且以 `nobody` 运行」、依赖 `strace`），PR #5 spec 中指向已删复现程序的句子，一并改为指向仓库 `tests/`。

被删内容与仓库对应物：

| 被删内容 | 仓库里的对应 | 差异 |
|---|---|---|
| PR #5 SDIO harness + `run.sh` | `tests/cases/main/{test,unit}_sdio_detect.c` | 7 个用例名一致；从 `sed` 抽出函数改为 `#include` 整份源文件 |
| PR #5 `_lv_ll_remove(NULL)` 复现程序 | `tests/cases/wifi/test_backend_release.c` + `backend_release.mutants` 的 W1 | 等价的回归测试；W1 换回 PR #5 之前的实现即复现崩溃 |
| PR #6 预览 harness（`--sdl`、`--drag`、`--music`） | `tests/support/app_env.c`、`tests/tools/{screenshots,preview}.c` | `system("echo …")` 打印改为直接断言 |
| PR #6 `check.sh` 11 项断言 | `tests/cases/main/test_brightness.c` 11 个用例 | strace 与 `su nobody` 换成 inotify 与 `/dev/full` |
| PR #6 `mutate.sh` + 9 条命令 | `tests/tools/mutate.sh` + `brightness.mutants` | M1–M7、M10、M11 锚点逐字相同 |
| PR #6 `u32test.c` + `build.sh` | `tests/cases/main/{test,unit}_brightness_arm32.c` | 6 项全在，另加 `sizeof(long) == 4` |
| PR #6 `build.sh` 末行严格编译 | 无 | 以 `-Wall -Wextra -Werror` 单独编译 `custom_brightness.c` 的关卡未迁移；主程序告警已生效，交叉编译时该文件 0 条告警，但不再有 `-Werror` 卡住；PR #6 plan Task 1 Step 2 已注明，列入后续改进 |

- [x] **Step 2: 核对**

```bash
git diff --stat
git diff -U0 docs/superpowers | grep '^-' | grep -v '^---' | grep -E '^\-\|' | head   # 证据表行不应出现在删除中（File Structure 表除外）
grep -c '```c' docs/superpowers/plans/2026-09-25-lvgl-example-luckfox-pico-ultra-w-wifi-detect.md docs/superpowers/plans/2026-09-27-lvgl-example-brightness-slider.md
```

Expected：删除行中的表格行只来自 File Structure 表；PR #5 plan 剩 1 个 C 代码块（双重 `fclose` 演示），PR #6 plan 剩 0 个。

---

### Task 10: 推送、PR 与 CI

- [x] **Step 1: 推送**：草稿 PR #7 在实现前已创建；每个 Task 提交后推送到 `cursor/native-tests-cbb9`，推送触发 `pull_request` run。
- [x] **Step 2: PR 的 `pull_request` run**（改 Dockerfile 前）：`build-image`（dest-lock 复用 dev 镜像）、glibc、uClibc、`native-tests` 四个 job 均 success；`native-tests` 日志中 `100% tests passed` 且含 32 位测试。LeakSanitizer 在容器内无 ptrace 相关致命错误，不需要 `ASAN_OPTIONS=detect_leaks=0`（spec §7）。Task 12 之后的 PR run 中 `build-image` 预期失败（spec D8）。
- [x] **Step 3: run URL、各 job 结果与耗时**见「验证证据」。

### Task 11: 回填证据与整理提交

- [x] **Step 1: 回填「验证证据」与「与计划的偏离及原因」**，已入库的完整代码正文改为指向仓库文件的一句话，各 Task 已勾选。
- [x] **Step 2: 在最终分支上重做 Task 1 Step 3 的对比**，结果与 spec §5.1 一致，见「验证证据」。
- [x] **Step 3: 重排提交**：spec 与本 plan 的全部 `docs(superpowers)` 提交合并为 `docs(superpowers): 📝 新增原生测试基础设施 spec 与 plan` 并位于最后，旧文档精简的提交独立位于其前；重排前后 `git diff` 为空。

### Task 12: 提交格式、`qemu-user` 入 Dockerfile 与 32 位测试命名

- [x] **Step 1: 落地**（各改动以 fixup 归入所属提交，提交序见 spec §8）：

| 文件 | 改动 | 归入提交 |
|---|---|---|
| `.cursor/Dockerfile`、`.cursor/Dockerfile.luckfox_pico` | 补装 `qemu-user` 并注释用途；`.cursor/Dockerfile` 另注明改该文件后 PR CI 失败的原因（备选文件不参与 CI，不写） | `chore(cloud-env): 🐳 两份 Dockerfile 补装 qemu-user`（新增，紧随 `build(cmake)`、位于全部测试提交之前：32 位测试依赖 `qemu-user`，且各测试提交中的 README 说法须在该提交处已成立） |
| `tests/cases/main/CMakeLists.txt`、`tests/README.md` | 32 位测试注册为 `main.brightness_arm32.conversion`；README 写明工具在两份 Dockerfile 中、测试名仍按三级规则 | `test(tests): ✅ 纳入 32 位亮度换算测试` |
| `tests/cases/main/brightness.mutants`、`tests/cases/main/brightness_arm32.mutants` | M12 移入后者（`tests: ^main\.brightness_arm32\.`），前者只含 PR #6 的 9 个变体 | `test(tests): ✅ 新增截图、SDL 预览与改坏检验手动目标` |
| `.github/workflows/build-luckfox-lvgl-demo.yml` | 删去 `native-tests` job 的 `apt-get install qemu-user` 步骤，仍以 `-DLUCKFOX_TESTS_ARM32=ON` 运行 | `ci(github-actions): 👷 新增原生测试 job` |
| `AGENTS.md` | `qemu-user` 在两份 Dockerfile 中；修改 Dockerfile 的 PR 合并前 CI 必然失败 | `docs(agents): 📝 更新告警、测试与原生渲染说明` |
| `docs/superpowers/plans/2026-09-27-lvgl-example-brightness-slider.md` | Task 1 Step 2 注明严格编译不在 `tests/` 中、无 `-Werror` 门禁 | `docs(superpowers): 📝 PR #5、#6 plan 中已入库的测试改为指向 tests/` |

全部提交标题统一为 `type(scope): emoji 主题`，测试类提交用 `test(tests)`，CI 提交用 `ci(github-actions)`。

- [x] **Step 2: 本地验证**

```bash
cmake -S tests -B /tmp/v2 -DLUCKFOX_TESTS_ARM32=ON && cmake --build /tmp/v2 -j"$(nproc)" && ctest --test-dir /tmp/v2 -j"$(nproc)"
ctest --test-dir /tmp/v2 -N -L arm32
MUTATE_WORK=/tmp/mw-v2 tests/tools/mutate.sh M12 M1
```

Expected：21/21 通过；`-L arm32` 只列出 `main.brightness_arm32.conversion`；`[M1] KILLED by: main.brightness.drag_writes_min_and_max`、`[M12] KILLED by: main.brightness_arm32.conversion`，末行 `2 run, 0 skipped`。

- [x] **Step 3: 重排核对**：`git diff <重排前 HEAD> HEAD` 为空。

---

### Task 13: `mutate.sh` 工作目录保护（并入 `test(tests): ✅ 新增截图、SDL 预览与改坏检验手动目标`）

- [x] **Step 1: 落地**：以仓库 `tests/tools/mutate.sh` 与 `tests/README.md`「改坏检验」节为准——脚本启动时会 `rm -rf "$MUTATE_WORK/src"`，所以先把 `MUTATE_WORK` 规范化（`realpath -m`，解析 `..` 与符号链接）并此后只用规范化路径，拒绝 `/`、仓库内的路径与仓库位于 `$MUTATE_WORK/src` 之内的目录，只使用不存在、为空或带标记文件 `.luckfox-mutate-workdir`（首次使用时创建，须为普通文件）的目录；`custom/` 含符号链接时拒绝（变体经副本写回会改到仓库）。均以退出码 2 结束。变体 `id` 限定为 `[A-Za-z0-9][A-Za-z0-9_.-]*`，避免日志路径逃出工作目录。
- [x] **Step 2: 验证**

```bash
mkdir -p /tmp/mt-guard/occupied/src && echo keep > /tmp/mt-guard/occupied/src/f
mkdir -p /tmp/mt-guard/marker-dir/.luckfox-mutate-workdir /tmp/mt-guard/marker-dir/src && touch /tmp/mt-guard/marker-dir/src/f
for w in /workspace /workspace/tests/x / /tmp/mt-guard/occupied /tmp/mt-guard/marker-dir; do MUTATE_WORK=$w tests/tools/mutate.sh M1; echo "exit=$?"; done
MUTATE_WORK=/workspace/new/../../tmp/mt-guard/dd tests/tools/mutate.sh __x; ls -d /workspace/new
mkdir -p /tmp/mt-guard/aw/src && git clone -q /workspace /tmp/mt-guard/aw/src/repo && cp tests/tools/mutate.sh /tmp/mt-guard/aw/src/repo/tests/tools/ && touch /tmp/mt-guard/aw/.luckfox-mutate-workdir
MUTATE_WORK=/tmp/mt-guard/aw /tmp/mt-guard/aw/src/repo/tests/tools/mutate.sh M1; echo "exit=$?"; ls -d /tmp/mt-guard/aw/src/repo
rm -rf /tmp/mt-ok && MUTATE_WORK=/tmp/mt-ok tests/tools/mutate.sh M1 S1 && MUTATE_WORK=/tmp/mt-ok tests/tools/mutate.sh W1
ln -s /etc/hostname custom/zz_link; MUTATE_WORK=/tmp/mt-ok tests/tools/mutate.sh M1; rm custom/zz_link
```

Expected：for 循环的五个均 `exit=2` 且不删除任何文件（`/tmp/mt-guard/occupied/src/f`、`/tmp/mt-guard/marker-dir/src/f` 仍在、`/workspace/src` 完整，标记是目录不算数）；含 `..` 的路径先被规范化，`/workspace/new` 不会被创建；仓库位于 `$MUTATE_WORK/src` 之内时打印 `MUTATE_WORK/src must not contain the repository` 并退出 2，仓库副本仍在；新目录 `M1`、`S1` 均 `KILLED`、`2 run, 0 skipped`、退出 0 并生成 `.luckfox-mutate-workdir`；复用该目录 `W1` `KILLED`；含符号链接时打印 `custom/ must not contain symlinks` 并退出 2。

## Execution Handoff

全部 Task 已完成。

## 验证证据

判据见 spec §6。环境为 Cloud Agent 容器：Ubuntu 24.04、gcc 13.3.0、`arm-linux-gnueabihf-gcc` 13.3.0、CMake 3.28.3、`qemu-arm` 8.2.2。原始输出存 Project Context `internal/native-tests/final-verification/`（本表命令均可在仓库根目录复现）。

| 项 | 判据 | 实测 |
|---|---|---|
| §6.1 告警提交对比 | Task 1 Step 1/3 的脚本在最终分支上重做：glibc、uClibc 均 0 error，告警数同 spec §5.1，产物差异只在 `.strtab` | 告警 glibc 4 → 132、uClibc 2 → 131；error 均为 0；主程序 `C_FLAGS` 含 `-std=gnu99` 计数 1，LVGL `C_FLAGS =` 为空；glibc `GLIBC_IDENTICAL`（3 171 276 字节）；uClibc `cmp -l` 23 字节，偏移 3 307 054–3 310 311（`cmp -l` 从 1 起计）全在 `.strtab`（3 306 224–3 357 349）；`.text`、`.rodata`、`.data`、`.bss`、`.init_array` 哈希相同，`SECTION_HEADERS_IDENTICAL`，符号表差异是局部静态符号的编号后缀（共 13 对，如 `start_ms.17729` → `.17724`、`last_x.17699` → `.17694`、`last_y.17700` → `.17695`）；uClibc 工具链 `yuangezhizao/luckfox-pico@dev` sha `d4cd3f7523440fad218388e7602ed63d45d58c81`（gcc 8.3.0） |
| §6.2 原生测试（root） | `tests/README.md` 命令构建运行，21 条全过，无 ASan/UBSan/LSan 报告 | `rm -rf build-tests && cmake -S tests -B build-tests && cmake --build build-tests -j8 && ctest --test-dir build-tests --output-on-failure -j8`：`100% tests passed, 0 tests failed out of 21`，Total Test time 4.56 s；输出中 `sanitizer`、`runtime error` 命中 0 次；运行结束 `/tmp/luckfox-tests-*` 残留 0 个 |
| §6.2 非 root | 以非 root 重跑，结果相同 | `chmod -R a+rwX build-tests && su nobody -s /bin/sh -c 'cd /workspace && ctest --test-dir build-tests -j8'`（uid 65534）：21/21 全过，3.77 s，无 sanitizer 输出；之后 `chown -R root:root` 并 `chmod -R go-w` 恢复 |
| §6.3 测试有效性（改坏检验） | `tests/tools/mutate.sh` 12 个变体均 `KILLED`，退出 0 | `tests/tools/mutate.sh`：`12 run, 0 skipped`，退出码 0，约 51 s。M1、M7 → `drag_writes_min_and_max`；M2 → `writes_deduplicated`；M3 → `popup_blocks_drag`；M4 → `initial_zero_not_written`；M5 → `hidden_zero_max`；M6 → `write_failure_reported_once`；M10 → `small_max_never_writes_zero_1`；M11 → `hidden_oversized_max`；M12 → `main.brightness_arm32.conversion`；S1 → `main.sdio_detect.prefix`；W1 → `wifi.backend_release.without_timer` 与 `twice` |
| §6.4 32 位 | `ON` 注册并通过；`AUTO` 缺工具时跳过其余照常；`ON` 缺工具 configure 报错 | `-DLUCKFOX_TESTS_ARM32=ON`：`Total Tests: 21`，`ctest -R arm32` 1/1 通过（0.02 s），`PASS sizeof(long) == 4` 等 7 项；`-DLUCKFOX_QEMU_ARM=/nonexistent/qemu-arm`（`AUTO`）：打印「未找到 arm-linux-gnueabihf-gcc 或 qemu-arm，跳过 32 位测试」，`Total Tests: 20`，20/20 通过；`ON` 加同一路径：configure 报错退出 1；`OFF`：`Total Tests: 20`。缺工具必须用不存在的路径，对 `-NOTFOUND` 取值 `find_program` 会重新搜索而找到工具 |
| §6.5 手动目标 | `screenshots` 产出 8 张图；`preview` 在 `DISPLAY=:1` 开窗、可拖动滑条、点 OFF 退出码 0 | `cmake --build build-tests --target screenshots`：480/720 各 `initial`、`left`、`popup`、`no-device` 共 8 张 PNG，已覆盖到 Project Context `media/native-tests/screenshots/`，布局与 PR #6 的 `artifacts/screenshots/brightness-*` 一致，`480-initial` 初始 80%；`PREVIEW_RES=480 PREVIEW_BACKLIGHT=255:204 DISPLAY=:1 ./build-tests/preview` 开出 480×480 窗口，xdotool 拖动滑条后显示 55%，按住 150 ms 点 OFF，约 3 s 内进程退出，退出码 0；截图 `media/native-tests/preview-480.png` |
| §6.6 CI | 改 Dockerfile 前最后一次 PR run 四个 job 均 success；本 PR 合并前 `build-image` 预期失败；合并后首次 dev push run 四个 job 均 success | 改 Dockerfile 前最后一次 PR run：[run 36837903711](https://github.com/yuangezhizao/luckfox_pico_lvgl_example/actions/runs/36837903711)（`eaa9b77`）🐳 构建 CI 镜像 10 s、🧪 原生测试 53 s、🛠️ glibc 36 s、🛠️ uClibc 34 s 全部 success；`native-tests` 日志 `100% tests passed, 0 tests failed out of 21`，Total Test time 7.13 s，含 32 位测试，该 run 的 job 内安装 `qemu-user` 8.2.2 成功。`eaa9b77` 与最终分支的差异除本 spec 与 plan 外，只有 Task 12 所列文件与 Task 13 的 `mutate.sh` 工作目录保护，已分别本地验证。[run 36832901260](https://github.com/yuangezhizao/luckfox_pico_lvgl_example/actions/runs/36832901260)（`faed23d`）同样全绿，LeakSanitizer 在容器内正常，无 ptrace 相关致命错误。本 PR 合并前 `build-image` 预期失败（spec D8）；合并后 dev push run 待观察 |
| §6.7 文档 | `AGENTS.md` 在 32 KiB 内；旧文档精简的 diff 只含删除的代码块与指针句，证据表逐字不变 | `wc -l -c AGENTS.md`：52 行、8 293 字节；`git diff origin/dev HEAD -U0 -- docs/superpowers/plans/2026-09-2[57]-*.md`：删除行 543 条，其中以 `\|` 开头的 3 行均为 File Structure 表行，两份旧 plan 的「验证证据」节与 `origin/dev` 逐字相同（PR #5 28 行、PR #6 19 行）；PR #5 plan 剩 1 个 C 代码块（双重 `fclose` 演示），PR #6 plan 剩 0 个 |
| 亮度写入次数 | `writes_deduplicated` 的 G 用例写入次数不超过 15 | `ctest -R writes_deduplicated -V`：`INFO writes=11`（与 PR #6 一致）；删掉去重逻辑为 37 次，测试失败 |
| Task 12 本地验证 | 32 位测试改名与 M12 拆分后全绿，M12 由新名字抓住 | `cmake -S tests -B /tmp/v2 -DLUCKFOX_TESTS_ARM32=ON` 构建后 ctest 21/21 通过；`ctest -N -L arm32` 只有 `main.brightness_arm32.conversion`；`MUTATE_WORK=/tmp/mw-v2 tests/tools/mutate.sh M12 M1`：`[M1] KILLED by: main.brightness.drag_writes_min_and_max`、`[M12] KILLED by: main.brightness_arm32.conversion`、`2 run, 0 skipped`；重排前后 `git diff` 为空 |
| Task 13 `mutate.sh` 工作目录保护 | 危险路径与非本脚本目录被拒绝、不删除文件；正常目录照常运行 | `MUTATE_WORK` 为 `/workspace`、`/workspace/tests/x`、`/`、含 `src/` 的非空目录、标记为目录的非空目录时均退出 2，提示原因，`/workspace/src`、`/tmp/mt-guard/occupied/src/f`、`/tmp/mt-guard/marker-dir/src/f` 完好；`/workspace/new/../../tmp/mt-guard/dd` 不再在仓库内留下 `new/`；以 `-` 开头的相对路径照常使用；仓库位于 `$MUTATE_WORK/src` 之内（含仓库本身就是 `$MUTATE_WORK/src`）时退出 2、仓库副本完好；清单里 `id: ../../esc` 被拒（退出 2）；新目录 `/tmp/mt-ok` 跑 `M1 S1` → 2 个 `KILLED`、`2 run, 0 skipped`、退出 0，生成 `.luckfox-mutate-workdir`；复用跑 `W1` → `KILLED`；`custom/` 含符号链接时退出 2；全量 `12 run, 0 skipped`、退出 0 |

## 与计划的偏离及原因

- Task 4 G 用例（`writes_deduplicated`）的 inotify 监听掩码为 `IN_OPEN | IN_CLOSE_WRITE`、计数只算 `IN_CLOSE_WRITE`：内核会合并队列末尾相邻的相同事件，只监听 `IN_CLOSE_WRITE` 时拖动结束后才读取恒得 1 次，删掉去重逻辑也不失败；当前代码 11 次，删掉去重 37 次、测试失败。
- Task 1 对比脚本把 `cmp -l` 放进 `{ …; || true; }`、`grep -c ' error:'` 后加 `|| true`：`cmp` 有差异时返回 1、`grep -c` 零匹配也返回 1，在 `set -euo pipefail` 下会让后续的段哈希、节头与符号表对比不再执行。最终分支已含改动，「改动前」取 `git archive origin/dev`。
- spec §5.1 的告警类别分布以实测为准：glibc 132 条中没有 `-Wunused-result`，该类别只出现在连 `-O3` 行一起上移的 150 条变体里（多出的 18 条依赖优化才能检测：`-Wunused-result` 13、`-Wformat-overflow` 4、`-Wnonnull` 1），本 PR 不启用 `-O3`，所以不在基线内。
- Task 6 的 `mutate.sh` 带退出码与清单校验：清单解析失败、id 写错、记录粘连（漏写记录间空行）时若仍退出 0，会把「检验没跑」误报成「全部通过」。退出码 0 表示全部 `KILLED`、1 表示有 `SURVIVED`、2 表示工具或清单错误，并校验 `file` 须在 `custom/` 下且不含 `..`、`old` 恰好匹配一次、重复 id 与记录内重复键、传入的 id 存在、至少跑了一个变体，且处理 `SIGPIPE`。
- Task 9 的精简范围包括旧 plan 的 Global Constraints、Architecture 与若干 Step 中对已删 `/tmp/…` harness、`./build.sh && ./check.sh` 和 `check.sh` 断言的引用，PR #6 plan 中与新 Interfaces 矛盾的断言说明（「F 只读且以 `nobody` 运行」、依赖 `strace`），以及 PR #5 spec 中指向已删复现程序的句子：不改则读者会按旧引用去找不存在的程序。验证证据表按规则逐字保留，其中仍出现的已删程序名（如 `u32test`）是历史输出。
- spec 与仓库实现对齐：capture 接口（`tst_capture_begin/end`、`tst_count_lines`）、fake_fs 注释、`mutate.sh` 的行为与退出码、`AGENTS.md` 篇幅限额的表述、32 位测试的 7 项 `CHECK` 以实现为准，保留两份描述会相互分叉；篇幅限额不写具体字节数，只写「远低于 32 KiB 限额」。
- `AGENTS.md` 与 `tests/README.md` 写明与实现一致的条件：32 位测试在 `qemu-user` 与交叉 gcc 缺任一时跳过；`tests/tools/mutate.sh` 依赖 `python3`；并发运行改坏检验须用不同的 `MUTATE_WORK`；测试工程对源码目录递归 glob、libdrm 与 cjson 头文件使用 Debian/Ubuntu 系统路径。不写则读者在新增子目录、并发跑改坏检验或换发行版时会踩坑。
- `qemu-user` 装在两份 Dockerfile 中而非 `native-tests` job 内临时安装：合并后 CI 与 Cloud Agent 环境都不再依赖临时安装；代价是本 PR 合并前 PR CI 的 `build-image` 必然失败，取舍与被否决的替代见 spec D8。
- 32 位测试名为 `main.brightness_arm32.conversion`、M12 单独放在 `brightness_arm32.mutants`：使交叉编译的测试也符合 spec §5.2 的 `<类别>.<对象>.<用例>` 命名与按对象拆分的改坏清单规则。
- 提交标题带 scope（`test(tests)`、`ci(github-actions)`），Dockerfile 改动为独立的 `chore(cloud-env)` 提交：沿用仓库惯例（PR #4 的 `ci(github-actions)`、PR #1–#3 的 `chore(cloud-env)`）。
- `tests/tools/mutate.sh` 校验工作目录：脚本启动时会 `rm -rf "$MUTATE_WORK/src"`，`MUTATE_WORK` 指向仓库或含 `src/` 的已有目录时会删掉其中的源码；现拒绝 `/` 与仓库内路径，只使用不存在、为空或带标记文件的目录，并拒绝 `custom/` 中的符号链接（Task 13）。

以上偏离均已在本 PR 内处理；其中第一条（亮度写入次数测试）已修复并经改坏检验确认：删掉去重逻辑（M2）时该测试失败。

## 遗留问题（后续改进）

以下问题不影响当前环境与 CI 的结论，修复会扩大本 PR 范围，留给后续 PR：

| 遗留问题 | 影响 |
|---|---|
| 32 位测试的自定义命令缺 `.h` 依赖（只声明了 `check.h` 与 `custom_brightness.c`） | 只改头文件时增量构建不会重编 32 位测试；改坏变体若改的是头文件会误报 `SURVIVED`，目前的变体都只改 `.c` |
| `qemu-arm -L /usr/arm-linux-gnueabihf` 的 sysroot 写死 | 换非 Ubuntu 布局的交叉工具链时需改；两份 Dockerfile 中都成立 |
| libdrm 与 cjson 的 include 路径写死为 Debian/Ubuntu 布局（`/usr/include/libdrm`、`/usr/include/cjson`） | 同上 |
| `LUCKFOX_TESTS_ARM32` 非法取值静默当作 `AUTO` | 拼错时不报错 |
| PR #6 的严格编译关卡（`-Wall -Wextra -Werror` 单独编译 `custom_brightness.c`）未迁入 `tests/` | `custom_brightness.c` 目前交叉编译 0 条告警，但没有 `-Werror` 门禁 |
| 被 ASan 终止的测试进程不跑 `atexit` | 改坏检验后 `/tmp/luckfox-tests-*` 会残留 |
