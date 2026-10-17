# Luckfox Pico LVGL Example — PR #8 真机遗留问题追溯与修复实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 确认 PR #8 真机验证遗留的 F1–F6（及复核遗留 R1–R3）是历史问题还是 PR #8 引入，然后按 spec §9 QA 定下的范围修复，每个修复配修复前失败的测试。

**Architecture:** 先在多个对比点编 uClibc 二进制交给作者板上对比（Task 0–1），再逐个问题 TDD 修复（Task 2 起），最后跑无界面 `ctest`、带界面截图与 `preview`、两条交叉编译，并请作者真机复测（结果见 spec §3.5）。

**Tech Stack:** C（gnu99）、LVGL 8.3.10、lv_drivers 8.1、CMake、uClibc 工具链 `arm-rockchip830-linux-uclibcgnueabihf`、`qemu-arm`、gcc + ASan/UBSan、SDL2 + Xvfb 或云端桌面、xdotool、ImageMagick。

**Spec:** [`docs/superpowers/specs/2026-10-10-lvgl-example-board-test-followups-design.md`](../specs/2026-10-10-lvgl-example-board-test-followups-design.md)

## 1. Global Constraints

- 分支 `pi/board-test-followups`，起点 `origin/dev` `13741e4`；PR #10。
- 提交身份 `Cursor Agent <cursoragent@cursor.com>`，SSH 签名；所有改写提交的 git 命令前加 `env -u GIT_AUTHOR_NAME -u GIT_AUTHOR_EMAIL -u GIT_COMMITTER_NAME -u GIT_COMMITTER_EMAIL`。
- 提交信息按 cz-conventional-emoji：`<type>(<scope>): <emoji> <subject>`，正文为要点列表。
- 先推送 spec 与 plan，实现完成后用 `git rebase` 整理提交，`docs(superpowers)` 放最后。
- spec 与 plan 不写本 PR 提交的哈希，用提交标题称呼（如「音乐 [9/10]」）；上游、PR #8 等外部提交的哈希与二进制、压缩包的文件名照写。

## 2. 修复 Task 通用步骤

每个 Task 按同一顺序（spec §5.2、PR #8 D1）：

1. 只加测试（含 `.mutants` 变体），在修复前运行，确认失败，失败输出记入 §4 验证证据。
2. 加修复，同一测试通过；`tests/tools/mutate.sh <id>...` 全部 `KILLED`。
3. 全量 `ctest --test-dir build-tests --output-on-failure -j8`（含 arm32）通过。
4. 提交：测试与修复同一个 `fix` 提交，标题带「页面 [n/N]」（N 在 rebase 时按最终数量校正）。

编号顺序（与提交顺序一致）：

| Task | 提交 | 问题 | 测试（`<类别>.<对象>.<用例>`） |
|---|---|---|---|
| 2 | WiFi [1/2] | F1、N1 | `wifi.scan_trigger.requests_then_reads`、`.repeat_single_read`、`.unreachable_hint` |
| 3 | WiFi [2/2] | N4 | `wifi.load.no_network_block` |
| 4 | 画板 [1/2] | F3 | `sketchpad.stroke.tap_draws_dot` |
| 5 | 画板 [2/2] | F2（含 R3 的 evdev 部分） | `sketchpad.touch.*`（5 条） |
| 6 | 音乐 [1/10] | F4 | `music.display_name.*`、`music.roller.long_names_fit` |
| 7 | 音乐 [2/10] | F5 初值 | `music.progress.initial_zero` |
| 8 | 音乐 [3/10] | F5 跨 `read` 丢行、R1 | `music.monitor.split_line`、`music.monitor.no_strtok` |
| 9 | 音乐 [4/10] | 跨线程调用 LVGL | `music.monitor.ui_thread_only` |
| 10 | 音乐 [5/10] | R2 | `music.monitor.peer_exit_reaps` |
| 11 | 音乐 [6/10] | F6 | `music.reenter.enable_recheck`、`.rescan`、`.remove_all_blocks_entry`、`.remove_one_rescans` |
| 13 | 音乐 [7/10] | N5 | `music.roller.no_repeat_rows`、`music.roller.no_repeat_after_rescan` |
| 14 | 音乐 [8/10] | N6 | `music.list.sorted_by_name`、`music.reenter.sorted_keeps_playlist` |
| 15 | 音乐 [9/10] | N8 | `music.roller.fits_song_count`、`.fits_after_rescan`、`.fits_720` |
| 17 | 音乐 [10/10] | N11 | `music.list_btn.toggle`、`.background_close`、`.checked_image` |
| 12 | 全局 [1/2] | R3 | `music.player.socket_cloexec`（evdev 部分已随 Task 5） |
| 16 | 全局 [2/2] | N10 | `main.font.scale_720_18`、`main.font.scale_480_18` |
| — | `docs(readme)` | N1、N4 | 无（README 写明 `/etc/wpa_supplicant.conf` 前提与官方完整模板；同步 `tests/README.md` 与 `AGENTS.md` 告警基线） |
| — | 收尾（`test(tests)`） | — | `screenshots` 新增音乐页列表与画板场景；全量 `ctest`、截图与 `preview`、两条交叉编译、`mutate.sh` 全量、逐提交测试（仓库外的脚本见附录 C）；作者按 spec §3.5 真机复测与补测 |

「—」两行在改号前为 Task 12、13（文档与收尾），精简 plan 时删去了标题，结果见 §4。

## 3. Tasks

### Task 0：编译对比点二进制

- [x] 稀疏检出 `yuangezhizao/luckfox-pico@dev` 的 `tools/linux/toolchain` 到 `/tmp/luckfox-pico`。
- [x] 用 `git worktree` 检出 `bf4fa1b`、`45732b6`、`fcc843a`、`6606bd2`、`a2fee6d`，各自 `env -u GLIBC_COMPILER LUCKFOX_SDK_PATH=/tmp/luckfox-pico cmake` + `make`，5 个都成功。
- [x] `qemu-arm` 冒烟 `bf4fa1b`、`a2fee6d`：动态库能加载，输出 `cannot open /dev/dri/card0`。
- [x] 编 `evdev_probe`（源码见附录 A），汇总 SHA256（spec §2.3）。
- [x] 交付到 `/cursor/stores/self/board-compare-20261010/`，`sha256sum -c SHA256SUMS` 全部 `OK`；另放 `board-compare-20261010.tar.gz`。
- [x] F1 解析差分：临时副本中分别链接 `45732b6` 与 `a2fee6d` 的 `custom_wifi.c`，5 份输入的结果见 spec §2.2 F1。

### Task 1：板上对比（作者执行）

- [x] 第 1 轮按 spec §3 执行（作者 + 本地 pi），结果回填 spec §2.2、§2.4。
- [x] 第 2 轮 F2 多点触控验证（spec §3.4），结果回填 spec §2.5。

### Task 2：WiFi Scan 主动扫描（F1、N1）✅

- 测试 `tests/cases/wifi/test_scan_trigger.c`：假 `wpa_cli` 对 `-i wlan0 scan` 输出 `scan_reply.txt`（默认 `OK`）。
  - `requests_then_reads`：按 Scan 后 `cmd.log` 立即有 `wpa_cli -i wlan0 scan`、还没有 `scan_results`，下拉框为 `scanning`；运行 3.1 s 后有 `scan_results`，下拉框是扫描结果。
  - `repeat_single_read`：1 s 内连按 3 次，最后一次之后 3.1 s 内 `scan_results` 只读 1 次。
  - `unreachable_hint`：`scan_reply.txt` 为 `Failed to connect to non-global ctrl_ifname: wlan0  error: No such file or directory`，提示标签可见且含 `wpa_supplicant not reachable`。
- 修复 `custom/custom_wifi.c`：Scan 回调 `popen("wpa_cli -i wlan0 scan")` 读输出；含 `OK` 或 `FAIL-BUSY` 时下拉框置 `scanning` 并启动或重置一个 3000 ms 一次性 `lv_timer`，到时调用 `_wifi_scanning_ssid()`；含 `Failed to connect`、`popen` 失败或其他输出时显示提示。`_wifi_scanning_ssid()` 读到 `Failed to connect` 也显示提示。
- 变体：删掉 `scan` 请求、定时器改为立即读、去掉重复按下时的重置、去掉提示。

### Task 3：Load 前检查 `network={}` 块（N4）✅

- 测试 `wifi.load.no_network_block`：配置只有三行、输入合法，按 Load 后 `cmd.log` 与 `wpa_cli.stdin` 为空、配置不变、提示含 `No network block`。
- 修复：`wifi_conf_has_network()`，能读且没有块时提示并返回；读不了时保持旧行为。
- 变体：`WIFI-Q31-no-block-check`（`tests/cases/wifi/load.mutants`）。

### Task 4：画板轻点出点（F3）✅

- 测试 `sketchpad.stroke.tap_draws_dot`：按下、保持 100 ms、原地抬起，落点 3×3 内有笔迹色像素。
- 修复：`LV_EVENT_PRESSED` 时在落点用 `lv_canvas_draw_rect()` 画直径等于线宽的圆点并记为起点；`last_x`、`last_y` 都初始化为 `LAST_VALUE`。
- 变体：删掉画点调用。

### Task 5：画板多点触控（F2）✅

- 测试 `tests/cases/sketchpad/test_touch.c`（UNIT `unit_touch.c` 读 `touch_fd`）：经 FIFO 逐帧回放 `touch_step1.txt`、`touch_step2.txt`（板上 §2.5 第 1、2 步的原始事件），另有 `primary_lift_waits_all_up`、`single_touch_fallback`、`fd_cloexec`。
- 修复：新增 `custom/custom_touch.{c,h}`，`src/main.c` 用 `custom_touch_init(EVDEV_NAME)` 与 `custom_touch_read` 替换 `evdev_init()`、`evdev_read`。
- 变体：`TOUCH-newest-primary`、`-any-lift-releases`、`-no-wait-all-up`、`-release-at-emulated`、`-no-fallback`、`-no-cloexec`。

### Task 6：音乐列表显示名（F4）✅

- 新增 `custom/` 函数 `music_display_name(name, out, size, font, letter_space, max_w)`：整名宽度不超过 `max_w` 时原样，否则按 UTF-8 字符从头尾交替保留，中间用 `...`，结果宽度不超过 `max_w`；`music_roller_apply(roller)` 用 roller `LV_PART_SELECTED` 的字体与内容宽度生成显示选项并设置，保持选中下标。`generated/setup_scr_Music_player.c` 在 roller 样式设置之后调用它；歌曲名标签改 `LV_LABEL_LONG_SCROLL_CIRCULAR`。
- 测试：`music.display_name.short_unchanged`、`.long_keeps_head_tail`、`.utf8_boundary`；`music.roller.long_names_fit`（3 个 68 字节、前 63 字节相同的文件名，各选项宽度不超过 roller 内容宽度且互不相同，480 与 720 都测）。
- 变体：不缩写、尾部保留 0 个字符。

### Task 7：进度初值（F5）✅

- 测试 `music.progress.initial_zero`：`setup_scr_Music_player()` 后进度滑块值为 0。修复：生成代码初值 50 改为 0。生成代码不配变体（PR #8 spec §2.6）。

### Task 8：监听线程按行缓冲（F5、R1）✅

- 测试：`split_line`：一行 `property-change` 分两次写入（间隔 50 ms），UI 定时器运行后进度为该值；`no_strtok`：`-Wl,--wrap=strtok` 统计监听线程内的调用为 0。
- 修复：监听线程用累积缓冲区按 `\n` 切行（`memchr`），不再用 `strtok`；无换行且缓冲区满的超长行整行丢弃直到下一个 `\n`。
- 变体：每次 `read` 前清空累积缓冲区。

### Task 9：只在 UI 线程更新控件（跨线程）✅

- 测试 `ui_thread_only`：写入完整一行后不运行 LVGL 定时器，等 100 ms 滑块值不变；运行 300 ms 后为新值。
- 修复：监听线程加锁写 `playback-time`、`duration` 与脏标志；`music_app_init()` 创建 200 ms 的 `lv_timer` 在 UI 线程读出并设置滑块（`slider_pressed` 时不设值）。
- 变体：定时器回调不设值。

### Task 10：mpv 退出后回收（R2）✅

- 测试 `peer_exit_reaps`：`pid` 指向测试 `fork` 的已退出子进程，对端关闭套接字后，监听线程 `waitpid` 回收（测试侧 `waitpid(pid, WNOHANG)` 得 `ECHILD`），`fd_mpv` 变为 -1，`mpv_send()` 返回 -1。
- 修复：`read` 返回 0 或不可恢复错误时，在锁内关闭 `fd_mpv` 并置 -1，`waitpid(pid, NULL, 0)`，线程退出；`mpv_send()` 在同一把锁内发送。
- 变体：删掉 `waitpid`。

### Task 11：运行中加入音乐（F6）✅

- 测试：`enable_recheck`：启动时目录为空、`MUSIC_ENABLE=0`，放入文件后点 MUSIC 进入音乐页；`rescan`：进入后返回，再加一个文件，再进入时 roller 有 2 项，对端收到 `stop` 与新文件的 `loadfile`；`remove_all_blocks_entry`：删光后点 MUSIC 不进入；`remove_one_rescans`：删掉 1 首后再进入列表少 1 项。
- 修复：`custom/` 新增 `music_recheck_enable()`（`MUSIC_ENABLE != 2` 时重新检查目录），`generated/events_init.c` 的 MUSIC 回调先调用它；`music_app_init()` 给音乐页注册 `LV_EVENT_SCREEN_LOAD_START`，重新扫描，列表变化时 `stop`、重新 `loadfile`、`playlist-pos 0`、暂停、进度归零、重建 roller。
- 变体：去掉重新检查、去掉列表比较后的重建。

### Task 12：文件描述符 CLOEXEC（R3）✅

- 板上实际顺序：`custom_init()` 先启动 mpv，之后才打开 fb 与 evdev，所以 mpv 本身不继承它们；会继承的是之后 `popen`、`system()` 起的子进程（其中 `udhcpc` 常驻），继承 `fd_mpv`、`/dev/fb0`、`/dev/input/event0`。
- 修复：mpv 套接字 `SOCK_CLOEXEC`；`lib/lv_drivers/display/fbdev.c` 的 `open` 加 `O_CLOEXEC`。触摸设备已由 Task 5 的 `custom_touch.c` 以 `O_CLOEXEC` 打开（`sketchpad.touch.fd_cloexec`），`lib/lv_drivers/indev/evdev.c` 不再使用，不改。
- 测试：`music.player.socket_cloexec`（连上假 mpv 后 `fcntl(fd_mpv, F_GETFD)` 含 `FD_CLOEXEC`）。fbdev 打开 `/dev/fb0` 后立即 ioctl，主机无法测，靠代码审查与板上复测 R3。
- 变体：去掉 `SOCK_CLOEXEC`（`lib/` 不能配变体，PR #8 spec §2.6）。

### Task 13：歌曲列表不再循环重复（N5，spec Q32）✅

- 现状：`generated/setup_scr_Music_player.c`、`music_roller_apply()`（`custom/custom_musicplayer.c`）、`music_refresh_list()` 三处 `lv_roller_set_options(..., LV_ROLLER_MODE_INFINITE)`；LVGL 8.3 循环模式把选项复制 `LV_ROLLER_INF_PAGES`（7）份，可见 6 行，1 首也显示多行。
- 测试：`music.roller.no_repeat_rows`：空目录、1 首、4 首时 roller 实际显示的行数（`lv_roller_get_options()` 的行数）等于歌曲数，选中末项后按 `LV_KEY_DOWN` 仍停在末项；`music.roller.no_repeat_after_rescan`：运行中加歌重新进入后同样成立，删光后直接加载音乐页（走 `music_refresh_list()` 的空列表分支）也只有 1 个空行。
- 修复：三处改为 `LV_ROLLER_MODE_NORMAL`；`music_roller_apply()` 用 `lv_roller_get_selected()` 取真实下标，不依赖循环模式的页偏移，无需改动。
- 变体：`MUSIC-N5-apply-infinite`、`MUSIC-N5-refresh-infinite`；`generated/` 那一处 `mutate.sh` 不支持（见 §5），改为手动检验。
- 截图：`screenshots` 的音乐列表场景每首一行；`preview` 看 1 首与运行中补到 4 首时的列表。

### Task 14：列表按文件名排序（N6，spec Q33）✅

- 现状：`music_scan_list()` 按 `readdir` 顺序建链表、发 `loadfile`，`music_dir_signature()` 也按 `readdir` 顺序拼签名；`music_refresh_list()` 用签名与 `music_roller_options()` 比较判断目录是否变化。
- 测试：`music.list.sorted_by_name`：按 03、01、04、02 的顺序创建文件，列表与 `loadfile` 顺序都是 01、02、03、04（主机 `readdir` 顺序不受控，断言只看结果有序）；`music.reenter.sorted_keeps_playlist`：目录未变时再进入不发 `stop`（签名与列表同序，否则每次都会重建并打断播放）。
- 修复：`insert_music_node()` 按 `strcmp` 升序插入；`music_scan_list()` 建完链表后再按顺序编号、`loadfile`；`music_dir_signature()` 用 `music_sig_insert()` 按同一规则插入拼接。不新增内存分配（见 §5）。
- 变体：`MUSIC-N6-list-unsorted`、`MUSIC-N6-head-not-updated`、`MUSIC-N6-stale-id`、`MUSIC-N6-sig-unsorted`（后者由 `sorted_keeps_playlist` 杀死）。

### Task 15：歌曲列表按歌曲数收缩、从列表按钮弹出（N8，spec Q38）✅

- 现状：生成代码把可见行写死为 6、外层面板 `Music_player_cont_1` 写死为 (−1.5, 92)、313×221，roller（宽 272）在面板中居中；面板底边 313 压住列表按钮（(139, 310)、32×32）顶边 3 px，水平中心对准按钮。roller 总把选中项放在正中，6 行时上下各 2.5 行，边缘那行只显示一半。
- 测试 `check_fit_at(songs, rows, margin, bottom)`：行数为 2n−1、最多 5；roller 高度按 `lv_roller_set_visible_row_count()` 的公式计算，1 行时放得下选中项字体；面板宽高为 roller 宽高加两倍边距，roller 在面板内左、上边距都等于 `margin`；面板底边等于 `bottom`，水平中心与按钮中心相差不超过 1 px。
  - `music.roller.fits_song_count`：480 屏 1、2、4、8 首为 1、3、5、5 行，边距 10、底边 305（按钮顶边 310 上方 5 px）。
  - `music.roller.fits_after_rescan`：运行中从 1 首补到 4 首再进入，`music_refresh_list()` 重建后同样成立。
  - `music.roller.fits_720`：720 屏 4 首为 5 行，边距 15、底边 458（按钮顶边 465 上方 7 px）。
- 修复：`music_roller_apply()` 末尾调用新增的 `music_list_fit()`：可见行 2n−1、最多 `MUSIC_LIST_MAX_ROWS`（5）；面板尺寸为 roller 尺寸加两倍 `MUSIC_LIST_MARGIN`（10 × `SCALE`）；`lv_obj_align_to(cont, 列表按钮, LV_ALIGN_OUT_TOP_MID, 0, -MUSIC_LIST_BTN_GAP × SCALE)`（5 px 间隙）。
- 变体：`MUSIC-N8-no-fit`、`-even-rows`、`-no-cap`、`-cont-fixed`、`-margin-design`、`-margin-unscaled`、`-width-fixed`、`-top-anchored`、`-no-gap`、`-gap-unscaled`、`-gap-sign`、`-align-left`。

### Task 16：720 屏 27 号字改用 24 号（N10，spec Q45）✅

- 现状：`luckfox_lv_obj_set_style_text_font()` 与 `luckfox_lv_style_set_text_font()` 按「行高 × SCALE」查表，表里只有 12、16、18、20、24、30、42、63；720 屏 18 号换算为 27，不设置字体，音乐列表选中项退回普通项的 18 号。720 屏用到 18 号字的只有这一处。
- 测试：`main.font.scale_720_18`：720 屏两个函数传入 18 号都得到 24 号；`main.font.scale_480_18`：480 屏仍为 18 号。
- 修复：两个函数的 `switch` 都加 `case 27:`，与 `case 24:` 共用 24 号字。
- 变体：`MAIN-N10-obj-no-27`、`MAIN-N10-style-no-27`。

### Task 17：列表按钮再点一次收起，展开时高亮（N11，spec Q50）✅

- 现状：`generated/events_init.c` 的列表按钮回调只清除外层面板的 `HIDDEN`，点音乐页背景才设置 `HIDDEN`；按钮设了 `LV_OBJ_FLAG_CHECKABLE`，松开时自行切换 CHECKED，点背景收起时不清除；4 种状态用同一张图 `_icn_chart_alpha_32x32`（全不透明，白底画在图里）。
- 测试（`tests/cases/music/test_list_btn.c`，点按钮时先发 `LV_EVENT_RELEASED` 再发 `LV_EVENT_CLICKED`，与板上一致）：
  - `toggle`：连点 3 次，依次展开、收起、展开，按钮 CHECKED 与面板是否显示一致。
  - `background_close`：展开后对音乐页发 `LV_EVENT_CLICKED`，面板收起且按钮退出 CHECKED；再点按钮展开且 CHECKED。
  - `checked_image`：按钮选中（含选中按下）状态的图不同于普通状态；与原图同尺寸、同格式；原图白色像素不变，最深的像素为 `#FFA300`，所有像素红色为 255、alpha 不变。图片指针从按钮取，修复前也能编译、运行。
- 修复：
  - `custom/_icn_chart_checked_alpha_32x32.c`：附录 B 的脚本从原图生成。
  - `custom_musicplayer.c`：`music_list_set_open()` 设置面板是否隐藏，并用 `lv_imgbtn_set_state()` 设置按钮状态；`music_list_toggle()`、`music_list_close()` 经过它；`music_list_btn_init()` 在 `music_app_init()` 中给选中、选中按下两种状态设橙色线条图。
  - `custom.h` 声明 `music_list_toggle()`、`music_list_close()`；生成代码的列表按钮回调调用前者，音乐页背景回调调用后者。
- 变体：`MUSIC-N11-show-only`、`-hide-only`、`-state-not-synced`、`-state-inverted`、`-no-checked-src`、`-no-checked-pressed-src`；生成代码的两处调用手动检验。

## 4. 验证证据

| 检查点 | 判据 | 结果 |
|---|---|---|
| 起点基线 `ctest` | 全过，含 arm32 | 2026-10-10，`13741e4`：78/78，arm32 2 条 |
| 第 1 轮板上对比 | F1–F6 来源 | F1、F3、F4、F6 确认历史问题；F5 两版均为 0，未复现；F2 单指三版均跟手，触摸范围 0–479、无 `ABS_MT_SLOT` 事件，最大帧间跳变 5 像素；`a2fee6d` 无 `cat: write error` |
| Task 2 修复前 | 3 条新用例失败 | `requests_then_reads`：`cmd.log` 只有 `scan_results`、无 `scan`；`repeat_single_read`：`scan` 0 次、`scan_results` 3 次；`unreachable_hint`：提示不可见 |
| Task 2 修复后 | 全过；变体全 `KILLED` | WiFi 26/26；`WIFI-F1-no-request`、`-read-now`、`-no-reset`、`-many-timers`、`WIFI-N1-no-hint` 5 个 `KILLED`；全量 81/81 |
| Task 4 修复前 | 新用例失败 | `tap_draws_dot`：`pen_near(240, 180)` 失败 |
| Task 4 修复后 | 全过；变体全 `KILLED` | 画板 5/5；`SKETCH-F3-no-dot`、`SKETCH-18`、`SKETCH-19` 3 个 `KILLED`；全量 82/82 |
| Task 5 修复前 | 回放复现板上现象 | 临时桩调用旧 `evdev_set_file` + `evdev_read`（不提交）：第 2 步 1 帧跳到 (388,207)、21 帧松开、最后 x=388；第 1 步 12 帧跳到第二根手指、5 帧松开；合成用例因旧驱动只认 `TRACKING_ID == 0` 而按不下；fd 无 `FD_CLOEXEC`；单点退回用例通过（旧行为本身支持单点） |
| Task 5 修复后 | 全过；变体全 `KILLED`；交叉编译通过 | 画板 10/10；6 个 `TOUCH-*` 变体 `KILLED`；全量 87/87；glibc rc=0 告警 120、uClibc rc=0 告警 119 |
| Task 6 修复前 | 新用例失败 | 临时桩（原样复制文件名，不提交）：`long_keeps_head_tail` 宽度超过 272 且无 `...`；`utf8_boundary` 31 处失败；`roller_fits_480/720` 宽度超过 270/406，歌曲名标签仍为 `LONG_WRAP` |
| Task 6 修复后 | 全过；变体全 `KILLED` | 92/92；`MUSIC-F4-*` 5 个与 PR #8 的 `MUSIC-2`、`MUSIC-4` `KILLED`；显示名如 `luckfox_rolle...lmnopq_01.mp3`（270 px） |
| Task 3 修复前 | 新用例失败 | `no_network_block`：`cmd.log` 有 `wpa_cli`、`wpa_cli.stdin` 有 `set_network 0`，提示不可见 |
| Task 3 修复后 | 全过；变体 `KILLED` | 105/105；`WIFI-Q31-no-block-check` `KILLED` |
| Task 7 修复前 / 后 | `initial_zero` | 修复前 got 50；修复后 93/93 |
| Task 8 修复前 | 新用例失败 | `split_line`：进度 got 0（拆开的 `playback-time` 丢失）；`no_strtok`：监听线程 `strtok` 3 次 |
| Task 8 修复后 | 全过；变体全 `KILLED` | 95/95；`MUSIC-5`、`MUSIC-6`、`MUSIC-F5-split-lost` `KILLED` |
| Task 9 修复前 | 新用例失败 | `ui_thread_only`：未运行 LVGL 定时器时 max 已为 200、值已为 12 |
| Task 9 修复后 | 全过；变体全 `KILLED` | 97/97；`MUSIC-UI-*` 4 个 `KILLED` |
| Task 10 修复前 | 新用例失败 | `peer_exit_reaps`：`fd_mpv` 仍为 4，测试侧 `waitpid` 收到子进程（未被回收） |
| Task 10 修复后 | 全过；变体全 `KILLED` | 98/98；`MUSIC-R2-*` 3 个 `KILLED` |
| Task 11 修复前 | 新用例失败 | `enable_recheck`：仍在主屏、`MUSIC_ENABLE` 为 0；`rescan`：再次进入 roller 仍 1 项，未发 `stop` 与 `loadfile`；`unchanged_keeps_playlist` 修复前即通过（守护修复后的行为） |
| Task 11 修复后 | 全过；音乐页变体全 `KILLED` | 101/101（含 `remove_all_blocks_entry`、`remove_one_rescans`）；音乐页 35 个变体全 `KILLED`（含更新后的 `MUSIC-7`、`MUSIC-14`、`MUSIC-C2`） |
| Task 12 修复前 / 后 | `socket_cloexec` | 修复前 `FD_CLOEXEC` 未置位；修复后 102/102，`MUSIC-R3-no-cloexec` `KILLED` |
| 真机复测（spec §3.5.1） | 12 项 | 全部通过；新发现 N5、N6、N7 |
| Task 13 修复前 | 新用例失败 | `no_repeat_rows`：显示行数空目录 7、1 首 7、4 首 28（应为 1、1、4），4 首时选中末项按下后绕回 0；`no_repeat_after_rescan`：1 首 7 行、2 首 14 行，末项按下绕回 0 |
| Task 13 修复后 | 全过；变体全 `KILLED` | 107/107；`MUSIC-N5-apply-infinite`、`MUSIC-N5-refresh-infinite` `KILLED`；手动把 `generated/` 那一处改回 `INFINITE`，`no_repeat_rows` 失败（空目录 7 行），还原后通过；`screenshots` 音乐列表每首一行 |
| Task 14 修复前 | 新用例失败 | `sorted_by_name`：主机 `readdir` 第一项为 `song_04.mp3`，`music_roller_options()` 不是升序，`loadfile` 顺序断言失败；`sorted_keeps_playlist` 修复前即通过（守护签名与列表同序） |
| Task 14 修复后 | 全过；变体全 `KILLED` | 109/109；`MUSIC-N6-*` 4 个 `KILLED`；音乐页 42 个变体全 `KILLED` |
| 音乐列表补测（spec §3.5.2） | 3 项 | 全部通过；新发现 N8、N10 |
| N10 来源 | 是否上游即有 | `origin/dev` 临时工作树中给截图工具加音乐列表场景（不提交）：480 屏选中项行高 18、普通项 12；720 屏两者同为 18，且是同一个字体对象；本 PR 未改字体换算与 roller 字体设置 |
| Task 15 修复前 | 新用例失败 | `fits_song_count`：roller 高度恒为 170（6 行），面板恒为 313×221；`fits_after_rescan`：1 首与补到 4 首后同样不变 |
| Task 15 修复后 | 全过；变体全 `KILLED` | `MUSIC-N8-*` 12 个 `KILLED`（`-margin-unscaled`、`-gap-unscaled` 只由 `fits_720` 杀死）；`preview` 截 480、720 两种分辨率下 1、2、4、8 首：1、3、5、5 行、没有半行，面板水平居中，四周边距 10 px（720 屏 15 px），底边 480 屏 y=305、720 屏 y=458，与按钮顶边留 5 px（7 px）间隙；实测 1 行高 480 屏 30 px、720 屏 36 px |
| Task 16 修复前 | 新用例失败 | `scale_720_18`：两个换算函数得到的都不是 24 号（第 25、28 行断言失败，`switch` 没有 27 号分支，不设置字体）；`scale_480_18` 通过 |
| Task 16 修复后 | 全过；变体全 `KILLED` | 113/113；`MAIN-N10-obj-no-27`、`MAIN-N10-style-no-27` `KILLED`；`screenshots` 720 屏音乐列表选中项为 24 号，缩写名放得下 |
| `wifi.scan_trigger` 负载压测 | 高负载下稳定 | 12 个空转进程占满 8 核：旧写法 5 轮中 3 轮失败（`repeat_single_read` 在 t≈3.2 s 已读到 `scan_results`）；改为按 LVGL tick 等待后，两条用例各重复 20 次全部通过；新写法在 `origin/dev` 的 `custom_wifi.c` 上 3 条用例全部失败，与 Task 2 修复前一致 |
| 列表收缩补测（spec §3.5.3） | 3 项 | 全部通过；新发现外层面板边距与位置问题、N11 |
| 外层面板与按钮补测（spec §3.5.4） | 3 项 | 全部通过；新发现面板与按钮太近、展开时按钮不变色 |
| 背景收起（主机） | 点音乐页背景能否收起 | `preview` 中展开列表后点击背景空白处 (420, 160)，外层面板收起 |
| Task 17 修复前 | 新用例失败 | `background_close`：点背景收起后按钮仍为 CHECKED，再点按钮展开时反而退出 CHECKED；`checked_image`：选中状态与普通状态是同一张图；`toggle` 修复前即通过（CHECKABLE 自行切换恰好一致），守护切换 |
| Task 17 修复后 | 全过；变体全 `KILLED` | 117/117；`MUSIC-N11-*` 6 个 `KILLED`；把 `generated/events_init.c` 的背景回调改回直接隐藏面板，`background_close` 失败，还原后通过；`preview` 在 480、720 屏展开时按钮线条为橙色，点背景或按按钮收起后恢复灰色 |
| F1 解析差分 | 两版显示 `scanning` 的条件是否相同 | 相同：只有表头、`wpa_cli` 连不上、只有隐藏网络三种输入两版都是 `scanning`；有可见 SSID 时两版都有选项，旧版按空格截断并粘连 |
| 补测二进制 | uClibc 产物，`qemu-arm` 冒烟停在 `cannot open /dev/dri/card0` | 均在 `/cursor/stores/self/board-retest-20261010/`，同目录 `SHA256SUMS`：复测用 `luckfox_lvgl_demo-pr10-7fc0af3`（未含 WiFi [2/2]）与 `-pr10-d02c5eb`（`b21a205ae8dd16faa4c6faebc14416ea6320435deff7cfa4cd43cc41e3abf916`）；音乐列表补测用 `-pr10-8ef1654`（`25db53aab7b1b3d346faaa584899a9a71f01f662e62b5333794cdac6a217b516`）；列表收缩补测用 `-pr10-08984b7`（`ce1c36fc86e8084ecdeea9e28b2bfe57222a1d6fa9ca570e6b189721261ef1a2`）；外层面板与按钮补测用 `-pr10-9b40919`（`bd6a26ea03fdb9ced5c140c2c6d1eec27a87b2781096b2d6bd384e113e7e1680`）；spec §3.5.5 用 `-pr10-1b4dbbd`（`b6e9c73c1ef4bc6490b48ef04de08ee631ab34018053ecc7ab54b42501ed3663`） |
| 收尾 主机 | 全量 `ctest`、变体 | 117/117（arm32 2 条）；`mutate.sh` 全量 119/119 `KILLED`（PR #8 的 62 个加本 PR 57 个），rc=0；`preview` 在 480、720 屏核对面板底边 y=305、y=458，展开时按钮为橙色，点背景或按按钮收起后为灰色 |
| 收尾 交叉编译 | 两条 rc=0 | glibc 121、uClibc 120（与 `AGENTS.md` 基线相同）；uClibc 产物 `qemu-arm` 冒烟能加载动态库 |
| 收尾 逐提交 | 每个提交单独构建并全量 `ctest` | 19 个提交逐个 `git archive` 到空目录、`-DLUCKFOX_TESTS_ARM32=ON` 构建并全量运行，全部 100% 通过：WiFi [1/2] 81、WiFi [2/2] 82、画板 [1/2] 83、画板 [2/2] 88、音乐 [1/10] 93、音乐 [2/10] 94、音乐 [3/10] 96、音乐 [4/10] 98、音乐 [5/10] 99、音乐 [6/10] 104、音乐 [7/10] 106、音乐 [8/10] 108、音乐 [9/10] 111、音乐 [10/10] 114、全局 [1/2] 115、全局 [2/2] 117、`docs(readme)` 117、`test(tests)` 117、`docs(superpowers)` 117；19 个提交均为 Cursor Agent 且带 SSH 签名；全量检验与逐提交测试先后执行 |
| 面板间隙与按钮高亮补测（spec §3.5.5） | 2 项 | 全部通过 |
| 合并前代码审查（§7） | 无 Critical、Important，CI 通过 | 满足合并门槛；1 条 Minor（触摸读取不处理 `SYN_DROPPED`）记入 §6.7 |

## 5. 与计划的偏离

- Task 2：高负载（多个构建并行）下 `repeat_single_read` 偶发失败。`tst_run_ms()`（PR #7）按循环次数计时，而 LVGL tick 取自 `CLOCK_MONOTONIC`，负载高时实际经过时间比名义长，3 秒定时器在检查点之前到期。本 PR 新增的 `test_scan_trigger.c` 改为按 LVGL tick 等待（`run_until()`），`tst_run_ms()` 不改；全量检验与逐提交测试先后执行，不并行。
- Task 4：线宽 2 的圆角 2×2 点被抗锯齿成灰色（`0x464646`），与线条的纯黑不一致；线宽不大于 2 时改画方点，大于 2 时才用 `LV_RADIUS_CIRCLE`。
- Task 4：触点换算抽成 `sketchpad_canvas_point()`，原有变体 `SKETCH-19` 的 `old` 由 `point.y -= coords.y1;` 改为 `point->y -= coords.y1;`。
- Task 5：多点模式下松开时若报告 `ABS_X/ABS_Y`，内核单点模拟会把它换到另一指，松开坐标落在另一指处可能误触按钮；改为单独保存主触点位置 `pen_x/pen_y`，并补断言与变体 `TOUCH-release-at-emulated`。
- Task 5、17：新编译单元 `custom_touch.c`、`_icn_chart_checked_alpha_32x32.c` 包含 `lvgl.h` 后各重复一次既有的 `lib/lv_conf.h` `-Wundef`（`LV_USE_GPU_NXP_PXP`），交叉编译告警各 +2，基线变为 glibc 121、uClibc 120，不是新类别；`AGENTS.md` 的基线随 `docs(readme)` 更新。新文件要放在 `custom/` 顶层：主工程 `CMakeLists.txt` 只收集 `custom/*.c`，不递归子目录。
- Task 5：R3 中 evdev 的 `O_CLOEXEC` 随新读取模块一并实现并测试，Task 12 只剩 mpv 套接字与 fbdev。
- Task 6：PR #8 的 `music.list.options_over_255`、`music.roller.select_long_name` 用 roller 显示文字比对完整文件名，改为按显示名开头辨认；完整名字仍由 `music_roller_options()` 与歌曲名标签校验。
- Task 6：变体 `MUSIC-F4-no-tail` 写成 `t = tail` 会让被测代码死循环、靠 120 s 超时判定，改为 `turn = 0`（只增长头部）；`utf8_boundary` 头尾要用字体中有字形的 3 字节符号（U+F001、U+F00C），否则头部截断点落不到多字节字符上，`MUSIC-F4-next-mid-char` 存活。
- Task 6：局部变量不能叫 `head`、`tail`，会遮蔽链表全局 `head`，交叉编译多出 1 条 `-Wshadow`；用 `keep_head`、`keep_tail`。
- Task 8：`"data": null` 仍按 0 处理，保持旧语义；解析抽成 `music_monitor_line()`，PR #8 的 `MUSIC-5` 的 `old` 随之更新。
- Task 10：`mpv_send()` 加锁后 PR #8 的 `MUSIC-7`、`MUSIC-14` 的 `old` 随之更新。
- Task 11：筛选规则抽成 `music_is_track()`，扫描循环只保留跳过时的打印；PR #8 的 `MUSIC-C2` 改为针对 `music_is_track()` 的换行检查（变体文件中 `\n` 会被解析为换行，`old` 取不含反斜杠的片段）。
- Task 12：板上 `custom_init()` 先启动 mpv，之后才打开 fb 与触摸设备，mpv 并不继承它们；PR #8 复核「mpv 继承 DRM、fb、evdev」的说法不成立，实际继承者是之后的 `popen`、`system` 子进程。
- Task 13、15、17：`mutate.sh` 只复制 `custom/` 到工作目录，变体的 `file` 必须在 `custom/` 下；`generated/` 中的改动（Task 13 的滚轮模式、Task 17 的列表按钮回调与音乐页背景回调）改为手动检验：改回原样后对应测试失败，还原后通过。
- Task 13：`music_refresh_list()` 设置选项后紧接着 `music_roller_apply()` 会以普通模式再设一次，该处的模式只在重建后列表为空时生效；`no_repeat_after_rescan` 要包含「删光后直接加载音乐页」的场景，否则 `MUSIC-N5-refresh-infinite` 存活。
- Task 14：不先收集文件名再 `qsort`：会新增内存分配，打乱 `music.list_alloc`、`music.empty_list` 按调用次数注入失败的计数；改为链表按序插入、签名按序插入，分配次数不变。
- Task 14：`sorted_by_name` 逐项选中检查歌曲名与 `playlist-pos`：主机 `readdir` 第 2 项恰好是 `song_02.mp3`，只查一项时 `MUSIC-N6-stale-id` 存活。
- Task 17：`custom/` 中新增的非 `static` 函数要在 `custom/custom.h` 声明，否则 `-Wmissing-prototypes` 使两条交叉编译告警各 +1；`music_list_toggle()`、`music_list_close()` 与 `music_recheck_enable()` 声明在同处。
- Task 17：测试里点按钮要先发 `LV_EVENT_RELEASED` 再发 `LV_EVENT_CLICKED`：`LV_OBJ_FLAG_CHECKABLE` 的自行切换发生在松开时，只发 `CLICKED` 测不到 CHECKED 与面板错位。
- Task 17：GUI Guider 导出的 8 位、16 位分支带抖动，同一颜色在不同位置量化结果不同，无法逐字节复现；生成脚本只核对 32 位分支与原图一致（本项目 `LV_COLOR_DEPTH 32`），8 位、16 位分支按四舍五入量化。

## 6. 后续事项

### 6.1 另开 docs PR：历史 spec 与 plan 标题补序号

**状态**：已由 PR #11 完成，设计、统计与决策见 [`2026-10-14-lvgl-example-heading-numbers-design.md`](../specs/2026-10-14-lvgl-example-heading-numbers-design.md)。PR #10 时的统计与暂定方案见 PR #10 合并时的 [`2026-10-10-lvgl-example-board-test-followups.md`](https://github.com/yuangezhizao/luckfox_pico_lvgl_example/blob/85a6dd48264938b267fc0e4ecd458f6ac14f0530/docs/superpowers/plans/2026-10-10-lvgl-example-board-test-followups.md#61-另开-docs-pr历史-spec-与-plan-标题补序号)。

### 6.2 Load 在没有 `network={}` 块时自动添加（spec Q31）

本 PR 只在没有块时提示（WiFi [2/2]）。完整支持需要：`wpa_cli add_network` 取得新 id 后对该 id `set_network`、`enable_network`、`save_config`，并在 `/etc/wpa_supplicant.conf` 末尾追加 `network={}` 块，而不是写死 `set_network 0`；同时处理配置中已有多个块时只改哪一个（PR #8 spec §1 已把「对所有 `network={}` 块一并改写」列为现状非目标）。属写配置的行为变更，另开 PR。

### 6.3 WiFi 页在 `wpa_supplicant not reachable` 时圆环一直转（N7，spec Q2）

复测 N1 时，左上角圆环在显示红色提示后仍持续转动。不影响 N1 判据，本 PR 不修；以后追查圆环的启停条件，连不上 `wpa_supplicant` 时应停止。

### 6.4 歌曲列表换成从顶部排列的列表（spec Q38）

本 PR 保留 roller，按歌曲数收缩（Task 15）。roller 总把选中项放在正中，选中第一首时上方必然留白。要两端都不留白，只能换成从第一首排起的普通列表。

关键约束：只要「滚动停在哪里就选中哪首」，列表总有一端留白。

| 选中位放在 | 选第一首时 | 选最后一首时 |
|---|---|---|
| 正中间（现在的 roller） | 上方空约半个框高 | 下方空约半个框高 |
| 最顶上 | 上方没有空白 | 下方要空出近一个框高，最后一首才能滚到顶 |
| 最底下 | 上方要空出近一个框高 | 下方没有空白 |

两端都不留白，就得把滚动与选歌分开，滚动只用来浏览，由另一个动作选歌：点一下（最常见）、长按（防误触但不直观），或用上一首、下一首按钮移动高亮。列表按钮的展开与收起已在本 PR 改为切换（Task 17）。另开 PR 时先定交互方式，再改：roller 相关的 8 处调用、生成代码、6 个测试文件与截图，列表相关的 F4、N5、F6、N8 需要重新上板。

### 6.5 CI 增加变体检验

**状态**：已由 PR #12 完成，设计、实测与决策见 [`2026-10-16-lvgl-example-ci-mutation-design.md`](../specs/2026-10-16-lvgl-example-ci-mutation-design.md)。PR #10 时的待办与待定问题见 PR #10 合并时的 [`2026-10-10-lvgl-example-board-test-followups.md`](https://github.com/yuangezhizao/luckfox_pico_lvgl_example/blob/85a6dd48264938b267fc0e4ecd458f6ac14f0530/docs/superpowers/plans/2026-10-10-lvgl-example-board-test-followups.md#65-ci-增加变体检验)。

### 6.6 单次动作按钮的按下反馈

上一首、下一首、返回按钮按下时没有视觉反馈（生成代码给按下状态设的 `0xFF33FF` 作用于按钮里的空文字标签，不生效）。可参照 LVGL Image button 示例，按下时加 30% 黑色着色（`img_recolor_opa`）；这些图标带透明像素，着色不会变成方块。本 PR 只做列表按钮的选中高亮（spec Q50），按下反馈以后再议。

### 6.7 触摸读取处理 `SYN_DROPPED`

`custom/custom_touch.c` 的 `touch_handle()` 不处理 `SYN_DROPPED`。UI 线程阻塞较久时（如 WiFi 页同步执行 `popen`），内核的事件队列可能溢出并丢事件；若丢的是主触点抬起时的 `ABS_MT_TRACKING_ID -1`，指针会一直保持按下，下次按下时从旧位置连出一条线。上游 `lib/lv_drivers/indev/evdev.c` 同样不处理，不是本 PR 引入的回归，板上测试也未出现。改法：收到 `SYN_DROPPED` 后丢弃事件直到下一个 `SYN_REPORT`，再用 `EVIOCGMTSLOTS` 重新读取各槽位的 `ABS_MT_TRACKING_ID` 与坐标；测试可在 `tests/cases/sketchpad/test_touch.c` 的回放中插入 `SYN_DROPPED`。

## 7. 合并前代码审查

2026-10-14 在开发会话中执行，只审查、不修改。范围：`origin/dev` 到合并前最终提交的 `custom/`、`src/`、`lib/`、`generated/`、`tests/` 与最新 CI 结果，对应作者合并前审查模板中的审查者 A（代码与构建）。审查者即实现者，独立性不如另开会话。文档事实与表述（审查者 B、C）未另行审查：收尾时已通读 spec 与 plan，并重跑附录 C 的脚本。

合并门槛为无 Critical、Important 且 CI 通过。结论：未发现 Critical、Important；CI 4 项通过；满足合并门槛。

### 7.1 发现的问题

| 级别 | 位置 | 证据 | 处理 |
|---|---|---|---|
| Minor | `custom/custom_touch.c` 的 `touch_handle()` | 不处理 `SYN_DROPPED`，事件队列溢出时可能丢掉主触点抬起的事件，指针保持按下，下次按下时从旧位置连线。上游 `lib/lv_drivers/indev/evdev.c` 同样不处理（两个文件中 `SYN_DROPPED`、`EVIOCGMTSLOTS` 均为 0 处），不是回归，板上未出现 | 本 PR 不改，记入 §6.7 |

### 7.2 核对过、未发现问题的地方

- **mpv 监听线程**（音乐 [3/10]–[5/10]）：`fd_mpv` 只由监听线程在 `mpv_fd_lock` 内关闭，UI 线程的 `mpv_send()` 在同一把锁内发送，不会写进已关闭或被复用的 fd；初始化在创建线程之前写入 `fd_mpv`。监听线程只在 `music_state_lock` 内写进度、时长与脏标志，不碰控件与歌曲链表；`waitpid` 只在对端关闭后回收 mpv。
- **按行拼接**：`music_monitor_feed()` 的缓冲区边界正确，超长行整行丢弃到下一个 `\n`；`music_monitor_line()` 先检查 `event` 是字符串再比较，`"data": null` 仍按 0 处理。
- **排序与目录签名**：`insert_music_node()` 按 `strcmp` 插入，绕回 `head` 时插在末尾，只在比 `head` 小时更新 `head`；`music_sig_insert()` 的比较规则与之一致，`memmove` 含结尾的 `\0`，扩容按 `used + strlen + 2` 计算，返回长度正确。`music_display_name()` 的缓冲区 `MAX_FILENAME_LEN + 4` 放得下「头 + `...` + 尾」，UTF-8 前后移动不越界。节点分配失败沿用上游的 `exit(1)`。
- **触摸状态机**（画板 [2/2]）：按 MT 协议 B 只跟随主触点；主触点抬起后要等全部接触点抬起才重新选主触点；打开设备时用 `EVIOCGABS(ABS_MT_SLOT)` 取当前槽位，超出 `TOUCH_MAX_SLOTS` 的槽位忽略；坐标按显示分辨率钳位；没有 MT 事件时退回 `ABS_X/ABS_Y` 加 `BTN_TOUCH`；以 `O_NONBLOCK | O_CLOEXEC` 打开。
- **WiFi 读结果的定时器**（WiFi [1/2]）：离开 WiFi 页时 `ui_load_scr_animation()` 的 `auto_del` 为 false，页面对象不删除，定时器在离开后触发也不会访问已释放的对象；连按只 `lv_timer_reset()`，不会创建多个定时器。
- **画板按下画点**（画板 [1/2]）：靠近边缘时点的坐标可能为负，由 `lv_canvas_draw_rect()` 裁剪。
- **列表按钮与背景回调**（音乐 [10/10]）：背景回调只在点击屏幕对象本身时触发，没有用到事件冒泡，点列表按钮不会同时收起；生成代码中列表按钮在 roller 之前创建，`music_roller_apply()` 调用 `music_list_fit()` 时按钮已经存在。
- **CLOEXEC 与字体**（全局 [1/2]、[2/2]）：mpv 套接字用 `SOCK_CLOEXEC`，`fbdev.c` 用 `O_CLOEXEC`；两个字体换算函数的 27 号分支都落到 24 号。
- **CI**：合并前最终提交上「构建 CI 镜像」「glibc · Ubuntu」「uClibc · Buildroot」「原生测试」4 项通过。

## 附录 A：`evdev_probe.c`

诊断工具只放在对比包里，不进仓库；F2 的修复已用板上录到的事件回放成测试（Task 5）。

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

## 附录 B：`_icn_chart_checked_alpha_32x32.c` 生成脚本

用法：`python3 gen_checked.py generated/images/_icn_chart_alpha_32x32.c custom/_icn_chart_checked_alpha_32x32.c`。混色公式与 `lv_color_brightness()` 一致，按图中最深像素归一化，使其正好为 `#FFA300`。

```python
#!/usr/bin/env python3
"""由 _icn_chart_alpha_32x32.c 生成列表按钮选中状态的图：深色线条按亮度混成 #FFA300，白底不变。"""
import re
import sys

SRC, DST = sys.argv[1], sys.argv[2]
ORANGE = (0xFF, 0xA3, 0x00)
text = open(SRC, encoding="utf-8").read()
branches = re.findall(r"(#(?:el)?if LV_COLOR_DEPTH[^\n]*\n)(.*?)(?=#elif|#endif)", text, re.S)
body32 = [b for h, b in branches if "== 32" in h][0]
raw = [int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]{2})", body32)]
pixels = [tuple(raw[i:i + 4]) for i in range(0, len(raw), 4)]  # B, G, R, A


def lum(b, g, r):
    return (r * 77 + g * 151 + b * 28) >> 8  # 与 lv_color_brightness() 相同


darkest = min(lum(b, g, r) for b, g, r, _ in pixels)


def recolor(p):
    b, g, r, a = p
    mix = min(255, (255 - lum(b, g, r)) * 255 // (255 - darkest))
    nr, ng, nb = ((o * mix + 255 * (255 - mix) + 127) // 255 for o in ORANGE)
    return nb, ng, nr, a


def encode(pix, header):
    out = []
    for b, g, r, a in pix:
        if "== 1" in header:
            out += [round(r * 7 / 255) << 5 | round(g * 7 / 255) << 2 | round(b * 3 / 255), a]
        elif "== 16" in header:
            v = round(r * 31 / 255) << 11 | round(g * 63 / 255) << 5 | round(b * 31 / 255)
            out += ([v >> 8, v & 0xFF] if "SWAP != 0" in header else [v & 0xFF, v >> 8]) + [a]
        else:
            out += [b, g, r, a]
    return out


def check_roundtrip():
    # GUI Guider 的 8 位、16 位分支带抖动，无法逐字节复现；只核对 32 位分支（本项目 LV_COLOR_DEPTH 32）
    for header, body in branches:
        want = [int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]{2})", body)]
        if "== 32" in header and encode(pixels, header) != want:
            sys.exit("编码与原文件不一致：" + header.strip())


check_roundtrip()
new = [recolor(p) for p in pixels]


def fmt(data):
    rows = [data[i:i + 32 * (len(data) // 1024)] for i in range(0, len(data), 32 * (len(data) // 1024))]
    return "".join("  " + ", ".join("0x%02x" % v for v in row) + ", \n" for row in rows)


out = text.replace("_icn_chart_alpha_32x32", "_icn_chart_checked_alpha_32x32")
out = out.replace("LV_ATTRIBUTE_IMG__ICN_CHART_ALPHA_32X32", "LV_ATTRIBUTE_IMG__ICN_CHART_CHECKED_ALPHA_32X32")
for header, body in branches:
    comment = re.match(r"(\s*/\*.*?\*/\n)", body, re.S)
    new_body = (comment.group(1) if comment else "") + fmt(encode(new, header))
    out = out.replace(header + body, header + new_body, 1)
note = ("/* 列表按钮选中（歌曲列表展开）状态的图标，由 generated/images/_icn_chart_alpha_32x32.c 生成：\n"
        " * 按亮度把深色线条混成 #FFA300（最深处为纯橙），白底不变；8 位、16 位分支按四舍五入量化，未像 GUI Guider 那样抖动。\n"
        " * 生成脚本见 plan 附录 B。\n"
        " * 名字中的 _alpha_ 沿用 GUI Guider 的规则，表示格式 LV_IMG_CF_TRUE_COLOR_ALPHA，与是否透明无关（spec Q50）。 */\n")
open(DST, "w", encoding="utf-8").write(note + out)
print("darkest", darkest, "pixels", len(new))
```

## 附录 C：本地检验脚本

这些脚本与命令不进仓库，只记在这里；Cloud Agent 环境重建后 `/tmp` 会清空，按本附录重建（例如存为 `/tmp/appc/*.sh` 后 `chmod +x`）。除特别说明外，都在仓库根目录执行；主机测试的构建目录约定为 `/tmp/bt`。各脚本已在本 PR 收尾时实测。

### C.1 环境准备

- `.cursor/Dockerfile` 已含：ARM 交叉工具链 `gcc-arm-linux-gnueabihf`、`qemu-user`、`libsdl2-dev`、`xdotool`。
- 要临时安装：`sudo apt-get install -y xvfb imagemagick`。Xvfb 用于没有桌面时开窗；ImageMagick 的 `import`、`convert`、`compare`、`montage` 用于截图、取像素与拼图。
- uClibc 工具链：按 `AGENTS.md` 稀疏检出 `yuangezhizao/luckfox-pico@dev` 的 `tools/linux/toolchain` 到 `/tmp/luckfox-pico`。
- 主机测试与 `preview`：

  ```bash
  cmake -S tests -B /tmp/bt -DLUCKFOX_TESTS_ARM32=ON
  cmake --build /tmp/bt -j"$(nproc)"
  ctest --test-dir /tmp/bt -j"$(nproc)" --output-on-failure
  cmake --build /tmp/bt --target screenshots   # 截图输出到 /tmp/bt/screenshots/
  cmake --build /tmp/bt --target preview       # 需要 SDL2
  ```

- 后台检验（变体检验、逐提交测试、交叉编译）先后执行，不并行；启动前删掉旧日志。结束 `preview` 等进程时用 `pkill -x` 或按 `ps` 找到的 pid，不用 `pkill -f`（会匹配到执行命令的 shell 自身）。

### C.2 变体检验

`tests/tools/mutate.sh` 在仓库内，用法见 `tests/README.md`。退出码 0 为全部 `KILLED`，1 为有变体存活，2 为工具或清单错误。

```bash
rm -rf /tmp/mut-all /tmp/mut-all.log
MUTATE_WORK=/tmp/mut-all tests/tools/mutate.sh > /tmp/mut-all.log 2>&1; echo "rc=$?"
grep -c KILLED /tmp/mut-all.log; grep SURVIVED /tmp/mut-all.log
# 只跑部分变体：tests/tools/mutate.sh MUSIC-N11-show-only MUSIC-N8-no-gap
```

### C.3 逐提交测试 `percommit.sh`

每行输出「提交标题 | 通过数」，全部为 `100% tests passed` 才算通过；本 PR 19 个提交约 7.6 分钟。

```bash
#!/bin/bash
# 逐提交测试：每个提交 git archive 到空目录，构建 tests/（含 arm32）并跑全量 ctest，结束时输出 DONE。
# 用法（仓库根目录）：bash percommit.sh [基准，默认 origin/dev] > /tmp/percommit.log 2>&1
set -u
BASE=${1:-origin/dev}; WORK=${PERCOMMIT_WORK:-/tmp/pc}; J=${J:-$(nproc)}
for c in $(git rev-list --reverse "$BASE"..HEAD); do
  d=$WORK/$c; rm -rf "$d"; mkdir -p "$d"; git archive "$c" | tar -x -C "$d"
  if cmake -S "$d/tests" -B "$d/b" -DLUCKFOX_TESTS_ARM32=ON >"$d/log" 2>&1 && cmake --build "$d/b" -j"$J" >>"$d/log" 2>&1; then
    r=$(ctest --test-dir "$d/b" -j"$J" 2>&1 | grep -E 'tests passed')
  else
    r="BUILD FAILED（见 $d/log）"
  fi
  echo "$(git log --oneline -1 "$c") | $r"
  rm -rf "$d/b"
done
echo DONE
```

### C.4 交叉编译、冒烟与补测二进制交付 `cross.sh`

告警数与 `AGENTS.md` 的基线（glibc 121、uClibc 120）对比。交付名用全局 [2/2]（最后一个 `fix` 提交）的短哈希，如 `pr10-1b4dbbd`；`SHA256SUMS` 整体重写并核对。

```bash
#!/bin/bash
# 两条交叉编译、告警计数与 uClibc 产物 qemu 冒烟；可选把 uClibc 产物交付到补测目录。
# 用法（仓库根目录）：cross.sh [交付名，如 pr10-<全局 [2/2] 的短哈希>]；uClibc 工具链需先稀疏检出到 /tmp/luckfox-pico（AGENTS.md）。
set -u
SDK=${LUCKFOX_SDK_PATH:-/tmp/luckfox-pico}; J=${J:-$(nproc)}
rm -rf /tmp/xg /tmp/xu
GLIBC_COMPILER=/usr/bin/arm-linux-gnueabihf- cmake -S . -B /tmp/xg >/dev/null 2>&1 && make -C /tmp/xg -j"$J" > /tmp/xg.log 2>&1
echo "glibc rc=$? 告警=$(grep -c 'warning:' /tmp/xg.log)"
env -u GLIBC_COMPILER LUCKFOX_SDK_PATH="$SDK" cmake -S . -B /tmp/xu >/dev/null 2>&1 && make -C /tmp/xu -j"$J" > /tmp/xu.log 2>&1
echo "uClibc rc=$? 告警=$(grep -c 'warning:' /tmp/xu.log)"
GCC=$(ls "$SDK"/tools/linux/toolchain/*/bin/*-gcc | head -1)
timeout 20 qemu-arm -L "$("$GCC" -print-sysroot)" -E LD_LIBRARY_PATH="$PWD/lib/uclibc/libdrm:$PWD/lib/uclibc/libcjson" /tmp/xu/luckfox_lvgl_demo > /tmp/qemu.log 2>&1
echo "qemu exit=$? $(head -1 /tmp/qemu.log)"   # 预期 exit=1、cannot open /dev/dri/card0
if [ $# -ge 1 ]; then
  S=/cursor/stores/self/board-retest-20261010; N=luckfox_lvgl_demo-$1
  cp /tmp/xu/luckfox_lvgl_demo "$S/$N"
  { grep -v "  $N\$" "$S/SHA256SUMS" 2>/dev/null; echo "$(sha256sum < "$S/$N" | cut -d' ' -f1)  $N"; } > /tmp/sums.new
  cp /tmp/sums.new "$S/SHA256SUMS" && (cd "$S" && sha256sum -c SHA256SUMS) && grep "  $N\$" "$S/SHA256SUMS"
fi
```

### C.5 `preview` 操作与核对

`pv-start.sh`、`pv-songs.sh`、`pv-check.sh` 放在同一目录（相互调用），默认用云端桌面 `DISPLAY=:1`；没有桌面时用 Xvfb（如 `Xvfb :99 &` 后 `DISPLAY=:99`）。窗口内坐标（`xdotool mousemove --window`）：

| 控件 | 480 屏 | 720 屏 |
|---|---|---|
| 主屏 WIFI、PAD、MUSIC | (50, 410)、(145, 410)、(240, 410) | MUSIC (360, 615) |
| 音乐页列表按钮 | (155, 326) | (224, 481)（按钮不缩放，仍为 32×32） |
| 音乐页背景空白处 | (420, 160) | (630, 240) |
| 返回 | (32, 32) | — |

```bash
#!/bin/bash
# 在桌面启动 preview（鼠标当触摸），音乐目录放 N 首（默认 4）。
# 用法：pv-start.sh [480|720] [歌曲数]；需先 cmake --build $BT --target preview。
BT=${BT:-/tmp/bt}; export DISPLAY=${DISPLAY:-:1}
pkill -x preview; sleep 1
rm -rf /tmp/pvtmp && mkdir -p /tmp/pvtmp
PREVIEW_RES=${1:-480} TMPDIR=/tmp/pvtmp setsid nohup "$BT/preview" > /tmp/pv.log 2>&1 < /dev/null &
sleep 3; mkdir -p "$(ls -d /tmp/pvtmp/luckfox-tests-*)/music"
"$(dirname "$0")/pv-songs.sh" "${2:-4}"
```

```bash
#!/bin/bash
# 把 preview 的音乐目录改成 01..N 首（N=0 为清空）；之后在窗口里返回主屏再点 MUSIC 即可刷新。
# 用法：pv-songs.sh <歌曲数>
M=$(ls -d /tmp/pvtmp/luckfox-tests-*/music 2>/dev/null | head -1)
[ -n "$M" ] || { echo "preview 未运行"; exit 1; }
rm -f "$M"/*.mp3
for i in $(seq 1 "${1:-4}"); do : > "$M/$(printf 'luckfox_roller_clip_test_long_common_prefix_abcdefghijklmnopq_%02d.mp3' "$i")"; done
ls "$M" | wc -l | xargs echo "当前歌曲数："
```

`pv-check.sh` 的预期（本 PR 收尾）：展开时按钮中心为 `srgb(255,224,168)`（橙色线条与白底的过渡色），收起时为 `srgb(182,176,187)`；面板底边 480 屏 305、720 屏 458，收起时为「无」。

```bash
#!/bin/bash
# 核对列表面板位置与列表按钮高亮：4 首时进 MUSIC，依次「展开、点背景、再展开、按按钮收起」，各截一张图，
# 打印按钮中心颜色与面板底边（面板左侧一列由上往下最后一次颜色变化的行，收起时为空）。
# 用法：pv-check.sh [480|720]；截图在 $OUT（默认 /tmp/pvshots）。需要 xdotool、ImageMagick。
RES=${1:-480}; OUT=${OUT:-/tmp/pvshots}; export DISPLAY=${DISPLAY:-:1}
if [ "$RES" = 480 ]; then MUSIC="240 410"; LIST="155 326"; BG="420 160"; COL=60; Y0=295; Y1=312
else MUSIC="360 615"; LIST="224 481"; BG="630 240"; COL=90; Y0=445; Y1=465; fi
"$(dirname "$0")/pv-start.sh" "$RES" 4 >/dev/null; mkdir -p "$OUT"
W=$(xdotool search --name 'Luckfox LVGL preview' | head -1)
click(){ xdotool mousemove --window "$W" $1 $2 mousedown 1; sleep 0.15; xdotool mouseup 1; sleep 1.2; }
click $MUSIC
for st in open bg reopen btnclose; do
  case $st in open|reopen|btnclose) click $LIST;; bg) click $BG;; esac
  f=$OUT/$RES-$st.png; import -window "$W" "$f"
  prev=""; edge=""
  for y in $(seq $Y0 $Y1); do c=$(convert "$f" -format "%[pixel:p{$COL,$y}]" info:); [ -n "$prev" ] && [ "$c" != "$prev" ] && edge=$y; prev=$c; done
  set -- $LIST; echo "$RES $st 按钮中心=$(convert "$f" -format "%[pixel:p{$1,$2}]" info:) 面板底边=${edge:-无}"
done
pkill -x preview
```

`preview_run.sh` 在 Xvfb 下截主屏、WiFi 页与画板，用于 Task 2、4、5 的带界面检验：

```bash
#!/bin/bash
# 无桌面时在 Xvfb 下运行 preview（480 屏），截主屏、WiFi 页（点 Scan 后）与画板（轻点 3 下、拖一笔）。
# 用法：preview_run.sh；截图在 /tmp/pv/*.png。需要 xvfb、xdotool、ImageMagick。
set -u
BT=${BT:-/tmp/bt}
mkdir -p /tmp/pv; rm -f /tmp/pv/*.png
Xvfb :99 -screen 0 600x600x24 >/dev/null 2>&1 & XV=$!
sleep 1
export DISPLAY=:99
PREVIEW_RES=480 "$BT/preview" >/tmp/pv/preview.log 2>&1 & PV=$!
sleep 3
W=$(xdotool search --name "Luckfox LVGL preview" | head -1)
eval "$(xdotool getwindowgeometry --shell "$W")"   # X Y WIDTH HEIGHT
tap(){ xdotool mousemove $((X+$1)) $((Y+$2)); sleep 0.1; xdotool mousedown 1; sleep 0.2; xdotool mouseup 1; sleep 0.6; }
drag(){ xdotool mousemove $((X+$1)) $((Y+$2)); xdotool mousedown 1; sleep 0.15; for i in $(seq 1 20); do xdotool mousemove $((X+$1+($3-$1)*i/20)) $((Y+$2+($4-$2)*i/20)); sleep 0.03; done; xdotool mouseup 1; sleep 0.5; }
shot(){ import -window root -crop ${WIDTH}x${HEIGHT}+${X}+${Y} /tmp/pv/$1.png; }
shot 00-main
tap 50 410                     # WIFI
sleep 1; shot 01-wifi
tap 65 400                     # Scan
sleep 1; shot 02-wifi-scan
tap 410 400                    # Back
sleep 1
tap 145 410                    # PAD
sleep 1
tap 120 100; tap 240 100; tap 360 100
drag 100 220 380 300
sleep 0.5; shot 03-pad
kill $PV $XV 2>/dev/null; wait 2>/dev/null
```

### C.6 板上补测结果归档 `archive.sh`

作者把补测 zip 交给 Cloud Agent 后，若要存入项目存储（如 `/cursor/stores/self/board-retest-results-20261010/supplement-<二进制名>/`），用本脚本；zip 含测试热点的 SSID 与密码原文，不放进仓库目录提交。

```bash
#!/bin/bash
# 把补测结果 zip 存入项目存储。存储单个文件上限约 100 MiB（超出时关闭文件报 Invalid argument，并留下 0 字节文件），
# 超过 SPLIT（默认 95M）就分段；SHA256SUMS 记录整包与各段的哈希，从存储读回、拼接后核对。
# 用法：archive.sh <zip> <目标目录>
set -eu
Z=$1; D=$2; SPLIT=${SPLIT:-95M}; B=$(basename "$Z"); T=$(mktemp -d)
mkdir -p "$D"
if [ "$(stat -c %s "$Z")" -gt "$(numfmt --from=iec "$SPLIT")" ]; then
  split -b "$SPLIT" -d "$Z" "$T/$B.part"
else
  cp "$Z" "$T/$B"
fi
(cd "$T" && sha256sum -- * > SHA256SUMS)
grep -q "  $B\$" "$T/SHA256SUMS" || echo "$(sha256sum < "$Z" | cut -d' ' -f1)  $B" >> "$T/SHA256SUMS"
cp "$T"/* "$D"/
(cd "$D" && sha256sum -c --ignore-missing SHA256SUMS)
WANT=$(grep "  $B\$" "$D/SHA256SUMS" | cut -d' ' -f1)
GOT=$(if ls "$D/$B".part* >/dev/null 2>&1; then cat "$D/$B".part*; else cat "$D/$B"; fi | sha256sum | cut -d' ' -f1)
[ "$WANT" = "$GOT" ] && echo "读回一致：$WANT"
rm -rf "$T"
```

### C.7 整理提交

- 修改并入已有提交：`git commit --fixup=<提交>`，再 `GIT_SEQUENCE_EDITOR=true git rebase -q -i --autosquash origin/dev`（两条命令前都加 §1 的 `env -u ...`）。前后核对 `git rev-parse HEAD^{tree}` 一致、`git cat-file -p <提交> | grep 'BEGIN SSH SIGNATURE'` 有签名，推送用 `--force-with-lease`。
- 批量改写提交说明 `reword.py`：

```python
#!/usr/bin/env python3
"""rebase -i 时批量改写提交说明。
映射文件为 JSON：{"标题中的关键字": ["新标题，不改则为 null", "正文"]}，路径由环境变量 REWORD_MAP 指定。
用法：REWORD_MAP=map.json GIT_SEQUENCE_EDITOR="python3 reword.py seq" GIT_EDITOR="python3 reword.py msg" git rebase -i origin/dev
（提交身份约定的 env -u ... 照常加在前面）
seq：把标题含关键字的 pick 改为 reword；msg：把对应提交的说明换成「新标题 + 空行 + 正文」，保留 # 注释行。
"""
import json
import os
import sys

mode, path = sys.argv[1], sys.argv[2]
mapping = json.load(open(os.environ["REWORD_MAP"], encoding="utf-8"))
lines = open(path, encoding="utf-8").read().splitlines()
if mode == "seq":
    out = ["reword " + l[5:] if l.startswith("pick ") and any(k in l for k in mapping) else l for l in lines]
    open(path, "w", encoding="utf-8").write("\n".join(out) + "\n")
    sys.exit(0)
title = lines[0] if lines else ""
key = next((k for k in mapping if k in title), None)
if key is None or title.startswith("fixup!"):
    sys.exit(0)
new_title, body = mapping[key]
comments = [l for l in lines if l.startswith("#")]
text = (new_title or title) + "\n\n" + body.rstrip("\n") + "\n"
if comments:
    text += "\n" + "\n".join(comments) + "\n"
open(path, "w", encoding="utf-8").write(text)
```
