# Luckfox Pico LVGL Example — 主屏亮度滑条实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在主屏时钟下方加一条 10%–100% 的亮度滑条，拖动时经 sysfs 实时调节 Luckfox Pico Ultra / Luckfox Pico Ultra W 的 `pwm-backlight`。

**Architecture:** 新增 `custom/custom_brightness.c/.h`（手写 LVGL，stdio 读写 `/sys/class/backlight/<首项>/{brightness,max_brightness}`），由 `custom/custom_main.c` 的 `main_app_init()` 挂载，`generated/` 不改。布局与交互用临时原生 harness（x86 LVGL + 假 sysfs）加自动断言与故意改坏检验验证，交叉编译由本机 glibc/uClibc 与 PR CI 验证，亮度效果由作者真机验证。

**Tech Stack:** C（GUI Guider 导出代码 + LVGL 8.3）、CMake、`arm-linux-gnueabihf-`（glibc）、`arm-rockchip830-linux-uclibcgnueabihf-`（uClibc，稀疏检出）、host `gcc` + SDL2 + ffmpeg + strace（harness、截图与断言）、`qemu-user`（uClibc 冒烟）。

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
| `/tmp/brightness-preview/{harness.c,build.sh,check.sh,mutate.sh}` | 原生预览、自动断言与故意改坏检验（不入库，备份在 Project Context `internal/brightness-preview/`） | 1、5 |

---

### Task 1: 原生预览 harness（不入库，已完成）

**Files:** Create（仓库外）：`/tmp/brightness-preview/harness.c`、`/tmp/brightness-preview/build.sh`。

**Interfaces:** 消费 `setup_ui()`、`events_init()`（`generated/`）与 `main.c` 才定义的 5 个全局量（harness 自行定义；不调用 `custom_init()`，见 spec §2.4）；产出 `/tmp/brightness-preview/preview`。用法：`PREVIEW_RES=480|720 ./preview <前缀> [--drag|--music]` headless 出 `<前缀>-*.ppm`；`./preview --sdl` 开窗，鼠标即触摸。环境变量 `BL` 为假背光目录（供 harness 内 `system()` 打印写入值）。

- [x] **Step 1: 写 harness**

```c
/* 亮度滑条的原生预览 harness（不入库）。 */
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lvgl.h"
#include "gui_guider.h"
#include "events_init.h"
#include "custom.h"

lv_ui guider_ui;
int QUIT_FLAG;
float SCALE;
int WIFI_ENABLE;
int MUSIC_ENABLE;

#define RES_MAX 720

static int RES = 720;
static uint32_t fb[RES_MAX * RES_MAX];
static int ptr_x, ptr_y, ptr_down;
static SDL_Renderer *renderer;
static SDL_Texture *texture;

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *px)
{
    for (int y = a->y1; y <= a->y2; y++)
        for (int x = a->x1; x <= a->x2; x++)
            fb[y * RES + x] = (px++)->full;
    if (texture && lv_disp_flush_is_last(drv)) {
        SDL_UpdateTexture(texture, NULL, fb, RES * 4);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }
    lv_disp_flush_ready(drv);
}

static void read_cb(lv_indev_drv_t *drv, lv_indev_data_t *d)
{
    (void)drv;
    d->point.x = ptr_x;
    d->point.y = ptr_y;
    d->state = ptr_down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void run_ms(int ms)
{
    for (int t = 0; t < ms; t += 5) {
        lv_timer_handler();
        usleep(5000);
    }
}

static void save_ppm(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    fprintf(f, "P6\n%d %d\n255\n", RES, RES);
    for (int i = 0; i < RES * RES; i++) {
        unsigned char rgb[3] = { (fb[i] >> 16) & 0xff, (fb[i] >> 8) & 0xff, fb[i] & 0xff };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

/* 按 480 设计坐标脚本化拖动，与例程一样按 SCALE 缩放。 */
static void drag(int x0, int x1, int y)
{
    ptr_x = x0 * SCALE; ptr_y = y * SCALE; ptr_down = 1;
    run_ms(100);
    for (int i = 1; i <= 20; i++) {
        ptr_x = (x0 + (x1 - x0) * i / 20) * SCALE;
        run_ms(20);
    }
    ptr_down = 0;
    run_ms(100);
}

int main(int argc, char **argv)
{
    int sdl = argc > 1 && strcmp(argv[1], "--sdl") == 0;

    if (getenv("PREVIEW_RES"))
        RES = atoi(getenv("PREVIEW_RES"));
    if (RES <= 0 || RES > RES_MAX)
        RES = RES_MAX;

    /* custom_init() 会 vfork() 启动 mpv，无 mpv 时子进程返回会破坏父进程栈（spec §2.4），因此不调用它，在此自行设置全局量。 */
    QUIT_FLAG = 0;
    SCALE = (float)RES / WIDTH;
    WIFI_ENABLE = 1;
    MUSIC_ENABLE = 0;

    lv_init();
    static lv_color_t buf[RES_MAX * RES_MAX];
    static lv_disp_draw_buf_t draw_buf;
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, RES * RES);
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &draw_buf;
    disp_drv.flush_cb = flush_cb;
    disp_drv.hor_res = RES;
    disp_drv.ver_res = RES;
    disp_drv.full_refresh = 1;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = read_cb;
    lv_indev_drv_register(&indev_drv);

    if (sdl) {
        SDL_Init(SDL_INIT_VIDEO);
        SDL_Window *win = SDL_CreateWindow(RES == 480 ? "Brightness preview (480x480)" : "Brightness preview (720x720)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, RES, RES, 0);
        renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, RES, RES);
    }

    setup_ui(&guider_ui);
    events_init(&guider_ui);
    /* 性能监视器在启动 300 ms 后再发生一次重绘前显示「?」；主屏时钟每秒刷新提供这次重绘，与真机一致。 */
    run_ms(1500);

    if (sdl) {
        for (;;) {
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_QUIT)
                    return 0;
                if (ev.type == SDL_MOUSEMOTION) { ptr_x = ev.motion.x; ptr_y = ev.motion.y; }
                if (ev.type == SDL_MOUSEBUTTONDOWN) ptr_down = 1;
                if (ev.type == SDL_MOUSEBUTTONUP) ptr_down = 0;
            }
            lv_timer_handler();
            usleep(5000);
        }
    }

    const char *out = argc > 1 ? argv[1] : "shot";
    char path[256];
    snprintf(path, sizeof(path), "%s-initial.ppm", out);
    save_ppm(path);
    if (argc > 2 && strcmp(argv[2], "--music") == 0) {
        ptr_x = 240 * SCALE; ptr_y = 410 * SCALE; ptr_down = 1;
        run_ms(100);
        ptr_down = 0;
        run_ms(300);
        snprintf(path, sizeof(path), "%s-popup.ppm", out);
        save_ppm(path);
        drag(100, 400, 290);
        snprintf(path, sizeof(path), "%s-popup-dragged.ppm", out);
        save_ppm(path);
        system("echo \"after drag on popup: $(cat $BL/brightness)\"");
    }
    if (argc > 2 && strcmp(argv[2], "--drag") == 0) {
        drag(240, 30, 290);
        snprintf(path, sizeof(path), "%s-left.ppm", out);
        save_ppm(path);
        system("echo \"after drag left: $(cat $BL/brightness)\"");
        drag(240, 460, 290);
        snprintf(path, sizeof(path), "%s-right.ppm", out);
        save_ppm(path);
        system("echo \"after drag right: $(cat $BL/brightness)\"");
    }
    return 0;
}
```

- [x] **Step 2: 写构建脚本**（编译单元与 include 路径按 [`2026-07-24-lvgl-example-cloudagent-env-design.md`](../specs/2026-07-24-lvgl-example-cloudagent-env-design.md) §6.1；增量编译只比较 `.c` 的 mtime，改了头文件须先删 `$OUT/obj`；目标文件按相对源码树的路径命名，改坏变体可复用基线对象；`STRICT=0` 跳过末行 `-Werror` 严格编译）

```bash
#!/usr/bin/env bash
set -euo pipefail
shopt -s globstar
R=${SRC:-/workspace}
D=${OUT:-/tmp/brightness-preview}
mkdir -p "$D/obj"
INC=(-I"$R/src" -I"$R/include" -I/usr/include/libdrm -I/usr/include/cjson -I"$R/lib" -I"$R/lib/lvgl" -I"$R/lib/lvgl/src" -I"$R/custom" -I"$R/generated" -I"$R/generated/guider_customer_fonts" -I"$R/generated/guider_fonts" -I"$R/generated/images")
DEFS=(-DBACKLIGHT_SYSFS_DIR="\"$D/sysfs\"")
objs=()
: > "$D/cmds.txt"
for f in "$R"/lib/lvgl/src/**/*.c "$R"/generated/**/*.c "$R"/custom/*.c "$D/harness.c"; do
  o="$D/obj/$(echo "${f#"$R"/}" | md5sum | cut -c1-12).o"
  if [[ ! -f "$o" || "$f" -nt "$o" || "$f" == "$R/custom/custom_brightness.c" || "$f" == "$D/harness.c" ]]; then
    printf "gcc -c -O1 -w %s '-DBACKLIGHT_SYSFS_DIR=\"%s/sysfs\"' %s -o %s\n" "${INC[*]}" "$D" "$f" "$o" >> "$D/cmds.txt"
  fi
  objs+=("$o")
done
xargs -d '\n' -P"$(nproc)" -I{} sh -c '{}' < "$D/cmds.txt"
gcc -o "$D/preview" "${objs[@]}" -lSDL2 -ldrm -lcjson -lpthread -lm
[[ ${STRICT:-1} == 1 ]] && gcc -c -std=gnu99 -Wall -Wextra -Werror "${INC[@]}" "${DEFS[@]}" "$R/custom/custom_brightness.c" -o /dev/null
echo "built $D/preview"
```

- [x] **Step 3: 出截图（480 与 720 各一轮）**

```bash
cd /tmp/brightness-preview && chmod +x build.sh && ./build.sh
export BL=/tmp/brightness-preview/sysfs/backlight
run(){ rm -rf sysfs; if [ -n "$1" ]; then mkdir -p $BL; echo "$1" > $BL/max_brightness; echo "$2" > $BL/brightness; fi; timeout 30 ./preview $3 $4 </dev/null; echo "exit=$?"; }
for R in 480 720; do export PREVIEW_RES=$R; mkdir -p out$R
  run 255 204 out$R/a --drag; run 255 0 out$R/b; run 255 204 out$R/m --music; run "" "" out$R/c
  for f in out$R/*.ppm; do ffmpeg -nostdin -loglevel error -y -i $f ${f%.ppm}.png; done
done
```

Expected：`out480/`、`out720/` 下分别有初始 80%、最左 10%、弹窗遮盖、无设备隐藏四类截图。循环内调用 ffmpeg 必须 `-nostdin`，否则会吞掉外层 `while read` 的输入。

- [x] **Step 4: 开窗供试用**（在 VM 桌面 `DISPLAY=:1`，tmux 会话 `brightness-preview`）

```bash
PREVIEW_RES=480 DISPLAY=:1 ./preview --sdl
```

---

### Task 2: 亮度模块（`feat(custom)`，已完成）

**Files:** Create `custom/custom_brightness.h`、`custom/custom_brightness.c`（以仓库为准）。

**Interfaces:** 产出 `void brightness_ui_create(lv_obj_t *parent);`。内部 `static` 状态：`backlight_dir[256]`、`backlight_max`、`backlight_last_written`（初值 −1，读到当前值后设为该值）、`backlight_write_error_reported`。

- [x] **Step 1:** 按 spec §5.1–§5.4 实现后端与控件；`custom.h` 已引入 `<linux/fcntl.h>`，sysfs 读写用 `fopen`/`fgets`/`fprintf`，写入以 `fprintf` 与 `fclose` 均成功判定。
- [x] **Step 2:** 控件创建后依次 `lv_obj_move_background(slider/value_label/title_label)`，使先创建的 `Main_win_music` 盖在其上。
- [x] **Step 3:** `./build.sh` 末行的 `-Wall -Wextra -Werror` 单独编译 `custom_brightness.c` 通过。

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

**Files:** 无仓库改动。仓库外：`/tmp/brightness-preview/check.sh`、`/tmp/brightness-preview/mutate.sh`。

**Interfaces:** 消费 Task 1 的 harness；`check.sh <OUT>` 对该目录下的 `preview` 跑全部断言，逐条打印 `PASS`/`FAIL`，任一失败则退出码 1。前提：`strace`（缺失时 `sudo apt-get install -y strace`）；以 root 运行，F 断言用 `su nobody` 制造只读写入失败，并把 `/tmp/check-f` 设为 777 供其输出截图。

- [x] **Step 1: 独立评审**：按 superpowers:requesting-code-review 派全新子代理（不继承会话），输入 spec、plan 与分支 diff，分两阶段（逐条对照 spec、代码质量）评审，报告写 Project Context `internal/brightness-slider-code-review.md`。结论「修复后可合并」：Critical 0、Important 1（I-1，Task 6 修复）、Minor 7（M-1、M-3、M-4 由 Task 7 修复；M-2、M-5、M-6 改 spec；M-7 随时钟缺陷移出本 PR 不再适用）。
- [x] **Step 2: 自动断言脚本 `check.sh`**（`PREVIEW_RES=480`）。断言：A 拖到最左/最右写入 26/255 并打印 `brightness: using $BL (max 255)`；H9/H1 `max_brightness` 为 9 与 1 时拖到最左写入 1；B 初值 0 启动后仍为 0（创建时不写）；M 弹窗上拖动后仍 204；C/D/E/N 无目录、`abc/100`、`0/100`、`30000000/100` 各恰好一行 `control hidden`，原因分别为 `no backlight device under` 与 `invalid max_brightness in`；F 只读且以 `nobody` 运行，恰好一行 `write backlight failed`、值仍 204；G `10/10` 下以 `O_WRONLY` 打开 `brightness` 不超过 15 次（基线 11 次，拖动过程换算值最多约 14 次变化）且结束为 10；全部 `exit=0`。

```bash
#!/usr/bin/env bash
# 亮度预览的自动断言；用法：check.sh [含 ./preview 的 OUT 目录]
set -u
D=${1:-/tmp/brightness-preview}
BL=$D/sysfs/backlight
export BL PREVIEW_RES=${PREVIEW_RES:-480}
cd "$D" && mkdir -p check
fail=0
ok(){ echo "PASS $1"; }
ng(){ echo "FAIL $1: $2"; fail=1; }
setup(){ rm -rf "$D/sysfs"; mkdir -p "$BL"; echo "$1" > "$BL/max_brightness"; echo "$2" > "$BL/brightness"; }
run(){ timeout 30 ./preview "$@" </dev/null 2>&1; }

setup 255 204; out=$(run check/a --drag); rc=$?
[[ $rc == 0 && $out == *"after drag left: 26"* && $out == *"after drag right: 255"* && $out == *"brightness: using $BL (max 255)"* ]] && ok A || ng A "rc=$rc $(grep after <<<"$out" | tr '\n' ' ')"

for mx in 9 1; do
  setup $mx $mx; out=$(run check/h$mx --drag); rc=$?
  [[ $rc == 0 && $out == *"after drag left: 1"* ]] && ok H$mx || ng H$mx "rc=$rc $(grep 'after drag left' <<<"$out")"
done

setup 255 0; out=$(run check/b); rc=$?; v=$(cat "$BL/brightness")
[[ $rc == 0 && $v == 0 ]] && ok B || ng B "rc=$rc brightness=$v"

setup 255 204; out=$(run check/m --music); rc=$?
[[ $rc == 0 && $out == *"after drag on popup: 204"* ]] && ok M || ng M "rc=$rc $(grep after <<<"$out")"

for c in "C:::no backlight device under $D/sysfs" "D:abc:100:invalid max_brightness in $BL" "E:0:100:invalid max_brightness in $BL" "N:30000000:100:invalid max_brightness in $BL"; do
  IFS=: read -r n mx br why <<<"$c"; rm -rf "$D/sysfs"; [[ -n $mx ]] && setup "$mx" "$br"
  out=$(run check/$n); rc=$?; cnt=$(grep -c "control hidden" <<<"$out"); cw=$(grep -cF "$why" <<<"$out")
  [[ $rc == 0 && $cnt == 1 && $cw == 1 ]] && ok $n || ng $n "rc=$rc hidden_lines=$cnt reason_lines=$cw $(grep 'brightness:' <<<"$out" | head -1)"
done

setup 255 204; chmod 444 "$BL/brightness"; chmod -R a+rX "$D"; mkdir -p /tmp/check-f && chmod 777 /tmp/check-f
out=$(su nobody -s /bin/sh -c "cd '$D' && BL='$BL' PREVIEW_RES=$PREVIEW_RES timeout 30 ./preview /tmp/check-f/f --drag" </dev/null 2>&1); rc=$?
cnt=$(grep -c "write backlight failed" <<<"$out"); v=$(cat "$BL/brightness")
[[ $rc == 0 && $cnt == 1 && $v == 204 ]] && ok F || ng F "rc=$rc error_lines=$cnt brightness=$v"

G_MAX_WRITES=${G_MAX_WRITES:-15}
setup 10 10; strace -f -qq -e trace=openat -o "$D/check/g.strace" timeout 30 ./preview check/g --drag </dev/null >/dev/null 2>&1; rc=$?
w=$(grep -c 'backlight/brightness", O_WRONLY' "$D/check/g.strace"); v=$(cat "$BL/brightness")
echo "INFO G writes=$w"
[[ $rc == 0 && $w -le $G_MAX_WRITES && $v == 10 ]] && ok G || ng G "rc=$rc writes=$w (max $G_MAX_WRITES) brightness=$v"

exit $fail
```

- [x] **Step 3: 基线**：`./build.sh && ./check.sh`，11 项全部 `PASS`，`INFO G writes=11`。
- [x] **Step 4: 改坏变体**：`mutate.sh` 只改 `/workspace` 的副本（`custom/` 复制、其余目录符号链接），复用基线对象增量编译后跑 `check.sh`。

```bash
#!/usr/bin/env bash
# 复制 /workspace 并对副本做一处改坏后编译、跑 check.sh；用法：mutate.sh <变体编号> <仓库内相对路径> <被替换原文> <替换为>
set -u
id=$1; file=$2; old=$3; new=$4
B=/tmp/brightness-preview; M=/tmp/mut-$id
rm -rf "$M"; mkdir -p "$M/src" "$M/out"
for d in lib generated include src; do ln -s "/workspace/$d" "$M/src/$d"; done
cp -a /workspace/custom "$M/src/custom"
cp -a "$B/obj" "$M/out/obj"; cp "$B/harness.c" "$B/build.sh" "$B/check.sh" "$M/out/"
python3 - "$M/src/$file" "$old" "$new" <<'PY'
import sys
p, old, new = sys.argv[1], sys.argv[2], sys.argv[3]
s = open(p).read()
if s.count(old) != 1:
    sys.exit("mutation anchor must match exactly once: %r (found %d)" % (old, s.count(old)))
open(p, 'w').write(s.replace(old, new))
PY
touch "$M/src/$file"
SRC="$M/src" OUT="$M/out" STRICT=0 bash "$M/out/build.sh" >/dev/null 2>&1 || { echo "BUILD FAILED $id"; exit 2; }
res=$(bash "$M/out/check.sh" "$M/out"); echo "$res" | grep -E "^(FAIL|INFO)" | sed "s/^/[$id] /"
grep -q '^FAIL' <<<"$res" && echo "[$id] KILLED" || echo "[$id] SURVIVED"
```

```bash
cd /tmp/brightness-preview && B=custom/custom_brightness.c
./mutate.sh M1 $B '#define BRIGHTNESS_MIN_PCT      10' '#define BRIGHTNESS_MIN_PCT      0'
./mutate.sh M2 $B $'    if (raw == backlight_last_written)\n        return;\n' ''
./mutate.sh M3 $B $'    lv_obj_move_background(slider);\n    lv_obj_move_background(value_label);\n    lv_obj_move_background(title_label);\n' ''
./mutate.sh M4 $B $'    pct = backlight_read_percent();\n' $'    pct = backlight_read_percent();\n    backlight_write_percent(pct);\n'
./mutate.sh M5 $B ' || backlight_max <= 0' ''
./mutate.sh M6 $B '} else if (!backlight_write_error_reported) {' '} else {'
./mutate.sh M7 $B 'long raw = (long)(((long long)pct * backlight_max + 50) / 100);' 'long raw = (long)(((long long)pct * backlight_max) / 100);'
./mutate.sh M10 $B $'    if (raw < 1)\n        raw = 1;\n' ''
./mutate.sh M11 $B ' || backlight_max > BRIGHTNESS_MAX_RAW' ''
```

Expected：9 个变体均打印 `KILLED`，失败断言见「验证证据」。

---

### Task 6: 小 `max_brightness` 时不写 0（评审 I-1，已完成）

**Files:** Modify `custom/custom_brightness.c` `backlight_write_percent()`：换算后 `if (raw < 1) raw = 1;`；`check.sh` 增断言 H9/H1。

- [x] **Step 1: 写失败断言**：`9/9` 与 `1/1` 下 `--drag`，断言输出含 `after drag left: 1`。
- [x] **Step 2: 确认失败**：修复前 `FAIL H9`、`FAIL H1`（最左写入 0），其余 PASS。
- [x] **Step 3: 修复**：`long raw = ((long)pct * backlight_max + 50) / 100;` 之后加 `if (raw < 1) raw = 1;`。
- [x] **Step 4: 确认通过**：`./build.sh && ./check.sh` 全部 PASS。
- [x] **Step 5: 改坏变体 M10**（删下限）被 H9/H1 抓住。

---

### Task 7: 评审 Minor 处理（M-1、M-3、M-4，已完成）

**Files:** Modify `custom/custom_brightness.c`；`check.sh` 调整断言。

**Interfaces:** 内部 `backlight_open()` 返回 `BACKLIGHT_OK`（0）、`BACKLIGHT_NO_DEVICE`、`BACKLIGHT_BAD_MAX` 三种结果。

- [x] **Step 1: 改断言（先红）**：A 另断言输出含 `brightness: using $BL (max 255)`；C 断言恰好一行 `no backlight device under`；D、E 断言恰好一行 `invalid max_brightness in $BL`；新增 N：`30000000/100` 下恰好一行 `invalid max_brightness`。
- [x] **Step 2: 确认失败**：修复前 A 无 `using` 行，D/E 原因为 `no backlight device`，N 创建了控件。
- [x] **Step 3: 实现**：`#include <limits.h>`；`#define BRIGHTNESS_MAX_RAW (INT_MAX / 100)`；`backlight_open()` 在目录缺失、无有效项、路径截断时返回 `BACKLIGHT_NO_DEVICE`，`max_brightness` 读不出、`<= 0` 或 `> BRIGHTNESS_MAX_RAW` 时返回 `BACKLIGHT_BAD_MAX`；`backlight_read_percent()` 把读到的 `raw` 夹到 `[0, max]`；`brightness_ui_create()` 按结果打印 spec §5.4 的三种日志之一。
- [x] **Step 4: 确认通过**：`./build.sh && ./check.sh` 全部 PASS。
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

**Files:** Modify `custom/custom_brightness.c` 的 `BRIGHTNESS_MIN_PCT`（5 → 10）与其注释；`check.sh` 断言 A；spec D2、FR2、FR3、§5.2、§6、§7、Q3 已随之修订。代码并入 `feat(custom)`，文档并入 `docs(superpowers)`。

- [x] **Step 1: 改断言（先红）**：A 期望 `after drag left: 26`。
- [x] **Step 2: 确认失败**：当前代码 `FAIL A`（最左写入 13），其余 PASS。
- [x] **Step 3: 修改**：`#define BRIGHTNESS_MIN_PCT      10`，注释写明本板 raw 25 为最暗可见。
- [x] **Step 4: 确认通过**：`./build.sh && ./check.sh` 全部 PASS，记录 G 写入次数是否仍不超过 15。实测全部 PASS，G 写入 11 次。
- [x] **Step 5: 改坏变体**：Task 5 Step 4 的 9 条 `./mutate.sh` 命令（M1 锚点为 `10`），9 个变体均 `KILLED`，结果见「验证证据」。
- [x] **Step 6: 交叉编译**：Task 4 两步，Expected 同 Task 4。
- [x] **Step 7: 重出截图**：Task 1 Step 3，最左截图显示 10%。
- [x] **Step 8: 并入提交**：代码并入 `feat(custom)`、文档并入 `docs(superpowers)`，整理前后代码树一致。

---

### Task 12: 真机复测（已完成）

**Files:** 无仓库文件。消费 Task 11 推送后 CI 的 uClibc artifact。

- [x] **Step 1:** 运行后仍打印 `using` 行；把滑条拖到最左，显示 10%，屏幕可见；`cat <using 行中的目录>/brightness` 为 `26`。实测：run 36419372359 的 uClibc artifact 打印 `brightness: using /sys/class/backlight/backlight (max 255)`，拖到最左后 `brightness` 为 `26`、屏幕可见；退出（OFF）后重进，滑条按 sysfs 当前值显示（spec D3）。

---

### Task 13: 32 位下换算不溢出（已完成）

**Files:** Modify `custom/custom_brightness.c` 的 `backlight_read_percent()`、`backlight_write_percent()` 与 `BRIGHTNESS_MAX_RAW` 注释；仓库外 `/tmp/u32/u32test.c`、`/tmp/u32/build.sh`。代码并入 `feat(custom)`。

**Interfaces:** 测试程序以 `#include "custom_brightness.c"` 直接调用文件内 `static` 函数，用 `-ffunction-sections -Wl,--gc-sections` 丢弃未引用的 `brightness_ui_create()` 及其 LVGL 依赖；以 glibc ARM 工具链编译为 32 位，`qemu-arm -L /usr/arm-linux-gnueabihf` 运行。原生 harness 的 `long` 为 64 位，测不出此问题。

- [x] **Step 1: 写失败测试**：`max_brightness=21474836` 时断言初值 100、写 100% 后为 `21474836`；`255/204` 时断言初值 80、写 10% 后为 `26`。

```c
/* 32 位目标上亮度换算的边界测试：直接包含 custom_brightness.c 以调用其中的 static 函数。 */
#define BACKLIGHT_SYSFS_DIR "/tmp/u32/sysfs"
#include "custom_brightness.c"

#include <sys/stat.h>

static int fails;

static void put(const char *attr, const char *value)
{
    char path[256];
    FILE *fp;

    snprintf(path, sizeof(path), "%s/backlight/%s", BACKLIGHT_SYSFS_DIR, attr);
    fp = fopen(path, "w");
    fputs(value, fp);
    fclose(fp);
}

static long get_brightness(void)
{
    long v = -1;
    FILE *fp = fopen(BACKLIGHT_SYSFS_DIR "/backlight/brightness", "r");

    if (fp != NULL) {
        if (fscanf(fp, "%ld", &v) != 1)
            v = -1;
        fclose(fp);
    }
    return v;
}

static void expect(const char *what, long got, long want)
{
    printf("%s %s: got %ld want %ld\n", got == want ? "PASS" : "FAIL", what, got, want);
    if (got != want)
        fails++;
}

static void reset_state(void)
{
    backlight_last_written = -1;
    backlight_write_error_reported = false;
}

int main(void)
{
    mkdir(BACKLIGHT_SYSFS_DIR, 0755);
    mkdir(BACKLIGHT_SYSFS_DIR "/backlight", 0755);
    printf("sizeof(long)=%zu\n", sizeof(long));

    put("max_brightness", "21474836\n");
    put("brightness", "21474836\n");
    reset_state();
    expect("max=21474836 open", backlight_open(), BACKLIGHT_OK);
    expect("max=21474836 initial pct", backlight_read_percent(), 100);
    put("brightness", "0\n");
    reset_state();
    backlight_read_percent();
    backlight_write_percent(100);
    expect("max=21474836 write 100%", get_brightness(), 21474836);

    put("max_brightness", "255\n");
    put("brightness", "204\n");
    reset_state();
    expect("max=255 open", backlight_open(), BACKLIGHT_OK);
    expect("max=255 initial pct", backlight_read_percent(), 80);
    backlight_write_percent(10);
    expect("max=255 write 10%", get_brightness(), 26);

    return fails ? 1 : 0;
}
```

```bash
#!/usr/bin/env bash
# 以 glibc ARM 工具链把 u32test.c 编成 32 位程序，并用 qemu-arm 运行。
set -u
R=${SRC:-/workspace}
cd /tmp/u32 && rm -rf sysfs
arm-linux-gnueabihf-gcc -std=gnu99 -O1 -w -ffunction-sections -fdata-sections -Wl,--gc-sections -I"$R/src" -I"$R/include" -I"$R/include/glibc/drm" -I"$R/include/glibc/drm/libdrm" -I"$R/include/glibc/cjson" -I"$R/lib" -I"$R/lib/lvgl" -I"$R/lib/lvgl/src" -I"$R/custom" -I"$R/generated" -I"$R/generated/guider_customer_fonts" -I"$R/generated/guider_fonts" -I"$R/generated/images" u32test.c -o u32test || exit 2
qemu-arm -L /usr/arm-linux-gnueabihf ./u32test
```

- [x] **Step 2: 确认失败**：修复前输出 `sizeof(long)=4`，`FAIL max=21474836 initial pct: got 10 want 100`、`FAIL max=21474836 write 100%: got -21474836 want 21474836`，`255` 两项 PASS。
- [x] **Step 3: 修复**：两处中间值改为 `(long long)` 计算后转回 `long`；`BRIGHTNESS_MAX_RAW` 注释改为「超过即视为异常，中间值一律用 long long」。M7 锚点随之改为新写法（Task 5 Step 4）。
- [x] **Step 4: 确认通过**：`u32test` 6 项全 PASS；`check.sh` 11 项全 PASS；M7 仍被 A 抓住（最左写入 25）；glibc、uClibc 交叉编译无新增告警。

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
