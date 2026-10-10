# Luckfox Pico LVGL Example — PR #8 真机遗留问题追溯与修复实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 确认 PR #8 真机验证遗留的 F1–F6（及复核遗留 R1–R3）是历史问题还是 PR #8 引入，然后按 spec §9 QA 定下的范围修复，每个修复配修复前失败的测试。

**Architecture:** 先在多个对比点编 uClibc 二进制交给作者板上对比（Task 0–1），再逐个问题 TDD 修复（Task 2 起，待 QA 后展开），最后跑无界面 `ctest`、带界面截图与 `preview`、两条交叉编译，并请作者真机复测。

**Tech Stack:** C（gnu99）、LVGL 8.3.10、lv_drivers 8.1、CMake、uClibc 工具链 `arm-rockchip830-linux-uclibcgnueabihf`、`qemu-arm`、gcc + ASan/UBSan、SDL2 + Xvfb。

**Spec:** [`docs/superpowers/specs/2026-10-10-lvgl-example-board-test-followups-design.md`](../specs/2026-10-10-lvgl-example-board-test-followups-design.md)

## 1. Global Constraints

- 分支 `pi/board-test-followups`，起点 `origin/dev` `13741e4`；草稿 PR。
- 提交身份 `Cursor Agent <cursoragent@cursor.com>`，SSH 签名；所有改写提交的 git 命令前加 `env -u GIT_AUTHOR_NAME -u GIT_AUTHOR_EMAIL -u GIT_COMMITTER_NAME -u GIT_COMMITTER_EMAIL`。
- 提交信息按 cz-conventional-emoji：`<type>(<scope>): <emoji> <subject>`，正文为要点列表。
- 先推送 spec 与 plan，实现完成后用 `git rebase` 整理提交，`docs(superpowers)` 放最后。
- 代码入库后精简本 plan，只保留未入库的内容（证据、偏离、板上记录）。

## 2. 修复 Task 通用步骤

每个 Task 按同一顺序（spec §5.3、PR #8 D1）：

1. 只加测试（含 `.mutants` 变体），在修复前运行，确认失败，失败输出记入 §4 验证证据。
2. 加修复，同一测试通过；`tests/tools/mutate.sh <id>...` 全部 `KILLED`。
3. 全量 `ctest --test-dir build-tests --output-on-failure -j8`（含 arm32）通过。
4. 提交：测试与修复同一个 `fix` 提交，标题带「页面 [n/N]」（N 在 rebase 时按最终数量校正）。

编号顺序（与提交顺序一致）：

| Task | 提交 | 问题 | 测试（`<类别>.<对象>.<用例>`） |
|---|---|---|---|
| 2 | WiFi [1/2] | F1、N1 | `wifi.scan_trigger.requests_then_reads`、`.repeat_single_read`、`.unreachable_hint` |
| 2b | WiFi [2/2] | N4 | `wifi.load.no_network_block` |
| 3 | 画板 [1/2] | F3 | `sketchpad.stroke.tap_draws_dot` |
| 4 | 画板 [2/2] | F2（含 R3 的 evdev 部分） | `sketchpad.touch.*`（5 条） |
| 5 | 音乐 [1/6] | F4 | `music.display_name.*`、`music.roller.long_names_fit` |
| 6 | 音乐 [2/6] | F5 初值 | `music.progress.initial_zero` |
| 7 | 音乐 [3/6] | F5 跨 `read` 丢行、R1 | `music.monitor.split_line`、`music.monitor.no_strtok` |
| 8 | 音乐 [4/6] | 跨线程调用 LVGL | `music.monitor.ui_thread_only` |
| 9 | 音乐 [5/6] | R2 | `music.monitor.peer_exit_reaps` |
| 10 | 音乐 [6/6] | F6 | `music.reenter.enable_recheck`、`music.reenter.rescan` |
| 11 | 全局 [1/1] | R3 | `music.player.socket_cloexec`（evdev 部分已随 Task 4） |
| 12 | `docs(readme)` ✅ `176d877` | N1 | 无（README 写明 `/etc/wpa_supplicant.conf` 前提；同步 `tests/README.md` 与 `AGENTS.md` 告警基线） |
| 13 | 收尾（`test(tests)` ✅ `b02beab`） | — | 全量 `ctest`、截图与 `preview`、两条交叉编译、`mutate.sh` 全量、逐提交测试、作者真机复测（spec §3.5，待执行） |

## 3. Tasks

### Task 0：编译对比点二进制

- [x] 稀疏检出 `yuangezhizao/luckfox-pico@dev` 的 `tools/linux/toolchain` 到 `/tmp/luckfox-pico`。
- [x] 用 `git worktree` 检出 `bf4fa1b`、`45732b6`、`fcc843a`、`6606bd2`、`a2fee6d`，各自 `env -u GLIBC_COMPILER LUCKFOX_SDK_PATH=/tmp/luckfox-pico cmake` + `make`，5 个都成功。
- [x] `qemu-arm` 冒烟 `bf4fa1b`、`a2fee6d`：动态库能加载，输出 `cannot open /dev/dri/card0`。
- [x] 编 `evdev_probe`（源码见附录 A），汇总 SHA256（spec §2.3）。
- [x] 按 Q1 交付到 `/cursor/stores/self/board-compare-20261010/`，`sha256sum -c SHA256SUMS` 全部 `OK`；另放 `board-compare-20261010.tar.gz`。
- [x] F1 解析差分（回应 Q4）：临时副本中分别链接 `45732b6` 与 `a2fee6d` 的 `custom_wifi.c`，5 份输入的结果见 spec §2.2 F1。

### Task 1：板上对比（作者执行）

- [x] 第 1 轮按 spec §3 执行（作者 + 本地 pi），结果回填 spec §2.2、§2.4。
- [ ] 第 2 轮 F2 多点触控验证（spec §3.4），待 QA Q12。

### Task 2：WiFi Scan 主动扫描（F1、N1）✅ `4efd251`

- 测试 `tests/cases/wifi/test_scan_trigger.c`：假 `wpa_cli` 对 `-i wlan0 scan` 输出 `scan_reply.txt`（默认 `OK`）。
  - `requests_then_reads`：按 Scan 后 `cmd.log` 立即有 `wpa_cli -i wlan0 scan`、还没有 `scan_results`，下拉框为 `scanning`；运行 3.1 s 后有 `scan_results`，下拉框是扫描结果。
  - `repeat_single_read`：1 s 内连按 3 次，最后一次之后 3.1 s 内 `scan_results` 只读 1 次。
  - `unreachable_hint`：`scan_reply.txt` 为 `Failed to connect to non-global ctrl_ifname: wlan0  error: No such file or directory`，提示标签可见且含 `wpa_supplicant not reachable`。
- 修复 `custom/custom_wifi.c`：Scan 回调 `popen("wpa_cli -i wlan0 scan")` 读输出；含 `OK` 或 `FAIL-BUSY` 时下拉框置 `scanning` 并启动或重置一个 3000 ms 一次性 `lv_timer`，到时调用 `_wifi_scanning_ssid()`；含 `Failed to connect`、`popen` 失败或其他输出时显示提示。`_wifi_scanning_ssid()` 读到 `Failed to connect` 也显示提示。
- 变体：删掉 `scan` 请求、定时器改为立即读、去掉重复按下时的重置、去掉提示。

### Task 2b：Load 前检查 `network={}` 块（N4）✅ `f745bd8`

- 测试 `wifi.load.no_network_block`：配置只有三行、输入合法，按 Load 后 `cmd.log` 与 `wpa_cli.stdin` 为空、配置不变、提示含 `No network block`。
- 修复：`wifi_conf_has_network()`，能读且没有块时提示并返回；读不了时保持旧行为。
- 变体：`WIFI-Q31-no-block-check`（`tests/cases/wifi/load.mutants`）。

### Task 3：画板轻点出点（F3）✅ `6372fd9`

- 测试 `sketchpad.stroke.tap_draws_dot`：按下、保持 100 ms、原地抬起，落点 3×3 内有笔迹色像素。
- 修复：`LV_EVENT_PRESSED` 时在落点用 `lv_canvas_draw_rect()` 画直径等于线宽的圆点并记为起点；`last_x`、`last_y` 都初始化为 `LAST_VALUE`。
- 变体：删掉画点调用。

### Task 4：画板多点触控（F2）✅ `5b89aba`

- 测试 `tests/cases/sketchpad/test_touch.c`（UNIT `unit_touch.c` 读 `touch_fd`）：经 FIFO 逐帧回放 `touch_step1.txt`、`touch_step2.txt`（板上 §2.5 第 1、2 步的原始事件），另有 `primary_lift_waits_all_up`、`single_touch_fallback`、`fd_cloexec`。
- 修复：新增 `custom/custom_touch.{c,h}`，`src/main.c` 用 `custom_touch_init(EVDEV_NAME)` 与 `custom_touch_read` 替换 `evdev_init()`、`evdev_read`。
- 变体：`TOUCH-newest-primary`、`-any-lift-releases`、`-no-wait-all-up`、`-release-at-emulated`、`-no-fallback`、`-no-cloexec`。

### Task 5：音乐列表显示名（F4）✅ `88154b7`

- 新增 `custom/` 函数 `music_display_name(name, out, size, font, letter_space, max_w)`：整名宽度不超过 `max_w` 时原样，否则按 UTF-8 字符从头尾交替保留，中间用 `...`，结果宽度不超过 `max_w`；`music_roller_apply(roller)` 用 roller `LV_PART_SELECTED` 的字体与内容宽度生成显示选项并设置，保持选中下标。`generated/setup_scr_Music_player.c` 在 roller 样式设置之后调用它；歌曲名标签改 `LV_LABEL_LONG_SCROLL_CIRCULAR`。
- 测试：`music.display_name.short_unchanged`、`.long_keeps_head_tail`、`.utf8_boundary`；`music.roller.long_names_fit`（3 个 68 字节、前 63 字节相同的文件名，各选项宽度不超过 roller 内容宽度且互不相同，480 与 720 都测）。
- 变体：不缩写、尾部保留 0 个字符。

### Task 6：进度初值（F5）✅ `fae733a`

- 测试 `music.progress.initial_zero`：`setup_scr_Music_player()` 后进度滑块值为 0。修复：生成代码初值 50 改为 0。生成代码不配变体（PR #8 spec §2.6）。

### Task 7：监听线程按行缓冲（F5、R1）✅ `817e662`

- 测试：`split_line`：一行 `property-change` 分两次写入（间隔 50 ms），UI 定时器运行后进度为该值；`no_strtok`：`-Wl,--wrap=strtok` 统计监听线程内的调用为 0。
- 修复：监听线程用累积缓冲区按 `\n` 切行（`memchr`），不再用 `strtok`；无换行且缓冲区满的超长行整行丢弃直到下一个 `\n`。
- 变体：每次 `read` 前清空累积缓冲区。

### Task 8：只在 UI 线程更新控件（跨线程）✅ `6a67d7f`

- 测试 `ui_thread_only`：写入完整一行后不运行 LVGL 定时器，等 100 ms 滑块值不变；运行 300 ms 后为新值。
- 修复：监听线程加锁写 `playback-time`、`duration` 与脏标志；`music_app_init()` 创建 200 ms 的 `lv_timer` 在 UI 线程读出并设置滑块（`slider_pressed` 时不设值）。
- 变体：定时器回调不设值。

### Task 9：mpv 退出后回收（R2）✅ `5c6a124`

- 测试 `peer_exit_reaps`：`pid` 指向测试 `fork` 的已退出子进程，对端关闭套接字后，监听线程 `waitpid` 回收（测试侧 `waitpid(pid, WNOHANG)` 得 `ECHILD`），`fd_mpv` 变为 -1，`mpv_send()` 返回 -1。
- 修复：`read` 返回 0 或不可恢复错误时，在锁内关闭 `fd_mpv` 并置 -1，`waitpid(pid, NULL, 0)`，线程退出；`mpv_send()` 在同一把锁内发送。
- 变体：删掉 `waitpid`。

### Task 10：运行中加入音乐（F6）✅ `b3e2c40`

- 测试：`enable_recheck`：启动时目录为空、`MUSIC_ENABLE=0`，放入文件后点 MUSIC 进入音乐页；`rescan`：进入后返回，再加一个文件，再进入时 roller 有 2 项，对端收到 `stop` 与新文件的 `loadfile`。
- 修复：`custom/` 新增 `music_recheck_enable()`（`MUSIC_ENABLE != 2` 时重新检查目录），`generated/events_init.c` 的 MUSIC 回调先调用它；`music_app_init()` 给音乐页注册 `LV_EVENT_SCREEN_LOAD_START`，重新扫描，列表变化时 `stop`、重新 `loadfile`、`playlist-pos 0`、暂停、进度归零、重建 roller。
- 变体：去掉重新检查、去掉列表比较后的重建。

### Task 11：文件描述符 CLOEXEC（R3）✅ `d02c5eb`

- 板上实际顺序：`custom_init()` 先启动 mpv，之后才打开 fb 与 evdev，所以 mpv 本身不继承它们；会继承的是之后 `popen`、`system()` 起的子进程（其中 `udhcpc` 常驻），继承 `fd_mpv`、`/dev/fb0`、`/dev/input/event0`。
- 修复：mpv 套接字 `SOCK_CLOEXEC`；`lib/lv_drivers` 的 `fbdev.c`、`evdev.c` 的 `open` 加 `O_CLOEXEC`。
- 测试：`music.player.socket_cloexec`（连上假 mpv 后 `fcntl(fd_mpv, F_GETFD)` 含 `FD_CLOEXEC`）；`main.evdev.cloexec`（`evdev_set_file(<临时文件>)` 后检查 `evdev_fd`）。fbdev 打开 `/dev/fb0` 后立即 ioctl，主机无法测，靠代码审查。
- 变体：去掉 `SOCK_CLOEXEC`（`lib/` 不能配变体，PR #8 spec §2.6）。

## 4. 验证证据

| 检查点 | 判据 | 结果 |
|---|---|---|
| 起点基线 `ctest` | 全过，含 arm32 | 2026-10-10，`13741e4`：78/78，arm32 2 条 |
| 第 1 轮板上对比 | F1–F6 来源 | F1、F3、F4、F6 确认历史问题；F5 两版均为 0，未复现；F2 单指三版均跟手，触摸范围 0–479、无 `ABS_MT_SLOT` 事件，最大帧间跳变 5 像素；`a2fee6d` 无 `cat: write error` |
| Task 2 修复前 | 3 条新用例失败 | `requests_then_reads`：`cmd.log` 只有 `scan_results`、无 `scan`；`repeat_single_read`：`scan` 0 次、`scan_results` 3 次；`unreachable_hint`：提示不可见 |
| Task 2 修复后 | 全过；变体全 `KILLED` | WiFi 26/26；`WIFI-F1-no-request`、`-read-now`、`-no-reset`、`-many-timers`、`WIFI-N1-no-hint` 5 个 `KILLED`；全量 81/81 |
| Task 3 修复前 | 新用例失败 | `tap_draws_dot`：`pen_near(240, 180)` 失败 |
| Task 3 修复后 | 全过；变体全 `KILLED` | 画板 5/5；`SKETCH-F3-no-dot`、`SKETCH-18`、`SKETCH-19` 3 个 `KILLED`；全量 82/82 |
| Task 4 修复前 | 回放复现板上现象 | 临时桩调用旧 `evdev_set_file` + `evdev_read`（不提交）：第 2 步 1 帧跳到 (388,207)、21 帧松开、最后 x=388；第 1 步 12 帧跳到第二根手指、5 帧松开；合成用例因旧驱动只认 `TRACKING_ID == 0` 而按不下；fd 无 `FD_CLOEXEC`；单点退回用例通过（旧行为本身支持单点） |
| Task 4 修复后 | 全过；变体全 `KILLED`；交叉编译通过 | 画板 10/10；6 个 `TOUCH-*` 变体 `KILLED`；全量 87/87；glibc rc=0 告警 120、uClibc rc=0 告警 119 |
| Task 5 修复前 | 新用例失败 | 临时桩（原样复制文件名，不提交）：`long_keeps_head_tail` 宽度超过 272 且无 `...`；`utf8_boundary` 31 处失败；`roller_fits_480/720` 宽度超过 270/406，歌曲名标签仍为 `LONG_WRAP` |
| Task 5 修复后 | 全过；变体全 `KILLED` | 92/92；`MUSIC-F4-*` 5 个与 PR #8 的 `MUSIC-2`、`MUSIC-4` `KILLED`；显示名如 `luckfox_rolle...lmnopq_01.mp3`（270 px） |
| Task 2b 修复前 | 新用例失败 | `no_network_block`：`cmd.log` 有 `wpa_cli`、`wpa_cli.stdin` 有 `set_network 0`，提示不可见 |
| Task 2b 修复后 | 全过；变体 `KILLED` | 105/105；`WIFI-Q31-no-block-check` `KILLED` |
| Task 6 修复前 / 后 | `initial_zero` | 修复前 got 50；修复后 93/93 |
| Task 7 修复前 | 新用例失败 | `split_line`：进度 got 0（拆开的 `playback-time` 丢失）；`no_strtok`：监听线程 `strtok` 3 次 |
| Task 7 修复后 | 全过；变体全 `KILLED` | 95/95；`MUSIC-5`、`MUSIC-6`、`MUSIC-F5-split-lost` `KILLED` |
| Task 8 修复前 | 新用例失败 | `ui_thread_only`：未运行 LVGL 定时器时 max 已为 200、值已为 12 |
| Task 8 修复后 | 全过；变体全 `KILLED` | 97/97；`MUSIC-UI-*` 4 个 `KILLED` |
| Task 9 修复前 | 新用例失败 | `peer_exit_reaps`：`fd_mpv` 仍为 4，测试侧 `waitpid` 收到子进程（未被回收） |
| Task 9 修复后 | 全过；变体全 `KILLED` | 98/98；`MUSIC-R2-*` 3 个 `KILLED` |
| Task 10 修复前 | 新用例失败 | `enable_recheck`：仍在主屏、`MUSIC_ENABLE` 为 0；`rescan`：再次进入 roller 仍 1 项，未发 `stop` 与 `loadfile`；`unchanged_keeps_playlist` 修复前即通过（守护修复后的行为） |
| Task 10 修复后 | 全过；音乐页变体全 `KILLED` | 101/101（Q28 后补 `remove_all_blocks_entry`、`remove_one_rescans`，并入音乐 [6/6]）；音乐页 35 个变体全 `KILLED`（含更新后的 `MUSIC-7`、`MUSIC-14`、`MUSIC-C2`） |
| Task 11 修复前 / 后 | `socket_cloexec` | 修复前 `FD_CLOEXEC` 未置位；修复后 102/102，`MUSIC-R3-no-cloexec` `KILLED` |
| Task 13 主机 | 全量 `ctest`、变体、截图、`preview` | 全新目录 `-DLUCKFOX_TESTS_ARM32=ON`：105/105（arm32 2 条）；`mutate.sh` 全量 93/93 `KILLED`（PR #8 的 62 个加本次 31 个），rc=0；`screenshots` 480、720 各 7 张，音乐列表显示缩写名、进度在最左、画板 3 个点加 1 条线；`preview` 在 Xvfb 下用 xdotool 操作：WiFi 页 Scan 显示 `wpa_supplicant not reachable`（本机无 `wpa_cli`），画板 3 次轻点出点、拖线落点与鼠标一致 |
| Task 13 交叉编译 | 两条 rc=0 | 干净构建 glibc 120、uClibc 119（各 +1，见偏离）；uClibc 产物 `qemu-arm` 冒烟能加载动态库；复测包 `/cursor/stores/self/board-retest-20261010/luckfox_lvgl_demo-pr10-d02c5eb`，SHA256 `b21a205ae8dd16faa4c6faebc14416ea6320435deff7cfa4cd43cc41e3abf916`（Q31 之后重编，替换先前的 `pr10-7fc0af3`） |
| Task 13 逐提交 | 每个提交单独构建并全量 `ctest` | 每个提交 `git archive` 到空目录、`-DLUCKFOX_TESTS_ARM32=ON` 构建并全量运行，全部 100% 通过：`4efd251` 81、`f745bd8` 82、`6372fd9` 83、`5b89aba` 88、`88154b7` 93、`fae733a` 94、`817e662` 96、`6a67d7f` 98、`5c6a124` 99、`b3e2c40` 104、`d02c5eb` 105、`176d877` 105、`b02beab` 105 |
| F1 解析差分 | 两版显示 `scanning` 的条件是否相同 | 相同：只有表头、`wpa_cli` 连不上、只有隐藏网络三种输入两版都是 `scanning`；有可见 SSID 时两版都有选项，旧版按空格截断并粘连 |

## 5. 与计划的偏离

- Task 3：线宽 2 的圆角 2×2 点被抗锯齿成灰色（`0x464646`），与线条的纯黑不一致；线宽不大于 2 时改画方点，大于 2 时才用 `LV_RADIUS_CIRCLE`。
- Task 3：触点换算抽成 `sketchpad_canvas_point()`，原有变体 `SKETCH-19` 的 `old` 由 `point.y -= coords.y1;` 改为 `point->y -= coords.y1;`。

- Task 4：多点模式下松开时若报告 `ABS_X/ABS_Y`，内核单点模拟会把它换到另一指，松开坐标落在另一指处可能误触按钮；改为单独保存主触点位置 `pen_x/pen_y`，并补断言与变体 `TOUCH-release-at-emulated`。
- Task 4：交叉编译告警各 +1，来自新编译单元 `custom_touch.c` 包含 `lvgl.h` 后重复一次既有的 `lib/lv_conf.h` `-Wundef`（`LV_USE_GPU_NXP_PXP`），不是新类别；收尾时统一更新 `AGENTS.md` 的基线。
- Task 4：R3 中 evdev 的 `O_CLOEXEC` 随新读取模块一并实现并测试，Task 11 只剩 mpv 套接字与 fbdev。

- Task 5：PR #8 的 `music.list.options_over_255`、`music.roller.select_long_name` 用 roller 显示文字比对完整文件名，改为按显示名开头辨认；完整名字仍由 `music_roller_options()` 与歌曲名标签校验。INFINITE 模式下 `lv_roller_get_options()` 返回重复多遍的串，新用例只看前 `option_cnt` 行。
- Task 5：`MUSIC-F4-no-tail` 原写法（`t = tail`）会让变体死循环、靠 120 s 超时判定，改为 `turn = 0`（只增长头部）；`utf8_boundary` 头尾改用字体中有字形的 3 字节符号（U+F001、U+F00C），否则头部截断点落不到多字节字符上，`MUSIC-F4-next-mid-char` 存活。
- Task 5：局部变量 `head`、`tail` 遮蔽了链表全局 `head`，交叉编译多出 1 条 `-Wshadow`，改名为 `keep_head`、`keep_tail`（fixup 并入）。
- Task 7：`"data": null` 仍按 0 处理，保持旧语义；解析抽成 `music_monitor_line()`，PR #8 的 `MUSIC-5` 的 `old` 随之更新。
- Task 9：`mpv_send()` 加锁后 PR #8 的 `MUSIC-7`、`MUSIC-14` 的 `old` 匹配不上，随该提交更新（fixup 并入）。
- Task 10：筛选规则抽成 `music_is_track()`，扫描循环只保留跳过时的打印；PR #8 的 `MUSIC-C2` 改为针对 `music_is_track()` 的换行检查（变体文件中 `\n` 会被解析为换行，`old` 取不含反斜杠的片段）。
- Task 11：板上 `custom_init()` 先启动 mpv，之后才打开 fb 与触摸设备，mpv 并不继承它们；PR #8 复核「mpv 继承 DRM、fb、evdev」的说法不成立，实际继承者是之后的 `popen`、`system` 子进程。
- Task 13：`git rebase -i --autosquash` 并入 fixup，`docs(superpowers)` 合并为最后一个提交；整理前后代码树一致。Q31 之后再次整理：WiFi [2/2] 挪到 WiFi [1/1] 之后，后者改名为 [1/2]，Q28 的两条测试并入音乐 [6/6]，README 改动并入 `docs(readme)`。

## 6. 后续事项

### 6.1 另开 docs PR：历史 spec 与 plan 标题补序号（spec Q25）

本 PR 起执行的标题编号规则（spec Q17–Q26，已写入作者个人 `~/.pi/agent/AGENTS.md`）：`## 2.`、`### 2.2`、`#### 2.2.1`，最多四级；plan 的 Task 标题保持 `### Task N：…`，Task 以外的小节不与 Task 混在同一章；附录写成 `## 附录 A`、`### A.1`。按该规则统计（2026-10-10，本 PR 两份文档除外）：

| 类别 | 文件数 | 标题总数 | 需要改 | 说明 |
|---|---|---|---|---|
| spec | 8 | 177 | 0 | 已是 `## 1.`、`### 2.1` |
| plan | 8 | 166 | 56 | 多为二级标题补序号，如 `## Global Constraints` → `## 1. Global Constraints` |

约 450 处 `§` 引用都指向 spec 章节号，spec 不改，plan 补的是原先没有的序号，所以引用不受影响。两个特例的处理在开 PR 时再与作者讨论：

- **特例 1**：`2026-10-01-lvgl-example-fix-existing-defects.md`（PR #8 的 plan）中，`### 主屏回归检查点`、`### WiFi 回归检查点`、`### 画板回归检查点`、`### 音乐页回归检查点`、`### 退出回归检查点` 5 个非 Task 小节位于各页面的 Task 章节（`## 主屏与时间（Task 1–5）` 等）之内，违反「不与 Task 混在同一章」。暂定就地编号（如 `### 6.1 主屏回归检查点`）并在 PR 中说明是例外；备选是挪到单独一章，但会让结构与当时的执行记录对不上。
- **特例 2**：`2026-10-06-lvgl-example-cloudagent-align-luckfox-pico.md`（PR #9 的 plan）的附录小节写作 `### A1`–`### A6`，按规则应改为 `### A.1`–`### A.6`；正文约 10 处「附录 A1」「附录 A2」等引用需同步改写。

### 6.2 Load 在没有 `network={}` 块时自动添加（spec Q31 C）

本 PR 只在没有块时提示（WiFi [2/2]）。完整支持需要：`wpa_cli add_network` 取得新 id 后对该 id `set_network`、`enable_network`、`save_config`，并在 `/etc/wpa_supplicant.conf` 末尾追加 `network={}` 块，而不是写死 `set_network 0`；同时处理配置中已有多个块时只改哪一个（PR #8 spec §1 已把「对所有 `network={}` 块一并改写」列为现状非目标）。属写配置的行为变更，另开 PR。

## 附录 A：`evdev_probe.c`

编译：`arm-rockchip830-linux-uclibcgnueabihf-gcc -O2 -Wall -Wextra -o evdev_probe evdev_probe.c`。

```c
/* evdev_probe：打印触摸设备的坐标范围与原始事件，用于定位画板落点问题。
 * 用法：evdev_probe [/dev/input/event0]，Ctrl-C 结束。 */
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#ifndef ABS_MT_SLOT
#define ABS_MT_SLOT 0x2f
#endif

static const char *abs_name(int c)
{
    switch (c) {
    case ABS_X: return "ABS_X";
    case ABS_Y: return "ABS_Y";
    case ABS_MT_SLOT: return "ABS_MT_SLOT";
    case ABS_MT_TOUCH_MAJOR: return "ABS_MT_TOUCH_MAJOR";
    case ABS_MT_WIDTH_MAJOR: return "ABS_MT_WIDTH_MAJOR";
    case ABS_MT_POSITION_X: return "ABS_MT_POSITION_X";
    case ABS_MT_POSITION_Y: return "ABS_MT_POSITION_Y";
    case ABS_MT_TRACKING_ID: return "ABS_MT_TRACKING_ID";
    default: return NULL;
    }
}

int main(int argc, char **argv)
{
    const char *dev = argc > 1 ? argv[1] : "/dev/input/event0";
    static const int codes[] = { ABS_X, ABS_Y, ABS_MT_SLOT, ABS_MT_POSITION_X, ABS_MT_POSITION_Y, ABS_MT_TRACKING_ID };
    struct input_event ev;
    char name[128] = "";
    int fd = open(dev, O_RDONLY);
    unsigned i;

    if (fd < 0) {
        fprintf(stderr, "open %s: %s\n", dev, strerror(errno));
        return 1;
    }
    ioctl(fd, EVIOCGNAME(sizeof(name)), name);
    printf("device %s name \"%s\"\n", dev, name);
    for (i = 0; i < sizeof(codes) / sizeof(codes[0]); i++) {
        struct input_absinfo a;
        if (ioctl(fd, EVIOCGABS(codes[i]), &a) == 0)
            printf("range %-20s min %d max %d fuzz %d flat %d res %d\n", abs_name(codes[i]), a.minimum, a.maximum, a.fuzz, a.flat, a.resolution);
        else
            printf("range %-20s (none)\n", abs_name(codes[i]));
    }
    fflush(stdout);
    while (read(fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
        const char *n;
        if (ev.type == EV_SYN) {
            printf("%ld.%06ld SYN\n", (long)ev.time.tv_sec, (long)ev.time.tv_usec);
        } else if (ev.type == EV_KEY) {
            printf("%ld.%06ld KEY code %d value %d%s\n", (long)ev.time.tv_sec, (long)ev.time.tv_usec, ev.code, ev.value, ev.code == BTN_TOUCH ? " (BTN_TOUCH)" : "");
        } else if (ev.type == EV_ABS) {
            n = abs_name(ev.code);
            if (n)
                printf("%ld.%06ld %s %d\n", (long)ev.time.tv_sec, (long)ev.time.tv_usec, n, ev.value);
            else
                printf("%ld.%06ld ABS code 0x%x %d\n", (long)ev.time.tv_sec, (long)ev.time.tv_usec, ev.code, ev.value);
        } else {
            printf("%ld.%06ld type %d code %d value %d\n", (long)ev.time.tv_sec, (long)ev.time.tv_usec, ev.type, ev.code, ev.value);
        }
        fflush(stdout);
    }
    return 0;
}
```
