# Luckfox Pico LVGL Example — CI 变体检验实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** workflow 新增 `mutation-tests`（`🧬 变体检验`）job，每次 `pull_request` 推送与 `dev` push 时全量运行 `tests/tools/mutate.sh`，退出码非 0 或出现 `SKIPPED` 即失败，失败时按类型打印相关日志；音乐页测试的目录读取顺序与文件系统无关；`mutate.sh` 输出显示进度；`AGENTS.md`、`tests/README.md` 写明，PR #10 plan §6.1、§6.5 只留状态行与链接。

**Architecture:** 先推送 spec 与 plan、开草稿 PR（Task 1）；仿照 `native-tests` 加 job（Task 2）；本机用 actionlint 与 CI 镜像复现（Task 3）；补文档（Task 4）；首次 CI 运行发现 `MUSIC-N6-list-unsorted` 在 runner 上存活（Task 5），据 spec Q12、Q13 插入 Task 6（固定读取顺序）与 Task 7（失败步骤按类型补打印）；合并前自查后按 spec Q14、Q15 插入 Task 8（输出显示进度）；在 runner 上实测耗时与稳定性（Task 9）；用临时分支的探针确认三类失败（Task 10）；整理提交、精简 plan、写 PR 正文（Task 11）；合并前自查（Task 12）。

**Tech Stack:** GitHub Actions、bash、CTest、`tests/tools/mutate.sh`（Python 3 解析清单）、C（`tests/support/fake_fs.c`）、docker（本机复现 CI 镜像）、loop 设备上的 ext4（`mkfs.ext4 -E hash_seed=`）、actionlint 1.7.12 + ShellCheck 0.9.0（按需安装，不入库）、`gh`。

**Spec:** [`docs/superpowers/specs/2026-10-16-lvgl-example-ci-mutation-design.md`](../specs/2026-10-16-lvgl-example-ci-mutation-design.md)

---

## 1. Global Constraints

- 分支 `pi/ci-mutation`，起点 `origin/dev` `1615931`（PR #11 合入）；PR #12 指向 `dev`，开发时为草稿，合并前改为 ready，执行者不合并。
- 不改 `custom/` 与 `.cursor/Dockerfile`；`tests/tools/mutate.sh` 只加进度输出（spec §3.11）；测试只改音乐源码的目录读取与 `list.mutants`（spec §1 目标 3、§3.10）。
- 提交身份 `Cursor Agent <cursoragent@cursor.com>`，SSH 签名；改写提交的 git 命令前加 `env -u GIT_AUTHOR_NAME -u GIT_AUTHOR_EMAIL -u GIT_COMMITTER_NAME -u GIT_COMMITTER_EMAIL`，推送前 `git cat-file -p HEAD | sed -n 3,4p` 核对。
- 提交信息 `<type>(<scope>): <emoji> <subject>`，正文为要点、写原因，以「详见 spec §…、Q…」结尾；文档中不写本 PR 自身提交的哈希。
- 推送：`GIT_TERMINAL_PROMPT=0 timeout 120 git push https://yuangezhizao@github.com/yuangezhizao/luckfox_pico_lvgl_example.git ...`，改写历史时写明 `--force-with-lease=pi/ci-mutation:<远端旧提交>`；`gh` 命令同样加 `timeout`。更新 PR 正文前先 `gh pr view --json body` 取回当前正文。
- 本机后台检验先后执行，不并行（耗时数字才可比）；等待后台进程时不用 `pgrep -f`（会匹配到等待命令自身）。
- 范围变化时，一次性复核 spec、plan、PR 正文与提交说明中相关的结论、数字和「只」「不变」「均」「全部」等说法。

## 2. Tasks

### Task 1：spec 与 plan，推送并开草稿 PR ✅

- [x] 写 spec（QA 记录 Q1–Q11）与本 plan，`check-headings.py`（附录 A.3）检查为 0。
- [x] 提交 `docs(superpowers)`，推送，开草稿 PR #12 指向 `dev`。

### Task 2：新增 `mutation-tests` job ✅

文件：`.github/workflows/build-luckfox-lvgl-demo.yml`

- [x] 在 `native-tests` 之后加 job（spec §3.1–§3.5）：`needs: build-image`、`if: github.event_name != 'schedule'`、`timeout-minutes: 30`、同一容器与 `--user 0`、job 级 `env` 的 `MUTATE_WORK`、`MUTATE_OUT` 写字面量。
- [x] 运行步骤：`mutate.sh 2>&1 | tee "$MUTATE_OUT" || rc=$?`，按 0、1、2 分别处理并写 `::error::`；rc=0 时断言 `^[0-9]+ run, 0 skipped$`。
- [x] 失败步骤 `if: failure()`（Task 7 按 spec Q13 改写）。

### Task 3：本机验证 ✅

- [x] `actionlint`（附录 A.1）检查 workflow 为 0 条。
- [x] 全量 `ctest`（`-DLUCKFOX_TESTS_ARM32=ON`）通过；本机 8 核全量 `mutate.sh` rc=0（附录 A.2）。
- [x] `ci-local.sh`（附录 A.2）在 CI 镜像中按 workflow 原文运行两个步骤：正常源码 rc=0；五个探针（附录 A.4）的结果符合 spec §3.3、§3.4。

### Task 4：文档 ✅

文件：`AGENTS.md`、`tests/README.md`、`docs/superpowers/plans/2026-10-10-lvgl-example-board-test-followups.md`

- [x] `AGENTS.md`「GitHub Actions（交叉编译门禁）」补一条（spec §3.8），`--user 0` 一句加上 `mutation-tests`。
- [x] `tests/README.md`「改坏检验」补一句；「注意」补目录读取顺序一句（随 Task 6）。
- [x] PR #10 plan §6.5 开头加状态行，标题不动。
- [x] PR 改为 ready 后按 spec Q16、Q17 精简 PR #10 plan 的 §6.1、§6.5：只留状态行，链接对应 spec 与 PR #10 合并时的原文（链接文字为文档名、提交为完整哈希）；原文两个锚点在该提交的 GitHub 渲染结果中存在。
- [x] 按 spec Q18 联网核对 AGENTS.md 的写法后，移除 `mutation-tests` 一条末尾的 spec 链接。
- [x] Task 9 后按 runner 实测补写耗时（`AGENTS.md`、workflow 注释）。

### Task 5：首次 CI 运行 ✅

- [x] PR #12 上首次运行：`🧬 变体检验` 失败，`[MUSIC-N6-list-unsorted] SURVIVED`、`119 run, 0 skipped`、rc=1；其余 4 个 job 通过。
- [x] `gh run rerun --job` 重跑 1 次，结果相同；两次运行步骤 11 分 42 秒、11 分 32 秒（附录 A.5）。
- [x] 按 systematic-debugging 定位：本机复现读取顺序依赖（spec §2.4），向作者提出 Q12、Q13。

### Task 6：固定音乐目录的 readdir 顺序（spec Q12）✅

文件：`tests/support/fake_fs.c`、`tests/support/fake_fs.h`、`tests/CMakeLists.txt`、`tests/cases/music/unit_list_alloc.c`、`tests/cases/music/list.mutants`、`tests/README.md`

- [x] `fs-seed.sh`（附录 A.6）找到升序、降序两个种子并挂载为 `TMPDIR`。
- [x] 先加变体 `MUSIC-N6-list-prepend`，修复前在本机默认、升序、降序三个文件系统上跑 N6 的 5 个变体：默认全部 `KILLED`；升序 4 个存活（`list-prepend` 除外）；降序 `list-unsorted`、`list-prepend` 存活。
- [x] 加 `tst_readdir()`、`tst_closedir()` 与编译覆盖（spec §3.10）；三个文件系统上全量 `ctest` 117/117，N6 的 5 个变体全部 `KILLED`；`screenshots`、`preview` 能构建。
- [x] 全量 `mutate.sh` 在本机默认与升序种子文件系统上 rc=0、`120 run, 0 skipped`。

### Task 7：失败步骤按类型补打印（spec Q13）✅

文件：`.github/workflows/build-luckfox-lvgl-demo.yml`

- [x] 存活时打印清单记录、`<id>.log` 与复现命令；基线不绿时 `ctest --rerun-failed --output-on-failure`；都没有时打印一行说明；`actionlint` 为 0 条。
- [x] 用伪造的 `mutate.out` 检查存活与清单错误两种输出；`ci-local.sh` 的基线、编译失败、清单错误探针输出符合 spec §3.4。
- [x] `ci-local.sh` 的存活探针输出符合 spec §3.4。

### Task 8：改坏检验输出显示进度（spec Q14、Q15）✅

文件：`tests/tools/mutate.sh`、`.github/workflows/build-luckfox-lvgl-demo.yml`、`tests/README.md`

- [x] `mutate.sh`：基线通过后打印开始提示，逐条结果行加 `[序号/总数]` 前缀（spec §3.11）；ShellCheck 0 条。
- [x] workflow 失败步骤识别 `SURVIVED`、`BUILD FAILED` 的正则加上前缀；actionlint 0 条；伪造的新格式输出能识别出存活与编译失败的 id。
- [x] 本机指定 3 个 id 运行：`running 3 mutants`、`[1/3]`–`[3/3]`。
- [x] 本机全量；`ci-local.sh` 的存活、编译失败、跳过探针。
- [x] 推送后 CI 再运行一次（§3「最终代码的 CI」）。

### Task 9：CI 实测与稳定性 ✅

- [x] Task 6、7 推送后的运行通过、`120 run, 0 skipped`；`gh run rerun --job` 重跑 2 次，共 3 次全部通过，记下耗时（附录 A.5）。
- [x] 按实测值更新 workflow 注释、`AGENTS.md`、spec §2.3、§3.5 与 §6 中的耗时。

### Task 10：失败探针 ✅

- [x] 从 PR 分支建 `pi/ci-mutation-probe-survive`、`-manifest`、`-skipped` 三个临时分支，各加一个探针提交（附录 A.4），`gh workflow run --ref` 触发。
- [x] 确认：survive 为 rc=1、失败步骤打印 `PROBE-survive` 的记录与复现命令；manifest 为 rc=2、`record missing tests`；skipped 为 rc=0 但判定失败并列出 `SKIPPED` 行；其余 job 不受影响。
- [x] 删除三个临时分支（本地与远端）。

### Task 11：整理提交、精简 plan、PR 正文 ✅

- [x] `git rebase` 整理为 spec §5 的五个提交（Task 8 前为四个），`test(tests)` 在最前、`docs(superpowers)` 在最后；核对身份与签名；逐提交跑全量 `ctest`（PR #10 plan 附录 C.3）。
- [x] 精简 plan：已完成 Task 写成「落地文件 + [x] 步骤」，不重复仓库正文，保留未入库的脚本与命令；无偏离则删去「与计划的偏离」一节。
- [x] `check-headings.py` 检查新 spec 与 plan 为 0，spec 的 Mermaid 图语法校验通过；PR 正文按 PR #10、#11 的格式写（标题编号 `## 1.`）。

### Task 12：合并前自查

- [x] 第 1 轮：按作者审查模板（A 代码与构建、B 事实与复现、C 规范与表述）由执行者自查，每项给脚本证据，只查不改；无 Critical、Important，5 条 Minor（spec §3.10「现有用例都是 4 项」、spec §2.3 标题日期、PR #10 plan §6.5 状态行漏写测试改动、本表「其余 4 个 job」、最终提交的 CI 未记录），作者确认修复；作者另提出输出显示进度（Task 8）。
- [x] 第 2 轮：Task 8 与 Minor 修复后，对最终提交按同一模板复查：无 Critical、Important，1 条 Minor（PR #10 plan §6.5 状态行未提进度输出），作者确认修复；另按作者要求核对提交说明：5 个提交的标题为 `<type>(<scope>): <emoji> <subject>`，emoji 与 gitmojis 3.15.0 的码点一致（`dx` 对应 `:technologist:`），标题不超过 100 字符（最长 43），正文每行不超过 100 字符，与 cz-conventional-emoji 1.1.8 的 `maxHeaderWidth`、`maxLineWidth` 默认值相符。
- [x] 第 3 轮（收尾）：按 spec Q19、Q20 删去 3 条与提交有关的 QA 并重新编号、补附录 A.7，用 `qref.py`（附录 A.7 ⑨）核对编号；按作者审查模板由执行者担任 A、B、C 自查，只查不改，结果在会话中报告作者；满足门槛后保持 ready，不合并。
- [ ] 满足门槛（无 Critical、Important，CI 通过）后把 PR 改为 ready，不合并。

## 3. 验证证据

| 项 | 结果 |
|---|---|
| 起点 `mutate.sh`（`origin/dev` `1615931`） | 本机 8 核 rc=0，`119 run, 0 skipped`，365 s；CI 镜像限 4 核 rc=0，同上，514 s（spec §2.3） |
| actionlint | 1.7.12（含 ShellCheck 0.9.0）对 workflow 为 0 条；Task 7 的第一版报 2 条 SC2013（`for id in $(…)`），改为 `while read` 后为 0 |
| `ci-local.sh`，Task 2 的 workflow 加 `1615931` 的测试 | 正常源码 rc=0，`119 run, 0 skipped`，524 s；manifest 探针 rc=2、`record missing tests`；skipped 探针 `118 run, 1 skipped`，判定失败 rc=1；survive 探针 `[PROBE-survive] SURVIVED`、`120 run, 0 skipped`、rc=1 |
| 首次 CI（Task 5） | 2 次均 `[MUSIC-N6-list-unsorted] SURVIVED`、rc=1；运行步骤 11 分 42 秒、11 分 32 秒，job 12 分 02 秒、12 分 05 秒；失败步骤为空（原设计，引出 spec Q13） |
| 读取顺序依赖（修复前） | 见 spec §2.4：升序种子上全量 119 个中 4 个 N6 存活、115 个 `KILLED`（8 核 377 s）；N6 的 5 个变体在三个文件系统上的存活情况见 Task 6 |
| 修复后（Task 6） | 三个文件系统上全量 `ctest` 117/117（含 arm32 2 条）；N6 的 5 个变体全部 `KILLED`；`screenshots`、`preview` 构建通过 |
| 失败步骤（Task 7） | 基线探针打印 `main.sysinfo.popen_failure` 的重跑输出（`runtime error: null pointer passed as argument 3`）；编译探针打印 `#error probe build failure`；清单探针打印「没有可补充的日志」；伪造的存活输出打印 `list.mutants` 中的记录、4 条 Passed 与复现命令。原设计打印的 `LastTest.log` 末尾 200 行看不到失败断言 |
| 本机，修复后（Task 6） | 8 核全量：默认文件系统 rc=0、`120 run, 0 skipped`、373 s；升序种子（`TMPDIR=/mnt/fsa/t`）rc=0、同上、371 s |
| `ci-local.sh`，修复后（Task 6、7） | 正常源码 rc=0、`120 run, 0 skipped`、518 s；survive 探针 `121 run, 0 skipped`、rc=1、518 s，失败步骤打印 `PROBE-survive` 的记录、1 条 Passed 与复现命令；skipped 探针 `119 run, 1 skipped`、判定失败 rc=1、514 s；manifest 探针 rc=2、0 s；baseline 探针 rc=2、34 s；build 探针 rc=2、134 s |
| 修复后 CI（Task 9） | 3 次全部通过、`120 run, 0 skipped`：运行步骤 10 分 10 秒、11 分 41 秒、9 分 18 秒，job 10 分 42 秒、12 分 12 秒、9 分 52 秒；第 1 次运行中其余 4 个 job 均通过（后 2 次只重跑变体检验） |
| CI 探针（Task 10） | `workflow_dispatch` 触发三个临时分支：survive 的 `🧬 变体检验` 失败，`[PROBE-survive] SURVIVED`、`121 run, 0 skipped`，失败步骤折叠组「PROBE-survive 存活：…」含复现命令，运行步骤 9 分 17 秒；manifest 失败，`record missing tests`、rc=2，失败步骤打印「没有可补充的日志」，运行步骤 0 s；skipped 失败，`119 run, 1 skipped`、判定失败，运行步骤 10 分 33 秒；三个分支的其余 4 个 job 均通过；分支已删除 |
| 输出进度（Task 8） | 本机指定 3 个 id：`baseline green: 117 tests; running 3 mutants`、`[1/3]`–`[3/3]`、rc=0；本机 8 核全量 `[  1/120]`–`[120/120]`、`120 run, 0 skipped`、rc=0、376 s；`ci-local.sh`：存活探针 `[ 21/121] [PROBE-survive] SURVIVED`、rc=1、517 s，失败步骤打印该记录与复现命令；编译失败探针 `[ 21/121] [PROBE-build] BUILD FAILED`、rc=2、135 s，失败步骤打印构建日志；跳过探针 `[ 20/120] [MAIN-16] SKIPPED`、`119 run, 1 skipped`、判定失败 rc=1、507 s |
| 整理提交（Task 11） | 每次 `git rebase`（Task 8 前、Task 8 后、收尾精简 QA 后）前后树都相同；最终 5 个提交均为 `Cursor Agent` 且带 SSH 签名；逐提交测试（PR #10 plan 附录 C.3 的 `percommit.sh`）5 个提交均 117/117，122 s（Task 8 前为 4 个提交、97 s） |
| 最终代码的 CI（Task 8） | 5 个 job 全部通过；`🧬 变体检验` 输出 `baseline green: 117 tests; running 120 mutants`、`[  1/120]`–`[120/120]`、`120 run, 0 skipped`，运行步骤 11 分 41 秒，job 12 分 11 秒；此后只改了文档与注释（spec、plan、`AGENTS.md`；`tests/README.md` 示例中的序号，实际为 `[ 35/120]`，原写 37；`fake_fs.c`、`list.mutants` 注释中随 spec Q19 重排的 QA 编号）；最终提交的 CI 见 PR 的 Checks |
| 文档检查 | `check-headings.py`（附录 A.3）对本 spec 与 plan 问题 0 处；spec 的 2 个 Mermaid 图用 PR #9 plan 附录 A.3 的 `check.mjs` 校验，均为 `flowchart-v2` OK；附录 A 的 4 个脚本与 `/tmp/cm` 中的现行版本逐字节一致（执行期间只改过注释） |

## 4. 与计划的偏离

- Task 5：首次 CI 运行发现 `MUSIC-N6-list-unsorted` 在 runner 上存活，本机却一直 `KILLED`。根因是测试依赖 ext4 的目录哈希种子（spec §2.4）。原范围「不改测试」扩大为固定音乐源码的读取顺序并新增一个变体（spec Q12），插入 Task 6；最初的 Task 6–8（探针、整理、自查）顺延，现为 Task 10–12，Task 5 中「3 次通过」的步骤移到现在的 Task 9。
- Task 2、7：原设计在变体存活时不补打印，首次 CI 的失败步骤为空（spec Q13）；基线失败时打印的 `LastTest.log` 按完成顺序排列，末尾看不到失败断言，改为 `ctest --rerun-failed --output-on-failure`。插入 Task 7。
- 合并前自查后作者要求变体检验的输出显示进度（spec Q14、Q15），原「不改 `mutate.sh`」改为只加进度输出；在 Task 7 后插入 Task 8，原 Task 8–11 顺延为 Task 9–12。
- PR 改为 ready 后，作者提出 PR #10 plan 的 §6.1、§6.5 只留状态行与链接（spec Q16、Q17），范围扩展到 PR #11 留下的 §6.1；只改文档，归入 Task 4。
- Q12 的选项 B 原写「tmpfs 按创建顺序返回」，实测本机内核 6.12 为逆序，已在 spec Q12 更正。
- Task 10：建探针分支时在工作区里 `git switch`，而本机全量 `mutate.sh` 正在后台运行；它直接读取工作区的 `tests/` 并复制 `custom/`，两次运行分别报 `cannot apply mutant` 与 `mutants list failed`（rc=2）。切回 PR 分支后重跑，结果见 §3。以后后台检验期间不切换分支，探针改在 `git worktree` 中做。
- 环境：本机 docker 为 fuse-overlayfs，容器内 `NAME_MAX` 251，两条长文件名用例失败，`ci-local.sh` 把 `/tmp` 绑定到宿主 ext4（spec §2.3）。`git push` 经 Cursor 的 askpass 取凭据，约 22 s，超时设为 120 s。开草稿 PR 时集成令牌报 `Resource not accessible by integration`，作者执行 `gh auth login` 后成功。第一个等待脚本用 `pgrep -f` 等前一个进程结束，匹配到了等待命令自身而一直等待，改为直接串行执行。

## 附录 A：未入库的脚本

脚本放在 `/tmp/cm/`，在仓库根目录执行。

### A.1 actionlint

```bash
mkdir -p /tmp/al && cd /tmp/al && curl -fsSL https://raw.githubusercontent.com/rhysd/actionlint/main/scripts/download-actionlint.bash | bash && cd -
sudo apt-get install -y shellcheck   # actionlint 用它检查 run: 脚本
/tmp/al/actionlint .github/workflows/build-luckfox-lvgl-demo.yml; echo "rc=$?"
```

### A.2 本机与 CI 镜像中的变体检验

本机 8 核全量：`tests/tools/mutate.sh` 已入库（PR #7），命令用 PR #10 plan 附录 C.2，计时在前面加 `time`；在 loop 文件系统上跑时再加 `TMPDIR=<挂载点>/t`（附录 A.6）。

`mksrc.sh`：把工作区（含未提交的改动）复制成源码目录，可选施加探针。

```bash
#!/bin/bash
# 把工作区中受 git 跟踪的文件（含未提交的改动）复制到 <目录>，供 ci-local.sh 使用；可选再施加 probe.sh 的探针。
# 用法（仓库根目录）：mksrc.sh <目录> [探针名]
set -euo pipefail
rm -rf "$1"; mkdir -p "$1"
git ls-files -z | xargs -0 tar -c | tar -x -C "$1"
if [ -n "${2:-}" ]; then "$(dirname "$0")/probe.sh" "$2" "$1" >/dev/null; fi
```

`ci-local.sh`：在 CI 镜像（`ghcr.io/yuangezhizao/luckfox-lvgl-demo-ci:<.cursor/Dockerfile 的 sha256 前 32 位>`，公开可拉取）中按 workflow 原文运行 `mutation-tests` 的两个 `run` 步骤。

```bash
#!/bin/bash
# 在 CI 镜像中按 workflow 原文运行 mutation-tests 的两个 run 步骤（限 4 核、--user 0），本机复现 CI 的判定与失败日志。
# 用法（仓库根目录）：ci-local.sh <源码目录>；源码目录由 mksrc.sh 或 git archive 得到。
# 容器 /tmp 绑定到宿主 ext4 目录：本机 docker 为 fuse-overlayfs，容器内 NAME_MAX 251，两条长文件名用例会失败（spec §2.3）。
set -euo pipefail
SRC=$(realpath "$1")
WF=.github/workflows/build-luckfox-lvgl-demo.yml
IMG=${IMG:-ghcr.io/yuangezhizao/luckfox-lvgl-demo-ci:$(sha256sum .cursor/Dockerfile | cut -c1-32)}
T=$(mktemp -d /tmp/ci-local.XXXXXX)
chmod 1777 "$T"
python3 - "$WF" "$T" <<'PY'
import sys, yaml
job = yaml.safe_load(open(sys.argv[1]))["jobs"]["mutation-tests"]
runs = [s["run"] for s in job["steps"] if "run" in s]
assert len(runs) == 2, runs
open(sys.argv[2] + "/step-run.sh", "w").write(runs[0])
open(sys.argv[2] + "/step-fail.sh", "w").write(runs[1])
open(sys.argv[2] + "/env", "w").write("".join("%s=%s\n" % kv for kv in job["env"].items()))
PY
echo "image=$IMG tmp=$T src=$SRC"
sudo -n docker run --rm --user 0 --cpuset-cpus 0-3 --memory 16g --env-file "$T/env" \
  -v "$SRC":/w -v "$T":/tmp -w /w "$IMG" bash -c '
    s=$SECONDS
    bash --noprofile --norc -eo pipefail /tmp/step-run.sh; rc=$?
    echo "step rc=$rc elapsed=$((SECONDS - s))s"
    if [ "$rc" -ne 0 ]; then echo "--- failure() 步骤 ---"; bash --noprofile --norc -eo pipefail /tmp/step-fail.sh; echo "fail-step rc=$?"; fi'
```

例：`for p in baseline build manifest skipped survive; do mksrc.sh /tmp/cm/p-$p $p; ci-local.sh /tmp/cm/p-$p > /tmp/cm/run-$p.log 2>&1; done`。

### A.3 `check-headings.py`

当时从 PR #11 plan（`2026-10-14-lvgl-example-heading-numbers.md`）附录 A.1 提取；该脚本已由 PR #16 入库到 `tests/tools/docs/`，依赖 `python3-markdown-it`：

```bash
python3 tests/tools/docs/check-headings.py docs/superpowers/specs/2026-10-16-lvgl-example-ci-mutation-design.md \
  docs/superpowers/plans/2026-10-16-lvgl-example-ci-mutation.md
```

### A.4 失败探针 `probe.sh`

```bash
#!/bin/bash
# 在给定目录（仓库根或 git archive 解出的源码）施加一个失败探针，验证 mutation-tests 会失败（spec §4 第 5 条）。
# 用法：probe.sh <survive|manifest|skipped|baseline|build> <目录>
#   survive ：新增一条只改注释的变体 PROBE-survive，期望 mutate.sh rc=1、[PROBE-survive] SURVIVED
#   manifest：删掉 MAIN-16 的 tests: 行，期望 rc=2、record missing tests
#   skipped ：把 MAIN-16 的 tests: 改成匹配不到用例的正则，期望 rc=0、0 skipped 判定失败
# 以下两个只在本机用 ci-local.sh 验证失败步骤打印的日志（spec §3.4），不推到 CI：
#   baseline：直接把 MAIN-16 的改动写进 custom/custom_main.c，期望 rc=2、baseline not green，打印重跑的失败用例
#   build   ：新增一条把注释换成 #error 的变体 PROBE-build，期望 rc=2、[PROBE-build] BUILD FAILED，打印 PROBE-build.build.log
set -euo pipefail
f=$2/tests/cases/main/sysinfo.mutants
grep -qx 'tests: ^main\\.sysinfo\\.' "$f"
case $1 in
  survive)
    test "$(grep -cF '/* AIC8800DC WLAN function 2; keep in sync with SDK insmod_wifi.sh */' "$2/custom/custom_main.c")" = 1
    printf '%s\n' '' 'id: PROBE-survive' 'file: custom/custom_main.c' \
      'old: /* AIC8800DC WLAN function 2; keep in sync with SDK insmod_wifi.sh */' \
      'new: /* probe: comment only */' 'tests: ^main\.sysinfo\.' >> "$f" ;;
  manifest) sed -i '/^tests: \^main\\\.sysinfo\\\.$/d' "$f" ;;
  skipped)  sed -i 's/^tests: \^main\\\.sysinfo\\\.$/tests: ^main\\.sysinfo_probe\\./' "$f" ;;
  baseline)
    python3 - "$2/custom/custom_main.c" <<'PY'
import sys
p = sys.argv[1]; s = open(p).read()
old = '        perror("popen failed");\n        return 0;\n'
assert s.count(old) == 1
open(p, "w").write(s.replace(old, '        perror("popen failed");\n'))
PY
    ;;
  build)
    test "$(grep -cF '/* AIC8800DC WLAN function 2; keep in sync with SDK insmod_wifi.sh */' "$2/custom/custom_main.c")" = 1
    printf '%s\n' '' 'id: PROBE-build' 'file: custom/custom_main.c' \
      'old: /* AIC8800DC WLAN function 2; keep in sync with SDK insmod_wifi.sh */' \
      'new: #error probe build failure' 'tests: ^main\.sysinfo\.' >> "$f" ;;
  *) echo "unknown probe: $1"; exit 2 ;;
esac
git -C "$2" diff --stat 2>/dev/null || true
cat "$f"
```

CI 上的探针（Task 10）：`git switch -c pi/ci-mutation-probe-<名> pi/ci-mutation && probe.sh <名> . && git commit -am "…"`，推送后 `gh workflow run build-luckfox-lvgl-demo.yml --ref pi/ci-mutation-probe-<名>`；验证后删除本地与远端分支。

### A.5 CI 耗时

```bash
# 每个 job 与步骤的起止时间；<job id> 取自 gh run view <run id> --json jobs
gh api repos/yuangezhizao/luckfox_pico_lvgl_example/actions/jobs/<job id> \
  --jq '[.started_at,.completed_at]|@tsv, (.steps[]|[.name,.started_at,.completed_at,.conclusion]|@tsv)'
gh run view --job <job id> --log | grep -E 'SURVIVED|SKIPPED|[0-9]+ run, |##\[error\]'
```

### A.6 目录哈希种子 `fs-seed.sh`

```bash
#!/bin/bash
# 用 loop 镜像上的 ext4 复现「读取顺序取决于目录哈希种子」（spec §2.4）。需要免密 sudo。
# 用法：fs-seed.sh find <期望顺序> [次数]   随机种子反复 mkfs，直到按 03 01 04 02 建的 4 个文件以期望顺序读出，打印种子
#       fs-seed.sh mount <种子> <挂载点>    用该种子建 64 MiB ext4 并挂载，挂载点下建 1777 的 t/，可作 TMPDIR
# 例：fs-seed.sh find "01 02 03 04"；fs-seed.sh mount e5bb29e1-580e-4076-be76-fc95834b43b9 /mnt/fsa
set -euo pipefail
mk() {  # $1=种子 $2=镜像文件
  truncate -s 64M "$2"
  mkfs.ext4 -q -F -E hash_seed="$1" "$2" >/dev/null
}
# shellcheck disable=SC2012  # 文件名固定为 song_NN.mp3，ls -U 即按 readdir 原序列出
order() {  # $1=挂载点：按 03 01 04 02 建文件后 ls -U 的顺序（只取编号）
  sudo -n mkdir "$1/d"
  for n in 03 01 04 02; do sudo -n touch "$1/d/song_$n.mp3"; done
  ls -U "$1/d" | sed -E 's/song_([0-9]+)\.mp3/\1/' | tr '\n' ' ' | sed 's/ $//'
}
case $1 in
  find)
    img=$(mktemp /tmp/fs-seed.XXXXXX.img); mnt=$(mktemp -d /tmp/fs-seed.XXXXXX)
    for i in $(seq 1 "${3:-200}"); do
      seed=$(cat /proc/sys/kernel/random/uuid)
      mk "$seed" "$img"; sudo -n mount -o loop "$img" "$mnt"
      o=$(order "$mnt"); sudo -n umount "$mnt"
      if [ "$o" = "$2" ]; then echo "try=$i seed=$seed order=$o"; rm -f "$img"; rmdir "$mnt"; exit 0; fi
    done
    echo "not found"; rm -f "$img"; rmdir "$mnt"; exit 1 ;;
  mount)
    img=/tmp/fs-seed-$2.img
    mk "$2" "$img"; sudo -n mkdir -p "$3"; sudo -n mount -o loop "$img" "$3"
    echo "order=$(order "$3")"
    sudo -n mkdir -m 1777 "$3/t"; echo "TMPDIR=$3/t" ;;
  *) echo "usage: $0 find <order> [tries] | mount <seed> <mountpoint>"; exit 2 ;;
esac
```

spec §2.4 用到的两个种子：升序 `e5bb29e1-580e-4076-be76-fc95834b43b9`（挂到 `/mnt/fsa`，`order=01 02 03 04`），降序 `647e99ee-211c-4cd6-bb4a-a5143255677c`（`/mnt/fsb`，`order=04 03 02 01`）。本机默认文件系统的种子用 `sudo dumpe2fs -h /dev/vdc | grep 'Directory Hash Seed'` 查看。

N6 的 5 个变体在某个文件系统上的结果：

```bash
N6="MUSIC-N6-list-unsorted MUSIC-N6-list-prepend MUSIC-N6-head-not-updated MUSIC-N6-stale-id MUSIC-N6-sig-unsorted"
rm -rf /tmp/m-n6; TMPDIR=/mnt/fsa/t MUTATE_WORK=/tmp/m-n6 tests/tools/mutate.sh $N6; echo "rc=$?"
```

### A.7 实测与核对命令

spec §2.3、§2.4 与 §3.6 中其余数字的来源（spec Q20）。`IMG` 为 CI 镜像，即 `IMG=ghcr.io/yuangezhizao/luckfox-lvgl-demo-ci:$(sha256sum .cursor/Dockerfile | cut -c1-32)`；本机 docker 需 `sudo`。

| 数字或结论 | 环境与提交 | 命令 |
|---|---|---|
| 镜像中有完整 `python3`，由 `iotop` 与 `libsdl2-dev` 的依赖链带入 | CI 镜像 `e8e7beed…` | ① |
| CI 镜像限 4 核全量 514 s；本机 docker 容器内 `NAME_MAX` 251，基线不绿 | `origin/dev` `1615931` | ② |
| 基线开销：配置与构建 11 s、全量 `ctest` 23 s（收尾复跑为 12 s、24 s） | 同上 | ③ |
| `MUTATE_WORK` 418 MB、239 个日志 | 本机 8 核全量（附录 A.2）之后 | `du -sh /tmp/mut-all; ls /tmp/mut-all/*.log \| wc -l` |
| PR #11 最后 6 次 CI 约 1.5 分钟（82–98 s） | GitHub | ④ |
| `dev` 没有必需检查 | GitHub | `gh api repos/yuangezhizao/luckfox_pico_lvgl_example/rules/branches/dev --jq '[.[].type]'` |
| 本机 ext4 与 tmpfs 的读取顺序，本机种子 `…0004` | 本机 | ⑤ |
| 调用 `tst_run_ms()` 的测试文件 9 个 | `origin/dev` `1615931` | `grep -rl tst_run_ms tests/cases \| wc -l` |
| workflow 新增 83 行 | 本 PR | `git diff --numstat origin/dev -- .github/workflows/build-luckfox-lvgl-demo.yml` |
| 失败步骤能从新格式输出中识别出存活与编译失败的 id | 本 PR | ⑥ |
| PR #10 plan 原文链接的两个锚点存在 | `85a6dd48264938b267fc0e4ecd458f6ac14f0530` | ⑦ |
| spec 的 Mermaid 图语法正确 | 本 PR | ⑧ |
| spec 的 QA 为 Q1–Q20 连续，spec、plan、提交说明与代码注释中的编号都指向存在的条目 | 本 PR | ⑨ |

```bash
# ① 镜像中的 Python 与来源
sudo docker run --rm --user 0 "$IMG" bash -c 'dpkg -l "python3*" | awk "/^ii/{print \$2}"; python3 -c "import glob, os, re, sys"
  apt-cache depends iotop | grep python3; dpkg -s libglib2.0-dev libglib2.0-dev-bin | grep ^Depends
  apt-cache rdepends --installed libibus-1.0-dev libpulse-dev'

# ② 4 核全量（/tmp 绑定到宿主 ext4；不绑定时 getconf NAME_MAX /tmp 为 251，两条长文件名用例失败、基线不绿）
rm -rf /tmp/cimut && mkdir -p /tmp/cimut/src /tmp/cimut/work /tmp/cimut/ctmp && chmod 1777 /tmp/cimut/ctmp
git archive 1615931 | tar -x -C /tmp/cimut/src
sudo docker run --rm --user 0 --cpuset-cpus 0-3 --memory 16g -v /tmp/cimut/src:/w -v /tmp/cimut/work:/work \
  -v /tmp/cimut/ctmp:/tmp -w /w -e MUTATE_WORK=/work/m1 "$IMG" \
  bash -c 'nproc; s=$(date +%s); tests/tools/mutate.sh; echo "rc=$? elapsed=$(( $(date +%s)-s ))s"'
sudo docker run --rm "$IMG" getconf NAME_MAX /tmp; sudo docker info | grep 'Storage Driver'

# ③ 基线开销
sudo docker run --rm --user 0 --cpuset-cpus 0-3 -v /tmp/cimut/src:/w -v /tmp/cimut/work:/work -v /tmp/cimut/ctmp:/tmp -w /w "$IMG" \
  bash -c 's=$SECONDS; cmake -S tests -B /work/bt >/dev/null && cmake --build /work/bt -j4 >/dev/null; m=$SECONDS
           ctest --test-dir /work/bt -j4 | grep "tests passed"; echo "build=$((m-s))s ctest=$((SECONDS-m))s"'

# ④ PR #11 的 CI 时长（秒）
gh run list --branch pi/heading-numbers --limit 6 --json createdAt,updatedAt -q '.[]|[.createdAt,.updatedAt]|@tsv' |
  python3 -c 'import sys,datetime as d; f=lambda x: d.datetime.fromisoformat(x.replace("Z","+00:00"))
for l in sys.stdin: a,b=l.split(); print(int((f(b)-f(a)).total_seconds()))'

# ⑤ 按 03 01 04 02 建文件后的读取顺序；本机 ext4 的哈希种子
for base in /tmp /dev/shm; do d=$(mktemp -d "$base/rd.XXXX"); for n in 03 01 04 02; do touch "$d/song_$n.mp3"; done
  echo "$base: $(ls -U "$d" | tr '\n' ' ')"; rm -rf "$d"; done
sudo dumpe2fs -h "$(df --output=source /tmp | tail -1)" | grep -E 'features|Directory Hash Seed|Default directory hash'

# ⑥ 失败步骤：用伪造的 mutate.out 运行 workflow 原文
python3 -c 'import yaml; j=yaml.safe_load(open(".github/workflows/build-luckfox-lvgl-demo.yml"))["jobs"]["mutation-tests"]
open("/tmp/step-fail.sh","w").write([s["run"] for s in j["steps"] if "run" in s][1])'
rm -rf /tmp/fk && mkdir -p /tmp/fk/w && echo 'compile error' > /tmp/fk/w/MUSIC-N8-gap-unscaled.build.log
printf '[ 35/120] [MUSIC-N6-list-unsorted] SURVIVED\n[ 38/120] [MUSIC-N8-gap-unscaled] BUILD FAILED, see /x\n' > /tmp/fk/out
MUTATE_OUT=/tmp/fk/out MUTATE_WORK=/tmp/fk/w bash --noprofile --norc -eo pipefail /tmp/step-fail.sh

# ⑦ 原文锚点（取该提交的 GitHub 渲染结果）
gh api -H 'Accept: application/vnd.github.html' \
  "repos/yuangezhizao/luckfox_pico_lvgl_example/contents/docs/superpowers/plans/2026-10-10-lvgl-example-board-test-followups.md?ref=85a6dd48264938b267fc0e4ecd458f6ac14f0530" |
  python3 -c 'import sys,re,urllib.parse as u; print([u.unquote(i) for i in re.findall(r"id=\"user-content-(6[15][^\"]*)\"", sys.stdin.read())])'

# ⑧ Mermaid：从 PR #9 plan 附录 A.3 提取 check.mjs 与 package.json
P9=docs/superpowers/plans/2026-10-06-lvgl-example-cloudagent-align-luckfox-pico.md; mkdir -p /tmp/mm
awk '/^### A\.3 /{a=1} a&&/^````js$/{p=1;next} p&&/^````$/{exit} p' $P9 > /tmp/mm/check.mjs
awk '/^### A\.3 /{a=1} a&&/^```json$/{p=1;next} p&&/^```$/{exit} p' $P9 > /tmp/mm/package.json
(cd /tmp/mm && npm install --silent && node check.mjs "$OLDPWD/docs/superpowers/specs/2026-10-16-lvgl-example-ci-mutation-design.md")

# ⑨ QA 编号核对：把下面的 qref.py 存到 /tmp/cm/，git log --format=%B origin/dev.. > /tmp/msg.txt 后运行 python3 /tmp/cm/qref.py /tmp/msg.txt
```

```python
# 核对本 PR spec 的 QA 编号与引用：条目须为 Q1..QN 连续；spec、plan、两处代码注释中的 Qn（含范围 Qa–Qb）都须 ≤ N，
# 指向其他 PR 的引用（前 30 字内出现「PR #8/#10/#11」或「其 spec」）与 QA 引言中的会话编号、重排前编号（前 30 字内有「会话」「原 Q」「本节有」）单独列出供人工确认；
# 代码注释只查本 PR 新增的行（git diff origin/dev）。用法：python3 qref.py [提交说明文件…]
import re, sys, subprocess
S = 'docs/superpowers/specs/2026-10-16-lvgl-example-ci-mutation-design.md'
files = [S, 'docs/superpowers/plans/2026-10-16-lvgl-example-ci-mutation.md',
         'tests/support/fake_fs.c', 'tests/cases/music/list.mutants'] + sys.argv[1:]
qa = [int(x) for x in re.findall(r'^\*\*Q(\d+)\. ', open(S).read(), re.M)]
N = len(qa); bad = 0
if qa != list(range(1, N + 1)):
    print('QA 编号不连续:', qa); bad += 1
def added(f):
    out = subprocess.run(['git', 'diff', '-U0', 'origin/dev', '--', f], capture_output=True, text=True).stdout
    return {l[1:] for l in out.split('\n') if l.startswith('+') and not l.startswith('+++')}
for f in files:
    keep = added(f) if f.startswith('tests/') else None
    for no, line in enumerate(open(f).read().split('\n'), 1):
        if keep is not None and line not in keep:
            continue
        for m in re.finditer(r'(?<![\w.])Q(\d+)(?:–Q(\d+))?(?!\d)', line):
            ctx = line[max(0, m.start() - 30):m.start()]
            ext = re.search(r'PR #(8|10|11)|其 spec|会话|原 Q|本节有', ctx)
            nums = [int(m.group(1))] + ([int(m.group(2))] if m.group(2) else [])
            if ext:
                print('外部引用 %s:%d: …%s%s' % (f, no, ctx[-20:], m.group(0)))
            elif max(nums) > N:
                print('越界 %s:%d: %s' % (f, no, m.group(0))); bad += 1
print('QA %d 条，问题 %d 处' % (N, bad)); sys.exit(1 if bad else 0)
```
