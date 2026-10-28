# Luckfox Pico LVGL Example — 文档检查脚本入库实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `check-headings.py`、`check-samples.sh` 入库到 `tests/tools/docs/`，新增 QA 编号检查，CI 新增 `📝 文档检查` job；依赖先在 runner 上临时 apt 安装，验证后挪进两份 Dockerfile；`AGENTS.md` 写明用法与提交、GitHub 操作的约定；历史 plan 中删除脚本副本；合并前按 Codex 审查与全面复查修复检查器对不渲染元素（`<script>`、`<style>`、`<template>`）与 QA 标题的误报及几处漏报。

**Architecture:** 先推送 spec 与 plan、开草稿 PR（Task 1）；脚本原样入库并处理出处与 SC2016（Task 2）；按 TDD 加 QA 编号检查（Task 3）；CI job 的 A 阶段，含一次故意失败（Task 4）；`AGENTS.md` 的提交与 GitHub 操作（Task 5）；历史 plan（Task 6）；改为 D 阶段并在本机镜像中验证（Task 7）；整理提交、精简 plan、PR 正文，合并前与作者确认，处理 Codex 审查（Task 8）；合并后核对 dev CI（Task 9）。

**Tech Stack:** Python 3（markdown-it-py 3.0.0 / 1.1.0）、bash、ShellCheck 0.9.0、GitHub Actions、Docker、`gh`。

**Spec:** [`docs/superpowers/specs/2026-10-27-lvgl-example-doc-check-tools-design.md`](../specs/2026-10-27-lvgl-example-doc-check-tools-design.md)

---

## 1. Global Constraints

- 分支 `pi/doc-check-tools`，起点 `origin/dev` `766aebc`（PR #15 合入）；PR 指向 `dev`，开发时为草稿，执行者不合并。
- 不改 C 代码、CMake 与用例，不需要本地运行 `ctest` 与 `mutate.sh`；CI 的变体检验照常运行，应为 `0 skipped`（spec §4、Q14）。
- 工作区中作者未提交的 `.cursor/install.sh`、`.cursor/start.sh` 与 `Google Chrome … .png` 不动、不提交；`git add` 只写明确的路径。
- 提交身份 `Cursor Agent <cursoragent@cursor.com>`，SSH 签名；所有创建或改写提交的 git 命令前加 `env -u GIT_AUTHOR_NAME -u GIT_AUTHOR_EMAIL -u GIT_COMMITTER_NAME -u GIT_COMMITTER_EMAIL`（临时提交也不例外），推送前 `git cat-file -p HEAD | sed -n 3,4p` 核对。
- 不带 `Co-authored-by`：内容提交照常经过钩子；改说明或 autosquash 的 rebase 加 `git -c core.hooksPath=/dev/null`；保留 author time，不用 `--reset-author`、`--ignore-date`、`--date`、`--committer-date-is-author-date`（spec §3.6）。改写历史用 rebase 的 fixup、reword；尚未推送的 HEAD 可用 `--amend` 去掉尾注（spec Q15，附录 A.4 的 `commit.sh`）；丢弃故意失败的临时提交用 `git reset --keep`。
- 提交信息按 cz-conventional-emoji 1.1.8：`<type>(<scope>): <emoji> <subject>`，正文为要点、写原因、每行不超过 100 字符，最后一个要点为 `- 详见 spec §…、Q…`；文档中不写本 PR 自身提交的哈希。
- 推送：`GIT_TERMINAL_PROMPT=0 timeout 120 git push https://yuangezhizao@github.com/yuangezhizao/luckfox_pico_lvgl_example.git ...`，改写历史时写明 `--force-with-lease=pi/doc-check-tools:<远端旧提交的完整哈希>`；`gh` 命令加 `timeout`；更新 PR 正文前先 `gh pr view --json body` 取回当前正文；输出中带令牌的地址先打码。
- 推送会取消正在运行的 CI，变体检验约 15–18 分钟；只改文档的修订攒到一起推送。
- 新增或修改 `.sh` 先在本地跑 `shellcheck -x -P SCRIPTDIR`，要求 0 条。
- 编辑文件与 `git add` 不放在同一批并行执行；写脚本与运行脚本分两步。
- 提交、推送前先运行两个脚本，`check-headings.py` 不为 0 处或 `check-samples.sh` 不为 0 个时中止，不提交、不推送（§6 过程）。
- 文档中的事实与数字都用命令算出：命令写在附录 A；CI 结果以 run 编号记录。spec、plan、PR 正文与评论发布前运行 `lint_copy_rules.py`。

## 2. Review Focus

- **QA 规则误伤**：正文段落以加粗的「Q 加数字」开头但不是 QA 条目（如引述旧编号）。规则只看段落中第一段可见内容是否为加粗（全面复查后，段首的 HTML 标签与隐藏内容不算）；Task 3 的正常样例覆盖代码块、行内代码与 `**QA …**`。
- **豁免清单按文件名匹配**：样例文件在临时目录中，名字是 `bad-*.md`、`ok-*.md`，不会误中豁免；默认检查全部 spec 时 6 份豁免文件必须通过（Task 3）。
- **jammy 镜像的 markdown-it-py 1.1.0 与 Python 3.10**：新增的 QA 检查也要在该镜像中通过（Task 7）。
- **CI 中 `check-samples.sh` 失败时看不到原因**：它只打印「不符合：…」行；job 失败时这些行就在日志中，不另上传 artifact。
- **检查器自身失败（如 import 错误）**：`check-samples.sh` 会把所有正常样例记为不符合并退出 1，job 失败；Task 4 的故意失败只验证标题错误这一路径。

## 3. 提交

1. `hammer(tests): 🔨 文档检查脚本 check-headings.py、check-samples.sh 入库`：`tests/tools/docs/` 两个脚本；`AGENTS.md`「文档」一节的用法。
2. `hammer(tests): 🔨 check-headings.py 检查 QA 编号`：脚本、样例；`AGENTS.md`「文档」一节 QA 一句。
3. `chore(cloud-env): 🔧 Dockerfile 安装 python3-markdown-it`：两份 Dockerfile；`AGENTS.md`「Cloud Agent 环境」一节的安装内容。
4. `ci(github-actions): 👷 新增文档检查 job`：workflow（容器 job，没有临时 apt 步骤）；`AGENTS.md`「GitHub Actions」一节。
5. `docs(agents): 📝 AGENTS.md 写明提交与 GitHub 操作的约定`：新增「提交与 GitHub 操作」一节。
6. `docs(superpowers): 📝 历史 plan 中删除已入库的文档检查脚本`：PR #11 plan 附录 A.1、A.5，PR #12、#13、#15 plan 的抽取命令。
7. `fix(tests): 🐛 check-headings.py 不检查 script、style 中不显示的内容`：脚本与样例（Codex 审查，Task 8）。
8. `docs(superpowers): 📝 新增文档检查脚本入库 spec 与 plan`：本 spec 与 plan。

开发期间第 8 个提交最先推送供作者审阅，`ci` 提交先以 A 阶段的形态（runner 上 apt）出现，没有 `chore` 提交；Task 7 插入 `chore` 并把 `ci` 改为容器 job、改写说明，Task 8 把 docs 提交挪到末尾，最终的历史中没有临时 apt 步骤。PR 改为 ready 后按 Codex 审查新增第 7 个提交 `fix(tests)`（第 2 条的修订以 fixup 并入并改写说明）：放在 docs 提交之前；没有并入提交 1、2，因为新样例插入的位置会与提交 2 冲突。

## 4. Tasks

### Task 1：spec 与 plan，推送并开草稿 PR ✅

落地：本 spec 与 plan；草稿 PR #16。

- [x] `check-headings.py`（附录 A.1 抽出的版本）为 0，`lint_copy_rules.py` 为 PASS；提交 8 推送，开草稿 PR #16。
- [x] 作者要求 PR 正文与历史 PR 格式一致：改为分支上的 blob 链接，§2、§3 用「方面 | 内容」「提交 | 内容」表格，§5 用加粗引导词。

### Task 2：脚本入库 ✅

落地：`tests/tools/docs/check-headings.py`、`tests/tools/docs/check-samples.sh`（0755）；`AGENTS.md`「文档」一节的用法。

- [x] 抽出后与 PR #11 plan 附录原文逐字节一致；加出处说明与文件级 `# shellcheck disable=SC2016`（理由另起一行），ShellCheck 0 条。
- [x] 修复两处入库后才暴露的问题（§6）：检查器路径转为绝对路径、历史文档取自脚本所在仓库；`mk` 生成失败时以 2 退出，`trap` 清理临时目录。
- [x] 30 个文件问题 0 处，68 个样例不符合 0 个；仓库根目录、`/tmp` 中运行都通过。提交 1。

### Task 3：QA 编号检查（TDD）✅

落地：`check-headings.py` 的 `QA_EXEMPT` 与 QA 检查；`check-samples.sh` 的 `qa1`–`qa5`、`17`–`20` 样例；`AGENTS.md`「文档」一节 QA 一句。

- [x] RED：新增样例后 `qa1`–`qa5` 都报「rc=0，应为 1」，「样例 77 个，不符合 5 个」。
- [x] GREEN：77 个样例不符合 0 个。实现中两处修正（§6）：加粗中含行内代码的条目按文字算；`2026-10-06` 的子编号使豁免增为 6 份。
- [x] 去掉豁免时 6 份历史 spec 共报 69 处；只去掉 `2026-10-10` 一项时报 7 处；markdown-it-py 1.1.0 下 30/0、77/0。提交 2。

### Task 4：CI job，A 阶段 ✅

落地：workflow 的 `doc-check`（A 阶段形态，不在最终历史中）；`AGENTS.md`「GitHub Actions」一节。

- [x] `actionlint` 1.7.12 为 0 条；推送后全部 job 通过，故意失败的临时提交使 `📝 文档检查` 失败，随后丢弃；A 阶段最终的提交 CI 全部通过（§5）。提交 4。

### Task 5：`AGENTS.md` 的提交与 GitHub 操作 ✅

落地：`AGENTS.md`「提交与 GitHub 操作」一节（spec §3.6 第 4 条的原文）。

- [x] `lint_copy_rules.py AGENTS.md` 为 PASS。提交 5。

### Task 6：历史 plan ✅

落地：PR #11 plan 附录 A 开头与 A.1、A.5；PR #12 plan 附录 A.3；PR #13 plan 附录 A.3；PR #15 plan 附录 A.4（含 `extract.py` 文档字符串中的例子，改为抽取 PR #10 plan 附录 C.4）。

- [x] 这三份 plan 中不再有从 PR #11 plan 抽取这两个脚本的命令；`check-headings.py` 为 0。提交 6。

### Task 7：改为 D 阶段，本机镜像验证 ✅

落地：两份 Dockerfile 的「文档检查」组；`AGENTS.md`「Cloud Agent 环境」的安装内容与 `--user 0` 名单；`doc-check` 改为容器 job。

- [x] 提交 3；`doc-check` 的改动以 fixup 并入提交 4，提交 4 的说明改写；rebase 后树不变、author time 不变、没有 `Co-authored-by`。`actionlint` 为 0。
- [x] 本机镜像验证与 D 阶段 CI 见 §5。

### Task 8：整理提交、精简 plan、PR 正文、合并前确认与 Codex 审查 ✅

- [x] 全部 13 个 shell 脚本 ShellCheck 0 条（同 CI 的文件列表）。
- [x] docs 提交挪到末尾，提交顺序为 §3；精简本 plan，补 §5、§6；spec、plan 用入库后的 `check-headings.py` 检查为 0，`lint_copy_rules.py` 为 PASS；PR 正文按 PR #14、#15 的格式更新。
- [x] 合并前准备：spec 状态行缩短为 PR #1–#4 的写法；不再做完整的 A、B、C 审查（spec §5：改动小、按 TDD 验证，CI、本机镜像与逐提交运行均通过）；满足门槛后把 PR 改为 ready，不合并。
- [x] Codex 审查（P2，PR 改为 ready 之后）：段落中行内的 `<script>`、`<style>` 之间的文字被当作可见文字，`说明 <script>Task 99</script>` 误报「引用了不存在的 Task 99」；先复现（行内误报，HTML 块正常），再按 TDD 修复：新增 `ok-21`、`ok-22`、`bad-hs1`，修复前 `ok-21`、`ok-22` 不符合，修复后 80/0。提交 7。
- [x] Codex 第 2 条（P2）：开始标签后有空行、结束标签在后面的段落时，每个 `inline` 都从 0 开始，中间段落仍被误报；复现属实（结束标签在后续段落、单独成为 HTML 块两种写法）。隐藏状态改为按文档顺序接续（`HtmlText` 也从当前状态开始），新增 `ok-23`、`ok-24`、`bad-hs2`，修复前 `ok-23`、`ok-24` 不符合。修复中发现隐藏状态会让标题文字变空、误报「缺序号」，标题改为不受隐藏状态影响（`ok-25`）；并对到文末仍未闭合的 `<script>`、`<style>` 报错（`bad-hs3`，加检查前不报）。85/0，以 fixup 并入提交 7。
- [x] Codex 第 3 条（P2）：行内 `<script>` 中 HTML 模板字符串里的 `<h2>` 仍报原始 HTML 标题，复现属实。逐一核对读取文档内容的全部路径，另复现隐藏区域中的 HTML 块 `<h2>`、QA 条目、Markdown 标题三处同类误报，统一为隐藏区域中的内容不参与任何检查（共用 `html_step()`，主循环记录每个 token 开始时的层数 `state_at`）；新增 `ok-26`–`ok-29`、`bad-hs4`，修复前 4 个正常样例不符合，修复后 90/0。以 fixup 并入提交 7。
- [x] Codex 第 4 条（P2，针对第 3 条修复前的提交 `a3f3cef`）：未闭合的 `<script>` 延续到后面的 HTML 块时仍记录其中的 `<h2>`。在旧提交上复现、在新提交上不再报，即上一条核对全部路径时修复的 HTML 块路径（`ok-27`）；回复并标记为已解决。
- [x] Codex 第 5、6 条（P2）：都能复现。第 6 条为本 PR 引入的误报（`<script>` 中字符串里的 `<style>` 多计一层，误报「到文末未闭合」）；第 5 条为 PR #11 原版就有的漏报（`Task<br>99`）。作者确认收尾标准后：第 6 条把隐藏状态改为记录当前所在的元素，新增 `ok-30`（行内）、`ok-31`（HTML 块），修复前两者不符合，修复后 92/0，以 fixup 并入提交 7；第 5 条回复依据并记入 spec §8（spec §5），后在全面复查中修复。
- [x] Codex 第 7 条（P2）：段首 `</script>` 之后的 `**Q3. c**` 没有被识别为 QA 条目，复现属实；在加入 QA 检查的提交 `ab49584` 上同样不报，`<b></b>**Q3. c**` 也不报，是 QA 规则「段落以加粗开头」的范围所致，与隐藏状态的修复无关。按收尾标准不在本 PR 修，回复依据并记入 spec §8，后在全面复查中修复。
- [x] 全面复查（作者要求）：逐段通读 `check-headings.py`，逐类构造写法试验。新发现误报：`**Q1. *斜体* 标题**`、`**Q1. [链接](x) 标题**` 报格式错误。按 TDD 一并修复该误报与第 5、7 条，并让 `<script/>` 与浏览器一致按开始标签处理：新增 `ok-32`、`ok-33`、`bad-br1`–`br3`、`bad-qa6`、`bad-qa7`，修复前 7 个都不符合，修复后 99/0；spec §8 的两项随之删去，在第 5、7 条的讨论中补充说明。以 fixup 并入提交 7。
- [x] Codex 第 8、9 条（针对全面复查之前的 `335edc9`）：在旧提交上复现、在新提交上不再报，即全面复查中修复的 `<script/>`（`ok-33`）与带链接的 QA 标题（`ok-32`）；回复并标记为已解决。
- [x] Codex 第 10 条（P2）：`**Q1. ![问题](x) 标题**` 误报格式错误，复现属实；同时发现 `**Q3.<br>换行**` 也误报。QA 标题改为复用 `rendered()` 取可见文字，新增 `ok-34`，修复前不符合，修复后 100/0。以 fixup 并入提交 7。
- [x] Codex 第 11 条（P2）：隐藏状态延续时，注释中的 `</script>` 不结束隐藏，误报「到文末未闭合」；行内、HTML 块两条路径都复现属实。行内改为在整段原始 HTML 中查找结束标签，HTML 块用 `set_cdata_mode()` 进入原始文本状态；新增 `ok-35`、`ok-36`、`bad-hs5`，修复前两个正常样例不符合，`bad-hs5` 报的是「到文末未闭合」而非失效引用；修复后 103/0，`bad-hs5` 报「引用了不存在的 Task 99」。Python 3.10、3.12 下都有 `set_cdata_mode()`。以 fixup 并入提交 7。
- [x] Codex 第 12 条（P2）：HTML 块中 `<script/>` 之后注释里的 `</script>` 不结束隐藏，复现属实。`handle_startendtag()` 模拟开始标签时同样调用 `set_cdata_mode()`；新增 `ok-37`（HTML 块与行内两种写法）、`bad-hs6`，修复前 `ok-37` 不符合、`bad-hs6` 报的是「到文末未闭合」；修复后 105/0，`bad-hs6` 报失效引用。以 fixup 并入提交 7。
- [x] Codex 第 13 条（P2）：`<template>` 中的 `<h2>`、Task 引用仍被检查，行内与 HTML 块两条路径都复现属实，PR #11 原版同样误报。隐藏状态改为元素栈，`<template>` 按普通 HTML 规则（可嵌套、注释起作用、可含 `<script>`）；新增 `ok-38`–`ok-40`、`bad-hs7`、`bad-hs8`，修复前 3 个正常样例与 `bad-hs8`（`<template>` 到文末未闭合）不符合，修复后 110/0。以 fixup 并入提交 7。
- [x] Codex 第 14、15 条（P2）：带格式的图片替代文本、QA 条目前的空链接，都能复现，都是漏报；分别在 PR #11 原版与加入 QA 检查的提交 `ab49584` 上同样漏报，写法极少见。按收尾标准不在本 PR 修，回复依据并记入 spec §8 第 3、4 项。
- [x] Codex 第 16 条（P2）：`<!-- </script><script> -->` 结束后又重新打开 `<script>`，其后的「Task 99」仍被检查，复现属实。行内路径改为与 HTML 块共用 `HtmlText` 解析，「原始 HTML 标题」的行内检查也改用它；新增 `ok-41`、`bad-hs9`、`bad-hs10`（结束标签之后注释中的 `<h2>` 在浏览器中可见，应报错），修复前 `ok-41` 与 `bad-hs10` 不符合；修复后 113/0。以 fixup 并入提交 7。
- [x] Codex 第 17 条（P2）：`## </script> 见 Task 99` 中结束标签之后的可见文字没有检查，复现属实；PR #11 原版会检查，是第 3 条修复引入的回归。隐藏区域中的标题仍跳过结构检查，可见文字照常进入引用检查；新增 `bad-hs11`，修复前不符合，修复后 114/0。以 fixup 并入提交 7。

### Task 9：合并后核对 dev CI

- [ ] 作者合并后跟进 dev 的 CI：`build-image` 重建镜像，`📝 文档检查` 等全部 job 通过；把 run 编号与结果评论到 PR。

## 5. 验证证据

| 项 | 结果 |
|---|---|
| 起点 | `766aebc` 上附录原文的 `check-headings.py` 检查 28 个文件问题 0 处，`check-samples.sh` 68 个（坏 52、正常 16）不符合 0 个（附录 A.2 ①） |
| 入库后 | 30 个文件问题 0 处；77 个样例不符合 0 个；ShellCheck 0.9.0 对 13 个脚本 0 条；`actionlint` 1.7.12 对 A、D 两种形态均为 0 条 |
| Codex 修复与全面复查后 | 本机 markdown-it-py 3.0.0 与 1.1.0、两份本机镜像（Python 3.12.3、3.10.12）中，30 个文件问题 0 处，114 个样例不符合 0 个；ShellCheck 0 条 |
| A 阶段 CI | run 40505696050 全部通过：`📝 文档检查` 20 s，其中 apt 步骤 9 s，输出 `python3-markdown-it is already the newest version (3.0.0-2)`、`0 newly installed`，即 runner 已预装；Python 3.12.3、markdown-it-py 3.0.0，30/0、77/0；`🧬 变体检验` `150 run, 0 skipped` |
| 故意失败 | run 40508745991：临时提交把 PR #11 plan 的 `### A.2` 改为 `### A.9`，`📝 文档检查` 报 4 处、以 1 退出；该提交随后丢弃 |
| A 阶段最终 | run 40508978534（docs fixup 之后、D 阶段之前的提交）全部通过：`📝 文档检查` 21 s，30/0、77/0；`🧬 变体检验` `150 run, 0 skipped` |
| FROM | `ubuntu:24.04@sha256:4fbb8e6a…` 没有 `python3-markdown-it`，也没有 `python3`；`luckfoxtech/luckfox_pico:1.0@sha256:915d4458…` 没有 `python3-markdown-it`，有 `python3` 3.10.6（附录 A.3 ①） |
| 本机镜像 | Docker 29.9.2（`fuse-overlayfs`），以 root 运行：`.cursor/Dockerfile` 镜像 Python 3.12.3、`python3-markdown-it` 3.0.0-2；`.cursor/Dockerfile.luckfox_pico` 镜像 Python 3.10.12、`python3-markdown-it` 1.1.0-2、`python3-attr` 21.2.0-1ubuntu1；两者都是 30/0、77/0（附录 A.3） |
| D 阶段 CI | run 40511935545（D 阶段首次推送；之后的推送，含 Codex 修复，结果相同，最终提交的 run 见 PR 正文）：`🐳 构建 CI 镜像` 按预期失败（`ghcr.io/yuangezhizao/luckfox-lvgl-demo-ci:93c649e620d16622eaa4e015d2439883: not found`），`📝 文档检查` 等容器 job 被跳过，`🐚 ShellCheck` 通过 |

## 6. 与计划的偏离

- Task 2：`check-samples.sh` 原先在 `/tmp` 中以绝对路径运行，入库后用 `bash tests/tools/docs/check-samples.sh` 运行时，脚本 `cd` 到临时目录后默认的检查器路径失效，68 个样例全报不符合；从其他目录运行时 `mk` 找不到历史文档，少生成 16 个样例却报「不符合 0 个」。按 §4 修复，spec §3.2 原只计划加出处与 SC2016 指令。
- Task 3：本 spec 的 Q13、Q15 加粗中含行内代码，起初被误报；`2026-10-06` spec 有子编号，grilling 时的统计只匹配 `**Qn. `，漏掉了它，豁免由 5 份增为 6 份，spec §2.5、§3.3、Q11 已同步。
- Task 8：PR 改为 ready 后 Codex 先后提出十七条 P2，计划中没有；新增 `fix(tests)` 提交修复（第 4 条在第 3 条的修订中已一并修复，第 5、7、8、9 条在作者要求的全面复查中修复，第 14、15 条按收尾标准记为后续事项）。前两条只修了被指出的路径，第 3 条起改为核对全部路径，统一为隐藏区域中的内容不参与任何检查；第 13 条把 `<template>` 纳入，隐藏状态改为元素栈；第 16 条起行内与 HTML 块共用 `HtmlText` 解析；到文末未闭合的 `<script>`、`<style>`、`<template>` 报错（spec §3.2 现行规则）。
- 过程：编辑本 plan 时，嵌在 heredoc 中的 `EOF` 行使外层 heredoc 提前结束，后面几行被当作命令执行（`sed` 与只读检查），没有改动文件；之后改用编辑工具。
- 过程：第 16 条修复后，本 plan 中直接写了「Task 99」而未放在「」内，`check-headings.py` 报 1 处，命令没有因此中止，照样推送；几分钟后修正并重新推送。当时 PR 上的 `📝 文档检查` 因 Dockerfile 改动被跳过，没有失败的 CI 记录。之后提交、推送前的检查不为 0 时中止（§1）。

## 附录 A：未入库的命令

在仓库根目录执行；临时文件放 `/tmp/pr16/`。

### A.1 环境准备

```bash
mkdir -p /tmp/pr16
# 基线用的附录原文：Task 6 之后 PR #11 plan 中不再有这两个脚本，从起点 766aebc 取出；日常使用直接用 tests/tools/docs/
H=docs/superpowers/plans/2026-10-14-lvgl-example-heading-numbers.md
git show 766aebc:$H | awk '/^### A\.1 /{a=1} a&&/^```python$/{p=1;next} p&&/^```$/{exit} p' > /tmp/pr16/check-headings.py
git show 766aebc:$H | awk '/^### A\.5 /{a=1} a&&/^```bash$/{p=1;next} p&&/^```$/{exit} p' > /tmp/pr16/check-samples.sh
# 本机工具缺失时；actionlint 按 PR #12 plan 附录 A.1 下载到 /tmp/al/
sudo apt-get install -y shellcheck python3-markdown-it
mkdir -p /tmp/al && (cd /tmp/al && curl -fsSL https://raw.githubusercontent.com/rhysd/actionlint/main/scripts/download-actionlint.bash | bash)
```

### A.2 实测与核对命令

```bash
# ①②④ 在起点 766aebc 的树中运行：spec 中的数字取自起点；在本 PR 之后的树中运行会多出本 spec 与 plan，AGENTS.md 也多报 1 处（新增的 ###）
B=/tmp/pr16/base; rm -rf "$B" && mkdir -p "$B" && git archive 766aebc | tar -x -C "$B"
MD=$(git ls-tree -r --name-only 766aebc | grep '\.md$')
# ① 基线：全部 spec 与 plan、样例总数与坏、正常各多少（旧版 check-samples.sh 以当前目录为仓库根）
sed 's/^rm -rf "\$T"$/echo KEEP $T/' /tmp/pr16/check-samples.sh > /tmp/pr16/cs-keep.sh
(cd "$B" && python3 /tmp/pr16/check-headings.py | tail -1 && bash /tmp/pr16/check-samples.sh | tail -1
  d=$(bash /tmp/pr16/cs-keep.sh | sed -n 's/^KEEP //p'); echo "bad=$(ls "$d"/bad-*.md | wc -l) ok=$(ls "$d"/ok-*.md | wc -l)"; rm -rf "$d")
grep -o 'mk "\$[A-Z0-9]*" q[0-9a-z]*' /tmp/pr16/check-samples.sh | wc -l      # mk 样例数
wc -l /tmp/pr16/check-headings.py /tmp/pr16/check-samples.sh

# ② markdown-it-py 1.1.0（jammy 的版本）：wheel 放入 PYTHONPATH 运行；jammy 各源中的版本
cd /tmp/pr16 && for s in markdown-it-py/1.1.0 mdurl/0.1.2 attrs/21.2.0; do
  u=$(curl -s --max-time 30 "https://pypi.org/pypi/$s/json" | python3 -c 'import json,sys;print([x["url"] for x in json.load(sys.stdin)["urls"] if x["url"].endswith(".whl")][0])')
  curl -s --max-time 60 -O "$u"; done; cd - >/dev/null
(cd "$B" && PYTHONPATH=$(ls /tmp/pr16/markdown_it_py-1.1.0*.whl):$(ls /tmp/pr16/mdurl*.whl):$(ls /tmp/pr16/attrs*.whl) \
  sh -c 'python3 -c "import markdown_it; print(markdown_it.__version__)"; python3 /tmp/pr16/check-headings.py | tail -1; bash /tmp/pr16/check-samples.sh | tail -1')
for s in jammy jammy-updates jammy-security; do printf '%s: ' $s
  curl -s --max-time 60 "http://archive.ubuntu.com/ubuntu/dists/$s/universe/binary-amd64/Packages.gz" | zcat |
    awk '/^Package: python3-markdown-it$/{p=1} p&&/^(Version|Depends):/{printf "%s ", $0} p&&/^$/{exit}'; echo; done
dpkg-query -W python3-markdown-it; python3 --version; python3 -m venv /tmp/pr16/v 2>&1 | head -1; python3 -m pip --version 2>&1

# ③ check-samples.sh 的 SC2016：每行条数
shellcheck --version | sed -n 2p
shellcheck -x -P SCRIPTDIR -f gcc /tmp/pr16/check-samples.sh | awk -F: '{print $2}' | sort -n | uniq -c

# ④ 全部 *.md 的问题数（起点的树）
(cd "$B" && python3 /tmp/pr16/check-headings.py $MD | tail -1
  for f in $MD; do case $f in docs/superpowers/*) ;; *) echo "$f: $(python3 /tmp/pr16/check-headings.py "$f" | tail -1)";; esac; done)

# ⑤ QA 编号现状：各 spec 的 QA 条目编号与写法；去掉豁免后运行检查器，按文件统计问题数（Task 3 之后，6 份）
for f in docs/superpowers/specs/*.md; do q=$(grep -o '^\*\*Q[0-9][^ .：*]*' "$f" | sed 's/^\*\*//' | tr '\n' ' ')
  [ -n "$q" ] && echo "$(basename "$f" -design.md): $q"; done
grep -c '^\*\*Q[0-9]' docs/superpowers/plans/*.md | grep -v ':0$' || echo 'plan 中没有 QA 条目'
sed 's/os.path.basename(f) in QA_EXEMPT/False/' tests/tools/docs/check-headings.py > /tmp/pr16/ch-noexempt.py
python3 /tmp/pr16/ch-noexempt.py | grep -o '^[^:]*' | sort | uniq -c

# ⑥ 提交与 GitHub 操作的现状（输出中的令牌打码）
M='s#(://|x-access-token:)[^@/ ]*@#\1***@#g; s/(gh[opsu]_)[A-Za-z0-9_]+/\1***/g'
git config --show-origin --get core.hooksPath; git config --global --get core.hooksPath || echo '无全局 core.hooksPath'
ls ~/.cursor/agent-hooks/*/; grep -n 'Co-authored-by' ~/.cursor/agent-hooks/*/commit-msg.cursor.co-author
for n in 7 8 9 10 11 12 13 14 15; do m=$(git log --merges --format='%H %s' origin/dev | grep "#$n " | cut -d' ' -f1)
  echo "PR #$n 提交 $(git rev-list --count "$m^1..$m^2") 个，带尾注 $(git log --format=%B "$m^1..$m^2" | grep -c Co-authored-by) 个"; done
timeout 30 gh auth status 2>&1 | sed -E "$M"
git config --show-origin --get-regexp '^(credential|url)\.' | sed -E "$M"
```

### A.3 本机镜像验证（Task 7）

```bash
# ① FROM 中是否已有 python3-markdown-it；构建两份镜像（Docker 按 Cursor 文档临时安装，不写入 Dockerfile；方法同 plans/2026-07-24-lvgl-example-cloudagent-env.md
#   「为嵌套容器配置 fuse-overlayfs + iptables-legacy」，遇 /etc/fuse.conf 的 conffile 提问用 --force-confold 保留现有文件）
sudo install -m 0755 -d /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor --yes -o /etc/apt/keyrings/docker.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/ubuntu $(. /etc/os-release && echo "$VERSION_CODENAME") stable" |
  sudo tee /etc/apt/sources.list.d/docker.list >/dev/null
sudo apt-get update -qq && sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -qq -o Dpkg::Options::=--force-confold \
  docker-ce docker-ce-cli containerd.io docker-buildx-plugin fuse-overlayfs iptables
sudo mkdir -p /etc/docker && printf '%s\n' '{' '  "storage-driver": "fuse-overlayfs"' '}' | sudo tee /etc/docker/daemon.json >/dev/null
sudo update-alternatives --set iptables /usr/sbin/iptables-legacy || true
sudo service docker start || (sudo dockerd > /tmp/pr16/dockerd.log 2>&1 &)
sudo docker info --format '{{.Driver}} {{.ServerVersion}}'
for img in "ubuntu:24.04@sha256:4fbb8e6a8395de5a7550b33509421a2bafbc0aab6c06ba2cef9ebffbc7092d90" \
           "luckfoxtech/luckfox_pico:1.0@sha256:915d44588085826cbeda4b969dbbe7d5e54bf779ba36cda3c5072ee9533e0417"; do
  sudo docker run --rm --entrypoint dpkg-query "$img" -W -f='${Package} ${Status}\n' python3-markdown-it python3 || true; done
sudo docker build --network host -q -f .cursor/Dockerfile -t dc-tools:active .cursor
sudo docker build --network host -q -f .cursor/Dockerfile.luckfox_pico -t dc-tools:alt .cursor

# ② 两份镜像中运行两个脚本（以 root 运行，同 CI 的 --user 0）
rm -rf /tmp/pr16/img-src && mkdir -p /tmp/pr16/img-src && git archive HEAD | tar -x -C /tmp/pr16/img-src
for t in active alt; do echo "== $t"
  sudo docker run --rm --user 0 -v /tmp/pr16/img-src:/w -w /w dc-tools:$t bash -c '
    python3 --version; python3 -c "import markdown_it; print(markdown_it.__version__)"
    dpkg-query -W -f="\${Package} \${Version}\n" python3-markdown-it python3-attr 2>/dev/null
    python3 tests/tools/docs/check-headings.py | tail -1; bash tests/tools/docs/check-samples.sh | tail -1'; done
```

### A.4 辅助脚本（提交与 CI 日志）

存到 `/tmp/pr16/` 后使用。`commit.sh`：内容提交经过钩子做密钥扫描，再关闭钩子 amend 去掉 `Co-authored-by`，核对 author time 不变（`AGENTS.md`「提交与 GitHub 操作」）。

```bash
#!/bin/bash
# 用法：commit.sh <说明文件>（先 git add）
set -euo pipefail
E=(env -u GIT_AUTHOR_NAME -u GIT_AUTHOR_EMAIL -u GIT_COMMITTER_NAME -u GIT_COMMITTER_EMAIL)
"${E[@]}" git commit -q -F "$1"
A=$(git log -1 --format=%ad --date=raw)
"${E[@]}" git -c core.hooksPath=/dev/null commit -q --amend -F "$1"
B=$(git log -1 --format=%ad --date=raw)
[ "$A" = "$B" ] || { echo "author time 变化：$A -> $B" >&2; exit 1; }
! git log -1 --format=%B | grep -q "^Co-authored-by: " || { echo "仍有 Co-authored-by" >&2; exit 1; }
git cat-file -p HEAD | sed -n 3,4p
git log --oneline -1
```

```bash
# 丢弃故意失败的临时提交：--keep 保留工作区中未提交的改动（不用 --hard）
git reset --keep HEAD~1
# CI 日志：gh run view --log 要等整个 run 结束；单个 job 结束后即可从 API 取（令牌不回显）
J=$(timeout 60 gh run view "$R" -R yuangezhizao/luckfox_pico_lvgl_example --json jobs --jq '.jobs[]|select(.name=="📝 文档检查")|.databaseId')
curl -sL --max-time 60 -H "Authorization: Bearer $(gh auth token)" \
  "https://api.github.com/repos/yuangezhizao/luckfox_pico_lvgl_example/actions/jobs/$J/logs" |
  grep -E 'already the newest|newly installed|Python 3|markdown-it-py [0-9]|检查 [0-9]+ 个文件|样例 [0-9]+ 个|应为|[0-9]+ run, [0-9]+ skipped' | cut -c30-
```
