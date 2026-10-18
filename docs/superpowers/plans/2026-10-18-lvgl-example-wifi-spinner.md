# Luckfox Pico LVGL Example — WiFi 页圆环启停实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** WiFi 页圆环按 `wpa_cli status` 的 `wpa_state` 分三组启停，连不上 wpa_supplicant 时停止（N7）；圆环初始隐藏，每次进入 WiFi 页立即查询状态；点 Load 后立即显示连接中；代码只改 `custom/custom_wifi.c`。

**Architecture:** 先推送 spec 与 plan、开草稿 PR（Task 1）；按 TDD 分三个 `fix` 提交：状态分组（Task 2）、初始隐藏与进入页面立即查询（Task 3）、Load 后显示连接中（Task 4，合并前按 Codex 审查插入）；PR #10 plan §6.3 加状态行、`AGENTS.md` 去掉耗时（Task 5）；本地全量验证（Task 6）；交付补测二进制，作者真机复测（Task 7）；CI（Task 8）；整理提交、精简 plan、PR 正文（Task 9）；合并前自查（Task 10）。

**Tech Stack:** C（gnu99）、LVGL 8.3、CMake/CTest、gcc + ASan/UBSan、`tests/tools/mutate.sh`、ARM 交叉工具链（glibc、uClibc）、`qemu-arm`、`gh`。

**Spec:** [`docs/superpowers/specs/2026-10-18-lvgl-example-wifi-spinner-design.md`](../specs/2026-10-18-lvgl-example-wifi-spinner-design.md)

---

## 1. Global Constraints

- 分支 `pi/wifi-spinner`，起点 `origin/dev` `1272564`（PR #12 合入）；PR #13 指向 `dev`，开发时为草稿，合并前改为 ready，执行者不合并。
- 代码只改 `custom/custom_wifi.c` 与 `tests/`，另有文档（含 `AGENTS.md`，spec Q12）；不改 `generated/`、`.cursor/Dockerfile`、workflow（spec Q4）。
- 提交身份 `Cursor Agent <cursoragent@cursor.com>`，SSH 签名；改写提交的 git 命令前加 `env -u GIT_AUTHOR_NAME -u GIT_AUTHOR_EMAIL -u GIT_COMMITTER_NAME -u GIT_COMMITTER_EMAIL`，推送前 `git cat-file -p HEAD | sed -n 3,4p` 核对。
- 提交信息按 cz-conventional-emoji 1.1.8（gaoac/cz-conventional-emoji）：`<type>(<scope>): <emoji> <subject>`，正文为要点、写原因、每行不超过 100 字符，`fix` 提交注明历史问题，最后一个要点为 `- 详见 spec §…、Q…`（与 PR #10–#12 相同，附录 A.6 的 `msg-check.py` 核对）；文档中不写本 PR 自身提交的哈希，补测二进制等产物的文件名除外。
- 推送：`GIT_TERMINAL_PROMPT=0 timeout 120 git push https://yuangezhizao@github.com/yuangezhizao/luckfox_pico_lvgl_example.git ...`，改写历史时写明 `--force-with-lease=pi/wifi-spinner:<远端旧提交>`；`gh` 命令加 `timeout`；更新 PR 正文前先 `gh pr view --json body` 取回当前正文。
- 后台检验（全量 `mutate.sh`、交叉编译）先后执行，不并行；检验期间不在工作区切换分支，探针用 `git worktree`；等待后台进程不用 `pgrep -f`、`pkill -f`。
- 文档中的数字都用命令重新算出，写明提交与命令（附录 A.2）。

## 2. 修复 Task 通用步骤

1. 只加测试与 `.mutants` 变体，修复前运行，确认失败，失败输出记入 §4。
2. 加修复，同一测试通过；`tests/tools/mutate.sh <id>...` 全部 `KILLED`。
3. 全量 `ctest --test-dir /tmp/bt --output-on-failure -j8`（含 arm32）通过，结果记入 §4；它代替逐提交测试（spec Q9）。
4. 测试与修复同一个 `fix` 提交，标题带「WiFi [n/3]」（Task 4 插入前为 [n/2]）。

## 3. Tasks

### Task 1：spec 与 plan，推送并开草稿 PR ✅

- [x] 写 spec（QA 当时为 Q1–Q8）与本 plan，`check-headings.py`（附录 A.3）为 0、Mermaid 校验通过；提交 `docs(superpowers)`，推送，开草稿 PR #13。

### Task 2：圆环按 `wpa_state` 分组启停（WiFi [1/3]，spec Q1、Q3、Q6）✅

文件：`custom/custom_wifi.c`、`tests/cases/wifi/wifi_env.h`、`tests/cases/wifi/test_status.c`、`tests/cases/wifi/status.mutants`、`tests/cases/wifi/CMakeLists.txt`

- [x] 假 `wpa_cli` 加 `status` 分支；加 6 条用例与 10 个 `WIFI-N7-*` 变体（spec §4）；修复前 5 条失败（§4）。
- [x] 修复后 6 条通过，变体全部 `KILLED`，全量 `ctest` 通过。
- [x] 按 spec Q11 把 `timer_unreachable_stops` 改为只等一次轮询，fixup 并入本提交，按 spec Q9 的例外单独跑全量 `ctest`。

### Task 3：圆环初始隐藏，进入 WiFi 页立即查询（WiFi [2/3]，spec Q4）✅

文件：`custom/custom_wifi.c`、`tests/cases/wifi/test_status.c`、`tests/cases/wifi/status.mutants`、`tests/cases/wifi/CMakeLists.txt`

- [x] 加 `initial_hidden`、`enter_queries_now` 与 3 个变体；修复前两条失败（§4）。
- [x] 修复后两条通过，变体全部 `KILLED`，全量 `ctest` 通过。

### Task 4：点 Load 后立即显示圆环（WiFi [3/3]，spec Q14）✅

文件：`custom/custom_wifi.c`、`tests/cases/wifi/test_status.c`、`tests/cases/wifi/status.mutants`、`tests/cases/wifi/CMakeLists.txt`

- [x] 探针确认 Codex 审查（P2）指出的问题：进入时未连接、点 Load 后圆环一次都不出现（spec §3.3、附录 A.7）。
- [x] 加 `load_shows_spinner` 与 3 个变体；修复前失败（§4）。
- [x] `wifi_status_connecting()`：Load 执行到 `wpa_cli reconfigure &` 后按连接中显示并 `lv_timer_reset()`；用例通过，变体全部 `KILLED`，全量 `ctest` 通过。
- [x] 新提交放在 WiFi [2/3] 之后，前两个 `fix` 改为 [1/3]、[2/3]（附录 A.6 的 `order-commits.py`、`reword.py`），各提交树不变。

### Task 5：PR #10 plan §6.3 状态行与 `AGENTS.md` ✅

文件：`docs/superpowers/plans/2026-10-10-lvgl-example-board-test-followups.md`、`AGENTS.md`

- [x] PR #10 plan §6.3 标题不动，正文改为状态行，链接本 spec 与 PR #10 合并时（`85a6dd48264938b267fc0e4ecd458f6ac14f0530`）的原文；锚点在该提交的 GitHub 渲染结果中存在（附录 A.2 ⑥）。
- [x] `AGENTS.md` 去掉变体检验的具体耗时（spec Q12），workflow 注释不改。

### Task 6：本地全量验证 ✅

- [x] 全量 `ctest`（`-DLUCKFOX_TESTS_ARM32=ON`）、全量 `mutate.sh`（PR #10 plan 附录 C.2）、两条交叉编译与 `qemu-arm` 冒烟（`cross.sh`，附录 A.3）；不做逐提交测试（spec Q9）。Task 4 之后重跑一遍（§4）。
- [x] `preview` 中用假 `wpa_cli` 切换状态核对（附录 A.4）。

### Task 7：补测二进制交付与真机复测（作者执行）✅

- [x] `cross.sh` 编译、冒烟；项目存储恢复后把 uClibc 产物以 `luckfox_lvgl_demo-pr13-<WiFi [2/2] 的短哈希>` 交付到 `/cursor/stores/self/board-retest-20261018/`，`sha256sum -c SHA256SUMS` 为 `OK`；之后提交哈希两次变化，按附录 A.5 核对产物不变并以新名字复制。
- [x] 作者执行 W1–W4 并追加 Scan 补测，全部通过，结果回填 spec §5.2.1；结果目录补 `SHA256SUMS` 并核对。
- [x] 交付含 WiFi [3/3] 的二进制（附录 A.5）。
- [x] 作者执行 W5、W6（spec §5.2，Q15），全部通过，结果回填 spec §5.2.2；结果 zip 由作者存入项目存储并附 `SHA256SUMS`，核对 `OK`（附录 A.5）。

### Task 8：CI ✅

- [x] 推送后 5 个 job 通过，`🧬 变体检验` 为 `133 run, 0 skipped`，耗时见 §4（PR #12 plan 附录 A.5 的命令）。
- [x] Task 4 之后：5 个 job 通过，`136 run, 0 skipped`（§4，附录 A.8 的 `ci-wait.sh`）。

### Task 9：整理提交、精简 plan、PR 正文 ✅

- [x] `git rebase` 整理为 spec §6 的五个提交，`docs(superpowers)` 在最后（附录 A.6）；每次整理前后树相同，身份与签名正确；WiFi [1/3]（当时为 [1/2]）只在 spec Q11 时改过测试，已按 spec Q9 的例外单独跑全量 `ctest`。
- [x] `check-headings.py`、`check-samples.sh`、Mermaid 校验与 QA 编号核对（附录 A.2 ⑦）；PR 正文按 PR #10–#12 的格式写（标题编号 `## 1.`）。

### Task 10：合并前自查 ✅

- [x] 第 1 轮：按作者审查模板由执行者担任 A（代码与构建）、B（事实与复现）、C（规范与表述），每项给脚本证据，只查不改，结果在会话中报告作者：无 Critical、Important，最终提交的 CI 5 项通过；2 条 Minor（PR 正文与本 plan 以 job 总时长说明 Q11 的效果，未区分 runner 波动；本 plan Goal 写「只改 `custom/custom_wifi.c`」，与改了 `AGENTS.md` 不一致），作者确认修复。
- [x] 满足门槛后把 PR 改为 ready，不合并。
- [x] 作者要求核对提交说明：对照 cz-conventional-emoji 1.1.8 源码、gitmojis 3.15.0、Conventional Commits 1.0.0 与 PR #10–#12 的提交，4 条格式都满足；与 PR #10–#12 不一致 2 处（「详见」单独成段、`fix` 未注明历史问题）、数字口径 1 处、可补充 1 处（Q11），作者确认后用 `reword.py` 改写，各提交树不变（§5、附录 A.6）。
- [x] 作者要求完整复核 spec 与 plan，并把用过、未入库的脚本写进本 plan：更新过时与前后不一致的说法，补附录 A.4 的截图核对与 A.5–A.8。
- [x] Codex 审查（P2）：核实问题存在，作者按 spec Q14 决定修复，插入 Task 4。
- [x] 推送后 CI 通过，回复 Codex 的评论（原因、修改、验证）并标记已解决（附录 A.8）。
- [x] 作者要求第 2 次完整复核 spec 与 plan（WiFi [3/3] 之后）：更新过时的编号、计数与说法，补附录 A.6 的 `reword-map-renumber.py` 与 A.8 的 `ci-wait.sh`、Codex 评论的查询与回复命令。
- [x] 合并前准备：PR 已为 ready、可合并，CI 通过；plan 已符合精简规则，不再重写；spec 状态行缩短为 PR #1–#4 的写法；不再做完整的 A/B/C 审查，只对 WiFi [3/3] 做增量只读核查（未发现问题），并由作者请 Codex 复审最新提交（未发现问题）。
- [x] 作者确认删去附录中多余的 2 个脚本：`move-docs-last.py`（被 `order-commits.py` 覆盖，且新增 `fix` 提交时会排错）、`reword-map-from-git.py`（实际改写时未用到）；A.7 第一段注明须在修复前的构建上运行。

## 4. 验证证据

| 项 | 结果 |
|---|---|
| 起点（`origin/dev` `1272564`） | 全量 `ctest` 117/117（arm32 2 条）；变体 120 个（附录 A.2 ④） |
| 探针（spec §2.4） | 无输出时圆环保持显示；`DISCONNECTED`、`INTERFACE_DISABLED`、`4WAY_HANDSHAKE` 沿用上一次的显示（附录 A.2 ②） |
| Task 2 修复前 | 新增 6 条中 5 条失败、`completed_shows_icon` 通过：`unreachable_stops` 连不上后圆环仍显示、`COMPLETED` 后连不上图标仍显示；`idle_states_stop` 从 `SCANNING` 切到 `DISCONNECTED` 圆环仍显示、从 `COMPLETED` 切过去图标与标签仍显示；`connecting_states_spin` 从 `COMPLETED` 切到 `AUTHENTICATING` 圆环不显示、图标仍显示；`state_line_only` 两种输入都按 `ssid=` 行中的文字显示；`timer_unreachable_stops`（改写前的两次等待写法）第一次轮询后圆环显示，改为连不上后第二次轮询仍显示 |
| Task 2 修复后 | WiFi 33/33；`WIFI-N7-*` 10 个与 PR #8 的 `WIFI-23`、`WIFI-C4` 全部 `KILLED`（`WIFI-N7-popen-fail-down` 由 `wifi.popen.failure` 杀死）；全量 123/123（arm32 2 条） |
| Task 3 修复前 | `initial_hidden`：创建页面后圆环显示；`enter_queries_now`：进入 300 ms 内 `wpa_cli status` 0 次，圆环显示、图标隐藏 |
| Task 3 修复后 | 全量 125/125（arm32 2 条）；`WIFI-N7-initial-spins`、`-no-enter-query`、`-enter-reset` 全部 `KILLED` |
| Codex 探针（Task 4） | 进入时 `DISCONNECTED`、点 Load 后改为 `COMPLETED`：WiFi [2/3] 上 Load 之后圆环一次都不出现；`origin/dev` 上圆环一直在转（N7 遗留）；修复后的 HEAD 点 Load 后立即显示圆环，轮询读到 `COMPLETED` 后显示图标（附录 A.7） |
| Task 4 修复前 | `load_shows_spinner`：点 Load 后圆环仍隐藏；1 s 内查询了一次（原本快到期的轮询） |
| Task 4 修复后 | 全量 126/126（arm32 2 条）；`WIFI-N7-load-no-spinner`、`-load-no-reset`、`-load-ready` 与 `WIFI-23` 全部 `KILLED`；`-load-no-reset` 第一版只删 `if` 的语句体，编译失败（`BUILD FAILED`），改为连同 `if` 一起删除 |
| 全量变体检验（Task 6，`timer_unreachable_stops` 改写前） | 本机 8 核：`baseline green: 125 tests; running 133 mutants`，133 个全部 `KILLED`，`133 run, 0 skipped`，rc=0，516 s |
| 交叉编译（Task 6） | glibc rc=0 告警 121、uClibc rc=0 告警 120，与 `AGENTS.md` 基线相同；uClibc 产物 `qemu-arm` 冒烟 exit=1、`cannot open /dev/dri/card0` |
| 全量变体检验与交叉编译（Task 4 之后） | 本机 8 核：`baseline green: 126 tests; running 136 mutants`，136 个全部 `KILLED`，`136 run, 0 skipped`，rc=0，478 s；glibc rc=0 告警 121、uClibc rc=0 告警 120；uClibc 产物 `qemu-arm` 冒烟 exit=1、`cannot open /dev/dri/card0` |
| `preview`（附录 A.4） | WiFi [2/2] 的代码树，480 屏：进入 WiFi 页时立即调用 `wpa_cli status`；连不上时圆环区颜色数 1（无圆环）；`SCANNING` 时 64（圆环转动）、图标区 1；`COMPLETED` 时圆环区 1、图标区 18（显示图标）；改回连不上后两区都为 1；圆环区的颜色数随动画帧变化（复核时为 67），大于 1 即有圆环 |
| 补测二进制（Task 7） | `/cursor/stores/self/board-retest-20261018/luckfox_lvgl_demo-pr13-8838e2e`，SHA256 `b9a822ecb14b68ae815ba2edfa7d6821e850bc94a5cb229753248427577a6a0d`，同目录 `SHA256SUMS` 核对 `OK`；整理提交前后两次编出的 uClibc 产物哈希相同；最初以 `-pr13-fc4c066` 交付；`timer_unreachable_stops` 改写后 WiFi [2/2] 的哈希变化（除 `tests/`、`docs/` 外树相同），复制为 `-pr13-d46d953`；按 cz-conventional-emoji 与 PR #10–#12 的写法改写提交说明后（树不变）再复制为 `-pr13-8838e2e`，三份 SHA256 相同 |
| 补测二进制（Task 4 之后） | `/cursor/stores/self/board-retest-20261018/luckfox_lvgl_demo-pr13-a329c22`（WiFi [3/3]，用于 W5、W6），SHA256 `3e1440c3ba244b62d70649daa0295cb819329683db9954400ad8ca805e20b258`，同目录 `SHA256SUMS` 4 份全部 `OK` |
| 真机复测 W1–W4（Task 7） | W1–W4 与追加的 Scan 补测全部通过（spec §5.2.1）；结果 zip 在项目存储 `board-retest-results-20261018/pr13-fc4c066/`，SHA256 `899928a82f9427648a079db0d75756ec92528a8648b92a8166838f0d047fca0a`，同目录 `SHA256SUMS` 核对 `OK`，解开后与同目录的 `board-retest-out-pr13/` 一致；含测试热点凭据，不进仓库（附录 A.5） |
| 真机补测 W5、W6（Task 7） | 全部通过（spec §5.2.2）；结果 zip 在项目存储 `board-retest-results-20261018/pr13-a329c22/`，SHA256 `2daf986a8026ad05a0dce0d37bde0992d781475b86c495bca9ac756fbc58754d`，同目录 `SHA256SUMS` 核对 `OK`，解开后与同目录的 `board-retest-out-pr13/` 一致；板上 `wpa_cli` 仍为 v2.6；含测试热点凭据，不进仓库（附录 A.5） |
| 首次 CI（Task 8） | 5 个 job 全部通过；`🧬 变体检验` 输出 `baseline green: 125 tests; running 133 mutants`、`133 run, 0 skipped`，job 15 分 15 秒，逐条变体共 840 s，其中 13 个 `WIFI-N7-*` 共 175 s（每个约 13.5 s，主要是 `timer_unreachable_stops` 的两次 5 s 等待） |
| `timer_unreachable_stops` 改为只等一次轮询（spec Q11） | 用例 10.2 s → 5.17 s；改动后的 WiFi [1/2] 单独全量 123/123，在其上换回 `origin/dev` 的 `custom_wifi.c` 时该用例失败（连不上后圆环仍显示）；13 个 `WIFI-N7-*` 全部 `KILLED`，本机 106 s；改动后 CI 运行两次（推送 `docs(agents)` 前后），5 个 job 都全部通过、`133 run, 0 skipped`：`WIFI-N7-*` 合计 104 s、115 s（改动前 175 s）；其余变体 574 s、668 s（改动前 665 s），job 12 分 36 秒、14 分 21 秒，总时长的差别来自 runner 波动（附录 A.2 ⑧） |
| 合并前自查第 1 轮（Task 10，WiFi [3/3] 之前） | 用例 125 条（WiFi 35）、WiFi [1/2] 单独 123 条（WiFi 33）；变体 133 个（N7 13 个）；`generated/` 修改提交 10 个；补测二进制构建输入与 HEAD 相同；`check-headings.py` 24 份 0 处、`check-samples.sh` 68 个全部符合、QA 13 条 0 处；本 PR 文档、PR 正文与评论中没有测试热点的名称与密码 |
| 提交说明（Task 10） | 改写前 4 条都满足 cz-conventional-emoji 1.1.8 的格式：emoji 为 U+1F41B、U+1F4DD，与 gitmojis 3.15.0 的 `:bug:`、`:memo:` 一致，标题最长 63 字符；PR #10–#12 的提交以 `- 详见` 作最后一个要点、PR #10 的 `fix` 提交注明历史问题（`history-scan.py`）。改写后 `msg-check.py` 4 条 OK，改写前后各提交的树相同（附录 A.6） |
| 第 1 次复核（Task 10，WiFi [3/3] 之前） | 附录 A.4–A.8 的命令在当前提交上重新运行：`move-docs-last.py`、`reword-map-from-git.py`（两者后来删去，见 §5）、`msg-check.py`、`typo-check.py`（spec、plan 均 0 处）、`history-scan.py`、`pv-wifi-check.sh` 与项目存储的 `sha256sum -c` 均符合；本 PR 文档中本 PR 的提交哈希只出现在产物文件名中 |
| WiFi [3/3] 之后的 CI（Task 8） | `docs(superpowers)` 推送后 5 个 job 全部通过；`🧬 变体检验` 输出 `baseline green: 126 tests; running 136 mutants`、`136 run, 0 skipped`，job 15 分 7 秒，逐条变体共 828 s，其中 16 个 `WIFI-N7-*` 共 151 s、其余 677 s（附录 A.2 ⑧） |
| Codex 评论（Task 10） | 回复写明原因、修改、验证，所在讨论串 `isResolved` 为 true（附录 A.8）；作者在 W5、W6 之后留言 `@codex review`，Codex 复审 WiFi [3/3] 之后的提交：代码审查未发现主要问题，自动触发的安全审查未发现安全问题 |
| 第 2 次复核（Task 10，WiFi [3/3] 之后） | 用例 126 条（WiFi 36）、WiFi [1/3] 单独 123 条；变体 136 个（N7 16 个）；`check-headings.py` 24 份 0 处、QA 15 条 0 处、`typo-check.py` 0 处、`msg-check.py` 5 条 OK；附录新增的 `reword-map-renumber.py`、`ci-wait.sh` 与评论查询命令均实际运行 |
| 第 2 次复核后的 CI | 5 个 job 全部通过；`136 run, 0 skipped`，job 14 分 42 秒，逐条变体共 801 s，其中 `WIFI-N7-*` 147 s、其余 654 s（附录 A.2 ⑧） |
| 最终提交的 CI | 见 PR #13 的 Checks |

## 5. 与计划的偏离

- Task 4：合并前 Codex 审查指出未连接时点 Load 不显示圆环，作者按 spec Q14 决定修复，按位置插入 Task 4，原 Task 4–9 顺延为 Task 5–10；新提交为 WiFi [3/3]，前两个 `fix` 由 [1/2]、[2/2] 改为 [1/3]、[2/3]，二进制与真机补测随之增加（spec §5.2 的 W5、W6）。
- Task 6：作者在实施中提出不必再做逐提交测试（spec Q9），删去 `percommit.sh` 一步，改为每个 `fix` 提交创建前的全量 `ctest`，整理提交前后核对各提交的树相同。
- Task 7：第一次交付时项目存储 `/cursor/stores/self` 能列出但为空，写文件与建目录都报 `No such file or directory`；先在 `/tmp` 编好并核对哈希，稍后再查时存储已恢复（PR #10 时的目录重新可见），交付成功。交付前先整理提交，使二进制名中的哈希即当时的 WiFi [2/2]；之后 spec Q11 的测试改写与提交说明改写使哈希两次变化，产物不变，以新名字复制（§4）。
- Task 8：首次 CI 的变体检验为 15 分 15 秒，作者按 spec Q11 把 `timer_unreachable_stops` 改为只等一次轮询，用 fixup 并入 WiFi [1/2]，按 spec Q9 的例外对该提交单独跑全量 `ctest`。
- 提交说明：合并前作者要求核对提交说明；对照 cz-conventional-emoji 1.1.8 源码、gitmojis 3.15.0、Conventional Commits 1.0.0 与 PR #10–#12 的提交后，「详见」改为最后一个要点、`fix` 提交注明历史问题、`docs(agents)` 的 PR #12 耗时改为 job 口径、WiFi [1/2] 补 Q11 一条，用 PR #10 plan 附录 C.7 的 `reword.py` 改写，各提交的树不变。
- 范围：实施中作者提出在 spec §8.1 记录 `preview` 的整理建议并在 PR 发评论（spec Q10），plan 附录 A.4 补了查看 WiFi 页的假 `wpa_cli`；按 spec Q12 增加 `AGENTS.md` 的改动与 `docs(agents)` 提交；真机复测时作者追加 Scan 补测，下拉框为空的观察只记录（spec Q13）。
- 复核：作者两次要求完整复核 spec 与 plan，并把用过、未入库的脚本写进本 plan（`/tmp` 在环境重建后清空）；第 1 次补附录 A.5–A.8 与 A.4 的截图核对（原先只写了引用的 `percommit.sh`、`archive.sh` 本 PR 未用），第 2 次在 WiFi [3/3] 之后补 A.6、A.8 的新增命令；之后作者确认删去 2 个多余脚本（`move-docs-last.py`、`reword-map-from-git.py`，见 Task 10）。
- 环境：开草稿 PR 与发评论时集成令牌报 `Resource not accessible by integration`，作者执行 `gh auth login` 后成功；环境刷新令牌后登录会失效，发评论前作者又登录了一次。

## 附录 A：未入库的脚本与命令

在仓库根目录执行；主机测试的构建目录为 `/tmp/bt`，脚本放在 `/tmp/ws/`。本 PR 用过的脚本与命令都在本附录，或在附录 A.3 指向的已入库 plan 中（PR #10 plan 附录 C 的 `cross.sh`、`reword.py`，PR #11、#9 plan 的文档检查脚本），按 A.3 的方法提取，不复制到本文件。

### A.1 环境准备

```bash
cmake -S tests -B /tmp/bt -DLUCKFOX_TESTS_ARM32=ON && cmake --build /tmp/bt -j"$(nproc)" && ctest --test-dir /tmp/bt -j"$(nproc)"
# uClibc 工具链：按 AGENTS.md 稀疏检出 yuangezhizao/luckfox-pico@dev 的 tools/linux/toolchain 到 /tmp/luckfox-pico
sudo apt-get install -y python3-markdown-it                       # check-headings.py
sudo apt-get install -y --no-install-recommends wpasupplicant     # 只为附录 A.2 ① 观察 wpa_cli 的输出
sudo apt-get install -y imagemagick                               # 附录 A.4 截图核对；xdotool、libsdl2-dev 已在 .cursor/Dockerfile
# node 与 npm（附录 A.3 的 Mermaid 校验、A.6 的 npm pack）：.cursor/Dockerfile 不含；本次用的是 pi 自带的 ~/.local/share/pi-node（v22），其他环境需自行安装
```

### A.2 实测与核对命令

| 数字或结论 | 环境与提交 | 命令 |
|---|---|---|
| `wpa_cli` v2.10 连不上时 stdout 为空、stderr 有错误、退出码 255（spec §2.3） | 本机，Ubuntu 24.04 的 `wpasupplicant` | ① |
| 探针：各状态下圆环与图标的显隐（spec §2.4） | `1272564` 的临时工作树 | ② |
| `00974b5` 之后 10 个提交改过 `generated/`；`wifi_app_init()` 中的隐藏来自 `00974b5`，生成代码只隐藏键盘（spec §2.6） | `1272564` | ③ |
| 全量 `ctest` 117/117、arm32 2 条、变体 120 个 | `1272564` | ④ |
| 提到 `preview` 的文档：`AGENTS.md` 1、`tests/README.md` 1、4 份 spec 共 22、5 份 plan 共 52；`tests/tools/` 只有 `preview.c`（spec §8.1） | `1272564` | ⑤ |
| PR #10 plan 原文中 §6.3 的锚点存在 | `85a6dd48264938b267fc0e4ecd458f6ac14f0530` | ⑥ |
| spec 的 QA 为 Q1–Q15 连续，spec、plan 与提交说明中本 PR 的 Qn 都指向存在的条目 | 本 PR | ⑦ |
| CI 中 `WIFI-N7-*` 与其余变体各自的耗时 | GitHub Actions 日志 | ⑧ |

```bash
# ① wpa_cli 连不上时的输出
wpa_cli -v | head -1
for c in "status" "-i wlan0 status" "-i wlan0 scan"; do
  out=$(wpa_cli $c 2>/tmp/err); echo "[$c] rc=$? stdout=[$out] stderr=[$(cat /tmp/err)]"; done

# ② 探针：临时工作树中加 status 分支与 test_probe.c（源码见下），只构建这一个目标
git worktree add -q --detach /tmp/wt-probe 1272564 && cd /tmp/wt-probe
sed -i 's#\*scan_results\*) cat#status) cat \x27%s/status.txt\x27 2>/dev/null ;;\\n*scan_results*) cat#; s#dir, dir, dir, dir, dir);#dir, dir, dir, dir, dir, dir);#' tests/cases/wifi/wifi_env.h
echo 'luckfox_add_test(probe UNIT unit_wifi.c CASES run)' >> tests/cases/wifi/CMakeLists.txt
cmake -S tests -B /tmp/bt-probe >/dev/null && cmake --build /tmp/bt-probe -j8 --target wifi_probe >/dev/null
/tmp/bt-probe/cases/wifi/wifi_probe run | grep spinner
cd - && git worktree remove --force /tmp/wt-probe

# ③ generated/ 的修改历史与初始隐藏的出处
git log --format='%h %ad %s' --date=short 00974b5.. -- generated/ | wc -l
git blame -L '/^void wifi_app_init/,/^}/' custom/custom_wifi.c | grep HIDDEN
git show 00974b5:custom/custom_wifi.c | sed -n '/^void wifi_app_init/,/^}/p' | tail -3
git show 00974b5:generated/setup_scr_WIFI.c | grep -c HIDDEN

# ④ 起点的用例数与变体数
ctest --test-dir /tmp/bt -j8 | grep -E 'tests passed|arm32'
grep -h -c '^id:' tests/cases/*/*.mutants | paste -sd+ | bc

# ⑤ 提到 preview 的文档与 tests/tools/ 的内容
git grep -c preview 1272564 -- AGENTS.md tests/README.md 'docs/superpowers/specs/*.md' 'docs/superpowers/plans/*.md'
for d in specs plans; do git grep -c preview 1272564 -- "docs/superpowers/$d/*.md" | awk -F: -v d=$d '{s+=$3} END{print d, s}'; done
git ls-tree --name-only 1272564 tests/tools/

# ⑥ PR #10 plan 原文锚点（取该提交的 GitHub 渲染结果）
gh api -H 'Accept: application/vnd.github.html' \
  "repos/yuangezhizao/luckfox_pico_lvgl_example/contents/docs/superpowers/plans/2026-10-10-lvgl-example-board-test-followups.md?ref=85a6dd48264938b267fc0e4ecd458f6ac14f0530" |
  python3 -c 'import sys,re,urllib.parse as u; print([u.unquote(i) for i in re.findall(r"id=\"user-content-(63[^\"]*)\"", sys.stdin.read())])'

# ⑦ QA 编号核对：把下面的 qref.py 存到 /tmp/ws/，git log --format=%B origin/dev.. > /tmp/msg.txt 后运行 python3 /tmp/ws/qref.py /tmp/msg.txt
```

```python
# 核对本 PR spec 的 QA 编号与引用（改自 PR #12 plan 附录 A.7 ⑨）：条目须为 Q1..QN 连续；spec、plan 与提交说明中的 Qn（含范围 Qa–Qb）须 ≤ N；
# 前 30 字内出现「PR #8/#10/#12」「其」或「会话」的视为外部引用或会话编号，单独列出供人工确认。用法：python3 qref.py [提交说明文件…]
import re, sys
S = 'docs/superpowers/specs/2026-10-18-lvgl-example-wifi-spinner-design.md'
files = [S, 'docs/superpowers/plans/2026-10-18-lvgl-example-wifi-spinner.md'] + sys.argv[1:]
qa = [int(x) for x in re.findall(r'^\*\*Q(\d+)\. ', open(S).read(), re.M)]
N = len(qa); bad = 0
if qa != list(range(1, N + 1)):
    print('QA 编号不连续:', qa); bad += 1
for f in files:
    for no, line in enumerate(open(f).read().split('\n'), 1):
        for m in re.finditer(r'(?<![\w.])Q(\d+)(?:–Q(\d+))?(?!\d)', line):
            ctx = line[max(0, m.start() - 30):m.start()]
            nums = [int(m.group(1))] + ([int(m.group(2))] if m.group(2) else [])
            if re.search(r'PR #(8|10|12)|其|会话', ctx):
                print('外部引用 %s:%d: …%s%s' % (f, no, ctx[-20:], m.group(0)))
            elif max(nums) > N:
                print('越界 %s:%d: %s' % (f, no, m.group(0))); bad += 1
print('QA %d 条，问题 %d 处' % (N, bad)); sys.exit(1 if bad else 0)
```

```bash
# ⑧ 按 CI 日志的时间戳拆分变体检验耗时：每条结果行与上一行（第一条与 baseline green 一行）的间隔，按 id 是否以 WIFI-N7 开头分别求和
J=<变体检验的 job id>   # gh run view <run id> --json jobs 取得
gh run view --job "$J" --log > /tmp/ci-mut.log
python3 - <<'PY'
import re, datetime as d
L = [(d.datetime.fromisoformat(m.group(1)), m.group(2)) for l in open('/tmp/ci-mut.log', encoding='utf-8', errors='replace')
     for m in [re.search(r'(20\d\d-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{6})\d*Z (\[ *\d+/\d+\] \[[^\]]+\])', l)] if m]
b = next(l for l in open('/tmp/ci-mut.log', encoding='utf-8', errors='replace') if 'baseline green' in l and '36;1m' not in l)
prev = d.datetime.fromisoformat(re.search(r'(20\d\d-\S{22})', b).group(1)[:26]); n7 = rest = 0
for t, n in L:
    s = (t - prev).total_seconds(); prev = t
    if 'WIFI-N7' in n: n7 += s
    else: rest += s
print('逐条变体 %.0f s；N7 %.0f s；其余 %.0f s' % (n7 + rest, n7, rest))
PY
```

`test_probe.c`（放在 `tests/cases/wifi/`）：

```c
/* 探针（不入库）：wpa_cli status 无 wpa_state 行或为各状态时，圆环与连接图标的显隐。 */
#include <string.h>
#include <unistd.h>
#include "app_env.h"
#include "check.h"
#include "gui_guider.h"
#include "lvgl.h"
#include "wifi_env.h"
void tst_wifi_status_update(void);
static void show(const char *tag)
{
    printf("%-22s spinner=%s icon=%s label=%s\n", tag,
           lv_obj_has_flag(guider_ui.WIFI_loading_spinner, LV_OBJ_FLAG_HIDDEN) ? "hidden" : "shown",
           lv_obj_has_flag(guider_ui.WIFI_wifi_log_img, LV_OBJ_FLAG_HIDDEN) ? "hidden" : "shown",
           lv_obj_has_flag(guider_ui.WIFI_loaded_wifi_label, LV_OBJ_FLAG_HIDDEN) ? "hidden" : "shown");
}
static void run(void)
{
    static const char *st[] = {"COMPLETED", "DISCONNECTED", "SCANNING", "INACTIVE", "COMPLETED", "INTERFACE_DISABLED",
                               "SCANNING", "ASSOCIATING", "COMPLETED", "4WAY_HANDSHAKE", "COMPLETED", NULL};
    char buf[128];
    wifi_env_setup(WIFI_ENV_CONF);
    tst_app_init(480, 1, 0);
    setup_scr_WIFI(&guider_ui);
    lv_scr_load(guider_ui.WIFI);
    show("initial");
    tst_wifi_status_update();
    show("(no output)");
    for (int i = 0; st[i]; i++) {
        snprintf(buf, sizeof(buf), "bssid=x\nwpa_state=%s\n", st[i]);
        tst_write_file("status.txt", buf);
        tst_wifi_status_update();
        show(st[i]);
    }
    tst_write_file("status.txt", "");
    tst_wifi_status_update();
    show("(no output again)");
}
int main(int argc, char **argv)
{
    static const tst_case_t cases[] = {{"run", run}};
    return tst_run_case(cases, TST_COUNT(cases), argc, argv);
}
```

### A.3 从其他 plan 提取的脚本

```bash
mkdir -p /tmp/ws
P10=docs/superpowers/plans/2026-10-10-lvgl-example-board-test-followups.md
P11=docs/superpowers/plans/2026-10-14-lvgl-example-heading-numbers.md
P9=docs/superpowers/plans/2026-10-06-lvgl-example-cloudagent-align-luckfox-pico.md
# PR #10 附录 C：cross.sh（C.4）；C.3 的 percommit.sh（spec Q9）与 C.6 的 archive.sh（结果由作者直接存入项目存储）本 PR 未用
for s in 'C.4:cross'; do
  awk -v h="### ${s%%:*} " 'index($0,h)==1{a=1;next} a&&/^```bash$/{p=1;next} p&&/^```$/{exit} p' $P10 > /tmp/ws/${s##*:}.sh; done
chmod +x /tmp/ws/*.sh
# PR #11 附录 A.1、A.5：check-headings.py 与 check-samples.sh
awk '/^### A\.1 /{a=1} a&&/^```python$/{p=1;next} p&&/^```$/{exit} p' $P11 > /tmp/ws/check-headings.py
awk '/^### A\.5 /{a=1} a&&/^```bash$/{p=1;next} p&&/^```$/{exit} p' $P11 > /tmp/ws/check-samples.sh
python3 /tmp/ws/check-headings.py docs/superpowers/specs/2026-10-18-lvgl-example-wifi-spinner-design.md \
  docs/superpowers/plans/2026-10-18-lvgl-example-wifi-spinner.md
bash /tmp/ws/check-samples.sh /tmp/ws/check-headings.py | tail -1
# PR #10 附录 C.7：reword.py（改写提交说明，附录 A.6）
awk '/^### C.7 /{a=1} a&&/^```python$/{p=1;next} p&&/^```$/{exit} p' $P10 > /tmp/ws/reword.py
# PR #9 附录 A.3：Mermaid 校验
mkdir -p /tmp/mm
awk '/^### A\.3 /{a=1} a&&/^````js$/{p=1;next} p&&/^````$/{exit} p' $P9 > /tmp/mm/check.mjs
awk '/^### A\.3 /{a=1} a&&/^```json$/{p=1;next} p&&/^```$/{exit} p' $P9 > /tmp/mm/package.json
(cd /tmp/mm && npm install --silent && node check.mjs "$OLDPWD/docs/superpowers/specs/2026-10-18-lvgl-example-wifi-spinner-design.md")
```

### A.4 在 `preview` 中查看 WiFi 页圆环

`preview` 的构建与运行方法见 `AGENTS.md`「运行 / 验证 GUI」与 `tests/README.md`「手动目标」，桌面操作脚本见 PR #10 plan 附录 C.5。WiFi 页要用假 `wpa_cli` 切换状态：`/tmp/pv-wifi/state` 不存在时模拟连不上，存在时按其中的 `wpa_state` 回答。在 Cloud Agent 的桌面（`DISPLAY=:1`）上运行，点主屏 WIFI 进入后，用 `echo SCANNING > /tmp/pv-wifi/state`、`echo COMPLETED > /tmp/pv-wifi/state`、`rm /tmp/pv-wifi/state` 切换，最多 5 s 后生效；`/tmp/pv-wifi/cmd.log` 记录每次调用。`preview` 没有 `wpa_supplicant.conf`，SSID 标签为空，只看得到图标。

```bash
mkdir -p /tmp/pv-wifi/bin && rm -f /tmp/pv-wifi/state && : > /tmp/pv-wifi/cmd.log
cat > /tmp/pv-wifi/bin/wpa_cli <<'SH'
#!/bin/sh
# preview 用的假 wpa_cli：/tmp/pv-wifi/state 存在时按其中的 wpa_state 回答，不存在时像连不上 wpa_supplicant 一样只写 stderr、以 255 退出。
S=/tmp/pv-wifi/state
echo "$(date +%T) wpa_cli $*" >> /tmp/pv-wifi/cmd.log
if [ ! -f "$S" ]; then echo "Failed to connect to non-global ctrl_ifname: (nil)  error: No such file or directory" >&2; exit 255; fi
case "$*" in
  status) printf "Selected interface 'wlan0'\nbssid=aa:bb:cc:dd:ee:ff\nssid=home\nid=0\nmode=station\nwpa_state=%s\n" "$(cat $S)" ;;
  *scan_results*) printf 'bssid / frequency / signal level / flags / ssid\naa:bb:cc:dd:ee:ff\t2412\t-40\t[WPA2-PSK-CCMP][ESS]\thome\n11:22:33:44:55:66\t2437\t-60\t[WPA2-PSK-CCMP][ESS]\tcafe\n' ;;
  *' scan') echo OK ;;
  '') cat > /dev/null ;;
esac
exit 0
SH
chmod +x /tmp/pv-wifi/bin/wpa_cli
cmake --build /tmp/bt --target preview -j8
pkill -x preview; sleep 1; rm -rf /tmp/pvtmp && mkdir -p /tmp/pvtmp
cd /tmp && DISPLAY=:1 PATH=/tmp/pv-wifi/bin:$PATH PREVIEW_RES=480 PREVIEW_BACKLIGHT=255:204 TMPDIR=/tmp/pvtmp \
  setsid nohup /tmp/bt/preview > /tmp/pv.log 2>&1 < /dev/null &
```

用 `xdotool` 自动核对时（PR #10 plan 附录 C.5 的 `click`）：主屏 WIFI 在窗口内 (50, 410)；圆环区 `-crop 90x90+25+20`、图标区 `-crop 64x34+375+45` 的颜色数（ImageMagick `%k`）为 1 时表示没有内容；图标区只取图标下半部分，上半部分与右上角的性能监视器重叠。

`pv-wifi-check.sh`：`preview` 按上面的命令启动并停在主屏后运行，依次切到连不上、`SCANNING`、`COMPLETED`、连不上，各截一张图。预期圆环区与图标区依次为 1/1、大于 1/1、1/大于 1、1/1（§4）。

```bash
#!/bin/bash
# preview 已按附录 A.4 启动并停在主屏：进入 WiFi 页，依次切到连不上、SCANNING、COMPLETED，各截一张图，
# 打印圆环区与图标区的颜色数（1 表示没有内容）与最近的 wpa_cli 调用。需要 xdotool、ImageMagick。
export DISPLAY=${DISPLAY:-:1}; OUT=${OUT:-/tmp/pvshots}; mkdir -p "$OUT"
W=$(xdotool search --name 'Luckfox LVGL preview' | head -1)
click(){ xdotool mousemove --window "$W" "$1" "$2" mousedown 1; sleep 0.15; xdotool mouseup 1; sleep 1.2; }
rm -f /tmp/pv-wifi/state
click 50 410                                   # 主屏 WIFI
for st in unreachable SCANNING COMPLETED unreachable; do
  if [ "$st" = unreachable ]; then rm -f /tmp/pv-wifi/state; else echo "$st" > /tmp/pv-wifi/state; fi
  sleep 5.5; f=$OUT/wifi-$st.png; import -window "$W" "$f"
  echo "$st 圆环区=$(convert "$f" -crop 90x90+25+20 +repage -format %k info:) 图标区=$(convert "$f" -crop 64x34+375+45 +repage -format %k info:)"
done
tail -2 /tmp/pv-wifi/cmd.log
```

### A.5 补测二进制交付与复测结果

`cross.sh`（附录 A.3）的交付目录写死为 PR #10 的日期，本 PR 改为 `board-retest-20261018`。产物名中的哈希按提交标题现查，文档中不写本 PR 的提交哈希。只按标题匹配：`git log --grep` 会匹配整条说明，WiFi [3/3] 的正文提到了「WiFi [2/3]」。

```bash
sed 's#S=/cursor/stores/self/board-retest-20261010#S=/cursor/stores/self/board-retest-20261018#' /tmp/ws/cross.sh > /tmp/ws/cross-1018.sh
chmod +x /tmp/ws/cross-1018.sh
F2=$(git log --format='%h %s' origin/dev..HEAD | grep -F 'WiFi [3/3]' | cut -d' ' -f1)   # 交付 W5、W6 用的提交；W1–W4 时为当时的 WiFi [2/2]
S=/cursor/stores/self; D=$S/board-retest-20261018
# 项目存储可写时直接交付；能列目录但写入报 No such file or directory 时，先只编译与冒烟，产物放 /tmp，恢复后再复制
t=$S/.probe-$$; if echo ok > "$t" && rm -f "$t"; then mkdir -p "$D" && /tmp/ws/cross-1018.sh "pr13-$F2"
else /tmp/ws/cross.sh && cp /tmp/xu/luckfox_lvgl_demo "/tmp/luckfox_lvgl_demo-pr13-$F2"; fi
# 提交哈希变化后（custom、src、lib、generated 不变）：在该提交的树上重新编 uClibc 产物，核对与已交付的哈希相同，再以新名字复制
rm -rf /tmp/src-f2 && mkdir /tmp/src-f2 && git archive "$F2" | tar -x -C /tmp/src-f2
(cd /tmp/src-f2 && env -u GLIBC_COMPILER LUCKFOX_SDK_PATH=/tmp/luckfox-pico cmake -S . -B /tmp/xu-f2 >/dev/null && make -C /tmp/xu-f2 -j"$(nproc)" > /tmp/xu-f2.log 2>&1)
echo "告警 $(grep -c 'warning:' /tmp/xu-f2.log)"; sha256sum /tmp/xu-f2/luckfox_lvgl_demo "$D"/luckfox_lvgl_demo-pr13-*
cp /tmp/xu-f2/luckfox_lvgl_demo "$D/luckfox_lvgl_demo-pr13-$F2"
(cd "$D" && sha256sum luckfox_lvgl_demo-pr13-* > SHA256SUMS && sha256sum -c SHA256SUMS)

# 复测结果：作者存入项目存储后补 SHA256SUMS，解开核对；查看时遮盖热点名与密码（HOT、PW 由作者提供，不写进文档）
R=$S/board-retest-results-20261018/pr13-fc4c066
(cd "$R" && sha256sum board-retest-results-pr13-fc4c066.zip > SHA256SUMS && sha256sum -c SHA256SUMS)
rm -rf /tmp/rr && mkdir /tmp/rr && unzip -q "$R/board-retest-results-pr13-fc4c066.zip" -d /tmp/rr
diff -r /tmp/rr/board-retest-out-pr13 "$R/board-retest-out-pr13" && echo "目录与 zip 一致"
sed -E "s/(ssid|psk)=(\"[^\"]*\"|.*)/\1=<已隐藏>/g; s/$HOT/<测试热点>/g; s/$PW/<密码>/g" /tmp/rr/board-retest-out-pr13/RESULTS.md
grep -h 'wpa_cli v\|Failed to connect' /tmp/rr/board-retest-out-pr13/out/app-pr13-session1.txt | sort | uniq -c   # 板上 wpa_cli 版本与连不上时的输出
# W5、W6 的结果（作者已附 SHA256SUMS）：核对、解开；out-a329c22/ 为本轮日志，两份配置的缩进差异用 cat -A 查看（Tab 显示为 ^I）
R2=$S/board-retest-results-20261018/pr13-a329c22
(cd "$R2" && sha256sum -c SHA256SUMS)
rm -rf /tmp/rr2 && mkdir /tmp/rr2 && unzip -q "$R2/board-retest-results-pr13-a329c22.zip" -d /tmp/rr2
diff -r /tmp/rr2/board-retest-out-pr13 "$R2/board-retest-out-pr13" && echo "目录与 zip 一致"
awk '/^## W5\/W6/{p=1} p' /tmp/rr2/board-retest-out-pr13/RESULTS.md | sed -E "s/$HOT/<测试热点>/g; s/$PW/<密码>/g"
sed -E "s/$HOT/<测试热点>/g; s/$PW/<密码>/g" /tmp/rr2/board-retest-out-pr13/out-a329c22/wpa-final-check.txt | cat -A | head -6
```

### A.6 整理提交与提交说明

`E` 为 §1 的身份约定前缀。修改并入已有提交用 fixup，`order-commits.py` 把提交排成「`fix`、其他、`docs(agents)`、`docs(superpowers)`」，fixup 紧跟目标；改写提交说明用 PR #10 plan 附录 C.7 的 `reword.py`（附录 A.3 提取）。

```bash
E='env -u GIT_AUTHOR_NAME -u GIT_AUTHOR_EMAIL -u GIT_COMMITTER_NAME -u GIT_COMMITTER_EMAIL'
git rev-list --reverse origin/dev..HEAD > /tmp/old.txt; T0=$(git rev-parse HEAD^{tree})
$E git commit --fixup=<目标提交>        # 已暂存的改动并入目标提交；T0 取在 commit 之后
GIT_SEQUENCE_EDITOR='python3 /tmp/ws/order-commits.py' $E git rebase -q -i --autosquash origin/dev
[ "$T0" = "$(git rev-parse HEAD^{tree})" ] && echo "树不变"
# 改写提交说明：映射为 JSON，键为标题中的关键字，值为 [新标题或 null, 正文]（reword.py 的说明）；改正文时手写，只改标题编号时用 reword-map-renumber.py
python3 /tmp/ws/reword-map-renumber.py 'WiFi [1/2]=WiFi [1/3]' 'WiFi [2/2]=WiFi [2/3]' > /tmp/ws/reword-map.json   # Task 4
REWORD_MAP=/tmp/ws/reword-map.json GIT_SEQUENCE_EDITOR="python3 /tmp/ws/reword.py seq" GIT_EDITOR="python3 /tmp/ws/reword.py msg" \
  $E git rebase -q -i origin/dev
paste /tmp/old.txt <(git rev-list --reverse origin/dev..HEAD) | while read -r a b; do
  [ "$(git rev-parse "$a^{tree}")" = "$(git rev-parse "$b^{tree}")" ] && echo "同树 ${b:0:7}" || echo "不同 ${b:0:7}"; done
python3 /tmp/ws/msg-check.py; python3 /tmp/ws/history-scan.py | head -30
# 取 cz-conventional-emoji 1.1.8 与 gitmojis 3.15.0 的源码，核对标题拼法、宽度默认值与 emoji 码点
rm -rf /tmp/czv && mkdir /tmp/czv && cd /tmp/czv && npm pack --silent cz-conventional-emoji@1.1.8 gitmojis@3.15.0 &&
  for t in *.tgz; do mkdir "${t%.tgz}" && tar -xzf "$t" -C "${t%.tgz}"; done
python3 -c 'import json; print(json.load(open("cz-conventional-emoji-1.1.8/package/package.json"))["repository"]["url"])'
grep -n "head = typeName" cz-conventional-emoji-1.1.8/package/engine.js; grep -n -A4 'maxHeaderWidth:' cz-conventional-emoji-1.1.8/package/index.js
node -e 'const g=require("./gitmojis-3.15.0/package/dist/index.cjs"); for (const e of g.gitmojis) if ([":bug:",":memo:"].includes(e.code)) console.log(e.code, [...e.emoji].map(c=>"U+"+c.codePointAt(0).toString(16).toUpperCase()).join(" "))'
cd -
```

`order-commits.py`：

```python
#!/usr/bin/env python3
"""git rebase -i 的 GIT_SEQUENCE_EDITOR：按 fix、其他提交、docs(agents)、docs(superpowers) 的顺序稳定排序；
fixup! 行与目标同类，autosquash 已把它们排在目标之后，稳定排序不会拆开。"""
import sys
p = sys.argv[1]
L = [l for l in open(p, encoding="utf-8").read().splitlines() if l and not l.startswith("#")]
def rank(l):
    if "docs(superpowers)" in l: return 3
    if "docs(agents)" in l: return 2
    if " fix(" in l or "fixup! fix(" in l: return 0
    return 1
open(p, "w", encoding="utf-8").write("\n".join(sorted(L, key=rank)) + "\n")
```

`reword-map-renumber.py`：

```python
#!/usr/bin/env python3
"""生成只改标题编号的 reword.py 映射：参数为「旧关键字=新关键字」，如 'WiFi [1/2]=WiFi [1/3]'；只按标题匹配，正文取自 git。
用法（仓库根目录）：python3 reword-map-renumber.py 'WiFi [1/2]=WiFi [1/3]' 'WiFi [2/2]=WiFi [2/3]' > /tmp/ws/reword-map.json"""
import json, subprocess, sys
pairs = [a.split("=", 1) for a in sys.argv[1:]]
m = {}
for r in subprocess.run(["git", "rev-list", "--reverse", "origin/dev..HEAD"], capture_output=True, text=True).stdout.split():
    title, _, body = subprocess.run(["git", "log", "-1", "--format=%B", r], capture_output=True, text=True).stdout.rstrip("\n").partition("\n\n")
    for old, new in pairs:
        if old in title:
            m[old] = [title.replace(old, new), body]
missing = [o for o, _ in pairs if o not in m]
if missing:
    sys.exit("标题中找不到：" + "、".join(missing))
print(json.dumps(m, ensure_ascii=False, indent=1))
```

`msg-check.py`：

```python
#!/usr/bin/env python3
"""核对 origin/dev..HEAD 的提交说明：标题为 <type>(<scope>): <emoji> <subject> 且 emoji 与 type 对应（cz-conventional-emoji 1.1.8 + gitmojis 3.15.0），
标题与正文每行不超过 100 字符，标题与正文之间空一行，正文全为「- 」要点且最后一个要点以「- 详见 」开头，不含 Co-authored-by，作者为 Cursor Agent 且带 SSH 签名。"""
import re, subprocess, sys
EMOJI = {"fix": "\U0001F41B", "docs": "\U0001F4DD", "test": "\u2705", "ci": "\U0001F477", "feat": "\u2728"}
bad = 0
for r in subprocess.run(["git", "rev-list", "--reverse", "origin/dev..HEAD"], capture_output=True, text=True).stdout.split():
    raw = subprocess.run(["git", "cat-file", "-p", r], capture_output=True, text=True).stdout
    L = subprocess.run(["git", "log", "-1", "--format=%B", r], capture_output=True, text=True).stdout.rstrip("\n").split("\n")
    m = re.match(r"^(\w[\w-]*)\(([\w-]+)\): (\S+) \S", L[0])
    errs = []
    if not m or EMOJI.get(m.group(1)) != m.group(3):
        errs.append("标题格式或 emoji")
    if max(len(l) for l in L) > 100:
        errs.append("超过 100 字符")
    body = L[2:]
    if len(L) < 3 or L[1] != "" or not body or not all(l.startswith("- ") for l in body) or not body[-1].startswith("- 详见 "):
        errs.append("正文不是要点或最后不是「- 详见」")
    if any("Co-authored-by" in l for l in L):
        errs.append("含 Co-authored-by")
    if "author Cursor Agent <cursoragent@cursor.com>" not in raw or "BEGIN SSH SIGNATURE" not in raw:
        errs.append("身份或签名")
    print(("OK  " if not errs else "BAD ") + L[0], "；".join(errs))
    bad += bool(errs)
sys.exit(1 if bad else 0)
```

`history-scan.py`：

```python
#!/usr/bin/env python3
"""扫描 origin/dev 最近 60 个提交与本 PR 的提交说明：「详见」是最后一个要点（- 详见）还是单独成段、是否注明历史问题、最长行。"""
import re, subprocess
revs = subprocess.run(["git", "rev-list", "--no-merges", "origin/dev", "-60"], capture_output=True, text=True).stdout.split()
revs += subprocess.run(["git", "rev-list", "origin/dev..HEAD"], capture_output=True, text=True).stdout.split()
for r in revs:
    m = subprocess.run(["git", "log", "-1", "--format=%B", r], capture_output=True, text=True).stdout.rstrip("\n").split("\n")
    if not re.match(r"^\w[\w-]*\(", m[0]):
        continue
    body = m[2:]
    fmt = "- 详见" if any(l.startswith("- 详见") for l in body) else ("段落详见" if any("详见" in l for l in body) else "无详见")
    print(r[:7], fmt, "历史问题" if "历史问题" in "\n".join(body) else "", "最长", max(len(l) for l in m), m[0][:36])
```

### A.7 修复前验证与变体检验

```bash
# 新用例在修复前的失败输出：只看失败断言与显隐。须在修复前的构建上运行（只加了测试、尚未加修复时）；在当前代码上运行全部通过
for c in unreachable_stops idle_states_stop connecting_states_spin state_line_only timer_unreachable_stops initial_hidden enter_queries_now load_shows_spinner; do
  echo "== $c"; /tmp/bt/cases/wifi/wifi_status "$c" 2>&1 | grep -E 'FAIL|spinner=' | head -6; done
# 改写后的 timer_unreachable_stops 修复前仍失败（spec Q11）；同时按 spec Q9 的例外对 WiFi [1/3]（当时为 [1/2]）单独跑全量
F1=$(git log --format='%h %s' origin/dev..HEAD | grep -F 'WiFi [1/3]' | cut -d' ' -f1)
rm -rf /tmp/v1 && mkdir /tmp/v1 && git archive "$F1" | tar -x -C /tmp/v1
cmake -S /tmp/v1/tests -B /tmp/v1/b -DLUCKFOX_TESTS_ARM32=ON >/dev/null && cmake --build /tmp/v1/b -j"$(nproc)" >/dev/null
ctest --test-dir /tmp/v1/b -j"$(nproc)" | grep 'tests passed'
git show origin/dev:custom/custom_wifi.c > /tmp/v1/custom/custom_wifi.c && cmake --build /tmp/v1/b -j"$(nproc)" >/dev/null
/tmp/v1/b/cases/wifi/wifi_status timer_unreachable_stops 2>&1 | grep -E 'FAIL|spinner='
# N7 变体子集与全量（计时）
rm -rf /tmp/mut-n7 && MUTATE_WORK=/tmp/mut-n7 tests/tools/mutate.sh $(grep '^id:' tests/cases/wifi/status.mutants | cut -d' ' -f2)
rm -rf /tmp/mut-all; s=$(date +%s); MUTATE_WORK=/tmp/mut-all tests/tools/mutate.sh > /tmp/mut-all.log 2>&1; echo "rc=$? $(( $(date +%s)-s ))s"
grep -c KILLED /tmp/mut-all.log; grep -E 'SURVIVED|SKIPPED|BUILD FAILED' /tmp/mut-all.log; tail -1 /tmp/mut-all.log
```

Load 探针（spec §3.3）：在 WiFi [2/3]（修复前）、HEAD 与 `origin/dev` 的临时工作树中各运行一次。进入时 `DISCONNECTED`，点 Load 后把状态改为 `COMPLETED`，打印各阶段的显隐与 Load 之后圆环是否出现过。`origin/dev` 没有 `status` 分支，与附录 A.2 ② 一样先用 `sed` 补上。

```bash
for ref in $(git log --format='%h %s' origin/dev..HEAD | grep -F 'WiFi [2/3]' | cut -d' ' -f1) HEAD origin/dev; do
  W=/tmp/wt-load-$(echo "$ref" | tr / -); rm -rf "$W"; git worktree add -q --detach "$W" "$ref"; cd "$W"
  grep -q 'status) if' tests/cases/wifi/wifi_env.h || sed -i 's#\*scan_results\*) cat#status) cat \x27%s/status.txt\x27 2>/dev/null ;;\\n*scan_results*) cat#; s#dir, dir, dir, dir, dir);#dir, dir, dir, dir, dir, dir);#' tests/cases/wifi/wifi_env.h
  cp /tmp/ws/test_loadprobe.c tests/cases/wifi/
  echo 'luckfox_add_test(loadprobe UNIT unit_wifi.c CASES run)' >> tests/cases/wifi/CMakeLists.txt
  cmake -S tests -B "$W/b" >/dev/null 2>&1 && cmake --build "$W/b" -j"$(nproc)" --target wifi_loadprobe >/dev/null 2>&1
  echo "== $ref"; "$W/b/cases/wifi/wifi_loadprobe" run 2>/dev/null | grep spinner
  cd - >/dev/null; git worktree remove --force "$W"
done
```

`test_loadprobe.c`（存为 `/tmp/ws/test_loadprobe.c`）：

```c
/* 探针（不入库）：进入页面时为 DISCONNECTED，点 Load 后到下一次轮询之间的圆环显隐。 */
#include <stdio.h>
#include <unistd.h>
#include "app_env.h"
#include "check.h"
#include "gui_guider.h"
#include "lvgl.h"
#include "wifi_env.h"
void tst_wifi_conf_load(const char *ssid, const char *psk);
static void show(const char *t)
{
    printf("%-34s spinner=%s icon=%s\n", t, lv_obj_has_flag(guider_ui.WIFI_loading_spinner, LV_OBJ_FLAG_HIDDEN) ? "hidden" : "shown",
           lv_obj_has_flag(guider_ui.WIFI_wifi_log_img, LV_OBJ_FLAG_HIDDEN) ? "hidden" : "shown");
}
static void run_until(uint32_t s, uint32_t ms)
{
    while (lv_tick_elaps(s) < ms) {
        lv_timer_handler();
        usleep(5000);
    }
    lv_timer_handler();
}
static void run(void)
{
    uint32_t t, seen = 0;
    wifi_env_setup(WIFI_ENV_CONF);
    tst_write_file("status.txt", "wpa_state=DISCONNECTED\n");
    tst_app_init(480, 1, 0);
    setup_scr_WIFI(&guider_ui);
    lv_scr_load(guider_ui.WIFI);
    t = lv_tick_get();
    run_until(t, 300);
    show("enter, DISCONNECTED (+300ms)");
    tst_wifi_conf_load("home", "homepass1");
    show("right after Load");
    tst_write_file("status.txt", "wpa_state=COMPLETED\n"); /* 连接很快完成 */
    t = lv_tick_get();
    while (lv_tick_elaps(t) < 5200) {
        lv_timer_handler();
        usleep(5000);
        if (!lv_obj_has_flag(guider_ui.WIFI_loading_spinner, LV_OBJ_FLAG_HIDDEN))
            seen = 1;
    }
    show("after next poll (COMPLETED)");
    printf("spinner shown at any time after Load: %s\n", seen ? "yes" : "no");
}
int main(int argc, char **argv)
{
    static const tst_case_t c[] = {{"run", run}};
    return tst_run_case(c, TST_COUNT(c), argc, argv);
}
```

### A.8 合并前自查与 CI

```bash
# 用例数、变体数与 generated/ 修改提交数
ctest --test-dir /tmp/bt -N | tail -1; ctest --test-dir /tmp/bt -N -L wifi | tail -1
grep -h -c '^id:' tests/cases/*/*.mutants | paste -sd+ | bc; grep -c '^id: WIFI-N7' tests/cases/wifi/status.mutants
git log --oneline 00974b5..origin/dev -- generated/ | wc -l
# 本 PR 的提交哈希只允许出现在产物文件名中；文档与 PR 正文中不得有测试热点的名称与密码（HOT、PW 由作者提供）
SP=docs/superpowers/specs/2026-10-18-lvgl-example-wifi-spinner-design.md; PL=docs/superpowers/plans/2026-10-18-lvgl-example-wifi-spinner.md
for h in $(git rev-list origin/dev..HEAD); do git grep -n "${h:0:7}" HEAD -- "$SP" "$PL" | grep -v "pr13-${h:0:7}"; done
gh pr view 13 --json body -q .body > /tmp/body.md; grep -c -e "$HOT" -e "$PW" "$SP" "$PL" /tmp/body.md
python3 /tmp/ws/typo-check.py "$SP" "$PL" /tmp/body.md
# CI：等全部结束（ci-wait.sh，源码见下），再取变体检验的结果行与耗时（逐条耗时见附录 A.2 ⑧）
bash /tmp/ws/ci-wait.sh 13
R=$(gh run list --branch pi/wifi-spinner --limit 1 --json databaseId -q '.[0].databaseId')
J=$(gh run view "$R" --json jobs -q '.jobs[]|select(.name|test("变体"))|.databaseId')
gh api "repos/yuangezhizao/luckfox_pico_lvgl_example/actions/jobs/$J" --jq '[.started_at,.completed_at]|@tsv'
gh run view --job "$J" --log | grep -E 'baseline green|[0-9]+ run, ' | grep -v '36;1m'
# 审查评论：Codex 的行内评论与回复、讨论串是否已解决；回复中链接的 spec §3.3 锚点
gh api repos/yuangezhizao/luckfox_pico_lvgl_example/pulls/13/comments --jq '.[]|[.id,.user.login,(.in_reply_to_id|tostring),(.body|.[0:40])]|@tsv'
gh api graphql -f query='query{repository(owner:"yuangezhizao",name:"luckfox_pico_lvgl_example"){pullRequest(number:13){reviewThreads(first:20){nodes{id isResolved comments(first:1){nodes{databaseId author{login}}}}}}}}' \
  --jq '.data.repository.pullRequest.reviewThreads.nodes[]|[.id,.isResolved,(.comments.nodes[0].databaseId|tostring),.comments.nodes[0].author.login]|@tsv'
gh api -H 'Accept: application/vnd.github.html' "repos/yuangezhizao/luckfox_pico_lvgl_example/contents/$SP?ref=pi/wifi-spinner" |
  python3 -c 'import sys,re,urllib.parse as u; print([u.unquote(i) for i in re.findall(r"id=\"user-content-(33[^\"]*)\"", sys.stdin.read())])'
# 回复与标记已解决：本 PR 用 pi 的 ManagePullRequest 工具（post_comment 的 in_reply_to、resolve_comment）完成；等价的 gh 命令如下，未实测
#   gh api repos/yuangezhizao/luckfox_pico_lvgl_example/pulls/13/comments/<评论 id>/replies -f body=@/tmp/reply.md
#   gh api graphql -f query='mutation{resolveReviewThread(input:{threadId:"<讨论串 id>"}){thread{isResolved}}}'
```

`ci-wait.sh`：

```bash
#!/bin/bash
# 等 PR 的 CI 全部结束（没有 pending 且至少 5 项），最多约 25 分钟，结束后打印各检查项与运行结论。用法：ci-wait.sh [PR 号，默认 13]
PR=${1:-13}
for i in $(seq 1 50); do
  out=$(timeout 60 gh pr checks "$PR" 2>&1)
  [ "$(echo "$out" | grep -c -E '\bpending\b')" = 0 ] && [ "$(echo "$out" | grep -c .)" -ge 5 ] && break
  sleep 30
done
echo "$out" | cut -f1-3
R=$(timeout 60 gh run list --branch pi/wifi-spinner --limit 1 --json databaseId -q '.[0].databaseId')
timeout 60 gh run view "$R" --json headSha,conclusion -q '[.headSha[0:7],.conclusion]|@tsv'
```

`typo-check.py`：

```python
#!/usr/bin/env python3
"""排版检查：去掉代码块、把行内代码换成占位后，统计中文与英文、数字或行内代码紧贴的位置，以及弯引号。用法：typo-check.py <文件>…"""
import re, sys
han = r"[\u4e00-\u9fff]"
for f in sys.argv[1:]:
    t = re.sub(r"```.*?```", "", open(f, encoding="utf-8").read(), flags=re.S)
    t2 = re.sub(r"`[^`\n]*`", "`X`", t)
    a = re.findall(".{0,6}(?:" + han + r"[A-Za-z0-9`]|[A-Za-z0-9`]" + han + ").{0,6}", t2)
    print(f.split("/")[-1], "紧贴", len(a), a[:5], "弯引号", len(re.findall("[\u201c\u201d]", t)))
```
