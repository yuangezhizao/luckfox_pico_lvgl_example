# Luckfox Pico LVGL Example — preview 脚本入库设计规格

- **日期**：2026-10-19
- **状态**：已落地，以临时 apt 方式推送的 CI 与两份镜像的本机验证通过，验证证据见 plan
- **分支**：`pi/preview-tools`（起点 `origin/dev` `c13283c`，即 PR #13 合入）
- **范围来源**：PR #13 spec [`2026-10-18-lvgl-example-wifi-spinner-design.md`](2026-10-18-lvgl-example-wifi-spinner-design.md) §8.1「整理 `preview` 的启动与操作方法」与其 Q10
- **关联计划**：[`../plans/2026-10-19-lvgl-example-preview-tools.md`](../plans/2026-10-19-lvgl-example-preview-tools.md)

---

## 1. 概述与目标

`tests/` 的手动目标 `preview` 用 SDL 开窗显示例程界面，鼠标当触摸。启动、换歌曲、切换 WiFi 状态、点击与截图核对的脚本散在 PR #10 plan 附录 C.5 与 PR #13 plan 附录 A.4 中，要用时得先从 Markdown 中提取（§2.1）。

目标：

1. 把 `preview` 的启动、操作与核对脚本放进 `tests/tools/preview/`（§3.1），桌面与无桌面用同一套脚本（§3.2）。
2. 用法写进 `tests/README.md`，`AGENTS.md` 只加一句指引（§3.7）。
3. CI 新增 ShellCheck job，并在 `native-tests` 中以无桌面方式实际运行两个核对脚本（§3.6）。
4. 脚本依赖的 `imagemagick`、`xdotool`、`xvfb` 开发期间在 CI 中临时 apt 安装，脚本验证通过后挪进 `.cursor/Dockerfile` 与 `.cursor/Dockerfile.luckfox_pico`（§3.5）。

**不在本规格范围**：`preview.c` 与测试工程的 C、CMake 代码；一次性的核对脚本（`percommit.sh`、`cross.sh`、`reword.py`、PR #9 的 `mesa-probe.sh` 等）仍留在各 plan 附录（Q1）。

## 2. 背景

### 2.1 现状

`origin/dev` `c13283c` 上提到 `preview` 的文档（`git grep -c preview`，plan 附录 A.2 ①）：

| 位置 | 文件数 | 次数 | 内容 |
|---|---|---|---|
| `AGENTS.md` | 1 | 1 | 一句构建与运行命令 |
| `tests/README.md`「手动目标」 | 1 | 1 | 环境变量与限制 |
| `docs/superpowers/specs/` | 5 | 31 | 设计背景（编译单元、Mesa 等） |
| `docs/superpowers/plans/` | 6 | 68 | 含下列脚本 |

`tests/tools/` 只有 `compile_check.cmake`、`mutate.sh`、`preview.c`、`screenshots.c`。可复用的 `preview` 脚本都在 plan 附录中：

| 来源 | 脚本 | 作用 |
|---|---|---|
| PR #10 plan 附录 C.5 | `pv-start.sh` | 在桌面（`DISPLAY=:1`）启动 `preview`，音乐目录放 N 首 |
| 同上 | `pv-songs.sh` | 把音乐目录改成 N 首 |
| 同上 | `pv-check.sh` | 核对音乐页列表面板位置与列表按钮高亮 |
| 同上 | `preview_run.sh` | 在 Xvfb 下截主屏、WiFi 页与画板 |
| PR #13 plan 附录 A.4 | 假 `wpa_cli` 与启动命令 | 按状态文件回答 `wpa_cli status` |
| 同上 | `pv-wifi-check.sh` | 核对 WiFi 页圆环与连接图标的显隐 |
| PR #9 plan 附录 A.4 | `mesa-probe.sh`、`gl.c` | 在 Docker 容器中探测 `preview` 加载的 Mesa 库（一次性） |

桌面与无桌面是两套写法：`pv-check.sh` 按窗口坐标点击（`xdotool mousemove --window`）、按窗口截图；`preview_run.sh` 自起 Xvfb `:99`，按根窗口坐标点击、截根窗口再裁剪。480 屏与 720 屏的坐标在 PR #10 plan 中是两张表。

### 2.2 现有脚本的实测（2026-10-19）

在 `c13283c` 上按 PR #10 plan 附录 C.1 构建 `preview`，从 plan 中提取脚本原样运行（plan 附录 A.2 ②）：

| 脚本 | 显示 | 结果 |
|---|---|---|
| `pv-check.sh 480`、`720` | 云端桌面 `:1` | 展开与再展开时按钮中心 `srgb(255,224,168)`、面板底边 480 屏 305、720 屏 458；点背景与按按钮收起后按钮中心 `srgb(182,176,187)`、面板底边「无」；与 PR #10 plan 附录 C.5 的预期相同 |
| `preview_run.sh` | Xvfb `:99` | 4 张截图均为 480×480，主屏、WiFi 页、点 Scan 后的红色提示与画板笔迹都正常 |
| `pv-wifi-check.sh` | 云端桌面 `:1` | 圆环区与图标区依次为 1/1、67/1、1/18、1/1，与 PR #13 plan §4 相符 |

ShellCheck 0.9.0：上述 4 个 PR #10 脚本共 13 条 note（SC2086 11 条、SC2012 2 条），没有 warning 与 error；仓库已有的 `tests/tools/mutate.sh`、`.cursor/install.sh`、`.cursor/start.sh` 为 0 条。

### 2.3 工具的来源

`c13283c` 上的情况；Dockerfile 的行号也按该提交（本 PR 改动后第 54 行变为第 55 行）。runner 一列按 `actions/runner-images` 的 `Ubuntu2404-Readme.md`，没有列出即为「无」（plan 附录 A.2 ⑤）：

| 工具 | `.cursor/Dockerfile` | Cloud Agent 平台 Aptfile | GitHub `ubuntu-24.04` runner |
|---|---|---|---|
| `libsdl2-dev` | 有 | 无 | 无 |
| `xdotool` | 无（第 28 行只是平台清单的注释） | 有 | 无 |
| `ffmpeg`、`x11-utils`（`xwd`）、`tigervnc-standalone-server` | 无 | 有 | 无 |
| `imagemagick` | 无（第 54 行注释说明 `--no-install-recommends` 避免拉上它） | 无 | 无 |
| `xvfb` | 无 | 无 | 有 |
| `shellcheck` | 无 | 无 | 有（0.9.0） |

本机的 Xvfb、ImageMagick 与 ShellCheck 是之前的会话手动安装的（`/var/log/apt/history.log`）。平台的 Xtigervnc 可以充当无桌面的 X 服务器：`Xtigervnc :98` 下 `preview` 正常开窗，截图正确（plan 附录 A.2 ③）。`native-tests` 等容器 job 用的 CI 镜像由 `.cursor/Dockerfile` 构建，没有平台 Aptfile 的包，所以 CI 容器中既没有 `xdotool`，也没有 X 服务器。

### 2.4 坐标与缩放

生成代码以 480 屏为设计尺寸。`luckfox_lv_obj_set_pos()`、`luckfox_lv_obj_set_size()` 按 `SCALE = 分辨率 / 480` 缩放，`lv_obj_set_size()` 不缩放（plan 附录 A.2 ④）：

| 控件 | 位置 | 尺寸 | 480 屏中心 | 720 屏中心 |
|---|---|---|---|---|
| 主屏 WIFI 按钮 | (5, 370)，缩放 | 90×80，缩放 | (50, 410) | (75, 615) |
| 音乐页列表按钮 | (139, 310)，缩放 | 32×32，不缩放 | (155, 326) | (224, 481) |
| 音乐页返回按钮 | (0, 0) | 64×64，不缩放 | (32, 32) | (32, 32) |
| WiFi 页连接图标 | (375, 15)，缩放 | 64×64，不缩放 | — | — |

所以 720 屏的坐标可以由 480 设计坐标换算，只有不缩放的尺寸要单独处理。PR #10 plan 中 720 屏的列表按钮 (224, 481) 即「缩放后的左上角 + 16」。

## 3. 设计（以 §7 QA 结论为准）

### 3.1 目录与文件（Q1、Q2）

`tests/tools/preview/`，`preview.c` 留在 `tests/tools/` 原处，不改 C 与 CMake：

| 文件 | 来源 | 作用 |
|---|---|---|
| `lib.sh` | 新增 | 公共函数：会话文件、坐标换算、找窗口、点击、拖动、截图、取像素、数颜色、轮询等待 |
| `start.sh` | `pv-start.sh` | 启动 `preview`；无 `DISPLAY` 或加 `--headless` 时自起 X 服务器 |
| `stop.sh` | 新增 | 结束本会话的 `preview` 与自起的 X 服务器 |
| `songs.sh` | `pv-songs.sh` | 把音乐目录改成 N 首 |
| `wifi.sh` | PR #13 附录 A.4 | 切换假 WiFi 状态 |
| `bin/wpa_cli` | PR #13 附录 A.4 | 假 `wpa_cli` |
| `shots.sh` | `preview_run.sh` | 截主屏、WiFi 页、点 Scan 后与画板 |
| `check-music-list.sh` | `pv-check.sh` | 核对列表面板位置与列表按钮高亮 |
| `check-wifi.sh` | `pv-wifi-check.sh` | 核对 WiFi 页圆环与连接图标 |

「辅助脚本写在 plan 附录、不入库」的做法只对 `preview` 工具例外（Q1）。

### 3.2 会话与运行方式（Q3）

- `start.sh [--headless] [480|720] [歌曲数]`：默认 480 屏、4 首。有 `DISPLAY` 且未加 `--headless` 时用当前显示（云端桌面为 `:1`）；否则从 `:99` 起找一个空闲的显示号，有 Xvfb 用 Xvfb，没有时用 Xtigervnc。
- 会话目录 `PREVIEW_WORK`（默认 `/tmp/luckfox-preview`）：`preview` 的 `TMPDIR`、日志、假 WiFi 的状态文件与调用记录、截图（`shots/`），以及记录显示号、分辨率、进程号与窗口号的 `session`。其他脚本读 `session` 工作，不再各自假定显示号与临时目录。`start.sh` 会清空会话目录中的 `tmp/`、`wifi/`，所以先确认目录归本脚本所有：规范化路径后拒绝 `/` 与仓库内的路径，只使用不存在、为空或带标记文件 `.luckfox-preview-workdir`（普通文件）的目录，首次使用时创建标记，做法同 `tests/tools/mutate.sh`（合并前按 Codex 审查增加，plan Task 10）。
- 构建目录 `PREVIEW_BUILD`，默认仓库根目录的 `build-tests`（与 `tests/README.md` 一致）。`PREVIEW_BACKLIGHT` 原样传给 `preview`。
- 无桌面时可用 `PREVIEW_XSERVER=Xvfb|Xtigervnc` 指定 X 服务器，用于两种都装了的环境逐一验证。
- `start.sh` 等到窗口与临时目录 `luckfox-tests-*` 都出现，最多 30 s（CI runner 上曾有一次 15 s 内窗口还没出现）：SDL 窗口先于界面创建，临时目录在界面初始化时才创建。窗口按 `_NET_WM_PID` 与名字同时匹配（`xdotool search --all`；不加 `--all` 时两个条件是「或」）；找不到时只考虑启动后新出现的同名窗口，恰好一个时改用它并打印两个 PID，启动前已有的同名窗口（手动启动或上次遗留的 `preview`）一律排除；超时时打印诊断（进程、同名窗口及其 PID、`preview.log` 末尾）。成功启动前任何一处退出（连不上显示、等不到窗口、认不出主屏等）都由 EXIT trap 结束本次启动的进程：已写会话文件时交给 `stop.sh`，否则直接结束 `preview` 与自起的 X 服务器（排除旧窗口与统一清理按 Codex 审查增加，plan Task 10）。看到主屏后再等 2 s 才返回：`tst_app_setup_ui()` 先运行 1.5 s 才进入 SDL 事件循环，主屏在此期间已画出，这时的点击，按下与抬起在同一轮事件循环中处理，LVGL 看不到按下。
- 所有点击按窗口坐标（`xdotool mousemove --window`），按住约 150 ms 再抬起；所有截图按窗口（`import -window`）。桌面与无桌面行为相同。翻页后按 (230, 400) 与 (384, 400) 两点的颜色判断当前页面（主屏、WiFi 页、画板），480、720 下颜色相同（plan 附录 A.3 ⑥）。
- `start.sh` 先调用 `stop.sh` 结束上一个会话；只结束 `session` 中记录的进程，不用 `pkill -f`。

### 3.3 坐标（Q8）

脚本中只写 480 设计坐标，`lib.sh` 按 `RES / 480` 取整换算，与 `luckfox_lv_obj_set_pos()` 的取整一致；不缩放的尺寸单独处理（§2.4），如列表按钮中心为「缩放后的左上角 + 16」，音乐页返回按钮固定 (32, 32)。核对脚本中实测得到的预期值（面板底边 305、458）按屏幕分别写死。

### 3.4 假 `wpa_cli` 与核对脚本（Q10、Q11、Q16）

- `start.sh` 默认把 `bin/` 放在 `PATH` 最前，并导出 `PREVIEW_WIFI_DIR`；状态文件不存在时像连不上 wpa_supplicant 一样只写 stderr、以 255 退出，存在时按其中的 `wpa_state` 回答 `status`；`scan_results` 返回两个假热点，`scan` 回答 `OK`，交互模式读完标准输入后退出。行为与 PR #13 plan 附录 A.4 相同。`wifi.sh SCANNING|COMPLETED|…|unreachable` 切换状态，最多 5 s 后由 WiFi 页的轮询生效。
- 核对脚本从主屏开始、结束时回到主屏，自带预期值：符合时打印实际值并以 0 退出，不符时打印预期与实际并以 1 退出，截图留在 `shots/`。
- 等待用轮询：每次操作后反复截图检查，满足预期即继续，超时（点击 10 s；WiFi 状态切换受 5 s 轮询周期限制，15 s）才判失败。不再用固定的 `sleep 1.2`、`sleep 5.5`。

| 脚本 | 步骤 | 预期 |
|---|---|---|
| `check-music-list.sh` | 放 4 首，进音乐页，依次展开、点背景、再展开、按按钮收起 | 展开时按钮中心 `srgb(255,224,168)`、面板底边 480 屏 305 / 720 屏 458；收起时按钮中心 `srgb(182,176,187)`、无面板底边 |
| `check-wifi.sh` | 连不上时进 WiFi 页，依次切到 `SCANNING`、`COMPLETED`、连不上 | 进入后立即调用 `wpa_cli status`；圆环区与图标区的颜色数依次为 1/1、>1/1、1/>1、1/1（1 表示没有内容） |

### 3.5 依赖与 Dockerfile（Q4–Q7）

分两个阶段：

1. **开发期间**：Dockerfile 不改。本机与 CI 都用 apt 临时安装 `imagemagick xdotool xvfb`；`tests/README.md` 写明安装命令，`start.sh` 缺工具时报错并提示该命令。PR 的 CI 复用 dev 签名的镜像，核对脚本在 CI 中实际运行。
2. **脚本验证通过后、合并前**：把这三个包加入 `.cursor/Dockerfile` 与 `.cursor/Dockerfile.luckfox_pico`（新增一组「preview 工具」，组内按字母序），同步改写 `c13283c` 第 54 行关于 imagemagick 的注释，删掉 workflow 中的 apt 步骤（Q18）。

整理提交时，Dockerfile 的提交放在 CI 的提交之前，最终的提交历史中不出现临时 apt 步骤；以临时 apt 方式通过的 CI 只以 run 记在 plan 中（作者要求，plan Task 10）。

第 2 阶段的提交推送后 `build-image` 按预期失败（镜像标签取 Dockerfile 内容哈希，`AGENTS.md`），`build-demo`、`native-tests`、`mutation-tests` 因 `needs: build-image` 被跳过。合并门槛相应调整（Q5）：

- 以临时 apt 方式推送的提交（Dockerfile 与 dev 相同），CI 全部通过，`🧬 变体检验` 为 `0 skipped`，preview 核对步骤通过；
- 本机用 Docker 构建两份镜像，在 `.cursor/Dockerfile` 的镜像中跑全量 `ctest`，并以无桌面方式运行全部 `preview` 脚本；另按 PR #9 plan 附录 A.4 的挂载方式在容器中使用宿主 `:1` 运行一次；`.cursor/Dockerfile.luckfox_pico` 的镜像核对三个工具可用，并以无桌面方式运行两个核对脚本；
- ShellCheck job 不依赖 `build-image`，Dockerfile 提交上照常通过；
- 合并后检查 dev push 的 CI，记入 plan。

Environment Build 在合并后重建才含这三个包，之前启动的 Agent 仍要手动安装，`tests/README.md` 的安装命令保留，写给旧环境与 Cloud Agent 以外的环境。

### 3.6 CI（Q9、Q14、Q15、Q17、Q18）

- **ShellCheck job**（`🐚 ShellCheck`）：在 `ubuntu-24.04` runner 上直接运行，不用容器、不依赖 `build-image`，`schedule` 时跳过（与其他 job 相同）。检查 `git ls-files` 列出的 `*.sh` 与 `tests/tools/preview/bin/wpa_cli`，`shellcheck -x -P SCRIPTDIR` 跟随 `source`，默认级别（含 style），要求 0 条。它只做静态检查，不执行脚本。
- **`native-tests` 新增步骤**「🖼️ preview 脚本核对（480/720）」：接在 `ctest` 之后，复用 `build-tests` 构建 `preview`；480、720 各执行一次 `start.sh --headless`、`check-music-list.sh`、`check-wifi.sh`、`shots.sh`、`stop.sh`。开发期间该步骤之前有一个临时 apt 安装步骤，第 2 阶段删掉；最终的提交历史中没有它（§3.5）。
- **失败时**：上传 `PREVIEW_WORK/shots/` 与日志（`preview.log`、`xserver.log`、`wifi/cmd.log`）为 artifact `preview-shots`，保留 7 天，没有文件时忽略；成功时不上传。

### 3.7 文档（Q13）

- `tests/README.md`「手动目标」新增「preview 脚本」小节：依赖与安装、会话目录、各脚本用法、设计坐标表与 CI 中的运行方式；合并前按作者要求补上何时运行与核对的覆盖范围、修改或新增脚本时用的 `lib.sh` 函数与验证方法、排查（plan Task 10）。合并前作者另要求给 `tests/README.md` 的标题加序号（`## 1.`–`## 8.`、`### 7.1`），单独作为一个提交。
- `AGENTS.md`：「运行 / 验证 GUI」保留现有一句，加一句指向 `tests/tools/preview/` 与 `tests/README.md`；CI 一节加入 ShellCheck job 与 preview 核对步骤；Dockerfile 一节在第 2 阶段同步。
- PR #10 plan 附录 C.5 开头加一行状态，指向新目录；正文不改。PR #13 spec §8.1 正文改为状态行，链接本 spec 与 PR #13 合并时（`c13283c`）的原文，做法同 PR #12 对 PR #10 plan §6.5 的处理。PR #13 plan 附录 A.4 开头同样加一行状态（Q13 的补充）。

### 3.8 不做的事

- 不改 `preview.c` 与测试工程的 C、CMake（Q2），不需要跑全量 `mutate.sh`；CI 的变体检验照常运行。
- 不迁移 `mesa-probe.sh` 等一次性脚本（Q1）。
- 截图不进仓库（Q12）。

## 4. 验证

1. **ShellCheck**：本机 `shellcheck -x -P SCRIPTDIR` 对全部脚本为 0 条。
2. **云端桌面 `:1`**：480、720 各运行一次全部脚本，核对脚本退出码 0，数值与 §2.2 的旧脚本结果相同。
3. **无桌面**：Xvfb 与 Xtigervnc 各运行一次（`--headless`），结果同上。
4. **失败路径**：本机故意改错一个预期值，核对脚本以 1 退出并留下截图；CI 中推送一个故意失败的临时提交，确认步骤失败、截图 artifact 上传，然后丢弃该提交。
5. **全量 `ctest`**（`-DLUCKFOX_TESTS_ARM32=ON`）通过。
6. **CI**：以临时 apt 方式推送时，5 个原有 job 与 ShellCheck 全部通过，`🧬 变体检验` 为 `0 skipped`，preview 核对步骤通过。
7. **镜像**：§3.5 第 2 阶段的本机 Docker 验证。
8. **截图**（Q12）：桌面、无桌面、Docker 镜像各一张拼图，存到 `/cursor/stores/self/preview-tools-20261019/`，同目录 `SHA256SUMS`，交付前先在 `/tmp` 核对哈希；同时复制到 `/opt/cursor/artifacts/`。作者拖进 PR 评论；plan 只记数值、文件名与 SHA256。
9. **文档**：PR #11 plan 附录 A.1 的 `check-headings.py`、附录 A.5 的 `check-samples.sh`；QA 编号与引用按 PR #13 plan 附录 A.2 ⑦ 的方法核对。
10. **合并后**：检查 dev push 的 CI（镜像重建、preview 核对使用镜像中的工具）。

## 5. 提交与 PR

- 分支 `pi/preview-tools`，PR #14 指向 `dev`，开发时为草稿，满足门槛后改为 ready；执行者不合并。
- 提交（plan §2）：`hammer(tests)` 脚本与用法；`chore(cloud-env)` 两份 Dockerfile；`ci(github-actions)` ShellCheck job 与核对步骤；`docs(tests)` `tests/README.md` 的标题加序号；`docs(superpowers)` 本 spec、plan 与三处历史文档的状态行。开发期间 `ci` 在 `chore` 之前并含临时 apt 步骤，合并前按作者要求改为现在的顺序（§3.5）。
- PR 标题：`hammer(tests): 🔨 preview 启动、操作与核对脚本入库`。提交说明按 cz-conventional-emoji 1.1.8，正文为要点、写原因，最后一个要点为 `- 详见 spec §…、Q…`；不带 `Co-authored-by`。

## 6. 风险

- **CI 中的时序**：runner 负载波动时截图与点击的时序可能变化；以轮询等待缓解（Q16），失败时有截图与日志可查（Q17）。实际发生过一次：runner 上 `preview` 15 s 内没有开窗，`start.sh` 的等待因此改为 30 s 并在超时时打印诊断（§3.2，plan §4）。
- **合并前看不到最终形态的 CI**：第 2 阶段提交的 `build-image` 失败，三个容器 job 被跳过；以本机镜像验证与合并后的 dev CI 覆盖（Q5）。两个阶段装的是同一 Ubuntu 源的同一批包。
- **镜像变大**：`imagemagick` 及其依赖进入 CI 镜像与 Cloud Agent 环境。同一时间构建对比，`.cursor/Dockerfile` 的镜像增大 34,055,696 字节，`.cursor/Dockerfile.luckfox_pico` 增大 41,648,105 字节（plan §4）；CI 中临时 apt 安装为 47 个包。
- **截图的颜色依赖渲染**：预期值来自 SDL 软件渲染与 LVGL 的绘制结果；LVGL、生成代码或 SDL 版本变化时需要重新核对。
- **ShellCheck 版本**：runner 的 shellcheck 随镜像更新，新版本可能多报；届时按新提示修改或在脚本中注明原因。

## 7. QA（grilling 记录）

问句与推荐取自问答原文；「答」是作者回答经润色后的版本。会话中共三轮：第 1 轮 10 问，作者追问第 4 问 B 与 C 的区别；第 2 轮为 Dockerfile 的 3 个子问题与 ShellCheck 的澄清；第 3 轮为截图的修订与核对脚本进 CI 的 4 个子问题。会话中的子问题（如「Q4.1」）在此按出现顺序连续编号为 Q5–Q7、Q15–Q18。合并前作者又问其他 PR 的文档中是否还有需要入库的脚本，记为 Q13 的补充；提交顺序与改写历史的问答按约定不记入 QA，经过见 plan Task 10。

**Q1. 迁移范围**

- 问：可迁移的有 PR #10 附录 C.5 的 4 个脚本、PR #13 附录 A.4 的假 `wpa_cli` 与 `pv-wifi-check.sh`、PR #9 附录 A.4 的 `mesa-probe.sh`。A 只迁移启动与操作脚本；B 再加两个核对脚本与无桌面截图；C 再加 `mesa-probe.sh`。「辅助脚本不入库」是否只对 preview 工具例外？
- 推荐：B。`mesa-probe.sh` 要临时装 Docker、是一次性探测，留在 PR #9 plan；`percommit.sh`、`cross.sh`、`reword.py` 等照旧留在 plan。
- 答：按推荐（§3.1）。

**Q2. 目录名、脚本名，以及与 `preview.c` 的关系**

- 问：A `tests/tools/preview/`，`preview.c` 不动；B `tests/preview/`；C 把 `preview.c` 也移进去（要改 CMake、跑全量 `mutate.sh`）。脚本名去掉 `pv-` 前缀。
- 推荐：A，命名见 §3.1。
- 答：按推荐。

**Q3. 桌面与无桌面是否合并**

- 问：A 合并，`start.sh` 默认用当前 `DISPLAY`，无 `DISPLAY` 或 `--headless` 时自起 X 服务器（有 Xvfb 用 Xvfb，否则用 Xtigervnc），其余脚本按窗口坐标与窗口截图；B 保留两套。
- 推荐：A。
- 答：按推荐（§3.2）。

**Q4. 依赖与 `.cursor/Dockerfile`**

- 问：A 把 xvfb、imagemagick 加入 Dockerfile（合并前 `build-image` 必然失败）；B 不改 Dockerfile，只用 Cloud Agent 已有的工具（`xwd | ffmpeg` 截图、python3 取像素、Xtigervnc）；C 不改 Dockerfile，用 ImageMagick 与 Xvfb，README 写明先 apt 安装。
- 推荐：B，新会话免安装。
- 补充（作者问「可以用 ImageMagick，B 与 C 的区别是什么」后）：区别只在是否用 ImageMagick 与 Xvfb。B 免安装，代价是改写取色逻辑；C 沿用原脚本写法，每个新环境先装一次。修订推荐为 C′：用 ImageMagick，无桌面时优先用平台的 Xtigervnc，只需装 imagemagick。
- 答：开发期间选 C，脚本验证通过、准备合并前把安装挪进 Dockerfile，即选 A（§3.5）。

**Q5. 改 Dockerfile 后的合并门槛**

- 问：Dockerfile 提交上 `build-image` 失败，三个容器 job 被跳过，与「CI 通过才改为 ready」冲突。A 前一提交 CI 全绿，Dockerfile 提交在本机用 Docker 构建镜像，在镜像中跑 `ctest` 并实际运行脚本，合并后再看 dev CI；B 只看前一提交的 CI。
- 推荐：A，即 PR #9 的 V2、V5 做法；`--no-install-recommends` 下工具是否完整也要在镜像中实际验证。
- 答：按推荐（§3.5）。

**Q6. Dockerfile 加哪些包**

- 问：A imagemagick 与 xvfb 都加；B 只加 imagemagick，无桌面时用平台的 Xtigervnc。
- 推荐：A，不依赖平台 Aptfile。
- 补充（作者决定核对脚本进 CI 后）：`xdotool` 是平台 Aptfile 装的，Dockerfile（`c13283c`）第 28 行只是注释，CI 容器中没有；修订为 `imagemagick xdotool xvfb`，新增「preview 工具」一组，按字母序。
- 答：按修订后的推荐（§3.5）。

**Q7. `.cursor/Dockerfile.luckfox_pico` 是否同步**

- 问：A 同步加；B 只改当前活动的 Dockerfile。
- 推荐：A，保持两份对等；备选镜像不参与 CI，本机构建一次确认能装上。
- 答：按推荐。

**Q8. 480 与 720 的坐标**

- 问：A 一张 480 设计坐标表，按 `RES/480` 换算，不缩放的尺寸单独处理；B 照旧两张表。
- 推荐：A，实测的预期值（面板底边 305、458）仍按屏幕分别写死。
- 答：按推荐（§3.3）。

**Q9. ShellCheck**

- 问：A 只在本机检查；B workflow 新增轻量 job，在 runner 上直接运行（预装 shellcheck 0.9.0），检查全部脚本，要求 0 条；C 不加。
- 推荐：B。这些脚本不在 CI 中运行，ShellCheck 是唯一的自动门禁。
- 补充（作者问「这个 workflow 难道不是在 CI 中运行的吗」后）：ShellCheck job 在 CI 中运行，但只静态检查脚本源码，不执行脚本；「不在 CI 中运行」指 preview 脚本本身（Q14 之后改为在 CI 中实际运行核对脚本）。该 job 不依赖 `build-image`，Dockerfile 提交上也照常运行。
- 答：按推荐（§3.6）。

**Q10. 假 `wpa_cli`**

- 问：A `start.sh` 默认把假 `wpa_cli` 放在 `PATH` 最前，初始为连不上，用 `wifi.sh` 切换，`scan_results` 返回两个假热点；B 加参数才启用；C 不入库。
- 推荐：A。
- 答：按推荐（§3.4）。

**Q11. 核对脚本的形式**

- 问：A 自带预期值，不符时以 1 退出并打印实际值；B 照旧只打印。
- 推荐：A。
- 答：按推荐（§3.4）。

**Q12. 截图怎样记入 plan**

- 问：A 截图提交进仓库；B plan 只记数值，PNG 存到 `/cursor/stores` 并附 `SHA256SUMS`；C 两者都做。
- 推荐：A。
- 补充（作者表示截图不进仓库，先存在项目存储，再上传到 PR 或评论后）：GitHub 没有上传评论附件的 API，gh 令牌没有 `gist` 权限，执行者无法自动贴图。修订为：存到 `/cursor/stores/self/preview-tools-20261019/` 并附 `SHA256SUMS`，同时复制到 `/opt/cursor/artifacts/`；每种运行方式一张拼图，作者拖进 PR 评论；plan 只记数值、文件名与 SHA256。
- 答：按修订后的推荐（§4 第 8 条）。

**Q13. 文档改动范围**

- 问：`tests/README.md` 新增小节；`AGENTS.md` 加一句指引，CI 一节同步；PR #10 plan 附录 C.5 加状态行；PR #13 spec §8.1 由作者事后改，还是由本 PR 一并改为状态行。
- 推荐：全部执行，PR #13 spec §8.1 由本 PR 一并改为状态行，PR #13 plan 附录 A.4 保持原样。
- 补充（合并前作者问「其他 PR 的文档中是否还有需要入库的脚本」后）：筛选全部历史 spec 与 plan 的代码块（plan 附录 A.2 ⑥），入库的只有 PR #10 plan 附录 C.5 与 PR #13 plan 附录 A.4 的脚本；PR #7 plan Task 6 只有一行 `preview` 启动命令，由 `start.sh` 覆盖，原命令仍可用，PR #6 plan 只在正文中提到；PR #9 plan 附录 A.4 的 `mesa-probe.sh` 是一次性探测，按 Q1 留在原处。推荐 PR #13 plan 附录 A.4 也加一行状态，与 PR #10 plan 附录 C.5 一致。
- 答：按推荐（§3.7），PR #13 plan 附录 A.4 加状态行。

**Q14. 核对脚本是否在 CI 中实际运行**

- 问：Dockerfile 装上工具后，可以在 `native-tests` 中以 `--headless` 运行核对脚本。A 本 PR 就加，但合并前无法在 PR 的 CI 中验证；B 本 PR 不加，记为后续事项；C 不做。
- 推荐：B。
- 答：本 PR 就加，但不走 A 的方式：开发期间在 CI 中先用 apt 安装，实际运行核对脚本，确认没有问题后再把安装挪进 Dockerfile（Q4），合并后 CI 正常运行，也省去另开 PR。执行者确认可行（§3.5、§3.6）。

**Q15. 核对步骤放在哪里**

- 问：A 在 `native-tests` 中接在 `ctest` 之后，复用构建目录；B 新开一个 job。
- 推荐：A，与最慢的 `mutation-tests` 并行，CI 总时长基本不变。
- 答：按推荐（§3.6）。

**Q16. 等待方式**

- 问：A 轮询，满足预期即继续，超时才判失败；B 保留固定等待，只加大时长。
- 推荐：A。
- 答：按推荐（§3.4）。

**Q17. 失败时的输出**

- 问：A 失败时打印实际与预期，并上传截图 artifact，保留 7 天；B 只打印数值（与 `mutation-tests` 一致）。
- 推荐：A，只在失败时上传。
- 答：按推荐（§3.6）。

**Q18. 挪进 Dockerfile 后，workflow 中的 apt 步骤**

- 问：A 删除，工具只来自镜像；B 改成缺少时才安装。
- 推荐：A，B 会永久留下多余的兜底；风险由本机镜像验证与合并后的 dev CI 覆盖。
- 答：按推荐（§3.5）。
