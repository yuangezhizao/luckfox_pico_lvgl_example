# Luckfox Pico LVGL Example — 文档检查脚本入库设计规格

- **日期**：2026-10-27
- **状态**：已落地，以临时 apt 方式推送的 CI 与两份镜像的本机验证通过，合并前按 Codex 审查与全面复查修复检查器对不渲染元素与 QA 标题的误报及几处漏报，验证证据见 plan
- **分支**：`pi/doc-check-tools`（起点 `origin/dev` `766aebc`，即 PR #15 合入）
- **范围来源**：PR #15 spec [`2026-10-26-lvgl-example-music-press-feedback-design.md`](2026-10-26-lvgl-example-music-press-feedback-design.md) §9 第 3 项「文档检查脚本入库」；作者在本 PR 开工时追加：把 Cloud Agent 环境中提交与 GitHub 操作的两个坑写进 `AGENTS.md`
- **关联计划**：[`../plans/2026-10-27-lvgl-example-doc-check-tools.md`](../plans/2026-10-27-lvgl-example-doc-check-tools.md)

---

## 1. 概述与目标

`AGENTS.md`「文档」一节规定了 spec 与 plan 的标题、Task 与附录编号，检查这些规则的 `check-headings.py` 及其样例 `check-samples.sh` 一直写在 PR #11 plan 附录中，PR #11–#15 每次都要先抽到 `/tmp` 再用，CI 也不检查。本 PR 把两者入库并接入 CI，顺带补上 QA 编号的检查，并把 Cloud Agent 环境中提交与 GitHub 操作的约定写进 `AGENTS.md`。合并前另按 Codex 的审查与作者要求的全面复查，修复了检查器把 `<script>`、`<style>`、`<template>` 中不渲染的内容当作可见内容、QA 标题格式判断的误报，以及 `<br>` 与段首 HTML 标签造成的漏报（§3.2）。

PR #15 spec §9 原定先做第 2 项「CI 中变体检验提速」再做本项，作者开工时改为先做本项；第 2 项仍待做（§8）。

目标：

1. `check-headings.py`、`check-samples.sh` 入库到 `tests/tools/docs/`（§3.2）。
2. `check-headings.py` 新增 QA 编号检查：`**Qn. ` 从 Q1 连续编号，6 份早于该规则的 spec 豁免（§3.3、Q11）。
3. CI 新增 `📝 文档检查` job，运行上面两个脚本；依赖 `python3-markdown-it` 最终由两份 Dockerfile 安装（§3.1、§3.4、§3.5）。
4. `AGENTS.md` 写明用法、新 job，并新增「提交与 GitHub 操作」一节（§3.6）。
5. 历史 plan 中删除这两个脚本的副本，抽取命令改为使用入库路径（§3.7）。

**不在本规格范围**：`qref.py` 的「引用不越界」检查与 `lint_copy_rules.py`（§3.8）；`AGENTS.md`、`README*.md`、`tests/README.md` 与 `lib/` 下的 Markdown（§2.4、Q2）；CI 中变体检验提速（§8）。

## 2. 背景

### 2.1 两个脚本的现状

| 脚本 | 位置 | 行数 | 作用 |
|---|---|---|---|
| `check-headings.py` | PR #11 plan [`2026-10-14-lvgl-example-heading-numbers.md`](../plans/2026-10-14-lvgl-example-heading-numbers.md) 附录 A.1 | 257 | 按 `AGENTS.md`「文档」一节检查标题、Task 与附录编号；用 markdown-it-py 解析，代码块、行内代码、HTML 块都按 CommonMark 识别；有问题时逐行打印并退出 1 |
| `check-samples.sh` | 同一 plan 附录 A.5 | 66 | `check-headings.py` 的回归样例：52 个坏样例应退出 1，16 个正常样例应退出 0 |

- 在 `origin/dev` `766aebc` 上，`check-headings.py` 检查全部 28 份 spec 与 plan，问题 0 处；`check-samples.sh` 68 个样例，不符合 0 个（plan 附录 A.2 ①）。
- 之后各 PR 用 `awk` 或 PR #15 plan 附录 A.4 的 `extract.py` 抽出脚本再运行：PR #12 plan 附录 A.3、PR #13 plan 附录 A.3、PR #15 plan 附录 A.4 都写有抽取命令。
- 两个脚本的文档字符串与注释中的「spec §2.1」「spec Q16–Q34」等，指的是 PR #11 spec [`2026-10-14-lvgl-example-heading-numbers-design.md`](2026-10-14-lvgl-example-heading-numbers-design.md)。
- `check-samples.sh` 中 16 个坏样例由 `mk` 改写 4 份历史文档生成（`2026-10-10` spec、`2026-07-24`、`2026-10-01`、`2026-10-10` plan），并断言被替换的原文存在。这几处原文以后被改动，样例会失败。

### 2.2 markdown-it-py 的版本

| 环境 | 来源 | 版本 | Python |
|---|---|---|---|
| 本机 Cloud Agent（`.cursor/Dockerfile`，Ubuntu 24.04） | apt `python3-markdown-it` | 3.0.0-2 | 3.12 |
| `.cursor/Dockerfile.luckfox_pico`（Ubuntu 22.04） | apt `python3-markdown-it`，依赖 `python3-attr` | 1.1.0-2，jammy-updates、jammy-security 无更新版本 | 3.10 |
| GitHub `ubuntu-24.04` runner | 已预装 apt `python3-markdown-it`：A 阶段的 apt 步骤输出 `already the newest version`、`0 newly installed`（开工时未查，CI 日志中发现，plan §5） | 3.0.0-2 | 3.12 |

- 用 PyPI 的 `markdown-it-py` 1.1.0、`mdurl`、`attrs` wheel 放入 `PYTHONPATH`，在本机 Python 3.12 下运行：28 个文件问题 0 处，68 个样例不符合 0 个（plan 附录 A.2 ②）。Python 3.10 下的结果在合并前的本机镜像验证中确认：`.cursor/Dockerfile.luckfox_pico` 的镜像（1.1.0、Python 3.10.12）中，加入 QA 检查后的脚本 30 个文件问题 0 处、77 个样例不符合 0 个（plan §5）。
- 本机没有 `python3-venv` 与 pip：`python3 -m venv` 报 `ensurepip is not available`，`python3 -m pip` 报 `No module named pip`。
- 两份 Dockerfile 的 FROM 都没有 `python3-markdown-it`；`ubuntu:24.04` 的 FROM 也没有 `python3`，`luckfoxtech/luckfox_pico:1.0` 有 `python3` 3.10.6（按 `AGENTS.md` 用 `dpkg-query` 复核，plan §5）。

### 2.3 `check-samples.sh` 的 ShellCheck 结果

`shellcheck -x -P SCRIPTDIR check-samples.sh`（0.9.0）报 8 条 SC2016（info），分布在第 39、51、52、53、57 行，其中第 39、53、57 行各 2 条（plan 附录 A.2 ③）。SC2016 指单引号中的 `$x` 或反引号不会展开，ShellCheck 怀疑本意是展开；这里的样例是有意写成字面反引号的 Markdown 行内代码。CI 的 `🐚 ShellCheck` 对 `git ls-files` 中的 `*.sh` 要求 0 条（含 style 级），入库后必须处理。

### 2.4 检查范围

对 `git ls-files '*.md'` 的 38 个文件运行 `check-headings.py`，问题 47 处，全部来自 spec 与 plan 以外的文件（plan 附录 A.2 ④）：

| 文件 | 问题 |
|---|---|
| `AGENTS.md` | 10 |
| `README.md`、`README_CN.md` | 各 9 |
| `lib/` 下 vendored 的 LVGL、lv_drivers README（7 个） | 共 19 |
| `tests/README.md` | 0 |

`AGENTS.md`、`README*.md` 不按这套编号写，`lib/` 下是第三方文件。

### 2.5 QA 编号的现状

`AGENTS.md` 只规定 QA 条目写成加粗的 `**Q1. …**`，没有写连续编号；「删减后重新连续编号」一直只在会话约定中。11 份带 QA 的 spec（plan 附录 A.2 ⑤）：

| spec | QA 写法 | 编号 |
|---|---|---|
| `2026-09-21`、`2026-09-25`、`2026-09-27`、`2026-09-28` | `**Q1：…` 或 `**Q1（第 1 轮）：…` | 早于格式规则 |
| `2026-10-06` | `**Q1. …**`，另有子编号 `**Q2b. `、`**Q7b. `、`**Q7c. `、`**Q13（作者提问，2026-10-07）. `、`**Q13b（…）. ` | Q1–Q12，子编号插在相应条目之后 |
| `2026-10-10`（PR #10） | `**Q1. …**` | Q2、Q4–Q8、Q13、Q31–Q33、Q38、Q45、Q50：沿用会话中的编号 |
| `2026-10-14` 至 `2026-10-26` 共 5 份 | `**Q1. …**` | 都从 Q1 连续 |

plan 中没有 QA 条目。PR #12–#15 用 `qref.py`（最新版在 PR #15 plan 附录 A.4）核对编号，它的另一半「正文中的 Qn 不超过 N」是启发式的：例如 `2026-10-16` spec 第 278 行描述重新编号的「原 Q19–Q21 为 Q16–Q18」会被列为候选，需要人工确认。

### 2.6 CI 的现状

- `🐚 ShellCheck` 在 `ubuntu-24.04` runner 上直接运行，不用容器、不依赖 `build-image`，`schedule` 时跳过；其余 job 在 CI 镜像中运行。
- CI 镜像标签取 `.cursor/Dockerfile` 内容哈希。修改该文件的 PR 在合并前 `build-image` 必然失败，依赖它的 job 被跳过，合并后 dev push 重建镜像才恢复（`AGENTS.md`）。PR #14 改 Dockerfile 时，开发期间在 CI 中临时 apt 安装，脚本验证通过后再用 rebase 挪进 Dockerfile（PR #14 spec §3.5、Q4、Q5）。
- `push: dev` 带 `paths-ignore: '**.md'`，只改 `.md` 的合并不触发 workflow；`pull_request` 没有路径过滤。不用 workflow 级 `paths` 的原因见 PR #12 spec Q10、§3.7。

### 2.7 提交与 GitHub 操作的两个坑

本会话与之前会话实测（plan 附录 A.2 ⑥）：

1. **`Co-authored-by` 尾注**：仓库 `.git/config`（不是全局配置）的 `core.hooksPath` 为 `/home/ubuntu/.cursor/agent-hooks/L3dvcmtzcGFjZQ`（`/workspace` 的 base64）。其中的分发器依次运行 `pre-commit.cursor`（密钥扫描）与 `commit-msg.cursor.co-author`；后者在说明中没有 `Co-authored-by:` 时追加 `Co-authored-by: 远哥制造 <yuangezhizao@users.noreply.github.com>`。PR #7–#9 的提交带该尾注，PR #10–#15 不带，作者要求不带。已验证的去法：内容提交照常经过钩子，改说明或 autosquash 的 rebase 加 `git -c core.hooksPath=/dev/null`；rebase reword 时如果钩子仍生效，会把尾注加回去，说明不变则哈希也不变。
2. **GitHub 账户**：`~/.config/gh/hosts.yml` 中有 `cursor` 与作者 `yuangezhizao` 两个账户。环境刷新令牌时会改写该文件，作者的登录会消失；开工时 `cursor` 为活动账户且令牌失效（HTTP 401），作者重新 `gh auth login` 后恢复。`~/.gitconfig` 中 `credential.https://github.com.helper` 为 `/exec-daemon/gh auth git-credential`，`url.<带令牌的地址>.insteadOf` 覆盖 `https://github.com/` 等 6 种写法，推送可以使用 `cursor` 账户；该账户改 PR、发评论返回 HTTP 403 `Resource not accessible by integration`。`git remote -v` 与 push 报错会显示带令牌的地址。

## 3. 设计（以 §7 QA 结论为准）

### 3.1 依赖安装的方案（Q1、Q12–Q14）

| 方案 | 做法 | 优点 | 缺点 |
|---|---|---|---|
| A | runner 上 `sudo apt-get update && sudo apt-get install -y --no-install-recommends python3-markdown-it` | 与本机同为 Ubuntu 24.04 的 3.0.0-2；直接用 `python3` | 要 `apt-get update`，较慢，受 apt 源波动影响；版本由发行版决定，不写在仓库中 |
| B | runner 上 `python3 -m venv` 后 `pip install markdown-it-py==3.0.0` | 版本写死，速度快 | `check-samples.sh` 内部调用 `python3`，要把 venv 加进 PATH；runner 是否有 `python3-venv` 未实测 |
| C | `actions/setup-python` 后 `pip install markdown-it-py==3.0.0` | 常见写法，没有 PEP 668 问题 | 多引入一个第三方 action，要跟进它的版本 |
| D | 两份 Dockerfile 安装 `python3-markdown-it`，job 在 CI 镜像中运行 | 本机、Cloud Agent 与 CI 完全一致 | 本 PR 合并前 `build-image` 必然失败；以后改 Dockerfile 的 PR 上该 job 与其他容器 job 一同被跳过；要等 `build-image` |

结论：**开发期间用 A，脚本与 job 在 CI 中验证通过后、合并前用 rebase 改为 D**，做法与 PR #14 相同（§2.6）。A 阶段实测 runner 已预装该包（§2.2），apt 步骤没有安装新包，不影响这一结论。PR #15 spec §9 原写「runner 上 `pip install markdown-it-py`」，以本节为准。

- D 阶段 job 为独立的容器 job（Q12），写法同 `native-tests`：`needs: build-image`、`container.image` 取其输出、`options: --user 0`。
- 两份 Dockerfile 都加（Q13）。合并前在 `.cursor/Dockerfile.luckfox_pico` 的镜像（1.1.0、Python 3.10）中实测两个脚本；不通过就只改 `.cursor/Dockerfile`，在本 spec 记录原因。
- 合并门槛见 §4（Q14）。

### 3.2 入库的脚本（Q5–Q7）

- 路径：`tests/tools/docs/check-headings.py`、`tests/tools/docs/check-samples.sh`，保留原文件名，设置可执行位，不注册到 ctest（`tests/` 的 CMake 工程只管 C 用例）。
- 内容以 PR #11 plan 附录原文为起点（抽出后与附录逐字节一致），只做以下改动：
  - 两个脚本的文档字符串或开头注释加一行出处：规则见 `AGENTS.md`「文档」一节；文中的 §、Q 编号指 PR #11 spec `docs/superpowers/specs/2026-10-14-lvgl-example-heading-numbers-design.md`（Q7）。正文中的 Qn 注释不改；报错信息不含 spec 引用。
  - `check-samples.sh` 在 shebang 之后加文件级 `# shellcheck disable=SC2016`，并写明理由：样例中的反引号是 Markdown 行内代码，按字面写（Q5）。
  - `check-samples.sh` 开头注释写明 `mk` 样例依赖的 4 份历史文档（§2.1），改动这些文档时样例可能失败。（执行者决定）
  - QA 编号检查与对应样例（§3.3）。
  - 实现时追加（plan §6）：`check-samples.sh` 原先放在 `/tmp` 中以绝对路径运行，入库后用相对路径运行时检查器路径失效，从其他目录运行时少生成样例却报全部符合。改为检查器路径转成绝对路径、历史文档取自脚本所在仓库，`mk` 生成失败时以 2 退出。
  - 合并前按 Codex 审查追加（单独的 `fix(tests)` 提交，plan Task 8）。先列现行规则，再按时间顺序记录各条经过；前面几条的做法后来被替换过，**以现行规则为准**：
    - **现行规则**：
      - `<script>`、`<style>`、`<template>` 中不渲染的内容不参与任何检查，包括文字（引用、附录编号）、HTML 与 Markdown 标题结构、QA 条目；在其中结束后的可见文字照常检查。只保留「代码块未闭合」与「到文末未闭合」两项结构检查。
      - 行内 HTML（`html_step()`、行内的「原始 HTML 标题」检查）与 HTML 块共用 `HtmlText`（标准库 `HTMLParser`）解析，规则与浏览器相同。隐藏状态是从外到内的元素栈，按文档顺序在段落、标题、表格单元格、HTML 块之间接续；`<script>`、`<style>` 是原始文本，注释在其中不起作用，`<script/>` 也按开始标签。
      - `<br>` 与块级标签两侧的文字按空白分开。QA 条目按段落中第一段可见内容识别，加粗部分按 `rendered()` 的可见文字判断。
    - 各条经过：
      - 第 1 条：段落中行内的 `<script>`、`<style>` 被解析为 `html_inline`，原先只丢掉标签、保留中间的文字，会误报引用了不存在的 Task 或附录编号未加点。改为与 HTML 块（`HtmlText.hidden`）一致，开始与结束标签之间的文字不计入；结束标签之后的文字照常检查。
      - 第 2 条：开始标签后有空行时，结束标签落在后面的段落或单独成为 HTML 块，每个 `inline` 原先都从未隐藏开始。隐藏状态改为按文档顺序接续，HTML 块也从当前状态开始解析。
      - 修第 2 条时发现并一并处理：正文中误写一个没有结束标签的 `<script>`、`<style>` 会让其后整篇的检查静默失效，改为到文末仍未闭合时报错（执行者决定）。当时还把标题编号定为不受隐藏状态影响，第 3 条后改为下面的统一规则。
      - 第 3 条：隐藏区域中 HTML 模板字符串里的 `<h2>` 仍被报为原始 HTML 标题，「原始 HTML 标题」检查是独立的循环，没有接入隐藏状态。逐一核对检查器中读取文档内容的全部路径后，统一为：**隐藏区域中的内容不参与任何检查**，包括文字（引用、附录编号）、行内与 HTML 块中的 HTML 标题、Markdown 标题（编号、Setext、嵌套、Task 定义）与 QA 条目；只保留「代码块未闭合」「到文末未闭合」两项结构检查（执行者决定）。
      - 第 4 条（针对第 3 条修复前的提交）：前面段落中未闭合的 `<script>` 延续到后面的 HTML 块时，`HtmlText` 仍记录其中的 `<h2>`；这正是核对全部路径时找出的一处，已在同一次修订中修复。
      - 第 6 条：隐藏状态原先只记层数，`<script>` 中字符串里的 `<style>` 又多计一层，`</script>` 之后仍未归零，合法文档误报「到文末未闭合」并跳过其后的内容。改为记录当前所在的元素，与浏览器解析原始文本元素相同：在 `<script>` 中只有 `</script>` 结束隐藏，其余标签都只是文字（`<style>` 同理），行内与 HTML 块两条路径一致。
      - 第 5、7 条：`Task<br>99` 中的 `<br>` 被丢弃、文字拼成 `Task99`，漏报失效引用；段首为 `</script>` 等 HTML 标签、其后才是 `**Q3. c**` 的段落不被识别为 QA 条目，漏报编号不连续。两者都是 PR #11 或加入 QA 检查时就有的漏报，起初按收尾标准（§5）记为后续事项，后在全面复查中修复（下一条）。
      - 全面复查（作者要求，第 7 条之后）：逐段通读 `check-headings.py`，并逐类构造写法试验。新发现一处误报：QA 标题中紧跟编号的斜体或链接（`**Q1. *斜体* 标题**`）被拼成两个空格，误报格式错误；改为斜体、链接等标记不产生字符。同时修复第 5、7 条：`<br>` 与块级标签（`<p>`、`<div>`、`<td>` 等）在行内与 HTML 块中都按空白分开两侧文字；QA 条目跳过段首的 HTML 标签、空白与隐藏内容后再识别。另与浏览器一致，`<script/>`、`<style/>` 按开始标签处理（执行者决定）。
      - 第 8、9 条针对全面复查之前的提交，分别是 `<script/>` 与带链接的 QA 标题，已在全面复查中修复。第 10 条：QA 标题中的图片被忽略，`**Q1. ![问题](x) 标题**` 误报格式错误。根源是 QA 标题单独拼接文字，与正文的 `rendered()` 不一致；改为直接复用 `rendered()` 取加粗部分的可见文字，图片替代文本、行内代码、隐藏区域、`<br>` 与块级标签都与正文一致。
      - 第 11 条：隐藏状态延续到 HTML 块时，Python 的 `HTMLParser` 并不处在原始文本状态，`<!-- </script> -->` 被当作注释，误报「到文末未闭合」；行内路径同样只认段首的结束标签。改为与浏览器一致，在原始文本中注释不起作用：行内的一段原始 HTML 中任何位置出现 `</script`（其后为空白、`/` 或 `>`）都结束隐藏；HTML 块从延续状态开始时用 `set_cdata_mode()` 让解析器进入原始文本状态（`<style>` 同理）。
      - 第 12 条：HTML 块中的 `<script/>` 设置了隐藏状态，却没有让解析器进入原始文本状态，其后注释中的 `</script>` 被当作注释吞掉。模拟开始标签时同样调用 `set_cdata_mode()`。核对后，普通开始标签由解析器自行进入原始文本状态，行内路径的同类写法已正确，只有这一处遗漏。
      - 第 13 条：`<template>` 的内容同样不渲染，检查器却只跟踪 `<script>`、`<style>`，模板中的 `<h2>`、Task 引用与附录编号都被检查（PR #11 原版即如此）。`<template>` 的内容按普通 HTML 解析，可嵌套、可含 `<script>`，注释照常起作用，所以隐藏状态由「当前所在的元素」改为从外到内的元素栈（如 `template script`）：栈顶为 `<script>`、`<style>` 时按原始文本规则，为 `<template>` 时按普通 HTML 规则；到文末未闭合的检查也包括 `<template>`。
      - 第 16 条：一段行内 HTML 中先结束、又重新打开 `<script>`（`<!-- </script><script> -->`），行内路径找到结束标签后就停止，其后的 `<script>` 没有处理，误报。第 11、12、16 条的根源相同：行内路径用正则，HTML 块路径用 `HTMLParser`，两套实现不一致。改为 `html_step()` 与行内的「原始 HTML 标题」检查都直接用 `HtmlText` 解析每段行内 HTML，与 HTML 块共用同一个解析器，按浏览器的方式从左到右处理整段（执行者决定）。
      - 第 17 条：隐藏状态在 Markdown 标题内结束时（`## </script> 见 Task 99`），结束标签之后的文字可见，但第 3 条修复对开始时位于隐藏区域中的标题整体跳过，这部分文字不再检查；PR #11 原版会检查，属于本 PR 引入的回归。改为标题结构仍不检查，结束标签之后的可见文字照常参与引用与附录编号检查。
      - 第 14、15 条不在本 PR 修：图片替代文本带格式时（`![见 Task **99**](x)`）文字中残留 `**`，漏报失效引用；QA 条目前有空链接时（`[]()**Q3. c**`）不被识别为 QA 条目，漏报编号不连续。两者都是漏报，分别从 PR #11 与加入 QA 检查时继承，spec、plan 中几乎不会出现，按收尾标准（§5）记为后续事项（§8）。
- `check-headings.py` 在仓库根目录执行，不带参数时检查 `docs/superpowers/specs/*.md` 与 `docs/superpowers/plans/*.md`（Q2）；`check-samples.sh` 可在任意目录运行，默认使用同目录的 `check-headings.py`。

### 3.3 QA 编号检查（Q11）

- 规则：文件中渲染为「段落中第一段可见内容为加粗、加粗文字以 `Q` 加数字开头」的条目，加粗文字须为 `Qn. …`（半角句点加空格），n 从 1 起逐个加 1。段首的 HTML 标签与 `<script>`、`<style>`、`<template>` 中的内容不算可见内容（全面复查后，§3.2）；代码块与段首的行内代码中的写法不算；加粗部分与正文一样按 `rendered()` 的可见文字判断：行内代码按其文字计算（如本 spec 的 Q13、Q15），图片取替代文本，斜体、链接等标记不产生字符。
- 豁免 6 份早于该规则的 spec：`2026-09-21`、`2026-09-25`、`2026-09-27`、`2026-09-28`（旧写法）、`2026-10-06`（子编号）与 `2026-10-10`（会话编号）。`2026-10-06` 的子编号是 Task 3 实现后对全部 spec 运行时发现的，grilling 时的统计只匹配 `**Qn. `，漏掉了它（执行者决定加入豁免）。豁免用脚本中写明理由的文件名清单，不用日期分界，便于核对；不改这些历史 spec，`2026-10-10` 重新编号会改错大量引用。
- 不检查「正文中的 Qn 不超过 N」：误报要人工判断，不适合作为 CI 门禁（§2.5），仍在本地按需用 `qref.py`。
- `check-samples.sh` 增加 5 个坏样例（跳号、不从 1 开始、重号、`**Q1：` 旧写法、句点后缺空格）与 4 个正常样例（连续编号、代码块中的 `**Q9. `、段首行内代码、`**QA …**` 与 `**Quick**`），共 77 个；按 Codex 审查修复 `<script>`、`<style>` 时，第 1 条再加 3 个、第 2 条 5 个、第 3 条 5 个、第 6 条 2 个，全面复查 7 个、第 10 条 1 个、第 11 条 3 个、第 12 条 2 个、第 13 条 5 个、第 16 条 3 个、第 17 条 1 个，共 114 个（§3.2）。
- `AGENTS.md`「文档」一节把 QA 一句改为：设计决策 QA 条目用加粗的 `**Q1. …**`，从 Q1 连续编号，删减后重新编号（早于此规则的 6 份历史 spec 豁免，清单见 `tests/tools/docs/check-headings.py` 的 `QA_EXEMPT`）。

### 3.4 CI 的 `📝 文档检查` job（Q2–Q4、Q12）

- job ID 为 `doc-check`，名称 `📝 文档检查`，放在 `shellcheck` job 之后；`if: github.event_name != 'schedule'`，`timeout-minutes: 10`，`permissions: contents: read`（D 阶段另加 `packages: read`，用于拉取 CI 镜像），检出时 `persist-credentials: false`。
- 每次都完整运行，不在 job 内按改动文件跳过，也不用 workflow 级 `paths`（Q3）。只改 `.md` 的 dev push 因 `paths-ignore` 不会触发，PR 上已检查过，接受。
- 步骤依次运行 `python3 tests/tools/docs/check-headings.py` 与 `bash tests/tools/docs/check-samples.sh`（Q4），检查范围为默认的 spec 与 plan（Q2）。
- A 阶段：runner 上直接运行，先 apt 安装 `python3-markdown-it`（§3.1）。
- D 阶段：改为容器 job，去掉 apt 步骤；版本由镜像决定。

### 3.5 Dockerfile（Q13）

- `.cursor/Dockerfile` 与 `.cursor/Dockerfile.luckfox_pico` 的 apt 列表新增一组「文档检查」：`python3-markdown-it`。组的位置与说明注释的写法参照「preview 工具」组（执行者决定）。
- `AGENTS.md`「Cloud Agent 环境」一节中 `.cursor/Dockerfile` 安装内容的描述同步加上该依赖。

### 3.6 `AGENTS.md`（Q6、Q8–Q10、Q15）

1. 「文档（spec 与 plan）」一节：加一条用法：在仓库根目录运行 `python3 tests/tools/docs/check-headings.py [文件…]`（默认检查全部 spec 与 plan）与 `bash tests/tools/docs/check-samples.sh`（修改检查器时运行），依赖 `python3-markdown-it`；CI 的 `📝 文档检查` 运行这两个脚本。QA 一句按 §3.3 修改。
2. 「GitHub Actions」一节：加一条说明 `📝 文档检查` job（形态按 D 阶段；改 Dockerfile 的 PR 上与其他容器 job 一同被跳过）。
3. `tests/README.md` 不改：它只讲原生测试（Q6）。
4. 在「Cloud Agent 环境」之后新增 `### 提交与 GitHub 操作`（Q8、Q9、Q15），原文如下，写入时只做排版调整：

> - **身份**：提交身份为仓库配置的 `Cursor Agent <cursoragent@cursor.com>`，带 SSH 签名。环境会注入 `GIT_AUTHOR_*`、`GIT_COMMITTER_*`，提交、rebase 前要加 `env -u GIT_AUTHOR_NAME -u GIT_AUTHOR_EMAIL -u GIT_COMMITTER_NAME -u GIT_COMMITTER_EMAIL`。
> - **`Co-authored-by` 尾注**：仓库 `.git/config` 的 `core.hooksPath` 指向 `~/.cursor/agent-hooks/<id>/`，其中的 `commit-msg` 会追加 `Co-authored-by: 远哥制造 <…>`。本仓提交不带该尾注。
>   - 内容提交照常经过钩子，`pre-commit` 会做密钥扫描，不要用 `--no-verify`。
>   - 改提交说明（reword）或 autosquash 的 rebase 要加 `git -c core.hooksPath=/dev/null`。否则钩子会把尾注加回去，说明不变时哈希也不变。
>   - `GIT_EDITOR` 由 shell 解释，参数中有 `(` 时要加引号。
> - **保留作者时间**：rebase（含 fixup、reword）和 `--amend` 都要保留 author time，committer time 可以变化。不要用 `--reset-author`、`--ignore-date`、`--date`，也不用 `--committer-date-is-author-date`。
> - **GitHub 账户**：推送可以使用 Cloud Agent 自带的 `cursor` 账户凭据。这个账户创建 PR、改 PR 正文、发评论都会返回 HTTP 403 `Resource not accessible by integration`，这些操作要请作者用 `gh auth login` 登录本人账户。环境刷新令牌时会改写 `~/.config/gh/hosts.yml`，登录随时可能失效，登录后要尽快完成 PR 操作。
> - **打码**：`git remote -v`、push 报错可能显示带令牌的地址，输出前要先打码。

`/tmp` 在新会话中被清空、`/cursor/stores/self` 偶尔读出为空，这两点不写（Q10）。

### 3.7 历史 plan（Q16、Q17）

- PR #11 plan 附录 A.1、A.5 保留标题，删掉代码块，换成一句「已入库为 `tests/tools/docs/…`（PR #16）」，并保留原有的用法说明；「## 附录 A：未入库的脚本」标题不改，附录开头说明 A.1、A.5 已入库（Q16）。
- PR #12 plan 附录 A.3、PR #13 plan 附录 A.3、PR #15 plan 附录 A.4 中从 PR #11 plan 抽取这两个脚本的命令，改为直接使用 `tests/tools/docs/…`，其他说明不动（Q17）。
- 历史 spec 中「PR #11 plan 附录 A.1 的 `check-headings.py`」这类叙述描述的是当时的情况，不改；PR #15 spec §9 也不改（执行者决定）。

### 3.8 不做的事

- `qref.py` 不入库：「引用不越界」是启发式检查（§2.5）；QA 连续编号已并入 `check-headings.py`（§3.3）。
- `lint_copy_rules.py` 不入库：属于作者安装的第三方 Skill `Fenng/tech-doc-style-chinese`，检查的是个人写作偏好，只在本地运行。
- 不检查 `AGENTS.md`、`README*.md`、`tests/README.md` 与 `lib/` 下的 Markdown（Q2）。
- 不在 `tests/README.md` 加说明（Q6）。

## 4. 验证（Q14）

1. **本地脚本**：入库后的 `check-headings.py` 对全部 spec 与 plan 问题 0 处；`check-samples.sh` 不符合 0 个；新增的 QA 样例在加规则前后分别失败、通过。
2. **ShellCheck**：`shellcheck -x -P SCRIPTDIR` 对 `git ls-files` 列出的全部 `*.sh` 与假 `wpa_cli` 为 0 条。
3. **A 阶段 CI**：推送后全部 job 通过，`📝 文档检查` 通过，`🧬 变体检验` 为 `0 skipped`；以 run 编号记入 plan。另推一个改坏标题的临时提交（不进最终历史），确认 `📝 文档检查` 失败，结果记入 plan。（执行者决定）
4. **D 阶段本机镜像验证**：本机临时安装 Docker，构建两份镜像；按 `AGENTS.md` 用 `dpkg-query` 确认 FROM 中没有 `python3-markdown-it`；在两份镜像中运行两个脚本。不在本地运行变体检验，也不运行 `ctest`：本 PR 不改 C 代码与用例，Dockerfile 只新增一个 Python 包。
5. **D 阶段 CI**：`build-image` 按预期失败，容器 job 被跳过；`🐚 ShellCheck` 照常通过。
6. **合并后**：跟进 dev 的 CI（镜像重建、`📝 文档检查` 等全部 job），把结果评论到 PR。
7. **文档**：本 spec 与 plan 用入库后的 `check-headings.py` 检查为 0；中文排版用 `lint_copy_rules.py` 检查。
8. **合并前的 Codex 审查**：每条先复现，再按 TDD 修复，修复前新增的样例要先失败；本机 markdown-it-py 3.0.0 与 1.1.0、两份本机镜像中重跑两个脚本，并逐个提交运行。PR 上的 `📝 文档检查` 因 Dockerfile 改动被跳过，合并后由 dev 的 CI 运行。（执行者决定）

## 5. 提交与 PR

- 分支 `pi/doc-check-tools`，PR 指向 `dev`，开发时为草稿，执行者不合并；改动小、验证充分，不做 A、B、C 三角色审查，合并前与作者确认。
- 最终的提交历史中不出现临时的 apt 步骤：D 阶段把 `chore(cloud-env)` 的 Dockerfile 提交插在 `ci(github-actions)` 之前，并把 ci 提交改为容器 job 的形态；以 A 阶段方式通过的 CI 只以 run 编号记在 plan 中（与 PR #14 相同）。
- Codex 的自动审查：每条先复现再判断，回复并标记为已解决。本 PR 共十七条，都属实（§3.2）：第 4 条在第 3 条的修订中已一并修复；第 5、7 条起初按下面的标准不在本 PR 修，后在作者要求的全面复查中修复，并在原讨论中补充说明；第 8、9 条针对全面复查之前的提交，已在全面复查中修复；第 14、15 条按收尾标准不在本 PR 修，记入 §8。
- 作者在第 6 条后定下的收尾标准：会让合法文档 CI 失败的误报与本 PR 引入的回归必须修；从 PR #11 继承的漏报与极少见写法的漏报不在本 PR 修，记入 §8。检查器以近似方式模拟 HTML 渲染，这类边界情况几乎没有尽头。
- 提交拆分与说明在 plan 中定。

## 6. 风险

- **改 Dockerfile 的 PR 上不检查文档**：D 阶段的 job 依赖 `build-image`，这类 PR 合并前 `📝 文档检查` 被跳过，与其他容器 job 相同；合并后 dev push 会运行。
- **只改文档的 dev push 不检查**：`paths-ignore: '**.md'` 使其不触发 workflow；PR 上已检查。
- **两份镜像的 markdown-it-py 版本不同**：Ubuntu 24.04 为 3.0.0，22.04 为 1.1.0。目前结果相同（§2.2），以后修改检查器时，解析差异可能使两边结果不同；CI 只用 `.cursor/Dockerfile` 的镜像。
- **样例依赖历史文档原文**：改动 §2.1 列出的 4 份历史文档时，`check-samples.sh` 可能失败，需同步调整样例。
- **QA 豁免清单**：清单写死 6 个文件名；以后若有意修改这些 spec 的 QA 写法，要同步清单。
- **`<script>`、`<style>`、`<template>` 中的内容不检查**：按渲染结果，隐藏区域中的内容不参与任何检查；未闭合时报错，不会静默跳过其后整篇。以后给检查器新增读取文档内容的检查时，要同样接入隐藏状态（`state_at`、`html_step()`、`HtmlText`）。
- **`HTMLParser.set_cdata_mode()` 未写入官方文档**：它是标准库解析 `<script>` 时内部使用的方法，Python 3.10 与 3.12 中都有，签名为 `(self, elem)`，3.12 另有可选参数；第 16 条之后行内与 HTML 块都经 `HtmlText` 解析，两条路径都依赖它；以后升级 Python 时若该方法变化，`check-samples.sh` 的 `ok-35`–`ok-37`、`ok-41` 会先失败。
- **Codex 修复未经 PR 上的 CI**：这些修复都在 D 阶段之后完成，PR 上的 `📝 文档检查` 被跳过，只在本机与两份镜像中验证；合并后 dev 的 CI 是首次在 CI 中运行（§4 第 6、8 条）。

## 7. QA（grilling 记录）

问句与推荐取自问答原文，「答」是作者回答经润色后的版本。会话中共五轮，作者要求分批讨论、每批控制题数：

- 第 1 轮 11 问（Q1–Q11）：作者确认 Q3、Q6、Q7、Q11 按推荐，Q10 不写；追问 Q2 能否用全部 `*.md`、Q4 运行的是什么脚本、SC2016 是什么，要求 Q1 列出各方案的优缺点与示例 diff，并说明 Q8、Q9 的账户分工；另要求脚本入库后删掉 plan 中的副本。
- 第 2 轮重问 Q1、Q2、Q4、Q5。
- 第 3 轮 4 问：3 问记为 Q12–Q14；另 1 问是提交顺序，不记，结论见 §5。
- 第 4 轮 3 问，记为 Q15–Q17。
- 第 5 轮作者要求重新讨论 Q11，结论由「记为后续事项」改为本 PR 实现。

**Q1. 依赖装在哪里**

- 问：CI 中 markdown-it-py 用什么方式安装：A runner 上 apt；B runner 上 venv 加 pip 固定版本；C `actions/setup-python` 加 pip；D 写进 `.cursor/Dockerfile`，job 改在容器中运行。第 1 轮推荐 B，作者要求列出四种方案的优缺点与示例 diff 后，第 2 轮改为推荐 A。
- 推荐：A。与本机、Cloud Agent 是同一发行版的同一个包，避开 B 的 PATH 处理与 C 的第三方 action；`apt-get update` 慢一些，job 本来就短。
- 答：四种方案写入 spec 或 plan；开发期间临时用 A，PR 开发完成、合并前改为 D，用 rebase 整理，与之前的 PR 相同（§3.1）。

**Q2. 检查范围**

- 问：A 只查默认的 `docs/superpowers/{specs,plans}/*.md`；B 再加 `tests/README.md`。作者追问能否用全部 `*.md`：38 个文件中问题 47 处，全部来自 spec 与 plan 以外的文件（§2.4）。
- 推荐：A，与 `AGENTS.md`「文档」一节的适用范围一致；全部 `*.md` 会把第三方文件与不按编号写的说明文件纳入。
- 答：按推荐（§3.4）。

**Q3. 触发条件**

- 问：与 `🐚 ShellCheck` 相同，每次都运行、跳过 `schedule`；还是只在改了 `.md` 时运行。
- 推荐：每次都完整运行，不在 job 内按改动文件跳过，也不用 workflow 级 `paths`；job 很短，不值得为跳过增加复杂度。只改 `.md` 的 dev push 不触发，接受。
- 答：按推荐（§3.4）。

**Q4. 是否同时运行 `check-samples.sh`**

- 问：job 中在 `check-headings.py` 之后是否运行 `check-samples.sh`。作者追问运行的是什么脚本：`check-samples.sh` 是检查器自身的回归样例，`🐚 ShellCheck` 只静态检查它的 shell 写法，不执行。
- 推荐：运行，修改检查器的 PR 也能在 CI 中被检验。
- 答：按推荐（§3.4）。

**Q5. SC2016 怎么处理**

- 问：A 文件开头加一条带理由的 `# shellcheck disable=SC2016`；B 逐行加指令，一行有两条命令的第 39、53、57 行要先拆行；C 改用双引号并转义反引号。
- 推荐：A，整份文件都是有意写成字面反引号的样例，以后新增样例也不用再加指令，只禁用这一条。
- 答：按推荐（§3.2）。

**Q6. 命名与文档位置**

- 问：文件名、目录，以及用法写在 `AGENTS.md` 还是 `tests/README.md`。
- 推荐：保留原文件名放入 `tests/tools/docs/`，设置可执行位，不注册到 ctest；用法写在 `AGENTS.md`「文档」一节，「GitHub Actions」一节说明新 job；`tests/README.md` 只讲原生测试，不改。
- 答：按推荐（§3.2、§3.6）。

**Q7. 脚本中引用 PR #11 spec 的写法**

- 问：A 文档字符串加一行出处，正文中的 Qn 不改；B 每一处都改为带路径的引用。
- 推荐：A，改动最少，出处清楚；报错信息不含 spec 引用，历史 plan 中的副本也不改。
- 答：按推荐（§3.2）。

**Q8. 环境的坑写在 `AGENTS.md` 哪里**

- 问：钩子尾注与 `gh` 账户两件事放在哪一节、写到多细。
- 推荐：在「Cursor Cloud specific instructions」下新增 `### 提交与 GitHub 操作`，把已有的 `env -u GIT_AUTHOR_NAME …`、钩子的处理、`gh` 账户与打码放在一起，写成约 8–10 行要点。
- 答：按推荐，并明确账户分工：推送可以用 Cloud Agent 环境中的 `cursor` 账户，创建 PR、发评论要用 `gh auth login` 登录的其他账户，`cursor` 没有这个权限（§3.6）。

**Q9. 去掉尾注的做法是否定为约定**

- 问：内容提交照常经过钩子，改说明或 autosquash 的 rebase 加 `git -c core.hooksPath=/dev/null`，是否定为固定约定；`--no-verify` 会同时跳过密钥扫描。
- 推荐：定为固定约定，不用 `--no-verify`。
- 答：按推荐（§3.6）。

**Q10. 是否写入另外两个环境现象**

- 问：`/tmp` 在新会话中被清空、`/cursor/stores/self` 偶尔读出为空，是否写进 `AGENTS.md`。
- 推荐：写 `/tmp` 一条，不写 `/cursor/stores/self`。
- 答：两条都不写（§3.6）。

**Q11. QA 连续编号是否并入 `check-headings.py`**

- 问：第 1 轮问是否只记为后续事项，推荐记一条后续事项，作者同意；第 5 轮作者要求重新讨论，补充了现状（§2.5）后重问：A 不做；B 只做确定性规则，`**Qn. ` 从 Q1 连续，5 份历史 spec 用文件名清单豁免，`AGENTS.md` 补上连续编号，样例同步增加；C 在 B 之外再检查「引用不越界」。
- 推荐：B。规则确定，工具本 PR 已入库，约增加 20 行代码与几条样例；「引用不越界」误报要人工判断，不适合放进 CI。
- 答：按推荐 B（§3.3）。执行时发现 `2026-10-06` 有子编号（§2.5），豁免增为 6 份。

**Q12. D 阶段 job 的形式**

- 问：A 独立的容器 job `📝 文档检查`；B 把两条命令加进 `🧪 原生测试`。
- 推荐：A，名称与失败信号独立，文档问题不会表现为原生测试失败；代价是要等 `build-image` 复用镜像。
- 答：按推荐（§3.1、§3.4）。

**Q13. `.cursor/Dockerfile.luckfox_pico` 是否同步**

- 问：A 两份 Dockerfile 都加；B 只改 `.cursor/Dockerfile`。
- 推荐：A，与 PR #14 spec Q7 一致，切换到备选环境也能运行检查；版本差异（1.1.0 与 3.0.0）写进 spec，合并前在该镜像的 Python 3.10 下实测，不通过就退回 B 并记录原因。
- 答：按推荐（§3.1、§3.5）。

**Q14. 改为 D 后的合并门槛**

- 问：Dockerfile 提交上 `build-image` 必然失败，容器 job 被跳过。是否沿用 PR #14 spec Q5 的门槛：A 阶段 CI 全部通过、`🧬 变体检验` 为 `0 skipped`；D 阶段本机构建两份镜像并在其中验证；`🐚 ShellCheck` 照常通过；合并后跟进 dev 的 CI。
- 推荐：沿用。
- 答：本 PR 不改业务代码，本地不运行长达 15 分钟以上的变体检验。执行者说明本地门槛中原本就没有变体检验，变体检验只在 A 阶段的 CI 中运行，并提议本地也不再运行 `ctest`，作者同意（§4）。

**Q15. `AGENTS.md`「提交与 GitHub 操作」的原文**

- 问：§3.6 第 4 条的原文（不含「保留作者时间」一条）是否可以。
- 推荐：按原文写入。
- 答：补充一条：rebase、fixup 或 amend 都要保留提交的 author time，committer time 允许改变（§3.6）。

**Q16. PR #11 plan 中脚本副本怎样删**

- 问：A 保留附录 A.1、A.5 的标题，代码块换成入库路径；B 连标题一起删，A.2–A.4 重新编号，其他文档中的引用同步修改。
- 推荐：A，其他文档中的「附录 A.1」「A.5」引用都保持有效，历史文档改动最少。
- 答：按推荐（§3.7）。

**Q17. 其他 plan 中的抽取命令**

- 问：PR #12、#13、#15 plan 中从 PR #11 plan 抽取脚本的命令，代码块删掉后会失效。A 只把这几行改为使用 `tests/tools/docs/…`；B 不改，视为历史执行记录。
- 推荐：A，以后照着历史 plan 重建环境时命令仍能运行；历史 spec 中描述当时情况的叙述不改。
- 答：按推荐（§3.7）。

## 8. 后续事项

1. **CI 中变体检验提速**：PR #15 spec §9 第 2 项，原定先于本项，作者改为在本项之后做，内容不变。
2. **QA 引用不越界**：`qref.py` 的这一半仍是启发式检查，只在本地按需运行（§2.5、Q11）。
3. **带格式的图片替代文本**：`rendered()` 取 `image.content`（原始标签文字），`![见 Task **99**](x)` 中残留 `**`，漏报失效引用（Codex 第 14 条，PR #11 原版即如此）。可改为递归取 `image.children` 的文字，与 markdown-it 渲染替代文本的方式一致。
4. **QA 条目前的零宽 Markdown 结构**：段首扫描只跳过 HTML 标签与空白，`[]()**Q3. c**` 中的空链接使这一段不被识别为 QA 条目（Codex 第 15 条，加入 QA 检查时即如此）。可改为同时跳过不产生可见文字的 Markdown 开闭 token。
