# 原生测试

`tests/` 是在 x86 主机上运行的独立 CMake 工程：用主机 gcc 以 ASan/UBSan 编译产品实际使用的 `lib/lvgl`、`generated/`、`custom/`，headless 运行，不需要开发板、`/dev/fb0` 或触摸屏。它与顶层交叉编译工程互不引用。

## 运行

```bash
cmake -S tests -B build-tests
cmake --build build-tests -j"$(nproc)"
ctest --test-dir build-tests --output-on-failure -j"$(nproc)"
```

- 依赖：`build-essential`、`cmake`、`libdrm-dev`、`libcjson-dev`（均在 `.cursor/Dockerfile` 中），以及 `python3`（`tools/mutate.sh` 用它解析清单）。
- 只跑一个页面：`ctest --test-dir build-tests -L wifi`；按名字筛选：`ctest --test-dir build-tests -R '^main\.brightness\.'`；只列出不运行：加 `-N`。

## 目录与分类

用例按页面放在 `cases/<页面>/`：控件或逻辑显示在哪一页，测试就放哪个目录；与页面无关的全局逻辑（tick、隐式声明等）归 `main/`。

| 目录 | 页面 | 对应源码 |
|---|---|---|
| `main/` | 主屏与时间（含亮度滑条、SDIO 板型判定） | `custom_main.c`、`custom_brightness.c`、`widgets_init.c`、`lv_conf.h` |
| `wifi/` | WiFi | `custom_wifi.c`、`setup_scr_WIFI.c` |
| `music/` | 音乐页 | `custom_musicplayer.c`、`setup_scr_Music_player.c` |
| `sketchpad/` | 画板（含触摸读取） | `custom_sketchpad.c`、`custom_touch.c`、`setup_scr_Sketchpad.c` |
| `exit/` | OFF 退出 | `custom/custom_fb.c`，调用点 `src/main.c` |
| `gif/` | GIF 页 | `setup_scr_Gif.c` |

还没有测试的页面不建目录；本表即登记处。

`support/` 是公共代码（不含用例，函数统一 `tst_` 前缀）：`check.h` 断言与用例分派，`app_env` 提供 `src/main.c` 的 5 个全局量与 headless 显示、脚本化触摸，`capture` 捕获输出，`fake_fs` 管理本进程临时目录。

## 命名

- 文件 `test_<被测对象>.c`，不加序号或日期前缀；需要调用被测文件里的 `static` 函数时，配一个 `unit_<被测对象>.c`：它 `#include` 被测 `.c`、导出 `tst_` 前缀的包装函数，随被测源码以 `-w` 编译，测试文件本身以 `-Wall -Wextra -Werror` 编译。
- CTest 测试名 `<类别>.<对象>.<用例>`，用例名描述行为；标签为类别名。
- 依赖界面或模块静态状态的用例，每个用例一个进程：同一可执行文件按命令行参数选用例（`tst_run_case()`）。

## 新增测试

1. 在对应页面目录新建 `test_xxx.c`，用 `tst_case_t` 表列出用例，`main()` 调 `tst_run_case()`。
2. 在该目录 `CMakeLists.txt` 加一行 `luckfox_add_test(xxx [UNIT unit_xxx.c] CASES <用例>...)`。
3. 新页面第一个测试：建目录与 `CMakeLists.txt`，在 `tests/CMakeLists.txt` 加 `add_subdirectory(cases/<页面>)`，并更新上表。

编译层检查在对应页面的 `CMakeLists.txt` 用 `luckfox_add_compile_check(<用例> FLAGS -Werror=<诊断> SOURCES <源文件>...)` 注册为 `<类别>.compile.<用例>`（标签 `<类别>;compile`，超时 120 秒），以普通 `-I` 逐个编译源文件，任一失败即测试失败。

## 32 位测试

目标板的 `long` 为 32 位，主机为 64 位，换算溢出只能在 32 位下测出：`main.brightness_arm32.conversion` 用 `arm-linux-gnueabihf-gcc` 编译、`qemu-arm` 运行（标签 `arm32`，`ctest -LE arm32` 可跳过）。`main.tick.monotonic_wrap` 同样在 ARM32 下验证单调时钟、毫秒换算与回绕；其编译选项 `-U_TIME_BITS -U_FILE_OFFSET_BITS` 取消工具链默认的 64 位时间与文件偏移宏，使 `time_t` 为 32 位，覆盖目标板上的溢出条件。这两个目标不经 `luckfox_add_test()`，而由 `cases/main/CMakeLists.txt` 的自定义命令交叉编译并注册，测试名仍按 `<类别>.<对象>.<用例>`。编译器 `arm-linux-gnueabihf-gcc` 与 `qemu-arm` 都由 `.cursor/Dockerfile` 与 `.cursor/Dockerfile.luckfox_pico` 提供；其他环境需 `sudo apt-get install -y gcc-arm-linux-gnueabihf qemu-user`。`-DLUCKFOX_TESTS_ARM32=AUTO`（默认）缺工具时跳过，`ON` 缺工具即报错，`OFF` 不注册。

## 注意

- 常规 UI 测试不调用 `custom_init()`：它会拉起 `mpv` 并做硬件相关初始化；`mpv` 的启动与连接由 `cases/music/` 的用例以假 `mpv` 单独测试。
- `custom_wifi.c` 的 `WPA_FILE_PATH=tst_wpa_conf_path()` 由 `tests/CMakeLists.txt` 的源文件 `COMPILE_DEFINITIONS` 覆盖，声明通过 `-include support/fake_fs.h` 引入，配置文件位于本进程临时目录；包含源码的 `unit_wifi.c` 在 `#include "custom_wifi.c"` 前用 `#define` 做相同覆盖。
- 捕获输出期间 `CHECK` 的打印也会被捕获，断言放在 `tst_capture_end()` 之后。
- 测试工程对 `lib/lvgl/src`、`generated/`、`custom/` 递归 glob 全部 `.c`，与产品 `CMakeLists.txt` 只 glob 指定目录不完全相同，在这些目录下新增子目录或非产品 `.c` 时需留意。
- 音乐源码的 `MUSIC_DIR_PATH=tst_music_dir()` 也通过源文件 `COMPILE_DEFINITIONS` 和 `-include support/fake_fs.h` 覆盖；`unit_list_alloc.c` 在包含源码前用 `#define` 做相同覆盖，目录由本进程独享；`MPV_SOCKET_PATH=tst_mpv_socket_path()` 使用同样的源文件属性与 UNIT 宏覆盖，假 mpv 的套接字也在本进程临时目录。
- 音乐源码另以 `readdir=tst_readdir`、`closedir=tst_closedir` 编译（`unit_list_alloc.c` 用 `#define` 做相同覆盖）：`readdir` 的顺序取决于文件系统（ext4 按文件名哈希与 mkfs 时的随机种子），若读到的恰为升序或降序，部分不排序的改坏也能通过排序用例。`tst_readdir()` 读出真实条目后按文件名排序，再以固定的非单调顺序返回（「.」「..」在前，其余后半与前半交错，如 01 02 03 04 → 03 01 04 02），结果与文件系统无关。
- `custom_fb.c` 的 `FB_CLEAR_MAX_BYTES=tst_fb_clear_max_bytes` 通过源文件 `COMPILE_DEFINITIONS` 和 `-include support/fake_fs.h` 覆盖，用例可修改该变量缩小清屏上限。路径宏是运行期 `const char *` 表达式，不能拼接字符串字面量或用 `sizeof` 计算路径长度。
- libdrm、cjson 头文件使用系统路径 `/usr/include/libdrm`、`/usr/include/cjson`（Debian/Ubuntu 布局）。
- 触摸读取 `custom_touch.c` 的用例经 FIFO 逐帧喂入 `struct input_event`；`touch_step*.txt` 是板上 `evdev_probe` 录到的多指事件原文，用例按文本回放。
- `custom_brightness.c` 在测试构建中以 `BACKLIGHT_SYSFS_DIR=tst_backlight_root()` 编译，背光目录位于本进程临时目录，`ctest -j` 并行互不干扰。

## 手动目标

不进默认构建、不注册为 ctest：

- `cmake --build build-tests --target screenshots`：以 480、720 输出主屏初始、滑条拖到最左、音乐弹窗、无背光设备、音乐页（含列表）、画板六类截图到 `build-tests/screenshots/`（PPM；装了 `ffmpeg` 时另存 PNG）。
- `cmake --build build-tests --target preview` 后 `PREVIEW_RES=480 PREVIEW_BACKLIGHT=255:204 DISPLAY=:1 ./build-tests/preview`：SDL 开窗，鼠标即触摸，点 OFF 退出；需要 SDL2（`libsdl2-dev`），找不到时不生成该目标。`PREVIEW_RES` 默认 480；不设 `PREVIEW_BACKLIGHT` 时没有背光设备，亮度控件隐藏；用 xdotool 等自动化点击时需按住约 150 ms 再抬起，否则点击可能被丢掉。

| 方面 | 本工程 | 真机 |
|---|---|---|
| 源码 | 同一份 `lib/lvgl`、`generated/`、`custom/` | 同左 |
| 显示 | 内存帧缓冲（截图）或 SDL 窗口 | `/dev/fb0` |
| 输入 | 脚本化触摸或鼠标 | `/dev/input/event0` 触摸屏 |
| 背光 | 临时目录下的假 sysfs | `/sys/class/backlight` |
| `custom_init()`（mpv 等） | 不调用 | 调用 |

截图与预览只用于对齐布局和复现交互，权威验证仍是真机。

### preview 脚本

`tools/preview/` 中的脚本用来启动 `preview`、模拟点击、截图并核对界面，在 Cloud Agent 桌面（`DISPLAY=:1`）与无桌面环境中用法相同。先执行 `cmake --build build-tests --target preview`。

- 依赖：`xdotool`、ImageMagick（`import`、`convert`）；无桌面时还要 Xvfb 或 Xtigervnc。`imagemagick`、`xdotool`、`xvfb` 由 `.cursor/Dockerfile` 与 `.cursor/Dockerfile.luckfox_pico` 安装；Environment Build 早于此改动的环境与其他环境缺少时，脚本以退出码 2 结束并提示 `sudo apt-get install -y imagemagick xdotool xvfb`。
- 会话目录 `PREVIEW_WORK`（默认 `/tmp/luckfox-preview`）：`session` 记录显示号、分辨率、进程与窗口；`tmp/` 是 `preview` 的 `TMPDIR`（音乐目录、假背光在其中的 `luckfox-tests-*/`）；`wifi/` 是假 `wpa_cli` 的状态与调用记录 `cmd.log`；`shots/` 是截图；另有 `preview.log`、`xserver.log`。`start.sh` 会清空其中的 `tmp/`、`wifi/`，所以只使用不存在、为空或带标记文件 `.luckfox-preview-workdir`（普通文件，首次使用时创建）的目录，拒绝 `/` 与仓库内的路径（做法同 `tools/mutate.sh`）；没有标记的旧目录要先删除。构建目录用 `PREVIEW_BUILD` 覆盖（默认仓库根目录的 `build-tests`），`PREVIEW_BACKLIGHT` 原样传给 `preview`。
- 核对脚本的退出码：0 符合；1 不符，打印实际值与预期值，截图留在 `shots/`；2 环境错误（缺工具、`preview` 未运行等）。每步操作后反复截图，连续两次符合即继续，超时才判失败。

| 脚本 | 用法 | 作用 |
|---|---|---|
| `start.sh` | `start.sh [--headless] [480\|720] [歌曲数]` | 启动 `preview`，默认 480 屏、4 首。有 `DISPLAY` 且未加 `--headless` 时用当前显示；否则从 `:99` 起找空闲显示号，自起 Xvfb（没有时用 Xtigervnc，可用 `PREVIEW_XSERVER` 指定）。先结束上一个会话，等到主屏可以点击时返回 |
| `stop.sh` | `stop.sh` | 结束本会话的 `preview` 与自起的 X 服务器，保留日志与截图 |
| `songs.sh` | `songs.sh <歌曲数>` | 把音乐目录改成 N 首（长文件名、共同前缀），返回主屏再点 MUSIC 即刷新 |
| `wifi.sh` | `wifi.sh [<wpa_state>\|unreachable]` | 切换假 `wpa_cli` 的状态，WiFi 页最多 5 s 后生效；不带参数时打印当前状态与最近 5 次调用 |
| `shots.sh` | `shots.sh` | 截主屏、WiFi 页、点 Scan 后（先设为连不上，出现红色提示）与画板（轻点 3 下、拖一笔），文件名 `<分辨率>-shot-*.png` |
| `check-music-list.sh` | `check-music-list.sh` | 放 4 首，在音乐页依次展开列表、点背景、再展开、按按钮收起：展开时列表按钮中心为 `srgb(255,224,168)`、面板底边 480 屏 305 / 720 屏 458，收起时为 `srgb(182,176,187)`、没有面板 |
| `check-wifi.sh` | `check-wifi.sh` | 连不上时进 WiFi 页（2 s 内应调用 `wpa_cli status`），依次切到 `SCANNING`、`COMPLETED`、连不上：圆环区与图标区的颜色数依次为 1/1、>1/1、1/>1、1/1（1 表示没有内容） |
| `bin/wpa_cli` | 由 `start.sh` 放在 `PATH` 最前 | 假 `wpa_cli`：状态文件不存在时像连不上 wpa_supplicant 一样只写 stderr、以 255 退出；存在时按其中的 `wpa_state` 回答 `status`，`scan_results` 返回两个假热点 |

常用顺序：

```bash
cmake --build build-tests --target preview
tests/tools/preview/start.sh 720          # 桌面；无桌面加 --headless
tests/tools/preview/check-music-list.sh
tests/tools/preview/check-wifi.sh
tests/tools/preview/shots.sh
tests/tools/preview/stop.sh
```

核对脚本与 `shots.sh` 要求从主屏开始，结束时回到主屏，可以接连运行；手动点过窗口后，先回到主屏再运行。在桌面上运行时不要同时操作该窗口，鼠标点击会打乱核对。

**何时运行**：

- 没有板子时查看或操作界面：在桌面上 `start.sh`，用鼠标点；用 `songs.sh`、`wifi.sh` 准备音乐列表与 WiFi 状态。
- 改了主屏、音乐页、WiFi 页或画板相关的 `generated/`、`custom/`，或改了这些脚本：推送前在本地跑两个核对脚本，CI 也会再跑。
- 要给 PR 或审查配截图：`shots.sh`，也可以直接用核对脚本留在 `shots/` 中的截图。

两个核对脚本只覆盖音乐列表面板与 WiFi 圆环这两处界面，其他界面的改动要另写核对脚本或 ctest 用例。界面有意改变时（如调整布局、配色），按实测值更新脚本中的预期值与上表。

**修改或新增脚本**：

- 脚本开头 `. "$(dirname "$0")/lib.sh"`，并用其中的函数，不要自行假定显示号与临时目录：
  - `pv_load`：读入会话；
  - `pv_tap`、`pv_click_px`、`pv_drag`：按设计坐标点击、按窗口像素点击、按设计坐标拖动；
  - `pv_shot`、`pv_pixel`、`pv_colors`：截图、取像素、数颜色；
  - `pv_wait`、`pv_wait_page`：轮询等待，不用固定的 `sleep`。
- 坐标只写 480 设计值；预期值在 480、720 下分别实测。
- 改完先在本地跑 ShellCheck（与 CI 相同）：`shellcheck -x -P SCRIPTDIR tests/tools/preview/*.sh tests/tools/preview/bin/wpa_cli`，要求 0 条；再在桌面与 `--headless` 下各以 480、720 跑一遍。

**排查**：

- `start.sh` 最多等 30 s，等到 `preview` 的窗口与临时目录。超时会打印诊断（进程、同名窗口及其 PID、`preview.log` 末尾）并以 2 退出；成功时打印等了几秒。找窗口时排除启动前已有的同名窗口（如手动启动的 `preview`）；启动中途失败时会结束本次启动的 `preview` 与自起的 X 服务器，不留后台进程。
- 核对不符时，先看 `shots/` 中对应步骤的截图；`<分辨率>-page.png` 是判断当前页面时截的图。再看 `preview.log`，以及 `wifi/cmd.log` 中的 `wpa_cli` 调用。
- CI 失败时，同样的文件在 artifact `preview-shots` 中。

脚本只写 480 设计坐标，按 `分辨率 / 480` 截断取整换算，与 `custom/custom_main.c` 的 `luckfox_lv_obj_set_pos()`、`luckfox_lv_obj_set_size()` 相同；生成代码中用 `lv_obj_set_size()` 设置的尺寸不缩放，要单独处理。常用的点击位置（设计坐标）：

| 页面 | 控件 | 位置 |
|---|---|---|
| 主屏 | WIFI、PAD、MUSIC | (50, 410)、(145, 410)、(240, 410) |
| WiFi | Scan、Back | (65, 400)、(410, 400) |
| 画板 | Back | (384, 400) |
| 音乐 | 背景空白处 | (420, 160) |
| 音乐 | 列表按钮 | 左上角 (139, 310) 缩放后加 16（32×32 不缩放）：480 屏 (155, 326)，720 屏 (224, 481) |
| 音乐 | 返回 | 窗口内 (32, 32)，两种屏相同（64×64 不缩放） |

这些脚本不进 ctest。CI 的 `🧪 原生测试` job 在 480、720 下各以 `--headless` 运行一遍两个核对脚本与 `shots.sh`，失败时上传截图与日志（保留 7 天）；`🐚 ShellCheck` job 静态检查仓库内的全部 shell 脚本，要求 0 条。脚本只用于开发中快速核对，权威验证仍是真机。

## 改坏检验

`tests/tools/mutate.sh [id...]` 逐条应用 `cases/*/*.mutants` 中的改坏变体，确认对应测试失败（`KILLED`）后还原；它把 `custom/` 复制到工作目录 `$MUTATE_WORK/src`（默认 `/tmp/luckfox-mutate/src`）、其余目录链接回仓库，以 `-DLUCKFOX_ROOT` 配置测试工程，不改动工作区。

`*.mutants` 放在被测测试旁、命名 `<对象>.mutants`；每条记录由 `id`（只含字母、数字、`_`、`.`、`-`，且以字母或数字开头）、`file`（须在 `custom/` 下）、`old`（须恰好出现一次）、`new`、`tests`（ctest 正则）五行 `键: 值` 组成，记录间空一行，`#` 开头为注释；值去掉冒号后恰好一个空格，`\n` 表示换行，`new:` 后为空表示删除。

退出码：0 表示全部 `KILLED`；1 表示有变体 `SURVIVED`（测试没能发现这处改坏）；2 表示工具或清单错误，包括基线不绿、构建失败、`file` 不在 `custom/` 下或含 `..`、`old` 不是恰好匹配一次、清单解析失败（未知键、重复 id、记录内重复键也算，后者通常是漏写记录间空行）、传入的 id 不存在、一个变体都没跑。

基线通过后先打印 `baseline green: <用例数> tests; running <N> mutants`，之后每个变体一行，以 `[序号/总数] [id]` 开头显示进度，如 `[ 35/120] [MUSIC-N6-list-unsorted] KILLED by: music.list.sorted_by_name (Failed)`。

对应测试未注册的变体（如缺 `qemu-arm` 时的 32 位用例）标为 `SKIPPED`，不影响退出码，结尾会汇总 `N run, M skipped`。

CI 的 `🧬 变体检验` job 在每次 `pull_request` 推送与 `dev` push 时全量运行本脚本，退出码非 0 即失败；CI 镜像有 `qemu-arm` 与交叉 gcc，全部用例都会注册，所以出现 `SKIPPED`（`tests:` 正则没匹配到用例）也算失败。

工作目录由 `MUTATE_WORK` 指定，默认固定为 `/tmp/luckfox-mutate`，并发运行须用不同的 `MUTATE_WORK` 区分。脚本启动时会 `rm -rf` 其中的 `src`，所以拒绝 `/`、仓库内的路径与仓库位于其 `src/` 之内的目录，只使用不存在、为空或带标记文件 `.luckfox-mutate-workdir`（普通文件，脚本首次使用时创建）的目录，否则以退出码 2 结束；`custom/` 中有符号链接时同样拒绝，因为变体会经副本写回仓库。
