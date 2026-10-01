# Luckfox Pico LVGL Example — 主屏亮度滑条实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在主屏时钟下方加一条 10%–100% 的亮度滑条，拖动时经 sysfs 实时调节 Luckfox Pico Ultra / Luckfox Pico Ultra W 的 `pwm-backlight`。

**Architecture:** 新增 `custom/custom_brightness.c/.h`（手写 LVGL，stdio 读写 `/sys/class/backlight/<首项>/{brightness,max_brightness}`），由 `custom/custom_main.c` 的 `main_app_init()` 挂载，`generated/` 不改。布局与交互用原生 harness（x86 LVGL + 假 sysfs，以仓库 `tests/` 为准）加自动断言与故意改坏检验验证，交叉编译由本机 glibc/uClibc 与 PR CI 验证，亮度效果由作者真机验证。

**Tech Stack:** C（GUI Guider 导出代码 + LVGL 8.3）、CMake、`arm-linux-gnueabihf-`（glibc）、`arm-rockchip830-linux-uclibcgnueabihf-`（uClibc，稀疏检出）、host `gcc` + SDL2 + ffmpeg（harness、截图与断言）、`qemu-user`（uClibc 冒烟）。

**Spec:** [`docs/superpowers/specs/2026-09-27-lvgl-example-brightness-slider-design.md`](../specs/2026-09-27-lvgl-example-brightness-slider-design.md)

## Global Constraints

- 滑条范围 10–100（百分比），永不向 `brightness` 写 0；10% 由本板真机最低可见 raw 25 确定（spec D2）。
- 换算：`pct = (raw × 100 + max / 2) / max`（`raw` 先夹到 [0, max]），`raw = (pct × max + 50) / 100` 且不小于 1，中间值以 `long long` 计算；`max_brightness` 有效范围 1 到 `INT_MAX / 100`（spec §5.2）。
- 背光目录宏 `BACKLIGHT_SYSFS_DIR`，默认 `"/sys/class/backlight"`（spec FR6）。
- 新文件不带 NXP 版权头；`generated/`、`CMakeLists.txt`、`src/main.c`、`lv_conf.h`、`custom/custom_musicplayer.c` 不改（spec NFR1）。
- 新代码不引入 `strcpy`/`strcat`/`sprintf`/`gets`；sysfs 用 stdio，不含 `<fcntl.h>`（spec NFR2）。
- glibc 与 uClibc 两条 CI 行均须编译通过（spec NFR3）。
- 提交序：`feat(custom)` → `docs(agents)` → `docs(superpowers)`（spec Q8）。

## File Structure

| 文件 | 职责 | Task |
|---|---|---|
| `custom/custom_brightness.h` | 只声明 `void brightness_ui_create(lv_obj_t *parent);` | 2 |
| `custom/custom_brightness.c` | 背光后端（查找、校验、读写、换算、日志）、三个控件、事件回调、层级下沉 | 2、6、7 |
| `custom/custom_main.c` | `#include "custom_brightness.h"`；`main_app_init()` 挂载 | 3 |
| `AGENTS.md` | uClibc 构建前提与 `qemu-user` 冒烟 | 8 |
| `tests/`（以仓库为准） | 预览与截图（`tests/tools/{preview,screenshots}.c`、`tests/support/app_env.c`）、亮度断言（`tests/cases/main/test_brightness.c`）、改坏检验（`tests/tools/mutate.sh`、`tests/cases/main/brightness.mutants`） | 1、5 |

---

### Task 1: 原生预览 harness（以仓库 `tests/` 为准，已完成）

**Files:** Create：`tests/support/app_env.c`、`tests/tools/screenshots.c`、`tests/tools/preview.c`、`tests/CMakeLists.txt`。

**Interfaces:** 消费 `setup_ui()`、`events_init()`（`generated/`）与 `main.c` 才定义的 5 个全局量（`tests/support/app_env.c` 自行定义；不调用 `custom_init()`，见 spec §2.4）；产出 `screenshots` 与 `preview` 两个目标。`PREVIEW_RES=480|720` 选分辨率，`PREVIEW_BACKLIGHT=<max>:<brightness>` 设假背光，`preview` 开窗时鼠标即触摸。

- [x] **Step 1: 写 harness**：以仓库 `tests/support/app_env.c`、`tests/tools/screenshots.c`、`tests/tools/preview.c` 为准——内存帧缓冲显示、脚本化触摸，headless 出 PPM 或 SDL 开窗。
- [x] **Step 2: 写构建脚本**：以 `tests/CMakeLists.txt` 为准（被测源码以 `-w` 编译）。严格编译（以 `-Wall -Wextra -Werror` 单独编译 `custom_brightness.c`，即验证证据中的「严格编译新模块」）不在 `tests/` 中：该文件由主程序的告警构建覆盖（交叉编译 0 条告警），但没有 `-Werror` 门禁。
- [x] **Step 3: 出截图（480 与 720 各一轮）**

截图以 `cmake --build build-tests --target screenshots` 为准。

Expected：`build-tests/screenshots/` 下 480、720 各有初始 80%、最左 10%、弹窗遮盖、无设备隐藏四类截图。

- [x] **Step 4: 开窗供试用**（在 VM 桌面 `DISPLAY=:1`，tmux 会话 `brightness-preview`）

开窗以 `preview` 目标为准（`PREVIEW_RES=480 PREVIEW_BACKLIGHT=255:204 DISPLAY=:1 ./build-tests/preview`）。

---

### Task 2: 亮度模块（`feat(custom)`，已完成）

**Files:** Create `custom/custom_brightness.h`、`custom/custom_brightness.c`（以仓库为准）。

**Interfaces:** 产出 `void brightness_ui_create(lv_obj_t *parent);`。内部 `static` 状态：`backlight_dir[256]`、`backlight_max`、`backlight_last_written`（初值 −1，读到当前值后设为该值）、`backlight_write_error_reported`。

- [x] **Step 1:** 按 spec §5.1–§5.4 实现后端与控件；`custom.h` 已引入 `<linux/fcntl.h>`，sysfs 读写用 `fopen`/`fgets`/`fprintf`，写入以 `fprintf` 与 `fclose` 均成功判定。
- [x] **Step 2:** 控件创建后依次 `lv_obj_move_background(slider/value_label/title_label)`，使先创建的 `Main_win_music` 盖在其上。
- [x] **Step 3:** `-Wall -Wextra -Werror` 单独编译 `custom_brightness.c` 通过。

---

### Task 3: 在 `main_app_init()` 挂载（`feat(custom)`，已完成）

**Files:** Modify `custom/custom_main.c`：include 区加 `#include "custom_brightness.h"`，`main_app_init()` 末尾加 `brightness_ui_create(guider_ui.Main);`。

**Interfaces:** 消费 Task 2 的 `brightness_ui_create()`；`main_app_init()` 由 `setup_scr_Main()` 在全部控件、`lv_obj_update_layout()`、`events_init_Main()` 之后调用。

- [x] **Step 1:** harness 走真实挂载路径（`setup_ui()` → `setup_scr_Main()` → `main_app_init()`），背光缺失日志每次仅一行（控件只创建一次）。
- [x] **Step 2:** glibc 交叉编译后 `arm-linux-gnueabihf-objdump -d build/luckfox_lvgl_demo | grep -A40 "<main_app_init>:" | grep -c brightness_ui_create` 输出 `1`。

---

### Task 4: 本机交叉编译与 uClibc 冒烟（已完成）

- [x] **Step 1: glibc**

```bash
cd /workspace && env -u LUCKFOX_SDK_PATH GLIBC_COMPILER=/usr/bin/arm-linux-gnueabihf- sh -c 'mkdir -p build && cd build && cmake .. && make -j"$(nproc)"'
```

Expected：ELF32 ARM；`custom/` 的告警仅既存 3 条（`custom_main.c` `open` 隐式声明、`custom_main.c` `-Wformat-truncation`、`custom_musicplayer.c` `-Wformat-overflow`）。

- [x] **Step 2: uClibc 与 qemu 冒烟**

```bash
git clone -q --depth 1 --filter=blob:none --sparse -b dev https://github.com/yuangezhizao/luckfox-pico.git /tmp/luckfox-pico && git -C /tmp/luckfox-pico sparse-checkout set tools/linux/toolchain
env -u GLIBC_COMPILER LUCKFOX_SDK_PATH=/tmp/luckfox-pico sh -c 'cmake -S /workspace -B /tmp/build-uclibc && make -C /tmp/build-uclibc -j"$(nproc)"'
readelf -h /tmp/build-uclibc/luckfox_lvgl_demo | grep -E "Class|Machine"; readelf -d /tmp/build-uclibc/luckfox_lvgl_demo | grep NEEDED; readelf -l /tmp/build-uclibc/luckfox_lvgl_demo | grep interpreter
sudo apt-get update -qq && sudo apt-get install -y -qq qemu-user
SYSROOT=$(/tmp/luckfox-pico/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/bin/arm-rockchip830-linux-uclibcgnueabihf-gcc -print-sysroot)
cd /tmp/build-uclibc && qemu-arm -L "$SYSROOT" -E LD_LIBRARY_PATH=/workspace/lib/uclibc/libdrm:/workspace/lib/uclibc/libcjson ./luckfox_lvgl_demo; echo "exit=$?"
```

Expected：ELF32 ARM，`NEEDED` 含 `libc.so.0`，解释器 `/lib/ld-uClibc.so.0`；qemu 输出含 `cannot open /dev/dri/card0`（`perror` 自带换行，其后还有一行 `: No such file or directory`），另有 `mpv: not found` 与 `Cannot connect to socket`，退出码 1。构建目录放在 `/workspace` 外（`.gitignore` 只忽略 `build/` 与 `install/`）。

- [x] **Step 3: 核实固件自带 `mpv`**（spec §2.4；`gh` 只读即可下载 artifact）

```bash
mkdir -p /tmp/fw && gh run download 35115207257 -R yuangezhizao/luckfox-pico -n luckfox-pico-firmware_ultra-emmc_on-ubuntu-24.04 -D /tmp/fw
cd /tmp/fw/output/image && grep rootfs.img SHA256SUMS | sha256sum -c -
for p in /usr/bin/mpv /usr/bin/opkg /usr/bin/apt; do printf '%s: ' $p; debugfs -R "stat $p" rootfs.img 2>&1 | grep -E "Type:|not found" | head -1; done
debugfs -R "ls -l /usr/lib" rootfs.img 2>/dev/null | grep -iE "mpv|avcodec|asound"
```

Expected：`rootfs.img: OK`；`/usr/bin/mpv` 为 `Type: regular`，`/usr/bin/opkg`、`/usr/bin/apt` 为 `File not found`；`/usr/lib` 下有 `libmpv.so.1.109.0`、`libavcodec.so.58.134.100`、`libasound.so.2.0.0`。缺 `debugfs` 时 `sudo apt-get install -y e2fsprogs`。

---

### Task 5: 独立两阶段评审与故意改坏检验（已完成）

**Files:** 无仓库改动（断言与改坏检验以仓库 `tests/` 为准）。

**Interfaces:** 11 项断言以仓库 `tests/cases/main/test_brightness.c` 为准（C 内断言，写入失败用指向 `/dev/full` 的符号链接、写入次数用 inotify，不依赖 strace/root）。

- [x] **Step 1: 独立评审**：按 superpowers:requesting-code-review 派全新子代理（不继承会话），输入 spec、plan 与分支 diff，分两阶段（逐条对照 spec、代码质量）评审，报告写 Project Context `internal/brightness-slider-code-review.md`。结论「修复后可合并」：Critical 0、Important 1（I-1，Task 6 修复）、Minor 7（M-1、M-3、M-4 由 Task 7 修复；M-2、M-5、M-6 改 spec；M-7 随时钟缺陷移出本 PR 不再适用）。
- [x] **Step 2: 自动断言**。断言：A 拖到最左/最右写入 26/255 并打印 `brightness: using $BL (max 255)`；H9/H1 `max_brightness` 为 9 与 1 时拖到最左写入 1；B 初值 0 启动后仍为 0（创建时不写）；M 弹窗上拖动后仍 204；C/D/E/N 无目录、`abc/100`、`0/100`、`30000000/100` 各恰好一行 `control hidden`，原因分别为 `no backlight device under` 与 `invalid max_brightness in`；F 的 `brightness` 指向 `/dev/full`，恰好一行 `write backlight failed`、值仍 204；G `10/10` 下以 `O_WRONLY` 打开 `brightness` 不超过 15 次（基线 11 次，拖动过程换算值最多约 14 次变化）且结束为 10；全部 `exit=0`。
- [x] **Step 3: 基线**：运行全部断言，11 项全部 `PASS`，`INFO G writes=11`。
- [x] **Step 4: 改坏变体**：`mutate.sh` 只改仓库的副本，跑断言。

改坏检验以 `tests/tools/mutate.sh` 与 `tests/cases/main/brightness.mutants` 为准（本 Task 的 9 个变体为 M1–M7、M10、M11；M12 属于 32 位测试）。

Expected：9 个变体均打印 `KILLED`，失败断言见「验证证据」。

---

### Task 6: 小 `max_brightness` 时不写 0（评审 I-1，已完成）

**Files:** Modify `custom/custom_brightness.c` `backlight_write_percent()`：换算后 `if (raw < 1) raw = 1;`；断言增 H9/H1。

- [x] **Step 1: 写失败断言**：`9/9` 与 `1/1` 下 `--drag`，断言输出含 `after drag left: 1`。
- [x] **Step 2: 确认失败**：修复前 `FAIL H9`、`FAIL H1`（最左写入 0），其余 PASS。
- [x] **Step 3: 修复**：`long raw = ((long)pct * backlight_max + 50) / 100;` 之后加 `if (raw < 1) raw = 1;`。
- [x] **Step 4: 确认通过**：全部断言 PASS。
- [x] **Step 5: 改坏变体 M10**（删下限）被 H9/H1 抓住。

---

### Task 7: 评审 Minor 处理（M-1、M-3、M-4，已完成）

**Files:** Modify `custom/custom_brightness.c`；调整断言。

**Interfaces:** 内部 `backlight_open()` 返回 `BACKLIGHT_OK`（0）、`BACKLIGHT_NO_DEVICE`、`BACKLIGHT_BAD_MAX` 三种结果。

- [x] **Step 1: 改断言（先红）**：A 另断言输出含 `brightness: using $BL (max 255)`；C 断言恰好一行 `no backlight device under`；D、E 断言恰好一行 `invalid max_brightness in $BL`；新增 N：`30000000/100` 下恰好一行 `invalid max_brightness`。
- [x] **Step 2: 确认失败**：修复前 A 无 `using` 行，D/E 原因为 `no backlight device`，N 创建了控件。
- [x] **Step 3: 实现**：`#include <limits.h>`；`#define BRIGHTNESS_MAX_RAW (INT_MAX / 100)`；`backlight_open()` 在目录缺失、无有效项、路径截断时返回 `BACKLIGHT_NO_DEVICE`，`max_brightness` 读不出、`<= 0` 或 `> BRIGHTNESS_MAX_RAW` 时返回 `BACKLIGHT_BAD_MAX`；`backlight_read_percent()` 把读到的 `raw` 夹到 `[0, max]`；`brightness_ui_create()` 按结果打印 spec §5.4 的三种日志之一。
- [x] **Step 4: 确认通过**：全部断言 PASS。
- [x] **Step 5: 改坏变体**：M5（删 `<= 0`）由 E 抓住，M11（删上界）由 N 抓住。
- [x] **Step 6: 交叉编译**：Task 4 两步，Expected 同 Task 4。

---

### Task 8: `AGENTS.md` 更正 uClibc 构建前提（`docs(agents)`，已完成）

**Files:** Modify `AGENTS.md`（以仓库为准）：`### 构建（标准开发流程）` 中 `LUCKFOX_SDK_PATH` 一行改为「无需完整 SDK，稀疏检出约 233 MB 工具链」并给出检出与构建命令；`### 运行 / 验证 GUI（重要陷阱）` 首段末尾补 `qemu-user` 冒烟命令，注明 `LD_LIBRARY_PATH` 须为绝对路径。

- [x] **Step 1:** 按上述要点修改，并在仓库根目录按新文本实跑 uClibc 构建与 qemu 冒烟，结果同 Task 4 Step 2。
- [x] **Step 2:** `wc -l -c AGENTS.md` 仍远低于 ~150 行、32 KiB；实测 52 行、7634 字节。
- [x] **Step 3:** 单独 `docs(agents)` 提交，置于 `feat(custom)` 之后、`docs(superpowers)` 之前。

---

### Task 9: 真机验证（已完成，下限不达标由 Task 11 修订）

**Files:** 无仓库文件。消费 run 36403515848（提交 `3c8fb4f`，当时 `BRIGHTNESS_MIN_PCT` 为 5）的 uClibc artifact；下限修订后的复测见 Task 12。结果回填「验证证据」。

- [x] **Step 1: 前置采集**

```bash
ls /sys/class/backlight/; cat /sys/class/backlight/*/max_brightness; cat /sys/class/backlight/*/brightness; lsmod | grep pwm_bl; which mpv
```

Expected：一个背光目录（预期 `backlight`），`max_brightness` 为 255，`pwm_bl` 已加载，`/usr/bin/mpv` 存在。实测：`backlight`、255、`brightness` 255、`pwm_bl 4629 0`、`/usr/bin/mpv`。

- [x] **Step 2: 功能**：`adb push` artifact 中的 `luckfox_lvgl_demo` 到 `/root`，运行；输出含 `brightness: using /sys/class/backlight/<名> (max 255)`；主屏出现滑条且不遮挡时钟、日期与底排按键；拖动时亮度连续变化、百分比同步；拖到最左屏幕仍可见；进出其他页面后滑条值保持；无音乐时点 MUSIC，弹窗盖住滑条。实测：输出 `Mode 0: 480x480 @ 60Hz`、`brightness: using /sys/class/backlight/backlight (max 255)`，其余各项作者确认正常。
- [x] **Step 3: 下限**：退出例程后以 Step 2 打印的 `using` 行中的目录为准，`B=<该目录>; for n in 1 3 5 8 13 20; do echo $n > "$B/brightness"; sleep 2; done; echo 255 > "$B/brightness"`，记录最低可见值。实测：命令行 25 最暗可见、24 看不清；GUI 10%（写 26）可见、9%（写 23）看不清；原下限 5%（写 13）不可见，按 Task 11 把 D2 改为 10%。
- [x] **Step 4: 缺失**：`rmmod pwm_bl` 后重启例程，控件不出现、串口有 `brightness: no backlight device …` 一行、其余功能正常；验证完 `insmod /oem/usr/ko/pwm_bl.ko`。实测：`rmmod` 后打印 `brightness: no backlight device under /sys/class/backlight, control hidden`；`insmod` 后恢复 `using` 行。

---

### Task 10: 整理提交（已完成）

**Files:** 无内容改动。

- [x] **Step 1:** 分支提交整理为 `feat(custom)` → `docs(agents)` → `docs(superpowers)` 三个，与历史 PR 一致；整理前后 `git diff --stat` 为空。

---

### Task 11: 按真机阈值把下限上调到 10%（已完成）

**Files:** Modify `custom/custom_brightness.c` 的 `BRIGHTNESS_MIN_PCT`（5 → 10）与其注释；断言 A；spec D2、FR2、FR3、§5.2、§6、§7、Q3 已随之修订。代码并入 `feat(custom)`，文档并入 `docs(superpowers)`。

- [x] **Step 1: 改断言（先红）**：A 期望 `after drag left: 26`。
- [x] **Step 2: 确认失败**：当前代码 `FAIL A`（最左写入 13），其余 PASS。
- [x] **Step 3: 修改**：`#define BRIGHTNESS_MIN_PCT      10`，注释写明本板 raw 25 为最暗可见。
- [x] **Step 4: 确认通过**：全部断言 PASS，记录 G 写入次数是否仍不超过 15。实测全部 PASS，G 写入 11 次。
- [x] **Step 5: 改坏变体**：Task 5 Step 4 的 9 个变体（M1 锚点为 `10`），9 个变体均 `KILLED`，结果见「验证证据」。
- [x] **Step 6: 交叉编译**：Task 4 两步，Expected 同 Task 4。
- [x] **Step 7: 重出截图**：Task 1 Step 3，最左截图显示 10%。
- [x] **Step 8: 并入提交**：代码并入 `feat(custom)`、文档并入 `docs(superpowers)`，整理前后代码树一致。

---

### Task 12: 真机复测（已完成）

**Files:** 无仓库文件。消费 Task 11 推送后 CI 的 uClibc artifact。

- [x] **Step 1:** 运行后仍打印 `using` 行；把滑条拖到最左，显示 10%，屏幕可见；`cat <using 行中的目录>/brightness` 为 `26`。实测：run 36419372359 的 uClibc artifact 打印 `brightness: using /sys/class/backlight/backlight (max 255)`，拖到最左后 `brightness` 为 `26`、屏幕可见；退出（OFF）后重进，滑条按 sysfs 当前值显示（spec D3）。

---

### Task 13: 32 位下换算不溢出（已完成）

**Files:** Modify `custom/custom_brightness.c` 的 `backlight_read_percent()`、`backlight_write_percent()` 与 `BRIGHTNESS_MAX_RAW` 注释；测试 `tests/cases/main/{test,unit}_brightness_arm32.c`。代码并入 `feat(custom)`。

**Interfaces:** 测试以 `#include "custom_brightness.c"` 直接调用文件内 `static` 函数，用 `-ffunction-sections -Wl,--gc-sections` 丢弃未引用的 `brightness_ui_create()` 及其 LVGL 依赖；以 glibc ARM 工具链编译为 32 位，`qemu-arm -L /usr/arm-linux-gnueabihf` 运行。原生 harness 的 `long` 为 64 位，测不出此问题。

- [x] **Step 1: 写失败测试**：`max_brightness=21474836` 时断言初值 100、写 100% 后为 `21474836`；`255/204` 时断言初值 80、写 10% 后为 `26`。

以仓库 `tests/cases/main/{test,unit}_brightness_arm32.c` 为准，ctest 名 `main.brightness_arm32.conversion`：`max=21474836` 初值 100、写 100% 得 21474836，`max=255` 初值 80、写 10% 得 26。

- [x] **Step 2: 确认失败**：修复前输出 `sizeof(long)=4`，`FAIL max=21474836 initial pct: got 10 want 100`、`FAIL max=21474836 write 100%: got -21474836 want 21474836`，`255` 两项 PASS。
- [x] **Step 3: 修复**：两处中间值改为 `(long long)` 计算后转回 `long`；`BRIGHTNESS_MAX_RAW` 注释改为「超过即视为异常，中间值一律用 long long」。M7 锚点随之改为新写法（Task 5 Step 4）。
- [x] **Step 4: 确认通过**：32 位换算测试 6 项全 PASS；11 项断言全 PASS；M7 仍被 A 抓住（最左写入 25）；glibc、uClibc 交叉编译无新增告警。

## Execution Handoff

全部 Task 已完成；Task 9、12 由作者在真机执行，Task 5 的独立评审由全新子代理执行，其余在 Cloud Agent 内完成。

## 验证证据

| 项 | 判据 | 实测 |
|---|---|---|
| 原生渲染 480/720 | 布局与交互 | 两种分辨率下初始 80%、最左 10%（写入 26）、弹窗遮盖、无设备隐藏四类截图符合 spec §5.3；在 Project Context `media/brightness-slider/` |
| 严格编译新模块 | host gcc `-Wall -Wextra -Werror` 零告警 | 通过 |
| 挂载 | `main_app_init` 调用一次 | `objdump` 计数 `1`；背光缺失日志每次一行 |
| 自动断言基线（Task 5 Step 3、Task 11 Step 4） | 11 项全 `PASS` | 全 `PASS`，G 写入 11 次；下限改 10% 前 A 失败（最左写入 13），改后全 `PASS` |
| 独立两阶段评审（Task 5 Step 1） | Critical/Important 修完 | 「修复后可合并」：Critical 0、Important 1（I-1）、Minor 7；报告在 Project Context `internal/brightness-slider-code-review.md` |
| I-1 先红后绿（Task 6） | 修复前失败、修复后通过 | 修复前 `FAIL H9`、`FAIL H1`（最左写入 0）；修复后全 PASS |
| Minor 先红后绿（Task 7） | 修复前失败、修复后通过 | 修复前 A 无 `using` 行，D/E 原因为 `no backlight device`，N 创建了控件；修复后全 PASS |
| 32 位换算（Task 13） | 上界处不溢出 | glibc ARM 编译、qemu-arm 运行 `u32test`：修复前初值 10、写 100% 得 `-21474836`；修复后 6 项全 PASS |
| 故意改坏（Task 5 Step 4、Task 11 Step 5，最终代码） | 9 个变体均被抓住 | M1 A：最左写入 1（下限 1 兜底，期望 26）；M2 G：写入 37 次（上限 15）；M3 M：弹窗上拖后 242；M4 B：初值 0 被写成 26；M5 E：除零 SIGFPE（`rc=136`）；M6 F：报错 36 行；M7 A：最左写入 25；M10 H1：最左写入 0；M11 N：`max_brightness=30000000` 时创建了控件 |
| glibc 交叉编译 | ELF32 ARM、无新增告警 | 通过；`custom/` 告警仅既存 3 条 |
| uClibc 本机编译与冒烟 | ABI 与 qemu 退出点 | 工具链稀疏检出约 5 s、233 MB，构建约 3 s；ELF32 ARM、`NEEDED libc.so.0`、`/lib/ld-uClibc.so.0`；qemu 至 `cannot open /dev/dri/card0`，`exit=1` |
| 固件含 `mpv` | 镜像中有 `/usr/bin/mpv` | run 35115207257 Ultra artifact `rootfs.img` 校验 OK；`debugfs` 见 `/usr/bin/mpv`、`libmpv.so.1.109.0`、`libavcodec.so.58`、`libasound.so.2`；无 `opkg`/`apt` |
| PR CI | glibc 与 uClibc 两行绿 | run 36428568144（最终代码树）CI 镜像、glibc、uClibc 三个 job 均 success；真机复测所用 run 36419372359 的代码与最终代码相差注释及换算中间值改为 `long long`，`max_brightness=255` 时各值不变（Task 13 的 `u32test` 覆盖） |
| 真机（Task 9） | 前置、运行、下限、缺失 | 前置：`backlight`、max 255、`pwm_bl` 已加载、`/usr/bin/mpv`；运行打印 `using /sys/class/backlight/backlight (max 255)`，布局、页面往返、弹窗遮盖均正常；下限：raw 25 最暗可见、24 看不清，原 5%（写 13）不可见；缺失：`rmmod` 后控件隐藏并打印 `no backlight device`，`insmod` 后恢复 |
| 真机复测（Task 12） | 最左 10% 可见、写 26 | 通过：`using` 行正常；最左写入 26、屏幕可见；退出重进后滑条显示 sysfs 当前值 |

## 与计划的偏离及原因

- **sysfs 读写由 `open`/`read`/`write` 改为 stdio**：`custom_brightness.c` 同时含 `<fcntl.h>` 与 `custom.h`（引入 `<linux/fcntl.h>`）时 host gcc 报 `/usr/include/asm-generic/fcntl.h:193:8: error: redefinition of 'struct flock'`（原定义 `/usr/include/x86_64-linux-gnu/bits/fcntl.h:35`）；spec NFR2、§5.2 已随之修订。
- **新增层级下沉**：同级控件按创建顺序绘制，亮度控件压在「Can't find music!」弹窗之上且可拖；改为在模块内 `lv_obj_move_background()`，spec §5.1 已写明约束。
- **挂载点由 `generated/setup_scr_Main.c` 改为 `custom/custom_main.c` 的 `main_app_init()`**：保持 `generated/` 冻结，取舍见 spec D6、Q10。
- **harness 不调用 `custom_init()`**：spec §2.4 的 `mpv` 缺陷使 harness 在无 `mpv` 时退出段错误；该缺陷不在本 PR 修（spec D10、§8）。
- **截图前等待 1.5 s**：LVGL 性能监视器（`LV_USE_PERF_MONITOR 1`）初始文字为 `?`，需在距上次统计 >300 ms 后再有一次重绘才更新；主屏时钟 1 s 刷新提供该重绘。
- **范围扩大**：纳入 FR7（`AGENTS.md`），依据见 spec Q9。
- **Task 1–4 先于 plan 实施**：为尽早预览 GUI，原型代码直接转为交付代码，未经每个 Task 后的独立评审，亮度模块也未先写失败测试；不重做，以 Task 5 的独立两阶段评审与故意改坏检验补齐这两道关卡，评审发现的问题按 Task 6、7 先红后绿修复。
