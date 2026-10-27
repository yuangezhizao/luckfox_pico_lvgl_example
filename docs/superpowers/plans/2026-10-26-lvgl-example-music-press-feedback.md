# Luckfox Pico LVGL Example — 音乐页按钮按下反馈实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 音乐页的上一首、下一首、播放按钮按下时叠 30% 黑色，播放模式、返回按钮按下时叠 50% 白色（播放模式在第 1 轮真机复测后由黑色改为白色，spec §5.1）；暂停时按下播放键不换图（第 2 轮真机复测后，spec Q14）；只改 `custom/`，主机测试按渲染出的像素断言。

**Architecture:** 先推送 spec 与 plan、开草稿 PR（Task 1）；按 TDD 先写像素测试与变体，确认失败后在 `custom_musicplayer.c` 实现（Task 2）；本地全量 `ctest`、`mutate.sh`、两条交叉编译与 `qemu-arm` 冒烟（Task 3）；preview 回归与按下截图（Task 4）；真机复测（Task 5）；PR #10 的收尾（Task 6）；整理提交、精简 plan、PR 正文，合并前与作者确认（Task 7）；合并后核对 dev CI（Task 8）。

**Tech Stack:** C（gnu99）、LVGL 8.3、CMake/CTest、ASan/UBSan、`tests/tools/mutate.sh`、xdotool、ImageMagick、Xvfb、arm-linux-gnueabihf 与 uClibc 交叉工具链、`qemu-arm`、GitHub Actions、`gh`。

**Spec:** [`docs/superpowers/specs/2026-10-26-lvgl-example-music-press-feedback-design.md`](../specs/2026-10-26-lvgl-example-music-press-feedback-design.md)

---

## 1. Global Constraints

- 分支 `pi/music-press-feedback`，起点 `origin/dev` `1fb0cc6`（PR #14 合入）；PR 指向 `dev`，开发时为草稿，满足门槛后改为 ready，执行者不合并。
- 只改 `custom/` 与 `tests/`，`generated/` 不改（spec Q5）。
- 着色值：变暗组（上一首、下一首、播放）`lv_color_black()`、`LV_OPA_30`（76）；变亮组（播放模式、返回）`lv_color_white()`、`LV_OPA_50`（127）；选择器 `LV_PART_MAIN | LV_STATE_PRESSED`；不加位移、变宽与过渡；播放键暂停时的按下图改回 `_btn_play`（spec §3.1、Q14）。
- 提交身份 `Cursor Agent <cursoragent@cursor.com>`，SSH 签名；所有创建或改写提交的 git 命令前加 `env -u GIT_AUTHOR_NAME -u GIT_AUTHOR_EMAIL -u GIT_COMMITTER_NAME -u GIT_COMMITTER_EMAIL`，推送前 `git cat-file -p HEAD | sed -n 3,4p` 核对。改写历史只用 rebase 的 fixup、reword，不用 `git commit` 重建提交。
- 提交信息按 cz-conventional-emoji 1.1.8：`<type>(<scope>): <emoji> <subject>`，正文为要点、写原因、每行不超过 100 字符，最后一个要点为 `- 详见 spec §…、Q…`；文档中不写本 PR 自身提交的哈希。
- 推送：`GIT_TERMINAL_PROMPT=0 timeout 120 git push https://yuangezhizao@github.com/yuangezhizao/luckfox_pico_lvgl_example.git ...`，改写历史时写明 `--force-with-lease=pi/music-press-feedback:<远端旧提交的完整哈希>`；`gh` 命令加 `timeout`；更新 PR 正文前先 `gh pr view --json body` 取回当前正文。
- 推送会取消正在运行的 CI，变体检验约 15 分钟；只改文档的修订攒到一起推送。
- 云端环境刷新令牌时会改写 `~/.config/gh/hosts.yml`，作者的 `gh auth login` 可能被覆盖成 `cursor` 账户，推送报 `Authentication failed`。此时请作者重新登录，登录后立即用附录 A.4 的 `push-and-body.sh` 推送并更新 PR 正文；不改用 `cursor` 令牌推送。
- `mutate.sh` 运行期间不在同一工作区切换分支；结束 `preview` 用 `tests/tools/preview/stop.sh` 或 `pkill -x preview`，不用 `pgrep -f`、`pkill -f`。
- 编辑文件与 `git add` 不放在同一批并行执行；写脚本与运行脚本分两步。
- 文档中的事实与数字都用命令重新算出：实测命令与测试脚本写在附录 A；CI 结果以 run 编号记录。

## 2. Review Focus

- 按住按钮后滑出再松开：所有对象带 `LV_OBJ_FLAG_PRESS_LOCK`，按钮保持按下、颜色保持到松开，松开仍触发点击；这是 LVGL 的默认行为，本 PR 不改（spec §4）。
- 播放中按下播放键、随机模式下按下播放模式键（`CHECKED | PRESSED`）：分别变暗、变亮；由 `darken_*` 中播放中的播放按钮与 `lighten_*` 中随机模式的播放模式按钮覆盖。
- 暂停时按下播放键：不再换成 `_btn_list_play`，与播放中一样只变暗；由 `darken_*` 中暂停时的播放按钮与 `play_paused` 覆盖。
- 返回主屏再进入音乐页：音乐页只建一次，样式仍在；由 `reenter` 覆盖。
- 720 屏：图标不缩放、位置缩放；由 `darken_720`、`lighten_720` 覆盖。

## 3. 提交

1. `ux(music): 🚸 音乐页按钮按下时着色`：`custom/custom_musicplayer.c`、`tests/cases/music/test_press_feedback.c`、`tests/cases/music/press_feedback.mutants`、`tests/cases/music/CMakeLists.txt`。
2. `docs(superpowers): 📝 新增音乐页按下反馈 spec 与 plan`：本 spec 与 plan，PR #10 plan §6.6 的状态行。

开发期间 `docs(superpowers)` 先推送供作者在 GitHub 上审阅（作者要求）；`ux(music)` 完成后即用 rebase 把它挪到末尾，树不变，使补测二进制名中的 `ux` 短哈希与最终提交一致。之后的文档修订以 fixup 并入 `docs(superpowers)`。

## 4. Tasks

### Task 1：spec 与 plan，推送并开草稿 PR ✅

- [x] spec（QA Q1–Q9）与本 plan，`check-headings.py`、`check-samples.sh` 为 0，QA 编号用 `qref.py` 核对；提交 `docs(superpowers)`，推送，开草稿 PR #15，标题 `ux(music): 🚸 音乐页按钮按下时着色`。
- [x] 作者要求 PR 正文与历史 PR 同格式：PR #10–#14 均为「1. 概述、2. 改动清单、3. 提交、4. 验证、5. 评审注意」，补齐后 3 章、去掉勾选清单，未完成的项写「待完成」。

### Task 2：按下反馈（TDD）✅

落地文件：`custom/custom_musicplayer.c`（`music_press_feedback_init()`）、`tests/cases/music/test_press_feedback.c`、`tests/cases/music/press_feedback.mutants`、`tests/cases/music/CMakeLists.txt`

- [x] 测试 `music.press_feedback.*` 7 条（spec §4）：真正按下后取帧缓冲比较像素；恢复用 `lv_indev_wait_release()` 打断按下（只发 `PRESS_LOST`），上一首、下一首另查正常松开。
- [x] 实现前运行：6 条失败，`list_unchanged` 通过（守护实现后的行为），输出见 §5。
- [x] 实现后 7/7 通过，音乐页 53/53；实测值见 §5。
- [x] 变体 12 个（`MUSIC-PRESS-*`），单独运行全部 `KILLED`。
- [x] 第 1 轮真机复测后（spec §5.1、Q10）：`back_*` 改为 `lighten_*`，依次查返回、循环与随机的播放模式，播放模式移出 `darken_*`；实现前 `lighten_*` 失败，播放模式 `not_lighter=708`；实现改为变暗组、变亮组两个数组，7/7 通过；变体改为 13 个，单独运行全部 `KILLED`。
- [x] 第 2 轮真机复测后（spec Q14）：`darken_*` 加入暂停时的播放按钮，`play_paused` 改为检查松开与按下的图都是 `_btn_play`；实现前 `darken_*` 与 `play_paused` 失败，暂停时 `not_darker=4871`；实现在 `music_press_feedback_init()` 里用 `lv_imgbtn_set_src()` 改回按下图，7/7 通过；新增变体 `paused-list-play`，共 14 个，单独运行全部 `KILLED`。

### Task 3：本地验证 ✅

- [x] 全量 `ctest`（`-DLUCKFOX_TESTS_ARM32=ON`）133/133；第 1 轮、第 2 轮真机复测后的改动之后各再跑一次，仍为 133/133。
- [x] 全量 `tests/tools/mutate.sh`：`148 run, 0 skipped`；第 1 轮后 `149 run, 0 skipped`；第 2 轮后 `150 run, 0 skipped`；均全部 `KILLED`。
- [x] PR #10 plan 附录 C.4 的 `cross.sh`（交付目录改为 `/cursor/stores/self/board-retest-<日期>`）：glibc、uClibc rc=0，告警 121、120；`qemu-arm` 冒烟 `exit=1`、`cannot open /dev/dri/card0`；第 1 轮二进制交付到 `board-retest-20261026/`，第 2、3 轮交付到 `board-retest-20261027/`，文件名为 `luckfox_lvgl_demo-pr15-<当时 ux(music) 的短哈希>`，SHA256 见 §5。

### Task 4：preview 回归与按下截图 ✅

- [x] 云端桌面 `:1` 与 `--headless` 下各以 480、720 运行 `start.sh`、`check-music-list.sh`、`check-wifi.sh`、`shots.sh`、`stop.sh`，全部通过。
- [x] 按下截图：附录 A.3 的 `press-shots.sh` 在桌面上以 480、720 各运行一次，两张拼图横向合成一张，存到 `/cursor/stores/self/music-press-feedback-20261026/`，`sha256sum -c` 为 `OK`。
- [x] 第 1 轮真机复测后的改动之后，上述回归在桌面与 `--headless` 下各以 480、720 重跑，全部通过；`press-shots.sh` 加了随机模式一行，拼图存到 `/cursor/stores/self/music-press-feedback-20261027/`，`sha256sum -c` 为 `OK`。
- [x] 第 2 轮后的改动之后再次重跑，全部通过；拼图以 `music_press_feedback_desktop_480_720_q14.png` 存到同一目录，`sha256sum -c` 为 `OK`。
- [x] 作者把 7 张图贴进 PR [评论](https://github.com/yuangezhizao/luckfox_pico_lvgl_example/pull/15#issuecomment-6438812487)，改为两张表；逐张说明见[评论](https://github.com/yuangezhizao/luckfox_pico_lvgl_example/pull/15#issuecomment-6438991328)。图 1–3 为上面三张按下截图；图 4–6 为设计阶段的对比图，存于 `/cursor/stores/self/pr15-press-mockups/`（spec §2.5）；图 7 为附录 A.2 ⑩ 生成的播放模式候选颜色拼图，另存为 `/cursor/stores/self/music-press-feedback-20261027/mode_candidates_480.png`，同目录 `SHA256SUMS` 已列出。

### Task 5：真机复测（作者执行）✅

- [x] 第 1 轮：用 Task 3 交付到 `board-retest-20261026/` 的二进制，作者在 480 屏上依次按住上一首、下一首、播放（暂停与播放中）、播放模式（循环与随机）、返回，确认颜色变化看得清、松开后恢复。6 项通过，随机模式未通过，结果见 §5 与 spec §5.1。
- [x] 第 2 轮（spec Q13）：用 `board-retest-20261027/` 的二进制，播放模式的循环、随机必测，其余 5 项各按一次确认没有回归。7 项全部通过；作者问到播放键两种状态的按下反馈为何不同，引出 spec Q14。
- [x] 第 3 轮（spec Q15）：用 `board-retest-20261027/` 中新交付的二进制，只测播放键暂停时、播放中各一次。全部通过，OFF 黑屏也已确认，结果见 §5。
- [x] 交给作者本机 pi 执行的提示词存在二进制旁：第 2 轮 `/cursor/stores/self/board-retest-20261027/PROMPT-pr15-4356493.md`，第 3 轮同目录 `PROMPT-pr15-cf764f9.md`，可作为以后真机复测的模板（准备、逐动作记录原话、判据、清理与归档）。第 1 轮的提示词不在本环境。

### Task 6：PR #10 的收尾 ✅

文件：`docs/superpowers/plans/2026-10-10-lvgl-example-board-test-followups.md`

- [x] PR #10 plan §6.6 正文改为状态行，写法同 §6.1、§6.3、§6.5：链接本 spec 与 PR #10 合并时（`85a6dd4`）的原文，锚点在该提交的 GitHub 渲染结果中存在。
- [x] 合并前更新 PR #10 遗留问题的现状：按作者要求发一版完整的新评论 `6437574022`，取代原评论 `6319354295` 中的表格。第 3 项列入已解决（#15 待合并），其余 6 项在本分支上重新核实未变，另新增 spec Q11 的播放模式触摸区一项。

### Task 7：整理提交、精简 plan、PR 正文、合并前确认 ✅

- [x] 按 §3 整理提交，整理前后树相同，身份与签名正确。
- [x] 精简本 plan：已完成 Task 写成「落地文件 + [x] 步骤」，§6 记有偏离，保留；附录只留实测命令与测试脚本。
- [x] `check-headings.py`、`check-samples.sh`、`qref.py` 为 0。
- [x] PR 正文按 PR #14 的格式更新；PR 上没有评论（含 Codex）。
- [x] CI 全部通过，`🧬 变体检验` 为 `0 skipped`（§5 最后一行）。
- [x] 作者要求通读 spec 与 plan、更新过时之处，并把以后可复用的脚本记入附录：spec 补 §9 后续事项（触摸区、变体检验提速、文档检查脚本入库）；plan 补附录 A.1 的工具安装、A.2 ⑪ 的 CI 用时、A.4 的辅助脚本与重建 `/tmp/pr15` 的命令，附录中 8 个脚本抽出后与实际用过的文件逐字节相同。
- [x] 合并前审查：按 spec §6 不做 A、B、C 三角色审查。代码改动只有 `custom/custom_musicplayer.c` 的 28 行，按 TDD 实现，14 个变体全部 `KILLED`，三轮真机复测通过，CI 全绿；文档中的脚本已抽出并逐字节核对，⑪ 的命令原样复跑过。作者同意，合并前自查最终的代码 diff 即可。
- [x] 与作者确认后把 PR 改为 ready，不合并。

### Task 8：合并后核对 dev CI

- [ ] 作者合并后，dev push 的 CI 全部通过；结果评论到 PR 中。

## 5. 验证证据

| 项 | 结果 |
|---|---|
| 起点（`1fb0cc6`） | 全量 `ctest`（`-DLUCKFOX_TESTS_ARM32=ON`）126/126，arm32 2 条（附录 A.1） |
| 实现前（Task 2） | `music.press_feedback.*` 7 条中 6 条失败：`darken_*` 与 `reenter` 各按钮 `changed=0`（如上一首 `area=4096 changed=0`）；`back_*`（后改为 `lighten_*`）`changed=0`、最暗像素按下后仍为 `000000`；`play_paused` 的 `img_recolor_opa` got 0、want 76；`list_unchanged` 通过 |
| 实现后（Task 2） | 7/7 通过。480 与 720 相同：上一首 3527/4096、下一首 3536/4096、播放模式 708/1024、播放中的播放键 5179/7056 个像素变暗，纯白像素按下后 R 最小为 179；返回按钮 173 个像素变亮，`000000` 变为 `7f7f7f`；环带均不变；音乐页 53/53 |
| 变体（Task 2） | `MUSIC-PRESS-*` 12 个全部 `KILLED`：`back-black`、`back-opa-30` 由 `back_*` 发现，`add-list` 由 `list_unchanged` 发现，其余由 `darken_*` 等发现 |
| 全量（Task 3） | `ctest` 133/133（arm32 2 条）；`mutate.sh` `baseline green: 133 tests; running 148 mutants`、`148 run, 0 skipped`，rc=0 |
| 交叉编译（Task 3） | glibc rc=0 告警 121、uClibc rc=0 告警 120，与 `AGENTS.md` 基线相同；`qemu-arm` `exit=1`、`cannot open /dev/dri/card0`；第 1 轮二进制 SHA256 `3e0646cdafd520a365bd6f542303587425f714bd5d666beb4eae664bddfb0d3d` |
| preview（Task 4） | 桌面与 `--headless` 各 480、720：`check-music-list.sh` 展开时按钮中心 `srgb(255,224,168)`、面板底边 305 / 458，收起时 `srgb(182,176,187)`；`check-wifi.sh` 圆环区/图标区依次 1/1、>1/1、1/18、1/1；`shots.sh` 正常 |
| 按下截图（Task 4） | `/cursor/stores/self/music-press-feedback-20261026/music_press_feedback_desktop_480_720.png`（SHA256 `eb25a3f6e008f089ac22b17dbffb60bda91d2a572dfd7c628acd1ad0edccc38e`）：480、720 各 6 行（上一首、下一首、播放模式、暂停时与播放中的播放键、返回），左松开、右按住 |
| 第 1 轮真机复测（Task 5） | 作者执行，480 屏，记录在 `/cursor/stores/self/board-retest-results-20261026/pr15-38d1171/`（`board-retest-results-pr15-38d1171.zip` 的 `sha256sum -c` 为 `OK`）。上一首、下一首、播放（暂停时、播放中）、循环模式、返回 6 项通过；随机模式未通过，作者原话「按住时颜色变化不太清楚，因为按钮太小了。其余正常」，松开恢复、切回循环正常。中途 USB 断连、整板重启一次，应用共启动 2 次 |
| 第 1 轮后的分析（附录 A.2 ⑩） | 循环、随机按下后圆底颜色相同：R 由 80 降到 56（30% 黑色）；上一首白色圆底由 255 降到 179。可见圆：播放模式 26×26，上一首按下后 28×28。候选按下效果的圆底 R：50% 黑色 40、30% 白色 132、50% 白色 167 |
| 实现前（第 1 轮后） | `lighten_480`、`lighten_720` 失败：播放模式循环、随机均 `changed=708 not_lighter=708`，最深像素 `504d6c -> 38364b`，期望 R 约 167；其余 5 条通过 |
| 实现后（第 1 轮后） | 7/7 通过。480 与 720 相同：播放模式循环 696/1024、随机 697/1024 个像素变亮，`504d6c` 变为 `a7a5b5`；返回仍为 173 个、`000000` 变为 `7f7f7f`；`darken_*` 中上一首、下一首、播放中的播放键与之前相同 |
| 变体（第 1 轮后） | `MUSIC-PRESS-*` 13 个全部 `KILLED`：`lighten-black`、`lighten-opa-30`、`drop-mode`、`drop-back` 由 `lighten_*` 发现，`add-list` 由 `list_unchanged` 发现，其余由 `darken_*` 发现 |
| 全量（第 1 轮后） | `ctest` 133/133（arm32 2 条）；`mutate.sh` `baseline green: 133 tests; running 149 mutants`、`149 run, 0 skipped`，149 个全部 `KILLED` |
| 交叉编译（第 1 轮后） | glibc rc=0 告警 121、uClibc rc=0 告警 120；`qemu-arm` `exit=1`、`cannot open /dev/dri/card0`；第 2 轮二进制 SHA256 `6d0ecfa313df3a1a7e29e9df393aaac3f007098f3234d9f2957e87ad5582ed3f` |
| preview（第 1 轮后） | 桌面与 `--headless` 各 480、720：`check-music-list.sh`、`check-wifi.sh`、`shots.sh` 全部通过 |
| 按下截图（第 1 轮后） | `/cursor/stores/self/music-press-feedback-20261027/music_press_feedback_desktop_480_720.png`（SHA256 `1de84dfaa95e48977851e15d7208f6f276624d30876526f6bc675edbed70b1f9`）：480、720 各 7 行，播放模式分循环、随机两行，按住时由深紫变为浅灰 |
| CI（第 1 轮后） | run 40459994875 全部通过：`🧬 变体检验` `baseline green: 133 tests; running 149 mutants`、`149 run, 0 skipped`；`🧪 原生测试` 133/133，480、720 的 `check-music-list.sh`、`check-wifi.sh` 通过 |
| 第 2 轮真机复测（Task 5） | 作者执行，480 屏，记录在 `/cursor/stores/self/board-retest-results-20261027/pr15-4356493/`（`board-retest-results-pr15-4356493.zip` 的 `sha256sum -c` 为 `OK`）。循环模式（原话「确认现象无误」）、随机模式（原话「确认」）、上一首、下一首、播放（暂停时、播放中）、返回 7 项全部通过。两次 USB 断连导致整板重启，部分结束标记缺失，重启后首次拉起应用失败、重试成功；收尾的 OFF 黑屏未验证。作者问：「为什么只有播放按钮切换播放和暂停的样式不一样？」 |
| 实现前（第 2 轮后） | `darken_480`、`darken_720`、`play_paused` 失败：暂停时的播放键 `changed=5179 not_darker=4871 min_r_white=56`；按下图不是 `_btn_play`；其余 4 条通过 |
| 实现后（第 2 轮后） | 7/7 通过。480 与 720 相同：暂停时与播放中的播放键都是 5179/7056 个像素变暗、白色符号按下后 R 最小为 179；其余按钮与之前相同 |
| 变体（第 2 轮后） | `MUSIC-PRESS-*` 14 个全部 `KILLED`，新增的 `paused-list-play` 由 `darken_*` 发现 |
| 全量（第 2 轮后） | `ctest` 133/133（arm32 2 条）；`mutate.sh` `baseline green: 133 tests; running 150 mutants`、`150 run, 0 skipped`，150 个全部 `KILLED` |
| 交叉编译（第 2 轮后） | glibc rc=0 告警 121、uClibc rc=0 告警 120；`qemu-arm` `exit=1`、`cannot open /dev/dri/card0`；第 3 轮二进制 SHA256 `207bc08bd1becdb42acda6809c1cfac026cb3900631f187ccf883ba5cb13952f` |
| preview（第 2 轮后） | 桌面与 `--headless` 各 480、720：`check-music-list.sh`、`check-wifi.sh`、`shots.sh` 全部通过 |
| 按下截图（第 2 轮后） | `/cursor/stores/self/music-press-feedback-20261027/music_press_feedback_desktop_480_720_q14.png`（SHA256 `d273e4fbf17b42dcd1a5b27e147b68c5a1e0e6eced36376e9ab8be171cad06f2`）：暂停时与播放中的播放键按住时都是蓝紫圆变暗、白色符号变灰 |
| CI（第 2 轮后） | run 40474511845 全部通过：`🧬 变体检验` `baseline green: 133 tests; running 150 mutants`、`150 run, 0 skipped`；`🧪 原生测试` 133/133，480、720 的 `check-music-list.sh`、`check-wifi.sh` 通过 |
| 第 3 轮真机复测（Task 5） | 作者执行，480 屏，记录在 `/cursor/stores/self/board-retest-results-20261027/pr15-cf764f9/`（`board-retest-results-pr15-cf764f9.zip` 的 `sha256sum -c` 为 `OK`）。暂停时、播放中按住都是蓝紫圆变暗、白色符号变灰，松开后恢复，播放状态切换正常；两种状态是否一致、看得清，原话「是」；OFF 黑屏确认。准备阶段 ADB 一度未连接，测试期间无断连或重启 |
| CI（最终提交） | run 40476521758 全部通过：`🧬 变体检验` `150 run, 0 skipped`；该次推送只改了 `.md`。本 PR 5 次 CI 中 `🧬 变体检验` 用时 14 分 31 秒至 17 分 38 秒、`🧪 原生测试` 154–161 秒，其中首尾两次推送只改了 `.md`（附录 A.2 ⑪），由此引出 spec §9 的后续事项 |

## 6. 与计划的偏离

- Task 2：`ux(music)` 完成后即把 `docs(superpowers)` 挪到末尾（§3），而非留到 Task 7：补测二进制名用 `ux` 的短哈希，提前重排使两者一致。第一次用 `sed -n '2p;1p'` 作 sequence editor 没有改变顺序（sed 仍按行号输出），改用交换两条 `pick` 的脚本。
- Task 3、4：本机没有 `shellcheck`，apt 安装 0.9.0（与 CI 相同）后检查附录 A.3 的脚本，0 条。
- Task 5：第 1 轮真机复测随机模式未通过，按第二轮 grilling（spec Q10–Q13）把播放模式改为叠 50% 白色，回到 Task 2 按 TDD 修改，Task 3、4 重跑，再做第 2 轮复测。代码改动以 fixup 并入 `ux(music)`。第 2 轮 7 项全部通过，但作者问到播放键两种状态的按下反馈为何不同，按第三轮 grilling（spec Q14、Q15）统一后再做第 3 轮复测，只测播放键。
- Task 6：原计划在 PR #10 评论 `6319354295` 下回复；作者要求改发一版完整的现状评论，越清晰越好，先发的补充评论已由作者删除。

## 附录 A：未入库的命令

只保留三类：支撑 spec 与本 plan §5 中事实、数字的实测命令（A.2），验证用的测试脚本（A.3），以及以后可以复用的辅助脚本（A.4）。在仓库根目录执行；主机测试的构建目录为 `build-tests`，一次性脚本放在 `/tmp/pr15/`。新环境中 `/tmp` 会被清空，按 A.4 重建。一次性修改 PR 正文的 `update-body*.py` 等脚本不保留。

### A.1 环境准备

```bash
cmake -S tests -B build-tests -DLUCKFOX_TESTS_ARM32=ON
cmake --build build-tests -j"$(nproc)"
ctest --test-dir build-tests -j"$(nproc)"          # 起点 126/126
cmake --build build-tests --target screenshots     # 480、720 的音乐页截图
cmake --build build-tests --target preview         # A.3 的 preview 回归与按下截图
# 本机缺的工具：shellcheck（检查 A.3、A.4 的 shell 脚本，0.9.0 与 CI 相同）、python3-markdown-it（check-headings.py）
sudo apt-get install -y shellcheck python3-markdown-it
# uClibc 工具链（cross.sh 用，AGENTS.md「构建」一节的稀疏检出）
git clone --depth 1 --filter=blob:none --sparse -b dev https://github.com/yuangezhizao/luckfox-pico.git /tmp/luckfox-pico
git -C /tmp/luckfox-pico sparse-checkout set tools/linux/toolchain
```

### A.2 实测与核对命令

```bash
# ① 音乐页 6 个图片按钮各状态的图（spec §2.1）
grep -n 'lv_imgbtn_set_src\|LV_OBJ_FLAG_CHECKABLE\|lv_obj_set_size' generated/setup_scr_Music_player.c
grep -n 'lv_imgbtn_set_src' custom/custom_musicplayer.c

# ② 图标的 alpha 分布（spec §2.2），imgstat.py 的源码见下
python3 /tmp/pr15/imgstat.py generated/images/_btn_{prev,next}_alpha_64x64.c generated/images/_back_alpha_64x64.c \
  generated/images/_btn_{play,list_play,pause}_alpha_84x84.c generated/images/_btn_{loop,rnd}_alpha_32x32.c \
  generated/images/_icn_chart_alpha_32x32.c
```

```python
#!/usr/bin/env python3
"""统计 GUI Guider 图片 32 位分支（B,G,R,A）的 alpha 分布与不透明像素颜色。"""
import re
import sys

for path in sys.argv[1:]:
    src = open(path, encoding="utf-8").read()
    body = src.split("#if LV_COLOR_DEPTH == 32", 1)[1].split("#endif", 1)[0]
    vals = [int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]{2})", body)]
    px = [vals[i:i + 4] for i in range(0, len(vals), 4)]
    alpha = [p[3] for p in px]
    opaque = [p for p in px if p[3] == 255]
    transparent = sum(1 for a in alpha if a == 0)
    partial = len(px) - transparent - len(opaque)
    colors = {}
    for b, g, r, a in px:
        if a > 0:
            key = "#%02X%02X%02X" % (r, g, b)
            colors[key] = colors.get(key, 0) + 1
    top = sorted(colors.items(), key=lambda kv: -kv[1])[:5]
    print(f"{path.split('/')[-1]}: 像素 {len(px)}，透明 {transparent}，半透明 {partial}，不透明 {len(opaque)}；"
          f"可见像素常见颜色 {top}")
```

```bash
# ③ 默认主题对 lv_btn 的按下样式与主屏 WIFI 按钮实测（spec §2.3），mock 的构建与源码见 ⑤
grep -n 'LV_USE_THEME_DEFAULT\|LV_THEME_DEFAULT_GROW\|define LV_DPI_DEF' lib/lv_conf.h
sed -n 798,807p lib/lvgl/src/extra/themes/default/lv_theme_default.c      # pressed、grow 加在 LV_STATE_PRESSED
grep -n 'styles->pressed\|styles->grow' lib/lvgl/src/extra/themes/default/lv_theme_default.c | head -6
grep -n 'lv_theme_default_init' lib/lvgl/src/hal/lv_hal_disp.c           # 注册显示时自动应用
cd /tmp/pr15/mock
for v in main-released main-pressed; do
  ./mock 480 $v 480-$v.ppm | grep WIFI
  convert 480-$v.ppm -format "$v center=%[pixel:p{50,395}] x1-2=%[pixel:p{3,395}]\n" info:
done
# main-released center=srgb(255,150,0) x1-2=srgb(255,255,255)
# main-pressed  center=srgb(220,129,0) x1-2=srgb(220,129,0)

# ④ LVGL 8.3 的 Image button 示例（spec §2.4）与 lv_imgbtn 的绘制
curl -sSfL https://raw.githubusercontent.com/lvgl/lvgl/release/v8.3/examples/widgets/imgbtn/lv_example_imgbtn_1.c
grep -n 'transform_width\|coords.x1 -= tw\|coords.x2 += tw\|for(i = coords_part.x1' lib/lvgl/src/extra/widgets/imgbtn/lv_imgbtn.c

# ⑤ 候选样式的渲染（spec §2.4、§2.5）：mock.c 链接测试工程已构建的 luckfox_app 与 tst_support
cd /tmp/pr15/mock
B=/workspace/build-tests
INC=$(grep C_INCLUDES $B/CMakeFiles/luckfox_screenshots.dir/flags.make | cut -d= -f2-)
cc -std=gnu99 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra $INC mock.c \
  $B/CMakeFiles/tst_support.dir/support/*.o $B/libluckfox_app.a -ldrm -lcjson -lpthread -lm -o mock
for r in 480 720; do
  for v in none black30 black30-backopa black30-backgray black30-backorange black30-shift black30-grow \
           others-none others others-checked-none others-checked; do
    ./mock $r $v $r-$v.ppm > /dev/null
  done
done
# 各按钮 64×64 区域内与未按下相比变化的像素数（480：上一首 76+342、下一首 340+342；720：114+513、510+513）
for r in 480 720; do
  if [ $r = 480 ]; then P=76+342; N=340+342; else P=114+513; N=510+513; fi
  for v in black30 black30-backgray black30-backopa black30-backorange; do
    printf "%s %-20s" $r $v
    for g in 0+0 $P $N; do
      printf " %s=%s" $g "$(compare -metric AE \( $r-none.ppm -crop 64x64+$g \) \( $r-$v.ppm -crop 64x64+$g \) null: 2>&1)"
    done; echo
  done
done
# 480/720 black30：0+0=0、上一首 3527、下一首 3536；其余三种：0+0=173，上一首、下一首同上
# transform_width 为 6 时上一首的三角形外框（相对 x=66, y=360）：+36 → +30，宽高 12x9 不变
for v in black30 black30-grow; do
  convert 480-$v.ppm -crop 80x30+66+360 +repage -colorspace gray -threshold 40% -negate -format "$v %@\n" info:
done
```

```c
/* 一次性 mockup（不入库）：进入音乐页，给按钮设候选的按下样式并置为 PRESSED，截图。
 * 用法：mock <480|720> <variant> <out.ppm>
 * variant：none | black30 | black30-backopa | black30-backorange | black30-backgray | black30-shift | black30-grow
 *          | others-none | others | others-checked-none | others-checked | main-released | main-pressed
 *          | mode-<loop|rnd>-<RRGGBB>-<opa> */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "app_env.h"
#include "fake_fs.h"
#include "gui_guider.h"
#include "lvgl.h"

extern int32_t fd_mpv;

static void recolor(lv_obj_t *o, uint32_t hex, lv_opa_t opa)
{
    lv_obj_set_style_img_recolor(o, lv_color_hex(hex), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_img_recolor_opa(o, opa, LV_PART_MAIN | LV_STATE_PRESSED);
}

int main(int argc, char **argv)
{
    int sv[2];
    const char *v;
    lv_obj_t *btns[3];

    if (argc != 4)
        return 2;
    v = argv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
        return 1;
    fd_mpv = sv[0];
    tst_mkdirs("music");
    tst_write_file("music/song_01.mp3", "");
    tst_app_init(atoi(argv[1]), 1, 1);
    tst_app_setup_ui();
    lv_event_send(guider_ui.Main_Music_btn, LV_EVENT_CLICKED, NULL);
    tst_run_ms(600);
    btns[0] = guider_ui.Music_player_pre_btn;
    btns[1] = guider_ui.Music_player_next_btn;
    btns[2] = guider_ui.Music_player_back_btn;
    if (strncmp(v, "main", 4) == 0) {
        /* 主屏 WIFI 按钮（lv_btn，默认主题）：main-released / main-pressed */
        lv_event_send(guider_ui.Music_player_back_btn, LV_EVENT_CLICKED, NULL);
        tst_run_ms(600);
        if (strcmp(v, "main-pressed") == 0)
            lv_obj_add_state(guider_ui.Main_Wifi_btn, LV_STATE_PRESSED);
        tst_run_ms(300);
        printf("WIFI btn coords: x1=%d y1=%d x2=%d y2=%d\n", guider_ui.Main_Wifi_btn->coords.x1,
               guider_ui.Main_Wifi_btn->coords.y1, guider_ui.Main_Wifi_btn->coords.x2, guider_ui.Main_Wifi_btn->coords.y2);
        return tst_save_ppm(argv[3]) == 0 ? 0 : 1;
    }
    if (strncmp(v, "others", 6) == 0) {
        btns[0] = guider_ui.Music_player_start_btn;
        btns[1] = guider_ui.Music_player_play_mode_btn;
        btns[2] = guider_ui.Music_player_music_list_btn;
        if (strcmp(v, "others-checked") == 0 || strcmp(v, "others-checked-none") == 0) {
            for (int i = 0; i < 3; i++)
                lv_obj_add_state(btns[i], LV_STATE_CHECKED);
        }
        if (strstr(v, "none") == NULL) {
            for (int i = 0; i < 3; i++)
                recolor(btns[i], 0x000000, LV_OPA_30);
        }
        for (int i = 0; i < 3; i++)
            lv_obj_add_state(btns[i], LV_STATE_PRESSED);
        tst_run_ms(300);
        return tst_save_ppm(argv[3]) == 0 ? 0 : 1;
    }
    if (strncmp(v, "mode-", 5) == 0) {
        /* mode-<loop|rnd>-<RRGGBB>-<opa>：播放模式按钮的候选按下着色，opa 为 0 时不按下 */
        char st[8];
        unsigned hex;
        int opa;
        lv_obj_t *m = guider_ui.Music_player_play_mode_btn;

        if (sscanf(v, "mode-%4[a-z]-%6x-%d", st, &hex, &opa) != 3)
            return 2;
        if (strcmp(st, "rnd") == 0)
            lv_obj_add_state(m, LV_STATE_CHECKED);
        if (opa > 0) {
            recolor(m, hex, (lv_opa_t)opa);
            lv_obj_add_state(m, LV_STATE_PRESSED);
        }
        tst_run_ms(300);
        return tst_save_ppm(argv[3]) == 0 ? 0 : 1;
    }
    if (strncmp(v, "black30", 7) == 0) {
        for (int i = 0; i < 3; i++)
            recolor(btns[i], 0x000000, LV_OPA_30);
    }
    if (strcmp(v, "black30-backopa") == 0)
        lv_obj_set_style_img_opa(btns[2], LV_OPA_40, LV_PART_MAIN | LV_STATE_PRESSED);
    if (strcmp(v, "black30-backorange") == 0)
        recolor(btns[2], 0xFFA300, LV_OPA_COVER);
    if (strcmp(v, "black30-backgray") == 0)
        recolor(btns[2], 0x808080, LV_OPA_COVER);
    if (strcmp(v, "black30-shift") == 0) {
        for (int i = 0; i < 3; i++)
            lv_obj_set_style_translate_y(btns[i], 2, LV_PART_MAIN | LV_STATE_PRESSED);
    }
    if (strcmp(v, "black30-grow") == 0) {
        for (int i = 0; i < 3; i++)
            lv_obj_set_style_transform_width(btns[i], 6, LV_PART_MAIN | LV_STATE_PRESSED);
    }
    if (strcmp(v, "none") != 0) {
        for (int i = 0; i < 3; i++)
            lv_obj_add_state(btns[i], LV_STATE_PRESSED);
    }
    tst_run_ms(300);
    return tst_save_ppm(argv[3]) == 0 ? 0 : 1;
}
```

```bash
# ⑥ 按住移出后松开仍发 CLICKED（spec §4）：所有对象默认 PRESS_LOCK，release 只在发生滚动时不发 CLICKED
grep -n 'obj->flags |= LV_OBJ_FLAG_PRESS_LOCK' lib/lvgl/src/core/lv_obj.c
grep -n 'static void indev_proc_release' -A35 lib/lvgl/src/core/lv_indev.c | grep -n 'PRESS_LOST\|CLICKED\|scroll_obj == NULL'
grep -n 'LV_EVENT_PRESS_LOST' -A1 lib/lvgl/src/core/lv_obj.c                # 清除 LV_STATE_PRESSED

# ⑦ 音乐页只建一次（spec §4 的 reenter）：返回主屏时 Music_player_del 置为 auto_del（false）
grep -n 'Music_player_del' generated/gui_guider.c generated/events_init.c
grep -n 'void ui_load_scr_animation' -A18 generated/gui_guider.c

# ⑧ 范围来源：PR #10 评论「尚未解决的 7 项」
timeout 60 gh api repos/yuangezhizao/luckfox_pico_lvgl_example/issues/comments/6319354295 --jq .body

# ⑨ 提交类型：gaoac/cz-conventional-emoji v1.1.8 把 :children_crossing: 映射为 ux；npm 包 1.1.8 的 repository 即该仓库，engine.js 与 v1.1.8 标签逐字节相同
# （未映射的 gitmoji 取 gitmoji 名，如 :hammer: → hammer，即仓库历史中的 hammer(tests)）
git clone -q https://github.com/gaoac/cz-conventional-emoji.git /tmp/pr15/gaoac
git -C /tmp/pr15/gaoac show v1.1.8:engine.js > /tmp/pr15/engine-v118.js
grep -n "':children_crossing:'\|':sparkles:': 'feat'\|':hammer:'" /tmp/pr15/engine-v118.js   # 只有前两行
git -C /tmp/pr15/gaoac show v1.1.8:index.js | grep -n -A6 'useGitmoji:'                       # 默认 true
curl -sSfL https://registry.npmjs.org/cz-conventional-emoji/-/cz-conventional-emoji-1.1.8.tgz | tar xzO package/engine.js \
  | cmp - /tmp/pr15/engine-v118.js && echo same
curl -sSfL https://registry.npmjs.org/cz-conventional-emoji/-/cz-conventional-emoji-1.1.8.tgz | tar xzO package/package.json \
  | grep -A2 '"repository"'
# gitmojis 版本取自该标签的 pnpm-lock.yaml（3.15.0）
git -C /tmp/pr15/gaoac show v1.1.8:pnpm-lock.yaml | grep -m1 -A2 '      gitmojis:'
curl -sSfL https://registry.npmjs.org/gitmojis/-/gitmojis-3.15.0.tgz | tar xzO package/dist/index.cjs \
  | grep -B3 -A4 'code: ":children_crossing:"'           # description: "Improve user experience / usability."，semver: "patch"

# ⑩ 第 1 轮真机复测后的播放模式分析（spec §5.1）：mock 的 mode 变体见 ⑤ 的源码，构建同 ⑤
cd /tmp/pr15/mock
for c in 000000-0 000000-76 000000-127 FFFFFF-76 FFFFFF-127; do
  for st in loop rnd; do ./mock 480 mode-$st-$c m-$st-$c.ppm > /dev/null; done
  printf "%-11s" $c
  for st in loop rnd; do
    printf " %s=%s" $st "$(convert m-$st-$c.ppm -crop 32x32+76+310 +repage -format %c histogram:info:- \
      | sort -rn | awk '$3 != "#FFFFFF" {print $3; exit}')"
  done; echo
done
# 000000-0    loop=#504D6D rnd=#504D6D   （未按下；帧缓冲中为 504d6c，ImageMagick 读出的 B 差 1，文档只引用 R）
# 000000-76   loop=#38364C rnd=#38364C   （第 1 轮的 30% 黑色）
# 000000-127  loop=#282636 rnd=#282636
# FFFFFF-76   loop=#848298 rnd=#848298
# FFFFFF-127  loop=#A7A5B5 rnd=#A7A5B5
convert 480-black30.ppm -crop 64x64+76+342 +repage -colorspace Gray -threshold 80% -negate -format "%@\n" info:      # 28x28+18+18
convert m-rnd-000000-0.ppm -crop 32x32+76+310 +repage -colorspace Gray -threshold 60% -negate -format "%@\n" info:  # 26x26+3+2
# 候选效果的拼图（spec §5.1）：每行左循环、右随机
rows=()
for c in 000000-0 000000-76 000000-127 FFFFFF-76 FFFFFF-127; do
  convert m-loop-$c.ppm m-rnd-$c.ppm -crop 48x48+68+302 +repage -scale 400% +append -gravity West -background white \
    -splice 300x0 -font DejaVu-Sans -pointsize 24 -annotate +8+0 "$c" mr-$c.png
  rows+=(mr-$c.png)
done
convert "${rows[@]}" -append -bordercolor gray -border 2 /tmp/pr15/mode-grid.png

# ⑪ 本 PR 各次 pull_request CI 的用时与改动（§5、spec §9）：run 按时间顺序，最后一个为最终提交
for id in 40292388675 40295762448 40459994875 40474511845 40476521758; do
  timeout 60 gh api repos/yuangezhizao/luckfox_pico_lvgl_example/actions/runs/$id/jobs \
    --jq '.jobs[] | [.name, .started_at, .completed_at] | @tsv' | python3 -c "
import sys, datetime as d
f = lambda s: d.datetime.fromisoformat(s.replace('Z', '+00:00'))
print('$id', ' | '.join('%s %ds' % (n, (f(b) - f(a)).total_seconds()) for n, a, b in (l.rstrip('\\n').split('\\t') for l in sys.stdin) if '变体' in n or '原生' in n))"
done
# 🧬 变体检验 882、1027、1051、871、1058 s（14 分 31 秒至 17 分 38 秒）；🧪 原生测试 154–161 s
# 每次推送相对上一次是否改了 .md 以外的文件（compare 按合并基点比较，只看 0 与非 0）
prev=1fb0cc6
timeout 60 gh run list --branch pi/music-press-feedback --limit 20 --json databaseId,headSha,event \
  --jq 'sort_by(.databaseId) | .[] | select(.event=="pull_request") | "\(.databaseId) \(.headSha)"' |
while read -r id sha; do
  echo "$id $(timeout 60 gh api "repos/yuangezhizao/luckfox_pico_lvgl_example/compare/$prev...$sha" \
    --jq '[.files[].filename | select(test("\\.md$") | not)] | length')"; prev=$sha
done
# 40292388675 0、40295762448 4、40459994875 4、40474511845 4、40476521758 0：首尾两次只改 .md，变体检验共 1940 s
```

### A.3 验证脚本

本机运行（Task 4）：先 `cmake --build build-tests --target preview`，桌面 `:1` 与无桌面各以 480、720 执行下列命令；`press-shots.sh` 的源码见下，交付的拼图只取桌面那一次。

```bash
for r in 480 720; do
  tests/tools/preview/start.sh $r            # 无桌面：env -u DISPLAY tests/tools/preview/start.sh --headless $r
  tests/tools/preview/check-music-list.sh && tests/tools/preview/check-wifi.sh && tests/tools/preview/shots.sh
  bash /tmp/pr15/press-shots.sh
  tests/tools/preview/stop.sh
done
convert /tmp/luckfox-preview/shots/{480,720}-press.png -background white -gravity North +append \
  /tmp/pr15/music_press_feedback_desktop_480_720.png
sha256sum /tmp/pr15/music_press_feedback_desktop_480_720.png
```

```bash
#!/usr/bin/env bash
# 音乐页按钮按下效果截图（plan Task 4，不入库）：依次按住上一首、下一首、播放模式（循环、随机）、播放（暂停时与播放中）、返回，
# 每个按钮截松开与按住两张，裁出按钮周围区域，拼成 <RES>-press.png（每行一个按钮，左松开、右按住，放大 3 倍）。
# 用法（仓库根目录，preview 已由 start.sh 启动并停在主屏）：bash /tmp/pr15/press-shots.sh；输出在 $PREVIEW_WORK/shots/。
set -euo pipefail
# shellcheck source=/dev/null
. tests/tools/preview/lib.sh

pv_need xdotool import convert compare
pv_load
M=8 # 裁剪时按钮四周多留的像素
OUT=$PREVIEW_WORK/shots/press
mkdir -p "$OUT"

# crop 名字 x y 边长：从窗口截图中裁出按钮区域（窗口像素坐标），写到 $OUT/<RES>-名字.png
crop() {
    local f
    f=$(pv_shot press-full)
    convert "$f" -crop "$(($4 + 2 * M))x$(($4 + 2 * M))+$(($2 - M))+$(($3 - M))" +repage "$OUT/$PV_RES-$1.png"
}

differs() { # differs a.png b.png：两图有不同的像素
    [ "$(compare -metric AE "$1" "$2" null: 2>&1 || true)" != 0 ]
}

# pressed_ready：截按住时的区域，与松开时不同即算画好（读 shoot 设的 name x y s）
pressed_ready() {
    crop "$name-down" "$x" "$y" "$s"
    differs "$OUT/$PV_RES-$name-up.png" "$OUT/$PV_RES-$name-down.png"
}

# shoot 名字 设计x 设计y 边长：位置按 RES/480 缩放，边长不缩放（生成代码用 lv_obj_set_size()）
shoot() {
    local cx cy
    name=$1 s=$4
    x=$(pv_sc "$2")
    y=$(pv_sc "$3")
    x=$((x < M ? M : x))
    y=$((y < M ? M : y))
    cx=$(($(pv_sc "$2") + s / 2))
    cy=$(($(pv_sc "$3") + s / 2))
    crop "$name-up" "$x" "$y" "$s"
    xdotool mousemove --window "$PV_WIN" "$cx" "$cy" mousedown 1
    pv_wait 5 pressed_ready || echo "$PV_RES $name：5 s 内按住与松开的截图没有差别" >&2
    xdotool mouseup 1
    convert "$OUT/$PV_RES-$name-up.png" "$OUT/$PV_RES-$name-down.png" -scale 300% +append \
        -gravity West -background white -splice 220x0 -font DejaVu-Sans -pointsize 22 -annotate +8+0 "$name" \
        "$OUT/$PV_RES-$name-row.png"
    rows+=("$OUT/$PV_RES-$name-row.png")
    sleep 0.5
}

rows=()
pv_wait_page main
"$PREVIEW_DIR/songs.sh" 4 >/dev/null
pv_tap 240 410 # 主屏 MUSIC
sleep 1.5
shoot prev 76 342 64
shoot next 340 342 64
shoot mode-loop 76 310 32    # 松开后切到随机
shoot mode-random 76 310 32  # 松开后切回循环
shoot play-paused 198 332 84 # 松开后开始播放（选中）
shoot play-playing 198 332 84
shoot back 0 0 64            # 松开后回到主屏
pv_wait_page main
convert "${rows[@]}" -background white -gravity West -append -bordercolor gray -border 2 \
    -gravity NorthWest -splice 0x36 -pointsize 26 -annotate +8+4 "$PV_RES: left released / right pressed" "$PREVIEW_WORK/shots/$PV_RES-press.png"
echo "$PREVIEW_WORK/shots/$PV_RES-press.png"
```

### A.4 辅助脚本与重建 `/tmp/pr15`

新环境中先手工建 `/tmp/pr15/extract.py`（源码见下），再用它从各 plan 附录中抽出脚本。`check-headings.py`、`check-samples.sh` 已由 PR #16 入库到 `tests/tools/docs/`，不再抽取。

```bash
mkdir -p /tmp/pr15/mock
X="python3 /tmp/pr15/extract.py"
# 交叉编译与交付：PR #10 plan 附录 C.4，交付目录改为当天（目录须先 mkdir -p）
$X docs/superpowers/plans/2026-10-10-lvgl-example-board-test-followups.md '^### C\.4 ' 1 /tmp/pr15/cross.sh
sed -i "s#board-retest-20261010#board-retest-$(date +%Y%m%d)#" /tmp/pr15/cross.sh
# 本 plan 的脚本：A.2 的 imgstat.py、mock.c，A.3 的 press-shots.sh，本节的其余 5 个
P=docs/superpowers/plans/2026-10-26-lvgl-example-music-press-feedback.md
$X $P '^### A\.2 ' 2 /tmp/pr15/imgstat.py
$X $P '^### A\.2 ' 4 /tmp/pr15/mock/mock.c
$X $P '^### A\.3 ' 2 /tmp/pr15/press-shots.sh
$X $P '^### A\.4 ' 3 /tmp/pr15/qref.py
$X $P '^### A\.4 ' 4 /tmp/pr15/swap-picks.py
$X $P '^### A\.4 ' 5 /tmp/pr15/reword-pick.py
$X $P '^### A\.4 ' 6 /tmp/pr15/push-and-body.sh

# 文档检查（Task 7）：前 3 个为 0；lint_copy_rules.py 来自作者安装的 Skill Fenng/tech-doc-style-chinese，只在本地运行
S=docs/superpowers/specs/2026-10-26-lvgl-example-music-press-feedback-design.md
python3 tests/tools/docs/check-headings.py $S $P docs/superpowers/plans/2026-10-10-lvgl-example-board-test-followups.md
bash tests/tools/docs/check-samples.sh
python3 /tmp/pr15/qref.py
python3 ~/.pi/agent/git/github.com/Fenng/tech-doc-style-chinese/scripts/lint_copy_rules.py $S $P

# 改写历史（§1）：E 去掉环境注入的作者变量
E="env -u GIT_AUTHOR_NAME -u GIT_AUTHOR_EMAIL -u GIT_COMMITTER_NAME -u GIT_COMMITTER_EMAIL"
GIT_SEQUENCE_EDITOR="python3 /tmp/pr15/swap-picks.py" $E git rebase -i 1fb0cc6          # 交换前两条 pick
$E git commit --fixup=<目标提交>                                                       # 修订并入目标提交
GIT_SEQUENCE_EDITOR=true $E git rebase -i --autosquash --autostash 1fb0cc6
GIT_SEQUENCE_EDITOR="python3 /tmp/pr15/reword-pick.py <短哈希>" GIT_EDITOR="cp <说明文件>" \
  $E git rebase -i --autosquash --autostash 1fb0cc6                                     # 改提交说明，可同时带 fixup
# 推送并更新 PR 正文（§1）：参数为远端旧提交的完整哈希
bash /tmp/pr15/push-and-body.sh "$(GIT_TERMINAL_PROMPT=0 timeout 60 git ls-remote \
  https://github.com/yuangezhizao/luckfox_pico_lvgl_example.git refs/heads/pi/music-press-feedback | cut -f1)"
```

```python
#!/usr/bin/env python3
"""从 Markdown 中提取某个标题之后的第 N 个代码块，写到文件。
用法：extract.py <md> <标题行的正则> <第几个代码块，从 1 起> <输出文件>
例：extract.py docs/superpowers/plans/2026-10-10-lvgl-example-board-test-followups.md '^### C\\.4 ' 1 /tmp/pr15/cross.sh"""
import re
import sys

md, head, nth, out = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4]
lines = open(md, encoding="utf-8").read().split("\n")
starts = [i for i, ln in enumerate(lines) if re.search(head, ln)]
if len(starts) != 1:
    sys.exit(f"标题正则匹配到 {len(starts)} 行，应为 1 行")
blocks, cur, fence = [], None, None
for ln in lines[starts[0] + 1:]:
    s = ln.lstrip()
    if cur is None and s.startswith("```"):
        fence = s[:len(s) - len(s.lstrip("`"))]
        cur = []
        continue
    if cur is not None:
        if s.startswith(fence) and s.strip("`") == "":
            blocks.append(cur)
            cur = None
            if len(blocks) == nth:
                break
            continue
        cur.append(ln)
if len(blocks) < nth:
    sys.exit(f"标题之后只有 {len(blocks)} 个代码块")
open(out, "w", encoding="utf-8").write("\n".join(blocks[nth - 1]) + "\n")
print(f"wrote {out}: {len(blocks[nth - 1])} lines")
```

```python
# 核对本 PR spec 的 QA 编号与引用（改自 PR #12 plan 附录 A.7 ⑨）：条目须为 Q1..QN 连续；spec、plan 与提交说明中的 Qn（含范围 Qa–Qb）须 ≤ N；
# 前 30 字内出现「PR #8/#10/#12」「其」或「会话」的视为外部引用或会话编号，单独列出供人工确认。用法：python3 qref.py [提交说明文件…]
import re, sys
S = 'docs/superpowers/specs/2026-10-26-lvgl-example-music-press-feedback-design.md'
files = [S, 'docs/superpowers/plans/2026-10-26-lvgl-example-music-press-feedback.md'] + sys.argv[1:]
qa = [int(x) for x in re.findall(r'^\*\*Q(\d+)\. ', open(S).read(), re.M)]
N = len(qa); bad = 0
if qa != list(range(1, N + 1)):
    print('QA 编号不连续:', qa); bad += 1
for f in files:
    for no, line in enumerate(open(f).read().split('\n'), 1):
        for m in re.finditer(r'(?<![\w.])Q(\d+)(?:–Q(\d+))?(?!\d)', line):
            ctx = line[max(0, m.start() - 30):m.start()]
            nums = [int(m.group(1))] + ([int(m.group(2))] if m.group(2) else [])
            if re.search(r'PR #(8|10|11|12|13|14)|其|会话', ctx):
                print('外部引用 %s:%d: …%s%s' % (f, no, ctx[-20:], m.group(0)))
            elif max(nums) > N:
                print('越界 %s:%d: %s' % (f, no, m.group(0))); bad += 1
print('QA %d 条，问题 %d 处' % (N, bad)); sys.exit(1 if bad else 0)
```

```python
#!/usr/bin/env python3
"""rebase -i 的 sequence editor：交换前两条 pick（把 docs 提交挪到 ux 提交之后）。"""
import sys

path = sys.argv[1]
lines = open(path, encoding="utf-8").read().split("\n")
picks = [i for i, l in enumerate(lines) if l.startswith("pick ")]
assert len(picks) == 2, lines
a, b = picks
lines[a], lines[b] = lines[b], lines[a]
open(path, "w", encoding="utf-8").write("\n".join(lines))
```

```python
#!/usr/bin/env python3
"""GIT_SEQUENCE_EDITOR（配合 --autosquash）：把指定短哈希那一行的 pick 改为 reword，fixup 行保持不动。
用法：reword-pick.py <短哈希> <todo 文件>"""
import sys

sha, todo = sys.argv[1], sys.argv[2]
lines = open(todo, encoding="utf-8").read().splitlines(keepends=True)
hit = 0
for i, line in enumerate(lines):
    parts = line.split()
    if len(parts) >= 2 and parts[0] == "pick" and parts[1].startswith(sha):
        lines[i] = "reword" + line[len("pick"):]
        hit += 1
if hit != 1:
    sys.exit(f"expected exactly one pick {sha}, got {hit}")
open(todo, "w", encoding="utf-8").writelines(lines)
```

```bash
#!/usr/bin/env bash
# 登录 yuangezhizao 后立即执行：推送本地 pi/music-press-feedback（force-with-lease 远端旧值），核对远端，更新 PR #15 正文。
# 用法（仓库根目录）：bash /tmp/pr15/push-and-body.sh <远端旧提交完整哈希>
set -euo pipefail
OLD=$1
URL=https://yuangezhizao@github.com/yuangezhizao/luckfox_pico_lvgl_example.git
timeout 30 gh auth status 2>&1 | grep -q 'account yuangezhizao' || { echo "gh 未登录 yuangezhizao" >&2; exit 2; }
git cat-file -p HEAD | sed -n 3,4p
GIT_TERMINAL_PROMPT=0 timeout 120 git push --force-with-lease=pi/music-press-feedback:"$OLD" "$URL" \
    pi/music-press-feedback:pi/music-press-feedback
REMOTE=$(GIT_TERMINAL_PROMPT=0 timeout 60 git ls-remote "$URL" refs/heads/pi/music-press-feedback | cut -f1)
[ "$REMOTE" = "$(git rev-parse HEAD)" ] || { echo "远端 $REMOTE 与本地 HEAD 不同" >&2; exit 1; }
echo "远端=$REMOTE"
timeout 90 gh pr edit 15 --body-file /tmp/pr15/pr-body.md
```
