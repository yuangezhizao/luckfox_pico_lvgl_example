# Luckfox Pico LVGL Example — preview 脚本入库实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `preview` 的启动、操作与核对脚本放进 `tests/tools/preview/`，桌面与无桌面共用；用法写进 `tests/README.md`；CI 新增 ShellCheck job，并在 `native-tests` 中以无桌面方式运行核对脚本；脚本依赖先在 CI 中临时 apt 安装，验证后挪进两份 Dockerfile。

**Architecture:** 先推送 spec 与 plan、开草稿 PR（Task 1）；脚本与文档（Task 2）；本机在桌面、Xvfb、Xtigervnc 下验证（Task 3）；CI 的 ShellCheck job 与临时 apt 的核对步骤，含一次故意失败（Task 4）；历史文档的状态行（Task 5）；本地全量 `ctest`（Task 6）；挪进 Dockerfile 并在本机镜像中验证（Task 7）；截图交付（Task 8）；整理提交、精简 plan、PR 正文（Task 9）；合并前自查，修复后先拿掉 Dockerfile 提交确认 CI 通过，再放回，最后改写历史去掉临时 apt 步骤（Task 10）；合并后核对 dev CI（Task 11）。

**Tech Stack:** bash、xdotool、ImageMagick、Xvfb / Xtigervnc、SDL2、CMake/CTest、ShellCheck 0.9.0、actionlint 1.7.12、GitHub Actions、Docker、`gh`。

**Spec:** [`docs/superpowers/specs/2026-10-19-lvgl-example-preview-tools-design.md`](../specs/2026-10-19-lvgl-example-preview-tools-design.md)

---

## 1. Global Constraints

- 分支 `pi/preview-tools`，起点 `origin/dev` `c13283c`（PR #13 合入）；PR #14 指向 `dev`，开发时为草稿，满足门槛后改为 ready，执行者不合并。
- 不改 `preview.c` 与测试工程的 C、CMake（spec Q2），所以不需要全量 `mutate.sh`；CI 的变体检验照常运行。
- 提交身份 `Cursor Agent <cursoragent@cursor.com>`，SSH 签名；所有创建或改写提交的 git 命令前加 `env -u GIT_AUTHOR_NAME -u GIT_AUTHOR_EMAIL -u GIT_COMMITTER_NAME -u GIT_COMMITTER_EMAIL`（临时提交也不例外），推送前 `git cat-file -p HEAD | sed -n 3,4p` 核对。
- 提交信息按 cz-conventional-emoji 1.1.8：`<type>(<scope>): <emoji> <subject>`，正文为要点、写原因、每行不超过 100 字符，最后一个要点为 `- 详见 spec §…、Q…`；文档中不写本 PR 自身提交的哈希。
- 推送：`GIT_TERMINAL_PROMPT=0 timeout 120 git push https://yuangezhizao@github.com/yuangezhizao/luckfox_pico_lvgl_example.git ...`，改写历史时写明 `--force-with-lease=pi/preview-tools:<远端旧提交>`；`gh` 命令加 `timeout`；更新 PR 正文前先 `gh pr view --json body` 取回当前正文。
- 推送会取消正在运行的 CI；只改文档的修订攒到一起推送。
- 结束 `preview` 用 `tests/tools/preview/stop.sh` 或 `pkill -x preview`，等待后台进程不用 `pgrep -f`、`pkill -f`。
- 编辑文件与 `git add` 不放在同一批并行执行（§5）。
- 文档中的事实与数字都用命令重新算出：实测命令与测试脚本写在附录 A；CI 的结果以 run 编号记录，在 GitHub 的 run 页面查看。

## 2. 提交

1. `hammer(tests): 🔨 新增 preview 启动、操作与核对脚本`：`tests/tools/preview/`、`tests/README.md`、`AGENTS.md` 的指引。
2. `chore(cloud-env): 🔧 Dockerfile 安装 preview 脚本所需的 imagemagick、xdotool、xvfb`：两份 Dockerfile，`AGENTS.md` 的环境说明与 `tests/README.md` 的依赖说明。
3. `ci(github-actions): 👷 新增 ShellCheck job，原生测试中核对 preview 脚本`：workflow（工具来自 CI 镜像，没有临时 apt 步骤）、`AGENTS.md` 的 CI 一节与「Lint」、`tests/README.md` 的 CI 一句。
4. `docs(tests): 📝 tests/README.md 的标题加序号`：只改 9 个标题（作者要求，Task 10）。
5. `docs(superpowers): 📝 新增 preview 脚本入库 spec 与 plan`：本 spec 与 plan，PR #10 plan 附录 C.5、PR #13 spec §8.1 与 PR #13 plan 附录 A.4 的状态行。

开发期间的顺序是 `hammer`、`ci`（含临时 apt 步骤）、`chore`（删除该步骤）、`docs`；合并前按作者要求改为上面的顺序，最终的树不变（Task 10）。

## 3. Tasks

### Task 1：spec 与 plan，推送并开草稿 PR ✅

- [x] spec（QA Q1–Q18）与本 plan，`check-headings.py` 为 0；提交 `docs(superpowers)`，推送，开草稿 PR #14。

### Task 2：脚本与文档 ✅

文件：`tests/tools/preview/{lib.sh,start.sh,stop.sh,songs.sh,wifi.sh,shots.sh,check-music-list.sh,check-wifi.sh,bin/wpa_cli}`、`tests/README.md`、`AGENTS.md`

- [x] 按 spec §3.1–§3.4 写脚本，坐标只写 480 设计值；`shellcheck -x -P SCRIPTDIR` 为 0 条（命令见 `tests/README.md`）。
- [x] `tests/README.md`「手动目标」新增「preview 脚本」小节；`AGENTS.md`「运行 / 验证 GUI」加一句指引。

### Task 3：本机验证 ✅

- [x] 云端桌面 `:1`、`--headless` 的 Xvfb 与 Xtigervnc，各在 480、720 下运行全部脚本，核对通过，数值与 spec §2.2 相同（附录 A.3 ①②）。
- [x] 失败路径：改错预期值时核对脚本以 1 退出并留下截图；没有会话、没有 `preview` 时以 2 退出（附录 A.3 ③）。

### Task 4：CI ✅

文件：`.github/workflows/build-luckfox-lvgl-demo.yml`、`AGENTS.md`、`tests/README.md`

- [x] ShellCheck job；`native-tests` 新增临时 apt 安装、核对步骤与失败时上传；actionlint 为 0 条；推送前在 dev 签名的 CI 镜像中模拟新步骤（附录 A.4 ①）。
- [x] 推送故意失败的临时提交：核对步骤失败、artifact 上传且含截图；之后丢弃该提交（附录 A.4 ②）。
- [x] 正常推送后 6 个 job 通过，`🧬 变体检验` 为 `136 run, 0 skipped`，preview 核对在 480、720 下通过（run 编号见 §4）。

### Task 5：历史文档的状态行 ✅

文件：`docs/superpowers/plans/2026-10-10-lvgl-example-board-test-followups.md`、`docs/superpowers/specs/2026-10-18-lvgl-example-wifi-spinner-design.md`、`docs/superpowers/plans/2026-10-18-lvgl-example-wifi-spinner.md`

- [x] PR #10 plan 附录 C.5 开头加一行状态；PR #13 spec §8.1 正文改为状态行，链接 `c13283c` 的原文，锚点在该提交的 GitHub 渲染结果中存在。
- [x] 合并前按 spec Q13 的补充：筛选全部历史 spec 与 plan 的代码块（附录 A.2 ⑥），PR #13 plan 附录 A.4 开头加一行状态。

### Task 6：本地全量 `ctest` ✅

- [x] `-DLUCKFOX_TESTS_ARM32=ON` 全量 126/126（附录 A.1）。

### Task 7：挪进 Dockerfile 与本机镜像验证 ✅

文件：`.cursor/Dockerfile`、`.cursor/Dockerfile.luckfox_pico`、`.github/workflows/build-luckfox-lvgl-demo.yml`、`AGENTS.md`、`tests/README.md`

- [x] 两份 Dockerfile 新增「preview 工具」组，改写 neofetch 推荐包的注释；三个包均不在两份 FROM（附录 A.5 ①）；删除 workflow 中的 apt 步骤，actionlint 为 0 条。
- [x] 本机构建两份镜像并与 dev 的 Dockerfile 同时构建对比大小；`.cursor/Dockerfile` 镜像中以 `ubuntu` 与 root 跑全量 `ctest` 与全部脚本，并挂载宿主 `:1` 运行一次；`.cursor/Dockerfile.luckfox_pico` 镜像中核对工具并运行核对脚本（附录 A.5 ②–⑤）。
- [x] 推送后 `build-image` 按预期失败（新标签没有 dev 签名镜像），3 个容器 job 被跳过，ShellCheck 通过（§4）。

### Task 8：截图交付 ✅

- [x] 桌面、无桌面（Xvfb）、`.cursor/Dockerfile` 镜像各一张拼图，存到 `/cursor/stores/self/preview-tools-20261019/` 并复制到 `/opt/cursor/artifacts/`，两处 `sha256sum -c` 为 `OK`（附录 A.6）。
- [x] 作者把三张拼图拖进 PR 评论（spec Q12）；执行者按作者要求另发一条评论，说明三张图的环境与截图方式及逐像素比较的结果（附录 A.6）。

### Task 9：整理提交、精简 plan、PR 正文 ✅

- [x] 按 §2 整理提交，fixup 并入 `docs(superpowers)` 并放在最后，整理前后树相同，身份与签名正确。
- [x] 精简本 plan；`check-headings.py`、`check-samples.sh` 与 QA 编号核对（PR #11 plan 附录 A.1、A.5，PR #13 plan 附录 A.2 ⑦）。
- [x] PR 正文按 PR #13 的格式更新（标题编号 `## 1.`）。

### Task 10：合并前自查 ✅

- [x] 按作者审查模板担任 A（代码与构建）、B（事实与复现）、C（规范与表述），每项给脚本证据，只查不改：无 Critical、Important，5 条 Minor（§4），作者确认全部修复。
- [x] 修复：`start.sh` 无桌面时连不上显示先结束自起的 X 服务器（并入 `hammer(tests)`，附录 A.3 ④）；`AGENTS.md`「Lint」注明 shell 脚本由 ShellCheck job 检查（并入 `ci(github-actions)`）；spec §2.3 注明 `c13283c` 与行号、表中统一写「无」；本 plan 的勾选与 §4。
- [x] 第一次按 apt 方式推送（run `39253856488`）：`🖼️ preview 脚本核对` 在 480 屏的 `start.sh` 失败，15 s 内没有等到窗口（§4）。改为最多等 30 s、按 `_NET_WM_PID` 与名字同时匹配（`--all`）、找不到时退回唯一的同名窗口、超时打印诊断，并入 `hammer(tests)`（spec §3.2，附录 A.3 ⑤）；本机 CI 镜像中循环 30 次 0 失败（附录 A.4 ③）。
- [x] 作者要求修复后先用临时 apt 的方式确认 CI 通过：暂时拿掉 `chore(cloud-env)`，推送 `hammer`、`ci`、`docs` 三个提交，6 个 job 通过；再把 `chore(cloud-env)` 放回 `docs(superpowers)` 之前并推送。调整 `start.sh` 后的 run `39257101679` 全部通过（§4）。
- [x] 用最终提交在 `.cursor/Dockerfile` 的镜像中重跑 `run-img.sh`（`start.sh` 调整后），记入 §4（作者要求合并前补上，附录 A.5 ③）。
- [x] 满足门槛（无 Critical、Important；以临时 apt 方式推送的提交 CI 全部通过；本机镜像验证通过）后把 PR 改为 ready，不合并。
- [x] 作者问整理提交的脚本是否要写进 plan：删去过时的 `order-commits.py` 与两段一次性的改写操作，补上当时缺少的 4 处命令；之后按作者要求只保留必要的命令（本 Task 最后一步）。
- [x] 作者要求完整复核 spec 与 plan，并把测试中用过、未入库的脚本写进 plan（`/tmp` 在环境重建后清空）：补附录 A.2 ⑦、A.3 ⑥（`measure-page.sh`）、A.5 ⑥，A.3 ①、A.6 补截图目录的来历，A.4 ③ 的 `loop.sh` 改为与实际运行的一致（编号为最后一步重编后的）；spec 的 §3.2、§3.5、§3.6、§4、§5、§6 与 QA 前言同步。
- [x] 作者要求确认入库脚本的源码不在本 plan 中：逐行比对 9 个入库文件中长度不少于 25 字符的行，只有 `# shellcheck source=/dev/null` 一行通用注释出现在附录 A.3 ⑥ 的 `measure-page.sh` 中。PR #10 plan 附录 C.5 与 PR #13 plan 附录 A.4 中的旧版脚本按 spec Q13 保留原文，开头有状态行。
- [x] 作者问 `tests/README.md` 是否需要补充：「preview 脚本」小节补上何时运行与核对的覆盖范围、修改或新增脚本（`lib.sh` 的函数、本地 ShellCheck 命令与验证方法）、排查；主体并入 `hammer(tests)`，提到 CI 的三处并入 `ci(github-actions)`；README 中的本地 ShellCheck 命令实际运行为 9 个文件 0 条，列出的函数在 `lib.sh` 中都存在。
- [x] 作者要求改写历史，去掉 `ci(github-actions)` 中的临时 apt 步骤：把 `chore(cloud-env)` 移到 `ci(github-actions)` 之前，各提交的 workflow 都没有临时步骤，最终的树不变（§2）。
- [x] 作者要求给 `tests/README.md` 的标题加序号：二级标题 `## 1.`–`## 8.`，「preview 脚本」为 `### 7.1`，一级标题不编号；仓库内没有指向这些标题的锚点链接，其他文档按名称引用，不受影响；PR #11 的 `check-headings.py` 为 0 处。单独作为 `docs(tests)` 提交，放在 `docs(superpowers)` 之前（§2）。
- [x] Codex 审查（P1，附录标题应写成 `## 附录 A`、去掉冒号与说明）：核实后不成立，不修改。仓库现有 6 个附录标题都是 `## 附录 X：说明`，没有只写字母的；制定该规则的 PR #11 自己把 PR #9 的附录改成了 `## 附录 A：未入库的验证脚本`（PR #11 spec §3.5），规则约束的是编号方式（字母、不占正文章号），`check-headings.py` 对本 plan 为 0 处。已回复 Codex 并标记已解决（§4）。
- [x] Codex 审查（P2）：`start.sh` 无条件 `rm -rf "$PREVIEW_WORK/tmp" "$PREVIEW_WORK/wifi"`，`PREVIEW_WORK` 可由用户覆盖，设成 `$HOME` 会删掉 `$HOME/tmp` 与 `$HOME/wifi`。修复前复现：模拟家目录中的两个用户文件被删除。核实属实，按 `tests/tools/mutate.sh` 的做法在 `lib.sh` 加 `pv_prepare_work`，`start.sh` 删除前调用（spec §3.2）；`tests/README.md` 写明限制；并入 `hammer(tests)`。测试见附录 A.3 ⑦，桌面与 `--headless` 各 480、720 回归通过，本机镜像中 `run-img.sh` 通过；已回复 Codex 并标记已解决（§4）。
- [x] Codex 第二轮审查（`start.sh`）：P1 桌面上已有同名窗口时，退回逻辑在新窗口出现前就选中旧窗口；P2 写完会话文件后认不出主屏时直接退出，`preview` 与自起的 Xvfb 残留。修复前都已复现（附录 A.3 ⑧），属实。修复：启动前记下已有的同名窗口，退回时只考虑新出现的；成功启动前的任何退出由 EXIT trap 统一清理，原来两处手工清理随之去掉（spec §3.2）；`tests/README.md`「排查」写明这两点；并入 `hammer(tests)`。修复中一度在全新显示上没有同名窗口时触发 `set -e`，由 P2 的测试发现并改正。附录 A.3 ④⑤⑦⑧ 重跑、桌面与 `--headless` 各 480、720 回归、本机镜像中 `run-img.sh` 均通过；已回复 Codex 并标记已解决（§4）。
- [x] 合并前准备：spec 状态行缩短为 PR #1–#4 的写法；Task 9、10 标题补 ✅；不再做完整的 A/B/C 审查（合并前自查已做过一轮，之后的代码改动均已在 CI、本机与镜像中验证，Codex 也已审查）。
- [x] 作者指出附录不必记录所有脚本：只保留支撑 spec 与 §4 中事实、数字的实测命令和测试脚本，删去整理提交（原 A.8）、合并前自查（原 A.9）、文档检查（原 A.3）、PR #13 spec 锚点核对、本地 ShellCheck（已在 `tests/README.md`）、读取 CI 日志与拼图交付的命令，附录重新编号为 A.1–A.6。

### Task 11：合并后核对 dev CI

- [ ] 作者合并后，dev push 的 CI 重建镜像，6 个 job 通过，preview 核对步骤使用镜像中的工具；结果回复在 PR 中。

## 4. 验证证据

| 项 | 结果 |
|---|---|
| 起点（`c13283c`） | 旧脚本实测见 spec §2.2（附录 A.2 ②） |
| ShellCheck（Task 2） | 本机与 CI 均为 0.9.0；新增 9 个文件（8 个 `*.sh` 与 `bin/wpa_cli`）0 条；CI 共检查 12 个文件（另有 `.cursor/install.sh`、`.cursor/start.sh`、`tests/tools/mutate.sh`） |
| 云端桌面 `:1`（Task 3） | 480、720 全部通过，每个分辨率约 31 s（启动、两个核对脚本、`shots.sh`）；`check-music-list.sh` 展开与再展开时按钮中心 `srgb(255,224,168)`、面板底边 305 / 458，收起时 `srgb(182,176,187)`、无；`check-wifi.sh` 圆环区/图标区依次为 1/1、>1/1（各次 56–76）、1/18、1/1 |
| `--headless`（Task 3） | Xvfb 与 Xtigervnc 各在 480、720 下全部通过，数值同上，每次约 30 s；结束后只剩桌面 `:1` 的 X 服务器 |
| 失败路径（Task 3） | 480 屏面板底边预期改为 306：`480 open 不符：…面板底边=305；预期 …、306`，退出码 1，约 11 s；`COMPLETED` 的图标区预期改为 1：退出码 1，约 21 s；两张截图留在 `shots/`；没有会话时 `check-wifi.sh` 与 `PREVIEW_BUILD` 无效时 `start.sh` 退出码 2 |
| CI 镜像中模拟（Task 4） | dev 签名镜像（标签 `e8e7beed…`）中以 `--user 0`：apt 新装 47 个包，480、720 全部通过，含构建 `preview` 共 86 s |
| CI 失败路径（Task 4） | 临时提交上 run `39241026037`：`🧪 原生测试` 的 `ctest` 126/126，核对步骤报 `480 open 不符：…面板底边=305；预期 …、306` 并以 1 失败；artifact `preview-shots` 140336 字节，含 `shots/480-music-{enter,open}.png`、`480-page.png`、`preview.log`、`xserver.log`、`wifi/cmd.log`；ShellCheck 通过 |
| CI（Task 4，Dockerfile 改动之前） | run `39241411409`：6 个 job 全部通过；`🧪 原生测试` 2 分 37 秒，其中临时 apt 9 s、preview 核对 75 s，480、720 两个核对脚本均通过，成功时没有 `preview-shots`；`🧬 变体检验` `baseline green: 126 tests; running 136 mutants`、`136 run, 0 skipped`，14 分 42 秒；ShellCheck 5 s |
| 全量 `ctest`（Task 6） | 126/126，arm32 2 条 |
| 镜像大小（Task 7） | 同一时间构建对比：`.cursor/Dockerfile` 1,283,314,529 → 1,317,370,225 字节（+34,055,696）；`.cursor/Dockerfile.luckfox_pico` 1,412,431,377 → 1,454,079,482 字节（+41,648,105） |
| `.cursor/Dockerfile` 镜像（Task 7） | `imagemagick 8:6.9.12.98+dfsg1-5.2build2`、`xdotool 1:3.20160805.1-5build1`、`xvfb 2:21.1.12-1ubuntu1.8`；`ubuntu` 与 root 各跑全量 `ctest` 126/126 与 480、720 的全部脚本，均通过，各约 90 s；挂载宿主 `:1` 时同样通过（68 s） |
| `.cursor/Dockerfile.luckfox_pico` 镜像（Task 7） | `imagemagick 8:6.9.11.60+dfsg-1.3ubuntu0.22.04.5`、`xdotool 1:3.20160805.1-4`、`xvfb 2:21.1.4-2ubuntu1.7~22.04.16`；480、720 的核对脚本通过（55 s）；jammy 的 neofetch 同样推荐 imagemagick |
| 最终提交的 CI（Task 7） | run `39245841129`：`🐳 构建 CI 镜像` 输出新标签 `6fe84fcf700ff305c92836d8df9cce38` 与「无已存在镜像；可信镜像只在 dev 上构建」，按预期失败；`build-demo`、`🧪 原生测试`、`🧬 变体检验` 被跳过；`🐚 ShellCheck` 通过；Task 10 放回 `chore(cloud-env)` 后的 run `39259422936` 相同；改写历史后的最终提交见 PR #14 的 Checks |
| 合并前自查（Task 10） | Minor 5 条：`AGENTS.md`「Lint」仍写没有 linter；spec §2.3 的行号未注明提交（`neofetch` 一行在 `c13283c` 为第 54 行、HEAD 为第 55 行）；spec §2.3 的表中「无」与「—」混用；`start.sh` 自起 X 服务器后连不上显示时不结束它；本 plan Task 7、9 的勾选与 §4 缺最终提交的 CI。本 PR 文档、`AGENTS.md`、`tests/README.md` 与 PR 正文中没有本 PR 的提交哈希（含整理前的） |
| `start.sh` 修复（Task 10） | 假 `Xvfb`（只 `sleep 300`）下：修复前 `start.sh` 以 2 退出、残留 1 个进程，修复后残留 0；桌面与 `--headless` 各 480、720 再跑一遍全部通过，结束后没有 `preview`、`Xvfb` 进程；ShellCheck 0 条 |
| CI 偶发失败（Task 10） | run `39253856488`（`hammer`、`ci`、`docs` 三个提交，临时 apt）：`🧪 原生测试` 的 `ctest` 126/126、apt 新装 47 个包，核对步骤在 480 屏报「15 s 内没有等到 preview 窗口与临时目录」并以 2 失败，artifact 只有日志（1299 字节）。`preview.log` 中有界面初始化打印的背光一行，`xserver.log` 中 Xvfb 编译键盘映射 78 次（每次 X 客户端连接一次，即等待循环跑满 75 次）。`xdotool search --pid 1 --name …` 能找到窗口、加 `--all` 后找不到，说明默认是「或」，75 次搜索期间不存在同名窗口，即那次 runner 上 `preview` 15 s 内还没开窗；`ps` 显示 `setsid` 后 `preview` 自成进程组（PGID = PID）。同一镜像在本机循环启动 30 次，0 次失败 |
| `start.sh` 调整（Task 10） | 包装脚本让 `$!` 与窗口 PID 不同时，打印「按 _NET_WM_PID 找不到窗口，改用唯一的同名窗口…（窗口 PID 329101，preview PID 329097）」并正常启动；不开窗口的假 `preview` 下 30 s 超时，打印进程与同名窗口的诊断，以 2 退出，自起的 Xvfb 被结束；桌面与 `--headless` 各 480、720 全部通过，等窗口 2–3 s |
| 修复后的 CI（Task 10，临时 apt） | run `39257101679`（`hammer`、`ci`、`docs` 三个提交，Dockerfile 与 dev 相同）：6 个 job 全部通过；`🧪 原生测试` 2 分 42 秒，其中临时 apt 9 s、preview 核对 74 s，480、720 的窗口都等了 2 s、两个核对脚本均通过，成功时没有 `preview-shots`；`🧬 变体检验` `baseline green: 126 tests; running 136 mutants`、`136 run, 0 skipped`，15 分 2 秒；ShellCheck 6 s |
| 最终提交的镜像验证（Task 10） | `start.sh` 调整后，最终提交在 `.cursor/Dockerfile` 的镜像中（`ubuntu`，`/tmp` 为 tmpfs）：全量 `ctest` 126/126；480、720 的两个核对脚本与 `shots.sh` 全部通过，窗口等了 2–3 s；89 s |
| 改写历史（Task 10） | 顺序改为 `hammer`、`chore`、`ci`、`docs`：4 个提交的 workflow 中都没有临时步骤；最终的树与改写前相同（之后只改了 `docs/` 与 `tests/README.md`）；`chore(cloud-env)` 只改两份 Dockerfile、`AGENTS.md` 与 `tests/README.md` 各一行 |
| Codex 审查（Task 10） | 审查改写历史后的提交，1 条 P1（附录标题格式），核实不成立：`git grep -h -E '^## 附录' origin/dev -- 'docs/superpowers/*.md'` 共 6 个附录标题，全部带「：说明」，只写字母的 0 个；回复并标记已解决 |
| Codex 审查 P2（Task 10，附录 A.3 ⑦） | 修复前：`PREVIEW_WORK` 指向含 `tmp/user-file.txt`、`wifi/notes.txt` 的目录时两者被删除。修复后：该目录、`/`、仓库本身、仓库内子目录、经符号链接指向仓库的路径、普通文件、标记为符号链接的目录都以 2 退出且不删除任何文件；不存在、空目录、已带标记的目录正常启动并创建标记；桌面与 `--headless` 各 480、720 全部通过；本机镜像（`--user 0`）中全量 `ctest` 126/126、全部脚本通过，88 s |
| Codex 第二轮（Task 10，附录 A.3 ⑧） | 修复前：P1 手动窗口 35651592 被记为会话窗口，`start.sh` 仍以 0 退出；P2 `start.sh` 以 2 退出后 Xvfb、假 `preview` 与其窗口都残留，会话文件仍在。修复后：P1 记为新窗口，窗口 PID 与 `start.sh` 启动的 PID 相同；P2 以 2 退出（「10 s 内没有进入 main 页面」），无残留，会话文件已删除。附录 A.3 ④（修复前残留 1、修复后 0）、⑤、⑦ 重跑符合；桌面与 `--headless` 各 480、720 通过；本机镜像（`--user 0`）全量 `ctest` 126/126、全部脚本通过，86 s |
| 截图比较（Task 8，附录 A.6） | 三组同名截图逐像素比较：布局一致；桌面与无桌面的差异块都在时钟数字、右上角 FPS/CPU、圆环、滚动歌名处，如 `480-shot-main` 2112 像素、`480-shot-pad` 69 像素（只在 FPS/CPU 处），`720-music-open` 0 像素 |
| 截图（Task 8） | `/cursor/stores/self/preview-tools-20261019/`：`preview_tools_desktop_480_720.png`（SHA256 `e0a2b5840234fb451ec561913ba2dd74fb69fa5af750cd6c5224f85908f0fcfa`）、`preview_tools_headless_xvfb_480_720.png`（`7b5143cab5565eac29847994ceb5228e5652cf133ab989bbcf5f861c3a588e7e`）、`preview_tools_docker_image_480_720.png`（`c55be5449cf67e1f4ef4705ebe6e9289b472426e0afb594be5e978357972c826`）；每张 12 格（480、720 各 6 格：主屏、Scan 后、画板、列表展开、`SCANNING`、`COMPLETED`） |

## 5. 与计划的偏离

- Task 1：spec、plan 的提交中 plan 为空文件。原因是编辑 plan 与 `git add` 在同一批中并行执行，`git add` 读到了正在改写的文件；以 fixup 补上，整理提交时并入（§1 增加一条约束）。
- Task 3：首次在桌面运行时发现 3 处 `start.sh` 的时序问题，修改后通过（spec §3.2）：进程刚 fork 时名字还不是 `preview`；窗口先于临时目录出现；`tst_app_setup_ui()` 的 1.5 s 内点击会丢。另为在同时装有 Xvfb 与 Xtigervnc 的本机逐一验证，加了 `PREVIEW_XSERVER`。
- Task 4：失败时除截图外还上传日志（spec §3.6）。故意失败的临时提交第一次建时漏了身份前缀，作者为环境变量中的值；推送后立即以正确身份改写并强推，该提交随后丢弃，不在最终历史中。
- Task 7：本机 Docker 的存储驱动为 fuse-overlayfs，容器内 `/tmp` 的 `NAME_MAX` 为 251，`music.list.options_over_255`、`music.mpv_cmd.escaping_and_length` 两条用例需要 255 字节的文件名而失败，dev 的镜像同样失败；给 `/tmp` 挂 tmpfs（`NAME_MAX` 255）后全部通过（附录 A.5 ③⑥）。CI 不受影响。
- Task 10：修复后第一次 CI 中 `start.sh` 在 runner 上 15 s 内没等到窗口（§4），等待改为 30 s 并加诊断；原先以为是 `--pid` 不匹配，实测 xdotool 默认按「或」匹配，改为 `--all`。
- Task 10：作者要求改写历史去掉临时 apt 步骤。以临时 apt 方式通过 CI 的提交（run `39257101679`）因此不在最终历史中，只以 run 记录；它与最终提交在 `tests/`、`custom/`、`generated/`、`src/`、`lib/` 上相同。
- Task 10：按最终的树重新建 `chore`、`ci`、`docs` 三个提交时用了 `git commit`，作者时间变成了改写当时（2026-10-21）；作者发现后，用 `git commit --amend --date` 改回第一次提交时的值（2026-10-19 的 19:31:04、18:55:57、16:47:00），各提交的树与提交说明不变。
- Task 10：编辑 plan 的命令把含 `PY` heredoc 的文本又放进了外层 `<<'PY'` 中，外层提前结束，后面几行被当作命令执行；误执行的 `git commit` 因没有暂存内容而失败，没有产生提交，`git reset --hard` 丢掉了尚未提交的 spec 修改，已重做。之后编辑脚本一律先写成文件再执行，嵌套的 heredoc 换用不同的分隔符。

## 附录 A：未入库的命令

只保留两类：支撑 spec 与本 plan §4 中事实、数字的实测命令，以及验证用的测试脚本；整理提交、合并前自查、文档检查、读取 CI 日志与交付截图等过程性命令不记录（作者要求，Task 10）。在仓库根目录执行；主机测试的构建目录为 `build-tests`（`preview` 脚本的默认值），测试脚本放在 `/tmp/ws/`，Docker 命令需要 `sudo`。

### A.1 环境准备

```bash
cmake -S tests -B build-tests -DLUCKFOX_TESTS_ARM32=ON && cmake --build build-tests -j"$(nproc)"
ctest --test-dir build-tests -j"$(nproc)" --output-on-failure
cmake --build build-tests --target preview -j"$(nproc)"
sudo apt-get install -y imagemagick xdotool xvfb shellcheck   # Environment Build 早于 Task 7 的环境；xdotool 在 Cloud Agent 中已由平台安装
```

### A.2 实测与核对命令

| 数字或结论 | 环境与提交 | 命令 |
|---|---|---|
| 提到 `preview` 的文档：`AGENTS.md` 1、`tests/README.md` 1、5 份 spec 共 31、6 份 plan 共 68；`tests/tools/` 的 4 个文件（spec §2.1） | `c13283c` | ① |
| 旧脚本的实测结果与 ShellCheck 条数（spec §2.2） | 本机，`c13283c` | ② |
| Xtigervnc 可作无桌面的 X 服务器（spec §2.3） | 本机 | ③ |
| 控件的位置与尺寸是否缩放（spec §2.4） | `c13283c` | ④ |
| runner 预装的工具；Dockerfile 中两行注释的行号（spec §2.3） | `c13283c`、HEAD | ⑤ |
| 本机的 Xvfb、ImageMagick 与 ShellCheck 是之前的会话手动安装的（2026-10-10、2026-10-17，spec §2.3） | 本机 | ⑦ |
| 历史 spec 与 plan 的代码块中涉及 preview 的位置：PR #7 plan Task 6（2 处）、PR #9 plan 附录 A.4（6）、PR #10 plan 附录 C.5（23）、PR #13 plan 附录 A.1（1）、A.2（3）、A.4（11）、PR #7 spec §5.2（1）（spec Q13 的补充） | `c13283c` | ⑥ |

```bash
# ① 提到 preview 的文档与 tests/tools/ 的内容
R=c13283c
git grep -c preview $R -- AGENTS.md tests/README.md
for d in specs plans; do git grep -c preview $R -- "docs/superpowers/$d/*.md" | awk -F: -v d=$d '{s+=$3;n++} END{print d, n "份", s}'; done
git ls-tree --name-only $R tests/tools/

# ② 从 PR #10 plan 附录 C.5、PR #13 plan 附录 A.4 提取旧脚本并运行（preview 在 /tmp/bt）
mkdir -p /tmp/appc
P10=docs/superpowers/plans/2026-10-10-lvgl-example-board-test-followups.md
P13=docs/superpowers/plans/2026-10-18-lvgl-example-wifi-spinner.md
awk '/^### C\.5 /{a=1} /^### C\.6 /{a=0} a&&/^```bash$/{n++;p=1;next} p&&/^```$/{p=0;next} p{print > ("/tmp/appc/blk" n ".sh")}' $P10
(cd /tmp/appc && mv blk1.sh pv-start.sh && mv blk2.sh pv-songs.sh && mv blk3.sh pv-check.sh && mv blk4.sh preview_run.sh && chmod +x ./*.sh)
shellcheck -f gcc /tmp/appc/pv-*.sh /tmp/appc/preview_run.sh | grep -o 'SC[0-9]*' | sort | uniq -c
shellcheck tests/tools/mutate.sh .cursor/install.sh .cursor/start.sh && echo "已有脚本 0 条"
BT=/tmp/bt DISPLAY=:1 OUT=/tmp/pvshots bash /tmp/appc/pv-check.sh 480; BT=/tmp/bt DISPLAY=:1 OUT=/tmp/pvshots bash /tmp/appc/pv-check.sh 720
BT=/tmp/bt bash /tmp/appc/preview_run.sh; identify -format '%f %wx%h\n' /tmp/pv/*.png
awk '/^### A\.4 /{a=1} /^### A\.5 /{a=0} a&&/^```bash$/{n++;p=1;next} p&&/^```$/{p=0;next} p{print > ("/tmp/appc/w" n ".sh")}' $P13
sed -i 's#^cmake --build /tmp/bt --target preview -j8#:#' /tmp/appc/w1.sh
bash /tmp/appc/w1.sh; sleep 4; DISPLAY=:1 OUT=/tmp/pvshots bash /tmp/appc/w2.sh; pkill -x preview

# ③ Xtigervnc 作无桌面的 X 服务器：开窗并用 xwd 截图
Xtigervnc :98 -geometry 800x800 -depth 24 -SecurityTypes None -rfbport -1 -localhost=1 > /tmp/xv98.log 2>&1 & XV=$!
sleep 1.5; mkdir -p /tmp/pvtmp98
DISPLAY=:98 TMPDIR=/tmp/pvtmp98 PREVIEW_RES=480 setsid nohup /tmp/bt/preview > /tmp/pv98.log 2>&1 < /dev/null &
sleep 3; W=$(DISPLAY=:98 xdotool search --name 'Luckfox LVGL preview' | head -1)
DISPLAY=:98 xwd -silent -id "$W" | ffmpeg -loglevel error -y -f xwd_pipe -i - /tmp/x98.png; identify /tmp/x98.png
pkill -x preview; kill "$XV"

# ④ 位置与尺寸：luckfox_ 前缀的按 SCALE 缩放并截断取整（与 lib.sh 的整数除法相同），lv_obj_set_size 不缩放
grep -n -A4 '^void luckfox_lv_obj_set_pos' custom/custom_main.c
grep -n -A1 'set_pos(ui->\(Main_Wifi_btn\|Music_player_music_list_btn\|Music_player_back_btn\|WIFI_wifi_log_img\),' generated/setup_scr_*.c

# ⑤ runner 预装的工具（没有列出即为无）；平台清单中 xdotool 与 neofetch 注释的行号
timeout 30 gh api repos/actions/runner-images/contents/images/ubuntu/Ubuntu2404-Readme.md -H 'Accept: application/vnd.github.raw' |
  grep -i -E 'xdotool|sdl|imagemagick|xvfb|shellcheck'
for r in c13283c HEAD; do echo "$r: $(git show $r:.cursor/Dockerfile | grep -n 'xdotool xclip' | cut -d: -f1) $(git show $r:.cursor/Dockerfile | grep -n 'neofetch 在 universe' | cut -d: -f1)"; done

# ⑥ 历史 spec 与 plan 的代码块中涉及 preview 的位置（按所在标题计数；本 PR 的两份文档除外）
python3 - <<'PYEOF'
import glob, re, subprocess
files = subprocess.run(['git', 'ls-tree', '-r', '--name-only', 'c13283c', 'docs/superpowers/'], capture_output=True, text=True).stdout.split()
for f in sorted(x for x in files if x.endswith('.md')):
    inb, head, hits = False, '', {}
    for l in subprocess.run(['git', 'show', 'c13283c:' + f], capture_output=True, text=True).stdout.split('\n'):
        if l.startswith('#') and not inb: head = l[:60]
        if re.match(r'^(```|````)', l): inb = not inb; continue
        if inb and re.search(r'preview|xdotool|Xvfb|import -window|DISPLAY=', l): hits[head] = hits.get(head, 0) + 1
    for h, n in hits.items(): print('%3d  %s  %s' % (n, f.split('/')[-1], h))
PYEOF

# ⑦ 本机 apt 的安装记录
zgrep -h -E '^(Start-Date|Commandline)' /var/log/apt/history.log* | grep -B1 -E 'Commandline.*(xvfb|imagemagick|shellcheck)'
```

### A.3 脚本验证

```bash
# ① 云端桌面 :1
export DISPLAY=:1; T=tests/tools/preview
for r in 480 720; do s=$SECONDS; ( $T/start.sh $r && $T/check-music-list.sh && $T/check-wifi.sh && $T/shots.sh ); echo "rc=$? $((SECONDS-s))s"; $T/stop.sh; done
rm -rf /tmp/shots-desktop && cp -r /tmp/luckfox-preview/shots /tmp/shots-desktop   # 附录 A.6 比较用

# ② 无桌面：Xvfb 与 Xtigervnc 各一遍；结束后核对没有残留的 X 服务器
unset DISPLAY
for xs in Xvfb Xtigervnc; do for r in 480 720; do
  ( PREVIEW_XSERVER=$xs $T/start.sh $r && $T/check-music-list.sh && $T/check-wifi.sh && $T/shots.sh ) > /tmp/hl-$xs-$r.log 2>&1
  echo "$xs $r rc=$? $(grep -c 通过 /tmp/hl-$xs-$r.log) 通过"; $T/stop.sh; done; done
ps -eo pid,args | grep -E '[X]vfb|[X]tigervnc'; grep -h '圆环区\|面板底边' /tmp/hl-*.log | sort | uniq -c

# ③ 失败路径：复制一份脚本改错预期值；没有会话、没有 preview 时退出码 2
rm -rf /tmp/pvbad && cp -r tests/tools/preview /tmp/pvbad
sed -i 's/480) EDGE=305/480) EDGE=306/' /tmp/pvbad/check-music-list.sh; sed -i "s/^step COMPLETED 1 '>1'/step COMPLETED 1 1/" /tmp/pvbad/check-wifi.sh
export DISPLAY=:1; $T/start.sh 480 >/dev/null
/tmp/pvbad/check-music-list.sh; echo "music rc=$?"
W=$(. /tmp/luckfox-preview/session; echo "$PV_WIN"); xdotool mousemove --window "$W" 32 32 mousedown 1; sleep 0.15; xdotool mouseup 1; sleep 1   # 回到主屏
/tmp/pvbad/check-wifi.sh; echo "wifi rc=$?"
$T/stop.sh; $T/check-wifi.sh; echo "无会话 rc=$?"; PREVIEW_BUILD=/nonexist $T/start.sh; echo "无 preview rc=$?"

# ④ 自起的 X 服务器连不上时（假 Xvfb 只 sleep）是否被结束；修复前的脚本由去掉 EXIT trap 还原（原先在这里单独 kill，后由 trap 统一清理）
mkdir -p /tmp/fakex && printf '#!/bin/sh\n# 假 Xvfb：不开显示，只占住进程\nexec sleep 300\n' > /tmp/fakex/Xvfb && chmod +x /tmp/fakex/Xvfb
rm -rf /tmp/pvold && mkdir /tmp/pvold && git archive HEAD tests/tools/preview | tar -x -C /tmp/pvold
sed -i 's/^trap cleanup EXIT$/:/' /tmp/pvold/tests/tools/preview/start.sh
for S in /tmp/pvold/tests/tools/preview/start.sh tests/tools/preview/start.sh; do
  ( unset DISPLAY; PATH=/tmp/fakex:$PATH PREVIEW_XSERVER=Xvfb PREVIEW_WORK=/tmp/pv-fakex $S 480 )
  echo "$S rc=$? 残留：$(ps -eo pid,args | grep -c '[0-9] sleep 300$')"
  ps -eo pid,args | awk '$2 == "sleep" && $3 == "300" { print $1 }' | xargs -r kill   # 只结束本测试的 sleep 300
done

# ⑤ 找窗口：xdotool 的 --pid 与 --name 默认是「或」；PID 不一致时的退回；不开窗口时的超时诊断
export DISPLAY=:1; PREVIEW_WORK=/tmp/pv-norm $T/start.sh 480 >/dev/null; P=$(. /tmp/pv-norm/session; echo "$PV_PID")
echo "[$(xdotool search --pid 1 --name 'Luckfox LVGL preview' | head -1)] [$(xdotool search --all --pid 1 --name 'Luckfox LVGL preview' | head -1)] [$(xdotool search --all --pid "$P" --name 'Luckfox LVGL preview' | head -1)]"
PREVIEW_WORK=/tmp/pv-norm $T/stop.sh
mkdir -p /tmp/pvwrap && printf '#!/bin/bash\n# 以子进程运行真正的 preview，自身 PID 与窗口的 _NET_WM_PID 不同\n%s/build-tests/preview & c=$!\ntrap '"'"'kill $c'"'"' TERM\nwait $c\n' "$PWD" > /tmp/pvwrap/preview
mkdir -p /tmp/pvslow && printf '#!/bin/sh\n# 一直不开窗口\nexec sleep 60\n' > /tmp/pvslow/preview; chmod +x /tmp/pvwrap/preview /tmp/pvslow/preview
PREVIEW_WORK=/tmp/pv-wrap PREVIEW_BUILD=/tmp/pvwrap $T/start.sh 480; PREVIEW_WORK=/tmp/pv-wrap $T/stop.sh
( unset DISPLAY; PREVIEW_WORK=/tmp/pv-slow PREVIEW_BUILD=/tmp/pvslow $T/start.sh --headless 480 ); echo "rc=$?"
ps -eo pid,args | awk '$2 == "sleep" && $3 == "60" { print $1 }' | xargs -r kill

# ⑥ 页面特征点与 Scan 后红色提示的位置（spec §3.2 与 shots.sh 的判据），measure-page.sh 的源码见下
DISPLAY=:1 bash /tmp/ws/measure-page.sh
```

`measure-page.sh`（存为 `/tmp/ws/measure-page.sh`）：

```bash
#!/bin/bash
# 测页面特征点与 Scan 后红色提示的位置（spec §3.2、shots.sh 的判据）：480、720 各启动一次 preview，
# 打印主屏、WiFi 页、画板在 (230, 400) 与 (384, 400) 的颜色，以及点 Scan 后红色像素的外框（换算回 480 设计坐标）。
# 用法：DISPLAY=:1 bash measure-page.sh（或在无桌面环境中不设 DISPLAY）
set -euo pipefail
T=tests/tools/preview; W=${PREVIEW_WORK:-/tmp/luckfox-preview}
for r in 480 720; do
    "$T/start.sh" "$r" >/dev/null
    (
        # shellcheck source=/dev/null
        . "$T/lib.sh"
        pv_load
        for page in main wifi pad; do
            case $page in
                wifi) pv_tap 50 410 ;;
                pad) pv_tap 410 400; pv_wait_page main; pv_tap 145 410 ;;
            esac
            sleep 1.5
            f=$(pv_shot "sig-$page")
            echo "$r $page (230,400)=$(pv_pixel "$f" "$(pv_sc 230)" "$(pv_sc 400)") (384,400)=$(pv_pixel "$f" "$(pv_sc 384)" "$(pv_sc 400)")"
        done
        pv_tap 384 400
        pv_wait_page main
    )
    "$T/shots.sh" >/dev/null
    python3 - "$W/shots/$r-shot-wifi-scan.png" "$r" <<'PYEOF'
import subprocess, sys
f, r = sys.argv[1], int(sys.argv[2])
raw = subprocess.run(['convert', f, '-depth', '8', 'rgb:-'], capture_output=True).stdout
xs, ys = [], []
for i in range(r * r):
    R, G, B = raw[3 * i], raw[3 * i + 1], raw[3 * i + 2]
    if R > 200 and G < 80 and B < 80:
        xs.append(i % r); ys.append(i // r)
s = lambda v: v * 480 // r
print(r, '红色提示外框', min(xs), min(ys), max(xs), max(ys), '设计坐标', s(min(xs)), s(min(ys)), s(max(xs)), s(max(ys)))
PYEOF
    "$T/stop.sh"
done
```

实测（2026-10-21，桌面 `:1`）：两种屏幕下主屏为 `srgb(255,150,0)` / `srgb(255,255,255)`，WiFi 页为 `srgb(255,255,255)` / `srgb(151,151,152)`，画板为 `srgb(255,150,0)` / `srgb(151,151,152)`；红色提示外框 480 屏为 (150, 247)–(394, 261)，720 屏换算回设计坐标为 (150, 246)–(391, 261)。

```bash
# ⑦ 会话目录的防护（Codex P2）：work-guard.sh 的源码见下
bash /tmp/ws/work-guard.sh
```

`work-guard.sh`（存为 `/tmp/ws/work-guard.sh`）：

```bash
#!/bin/bash
# 会话目录的防护（Codex P2）：start.sh 只在 PREVIEW_WORK 归本脚本所有时才删除其中的 tmp/、wifi/。
# 用法（仓库根目录）：bash work-guard.sh；每种情况打印退出码，以及用户文件是否还在。需要已构建的 build-tests/preview。
set -u
T=tests/tools/preview; B=/tmp/wg; rm -rf "$B"; mkdir -p "$B"
run() { ( unset DISPLAY; PREVIEW_WORK="$1" "$T/start.sh" --headless 480 >/dev/null 2>"$B/err" ); rc=$?; PREVIEW_WORK="$1" "$T/stop.sh" 2>/dev/null; echo "$rc"; }
# ① 已有用户文件的目录（如 $HOME）：拒绝，文件不动
mkdir -p "$B/home/tmp" "$B/home/wifi"; echo 1 >"$B/home/tmp/user-file.txt"; echo 1 >"$B/home/wifi/notes.txt"
rc=$(run "$B/home"); echo "① 有用户文件 rc=$rc 文件 $(find "$B/home" -type f | wc -l)/2 $(head -c 80 "$B/err")"
# ② / 与仓库内的路径：拒绝
ln -s "$PWD" "$B/repolink"   # 经符号链接指向仓库内
for w in / "$PWD" "$PWD/build-tests/pv" "$B/repolink/pv"; do rc=$(run "$w"); echo "② $w rc=$rc $(head -c 60 "$B/err")"; done
# ③ 是文件而不是目录：拒绝
: >"$B/afile"; rc=$(run "$B/afile"); echo "③ 文件 rc=$rc $(head -c 50 "$B/err")"
# ④ 标记是符号链接：拒绝
mkdir -p "$B/link"; ln -s /etc/hostname "$B/link/.luckfox-preview-workdir"; rc=$(run "$B/link"); echo "④ 标记为符号链接 rc=$rc $(head -c 50 "$B/err")"
# ⑤ 不存在、空目录、已带标记的目录（第二次运行）：正常
rc=$(run "$B/new"); echo "⑤ 不存在 rc=$rc 标记 $( [ -f "$B/new/.luckfox-preview-workdir" ] && echo 有 || echo 无)"
mkdir -p "$B/empty"; rc=$(run "$B/empty"); echo "⑤ 空目录 rc=$rc"
rc=$(run "$B/new"); echo "⑤ 带标记的目录再次运行 rc=$rc"
```

```bash
# ⑧ 已有同名窗口时找窗口、启动中途失败时的清理（Codex 第二轮）：start-guard.sh 的源码见下；修复前的行为可在去掉 EXIT trap、退回时不排除旧窗口的 start.sh 上复现
bash /tmp/ws/start-guard.sh
```

`start-guard.sh`（存为 `/tmp/ws/start-guard.sh`）：

```bash
#!/bin/bash
# shellcheck disable=SC1091,SC2009  # 会话文件运行时才生成；统计进程按名字精确计数，不用 pgrep
# Codex 第二轮审查的两条（start.sh）：
# P1 桌面上已有同名的 preview 窗口时，start.sh 是否误用旧窗口；
# P2 写完会话文件后认不出主屏时，start.sh 是否留下 preview 与自起的 Xvfb。
# 用法（仓库根目录，需要已构建的 build-tests/preview 与桌面 :1）：bash start-guard.sh
set -u
T=tests/tools/preview; B=/tmp/sg; rm -rf "$B"; mkdir -p "$B"

# ---- P1 ----
mkdir -p "$B/manual"
DISPLAY=:1 TMPDIR="$B/manual" PREVIEW_RES=480 setsid nohup build-tests/preview >"$B/manual.log" 2>&1 </dev/null &
M=$!
sleep 3
OLD=$(DISPLAY=:1 xdotool search --all --pid "$M" --name 'Luckfox LVGL preview' | head -1)
( export DISPLAY=:1 PREVIEW_WORK="$B/p1"; "$T/start.sh" 480 >"$B/p1.out" 2>&1 )
echo "P1 start.sh rc=$?"
NEW=$(. "$B/p1/session"; echo "$PV_WIN")
NEWPID=$(. "$B/p1/session"; echo "$PV_PID")
echo "P1 手动窗口 $OLD；会话记录的窗口 $NEW（$( [ "$NEW" = "$OLD" ] && echo 误用了旧窗口 || echo 是新窗口)）；新窗口的 PID $(DISPLAY=:1 xdotool getwindowpid "$NEW" 2>/dev/null)，start.sh 启动的 PID $NEWPID"
grep -h '改用唯一的同名窗口' "$B/p1.out" | cut -c1-80
PREVIEW_WORK="$B/p1" "$T/stop.sh"
kill "$M" 2>/dev/null; sleep 1

# ---- P2 ----
mkdir -p "$B/fake"
cat >"$B/fake/preview" <<'EOF'
#!/bin/bash
# 假 preview：建好临时目录，开一个同名但不是主屏的窗口（xmessage 作子进程，自身进程名为 preview）
mkdir -p "$TMPDIR/luckfox-tests-fake"
xmessage -title "Luckfox LVGL preview (480x480)" "not the main screen" & c=$!
trap 'kill $c' TERM
wait $c
EOF
chmod +x "$B/fake/preview"
( unset DISPLAY; PREVIEW_WORK="$B/p2" PREVIEW_BUILD="$B/fake" PREVIEW_XSERVER=Xvfb "$T/start.sh" --headless 480 >"$B/p2.out" 2>&1 )
echo "P2 start.sh rc=$?；$(grep -o '10 s 内没有进入 main 页面' "$B/p2.out" | head -1)"
sleep 1
echo "P2 残留：Xvfb $(ps -eo comm | grep -cx Xvfb) 个，假 preview $(ps -eo comm | grep -cx preview) 个，xmessage $(ps -eo comm | grep -cx xmessage) 个，会话文件 $( [ -f "$B/p2/session" ] && echo 在 || echo 不在)"
# 清理本测试留下的进程
PREVIEW_WORK="$B/p2" "$T/stop.sh" 2>/dev/null
for p in Xvfb xmessage; do ps -eo pid,comm,args | awk -v p="$p" '$2 == p && $0 ~ /\/tmp\/sg|:99|not the main/ { print $1 }' | xargs -r kill; done
```

### A.4 CI

```bash
# ① 推送前在 dev 签名的 CI 镜像中模拟 native-tests 的新步骤（--user 0 与 CI 相同）
TAG=$(git show origin/dev:.cursor/Dockerfile | sha256sum | cut -c1-32); IMG=ghcr.io/yuangezhizao/luckfox-lvgl-demo-ci:$TAG
sudo docker pull -q "$IMG"
rm -rf /tmp/ci-sim && mkdir -p /tmp/ci-sim/out && git archive HEAD | tar -x -C /tmp/ci-sim
sudo docker run --rm --user 0 -v /tmp/ci-sim:/w -w /w "$IMG" bash -c '
set -euo pipefail
apt-get update >/dev/null && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends imagemagick xdotool xvfb > /w/out/apt.log 2>&1
grep "newly installed" /w/out/apt.log
cmake -S tests -B build-tests >/dev/null && cmake --build build-tests --target preview -j"$(nproc)" >/w/out/build.log 2>&1
export PREVIEW_WORK=/w/out/pv
for res in 480 720; do
  tests/tools/preview/start.sh --headless "${res}"; tests/tools/preview/check-music-list.sh; tests/tools/preview/check-wifi.sh
  tests/tools/preview/shots.sh; tests/tools/preview/stop.sh
done'

# ② CI 的失败路径：在临时提交中把 480 屏的面板底边预期改错，推送后核对步骤应以 1 失败并上传 artifact，之后丢弃该提交
sed -i 's/480) EDGE=305/480) EDGE=306/' tests/tools/preview/check-music-list.sh
# artifact 的内容；Task 10 的偶发失败（R=39253856488）中 Xvfb 每有一个 X 客户端连接就编译一次键盘映射，preview.log 中有界面初始化打印的背光一行
R=${R:?先设为该次 run 的 id}
rm -rf /tmp/art && mkdir /tmp/art && (cd /tmp/art && timeout 120 gh run download "$R" -R yuangezhizao/luckfox_pico_lvgl_example -n preview-shots) && find /tmp/art -type f | sort
echo "xkbcomp $(grep -c 'The XKEYBOARD keymap compiler' /tmp/art/xserver.log) 次；背光行 $(grep -c 'brightness: no backlight' /tmp/art/preview.log)"
```

```bash
# ③ CI 镜像中循环启动 30 次（Task 10），失败时打印会话
TAG=$(git show origin/dev:.cursor/Dockerfile | sha256sum | cut -c1-32); IMG=ghcr.io/yuangezhizao/luckfox-lvgl-demo-ci:$TAG
rm -rf /tmp/ci-sim2 && mkdir -p /tmp/ci-sim2 && git archive HEAD | tar -x -C /tmp/ci-sim2 && cp /tmp/ws/loop.sh /tmp/ci-sim2/ && chmod +x /tmp/ci-sim2/loop.sh
sudo docker run --rm --user 0 -v /tmp/ci-sim2:/w -w /w "$IMG" /w/loop.sh
```

`loop.sh`（存为 `/tmp/ws/loop.sh`，下面为实际运行的原文；`x11-utils` 是为备用的 `xprop` 诊断而装，未用到）：

```bash
#!/bin/bash
set -uo pipefail
apt-get update >/dev/null && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends imagemagick xdotool xvfb x11-utils >/dev/null 2>&1
cmake -S tests -B /tmp/b >/dev/null && cmake --build /tmp/b --target preview -j"$(nproc)" >/dev/null 2>&1
export PREVIEW_BUILD=/tmp/b PREVIEW_WORK=/tmp/lp
fail=0
for i in $(seq 30); do
  if ! out=$(tests/tools/preview/start.sh --headless 480 2>&1); then
    fail=$((fail+1)); echo "== 第 $i 次失败：$out" | tail -2
    . /tmp/lp/session 2>/dev/null; echo "PV_PID=$PV_PID DISPLAY=$PV_DISPLAY"
  fi
  tests/tools/preview/stop.sh
done
echo "失败 $fail/30"
```

### A.5 本机镜像验证

```bash
# ① 三个包是否已在两份 FROM 中；neofetch 的推荐包
for img in "ubuntu:24.04@sha256:4fbb8e6a8395de5a7550b33509421a2bafbc0aab6c06ba2cef9ebffbc7092d90" \
           "luckfoxtech/luckfox_pico:1.0@sha256:915d44588085826cbeda4b969dbbe7d5e54bf779ba36cda3c5072ee9533e0417"; do
  sudo docker run --rm --entrypoint dpkg-query "$img" -W -f='${Package} ${Status}\n' imagemagick xdotool xvfb; done
apt-cache show neofetch | grep -m1 Recommends

# ② 本 PR 与 dev 的两份 Dockerfile 同时构建，对比大小（字节）
rm -rf /tmp/devctx && mkdir -p /tmp/devctx && git archive origin/dev .cursor | tar -x -C /tmp/devctx
sudo docker build -q -f .cursor/Dockerfile -t pv-tools:active .cursor; sudo docker build -q -f .cursor/Dockerfile.luckfox_pico -t pv-tools:alt .cursor
sudo docker build -q -f /tmp/devctx/.cursor/Dockerfile -t pv-tools:dev-active /tmp/devctx/.cursor
sudo docker build -q -f /tmp/devctx/.cursor/Dockerfile.luckfox_pico -t pv-tools:dev-alt /tmp/devctx/.cursor
for t in dev-active active dev-alt alt; do echo "$t $(sudo docker image inspect pv-tools:$t --format '{{.Size}}')"; done

# ③ .cursor/Dockerfile 镜像：ubuntu 与 root 各跑一遍 run-img.sh（源码见下）。本机 Docker 为 fuse-overlayfs，容器内 /tmp 的
#    NAME_MAX 为 251，两条要 255 字节文件名的用例会失败（dev 的镜像同样失败），所以 /tmp 挂 tmpfs
sudo docker info --format '{{.Driver}}'
# 容器中以 root 写入的文件宿主删不掉，重跑时换一个新目录（Task 10 用的是 /tmp/img-src3）
rm -rf /tmp/img-src && mkdir -p /tmp/img-src && git archive HEAD | tar -x -C /tmp/img-src && chmod -R a+rwX /tmp/img-src
cp /tmp/ws/run-img.sh /tmp/img-src/ && chmod +x /tmp/img-src/run-img.sh
for u in ubuntu 0; do sudo docker run --rm --user $u --tmpfs /tmp:exec,size=4g -v /tmp/img-src:/w pv-tools:active bash -c 'getconf NAME_MAX /tmp; /w/run-img.sh'; done

# ④ 挂载宿主 :1（PR #9 plan 附录 A.4 mesa-probe.sh 的方式）
xa=$(mktemp); cp "${XAUTHORITY:-$HOME/.Xauthority}" "$xa"; chmod 644 "$xa"
sudo docker run --rm --network host --ipc=host --tmpfs /tmp:exec,size=4g -v /tmp/.X11-unix:/tmp/.X11-unix:ro -v "$xa":/xauth:ro \
  -e XAUTHORITY=/xauth -e DISPLAY=:1 -v /tmp/img-src:/w pv-tools:active bash -c '
set -euo pipefail; cd /w; B=/tmp/b
cmake -S tests -B $B >/dev/null && cmake --build $B --target preview -j"$(nproc)" >/dev/null
export PREVIEW_BUILD=$B PREVIEW_WORK=/tmp/luckfox-preview
for res in 480 720; do
  tests/tools/preview/start.sh "$res"; tests/tools/preview/check-music-list.sh | tail -1; tests/tools/preview/check-wifi.sh | tail -1
  tests/tools/preview/shots.sh >/dev/null; tests/tools/preview/stop.sh
done'; rm -f "$xa"

# ⑤ .cursor/Dockerfile.luckfox_pico 镜像：工具与核对脚本
sudo docker run --rm --tmpfs /tmp:exec,size=4g -v /tmp/img-src:/w pv-tools:alt bash -c '
dpkg-query -W -f="\${Package} \${Version}\n" imagemagick xdotool xvfb
set -euo pipefail; cd /w; B=/tmp/b
cmake -S tests -B $B >/dev/null && cmake --build $B --target preview -j"$(nproc)" >/dev/null
export PREVIEW_BUILD=$B PREVIEW_WORK=/tmp/luckfox-preview
for res in 480 720; do
  tests/tools/preview/start.sh --headless "$res" >/dev/null; tests/tools/preview/check-music-list.sh | tail -1
  tests/tools/preview/check-wifi.sh | tail -1; tests/tools/preview/stop.sh
done'

# ⑥ 不挂 tmpfs 时（容器内 /tmp 在 fuse-overlayfs 上，NAME_MAX 251）：本 PR 与 dev 的镜像都有同样的 2 条用例失败，与本 PR 无关
for t in active dev-active; do echo "== $t"
  sudo docker run --rm --user 0 -v /tmp/img-src:/w pv-tools:$t bash -c 'cd /w && getconf NAME_MAX /tmp && cmake -S tests -B /tmp/b -DLUCKFOX_TESTS_ARM32=ON >/dev/null &&
    cmake --build /tmp/b -j"$(nproc)" >/dev/null && ctest --test-dir /tmp/b -j"$(nproc)" 2>&1 | grep -E "tests passed|\(Failed\)"'; done
```

`run-img.sh`（存为 `/tmp/ws/run-img.sh`）：

```bash
#!/bin/bash
# 镜像内验证：工具来源、全量 ctest（arm32）与 --headless 下的全部 preview 脚本
set -euo pipefail
cd /w
echo "user=$(id -un) $(convert -version | head -1 | cut -c1-40) xdotool=$(command -v xdotool) Xvfb=$(command -v Xvfb)"
dpkg-query -W -f='${Package} ${Version}\n' imagemagick xdotool xvfb
B=/tmp/bt-img
cmake -S tests -B $B -DLUCKFOX_TESTS_ARM32=ON >/dev/null && cmake --build $B -j"$(nproc)" >/dev/null && ctest --test-dir $B -j"$(nproc)" | grep -E "tests passed"
cmake --build $B --target preview -j"$(nproc)" >/dev/null
export PREVIEW_BUILD=$B PREVIEW_WORK=/tmp/luckfox-preview
for res in 480 720; do
  tests/tools/preview/start.sh --headless "$res"
  tests/tools/preview/check-music-list.sh | tail -1
  tests/tools/preview/check-wifi.sh | tail -1
  tests/tools/preview/shots.sh >/dev/null
  tests/tools/preview/stop.sh
done
mkdir -p /w/out && cp -r /tmp/luckfox-preview/shots "/w/out/shots-$(id -un)"
```

### A.6 截图比较

三组截图：桌面为附录 A.3 ① 结束后复制的 `/tmp/shots-desktop`；无桌面为下面第一段命令的 `/tmp/pv-headless/shots`；镜像为附录 A.5 ③ 的 `/tmp/img-src/out/shots-ubuntu`（拼图用），Task 10 重跑时目录为 `/tmp/img-src3`，逐像素比较用的是它。拼图与交付的命令不记录。

```bash
# 无桌面（Xvfb）的截图
T=tests/tools/preview; export PREVIEW_WORK=/tmp/pv-headless; rm -rf "$PREVIEW_WORK"
for r in 480 720; do ( unset DISPLAY; PREVIEW_XSERVER=Xvfb $T/start.sh --headless $r && $T/check-music-list.sh && $T/check-wifi.sh && $T/shots.sh >/dev/null ); $T/stop.sh; done
unset PREVIEW_WORK
# 三组截图逐像素比较（PR 评论）：不同像素数，以及桌面与无桌面之间的差异块（宽x高+x+y），用来判断差异是否只在时钟、FPS/CPU、圆环、滚动歌名上
A=/tmp/shots-desktop; B=/tmp/pv-headless/shots; C=/tmp/img-src3/out/shots-ubuntu
for r in 480 720; do for n in shot-main shot-wifi-scan shot-pad music-open wifi-SCANNING wifi-COMPLETED; do f=$r-$n.png
  blocks=$(convert "$A/$f" "$B/$f" -compose difference -composite -threshold 0 -define connected-components:verbose=true \
    -define connected-components:area-threshold=5 -connected-components 8 null: 2>/dev/null | awk 'NR > 1 && $NF != "srgb(0,0,0)" && $NF != "gray(0)" { print $2 }' | tr '\n' ' ')
  echo "$f 桌面/无桌面=$(compare -metric AE "$A/$f" "$B/$f" null: 2>&1) 无桌面/镜像=$(compare -metric AE "$B/$f" "$C/$f" null: 2>&1) 差异块：$blocks"
done; done
# 交付到项目存储的三张拼图（§4 的 SHA256）
(cd /cursor/stores/self/preview-tools-20261019 && sha256sum -c SHA256SUMS)
```
