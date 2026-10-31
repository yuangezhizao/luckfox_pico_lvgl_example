# Luckfox Pico LVGL Example — AGENTS.md 精简与 harness 审视实施计划

> **For agentic workers:** 作者同意本 PR 不必严格遵循 superpowers 的执行流程（spec Q4、Q11）；按下面的 Task 顺序执行，复选框（`- [ ]`）用于跟踪。

**Goal:** 根 `AGENTS.md` 降到 8 KiB 以内，只留几乎每个任务都用得到、读代码推不出来的内容；移出的内容按「离用到的地方最近」放进子目录 `AGENTS.md`、`tests/README.md` 与代码注释；写明指令优先级；CI 守住体积预算；仓库外的 harness 能改的改、不能改的写明改法。

**Architecture:** 推送 spec 与 plan、开草稿 PR（Task 1）；在 `origin/dev` 上做基线冷启动问答（Task 2）；代码注释（Task 3）；子目录说明文件（Task 4）；重写根文件与逐句对照表（Task 5）；CI 体积检查（Task 6）；仓库外 harness（Task 7）；本分支冷启动问答（Task 8）；整理与收尾（Task 9、Task 10）。

**Tech Stack:** Markdown、GitHub Actions、bash、Python 3（markdown-it-py）、Codex CLI 0.166.0、Node.js v26.13.0。

**Spec:** [`docs/superpowers/specs/2026-11-01-lvgl-example-agents-md-slim-design.md`](../specs/2026-11-01-lvgl-example-agents-md-slim-design.md)

---

## 1. Global Constraints

- 分支 `cursor/agents-md-slim-d7ec`，起点 `origin/dev` `de8dddb`（PR #16 合入）；PR 指向 `dev`，开发时为草稿，执行者不合并。
- 不改 C 代码与 CMake 的行为，只加注释；不改用例；本地不运行 `ctest` 与 `mutate.sh`，由 CI 运行（spec §4 第 6 条）。
- 提交身份与尾注按 `AGENTS.md`「提交与 GitHub 操作」：`env -u GIT_AUTHOR_NAME -u GIT_AUTHOR_EMAIL -u GIT_COMMITTER_NAME -u GIT_COMMITTER_EMAIL`；内容提交照常经过钩子，去尾注用 `git -c core.hooksPath=/dev/null`；保留 author time。
- 提交信息按 git cz：`<type>(<scope>): <emoji> <subject>`，正文要点写原因，最后一个要点为 `- 详见 spec §…`。
- 去向不明的句子不删：每一句都要在附录 B 的对照表中有落点（用户规则 4）。
- 推送会取消正在运行的 CI；本 PR 不改代码，作者同意该推送时直接推送（§6）。
- 提交、推送前运行 `python3 tests/tools/docs/check-headings.py`，不为 0 处时中止。
- 冷启动问答一律只读：在 `/tmp` 下的 worktree 中运行，结束后核对 worktree 与 `/workspace` 的 `git status --short` 为空（附录 A.2）。

## 2. Review Focus

- **信息是否丢失**：附录 B 每一行的「新位置」都能找到原句的意思；尤其是 gotcha 与踩坑根因（uClibc 与 glibc 的 ABI、`LD_LIBRARY_PATH` 须为绝对路径、改 Dockerfile 的 PR 上 `build-image` 必然失败、`-std=c99` 编译失败）。
- **去向表能否把模型带到正确的文件**：Codex 与 pi 不自动加载子目录文件（spec §2.2），问答第 4、5、6 题检验这一点。
- **优先级一节的措辞**：是否会被理解为「可以无视用户规则 2、7」；问答第 10 题检验。
- **体积检查的报错**：超限时打印字节数与 spec §3.1 的位置，不只是退出 1。

## 3. 提交

拟定，执行时可按实际调整并回写：

1. `docs(custom): 💡 板型判据与 CMake 告警段的背景写进代码注释`：`custom/custom_main.c`、`CMakeLists.txt`。
2. `docs(agents): 📝 新增子目录 AGENTS.md，测试与 GUI 验证说明并入 tests/README.md`：`.cursor/AGENTS.md`、`.github/AGENTS.md`、`docs/superpowers/AGENTS.md`、`tests/README.md`、`tests/tools/docs/` 两个脚本的注释。
3. `docs(agents): 📝 精简根 AGENTS.md`：根 `AGENTS.md`。
4. `ci(github-actions): 👷 文档检查限制根 AGENTS.md 的体积`：workflow。
5. `docs(superpowers): 📝 新增 AGENTS.md 精简 spec 与 plan`：本 spec 与 plan，最后一个提交。

## 4. Tasks

### Task 1：spec 与 plan，推送并开草稿 PR ✅

落地：本 spec 与 plan；草稿 PR #17（由 Cloud Agent 自带的 PR 管理工具创建），正文开头为 spec §1.1。

### Task 2：基线冷启动问答 ✅

落地：`origin/dev` 上 Cursor 子代理与 Codex 两路问答，评分见 §5。Cursor 子代理在 Task 5 之前运行；Codex 由作者设备码登录后运行。

### Task 3：代码旁注释 ✅

落地：`custom/custom_main.c` 板型判定函数的注释；`CMakeLists.txt` 两处 `add_compile_options()` 的注释。告警条数与变体清单的核对见 §5。提交 1。

### Task 4：子目录说明文件 ✅

落地：`.cursor/AGENTS.md`、`.github/AGENTS.md`、`docs/superpowers/AGENTS.md`；`tests/README.md` §2 补 `app_env.c` 的来历、§7 补产物不能在 VM 上运行的原因与 qemu 冒烟；`tests/tools/docs/` 两个脚本注释中的规则出处。体积检查的说明随提交 4 加入 `.github/AGENTS.md`，使每个提交单独看都自洽。提交 2。

### Task 5：根 `AGENTS.md` ✅

落地：按 spec §3.2 重写，「提交与 GitHub 操作」按 spec §2.7 更新；对照 SDK 仓库后更正 `.gitignore` 一句、加「按改动范围选检查」（spec §2.8、Q13）；逐句去向见附录 B。提交 3。

### Task 6：CI 体积检查 ✅

落地：`📝 文档检查` 的「📏 检查根 AGENTS.md 的体积」一步。故意失败的临时提交见 §5，已丢弃。提交 4。

### Task 7：仓库外 harness

- [x] Project Context `workflows/codex-dev.md`「给 Codex 的公共指令要点」加一条（spec §3.6），已告知协调者。
- [x] 本 VM 的 co-author 钩子改为 noreply 地址（spec §2.7、§3.6）。
- [ ] 作者按 spec §3.6.1 修改 Cursor User Rules 后，由协调者同步 `internal/user-rules.md`。

### Task 8：本分支冷启动问答 ✅

落地：本分支上两路复测，与基线的比较见 §5。

### Task 9：整理提交、精简 plan、PR 正文

- [x] 提交顺序为 §3，spec 与 plan 在最后；本 plan 删去已完成 Task 的步骤，只留落地记录、§5、§6 与附录。
- [x] PR 正文按 PR #14–#16 的格式更新，开头用 spec §1.1；CI 全部通过后改为 ready，不合并。作者要求暂时跳过变体检验，其余 6 个 job 通过后改为 ready（run 41239963455）。
- [x] Codex 审查（P2）：「CI 每次推送都会运行全部检查与变体检验」不准确，复现属实：workflow 的 `push` 只监听 `dev` 且 `paths-ignore: '**.md'`，`pull_request` 只针对 `dev`，本 PR 开 PR 前推送分支时也没有触发。改为指向 `dev` 的 PR 每次推送与 `dev` 上不只改 `.md` 的推送，并写明没开 PR 的分支推送不触发；以 fixup 并入提交 3。

### Task 10：合并后核对 dev CI

- [ ] 作者合并后跟进 dev 的 CI，把结果评论到 PR。

## 5. 验证证据

| 项 | 结果 |
|---|---|
| 起点体积 | `origin/dev` `de8dddb` 的根 `AGENTS.md` 74 行、14,883 字节、9,800 字符；21 个提交改过它，已合并的 16 个 PR 中 14 个改过（只有 #2、#15 没改）（附录 A.4） |
| 文档检查 | 加入本 spec 与 plan 后 32 个文件问题 0 处，114 个样例不符合 0 个 |
| 基线问答，Cursor | 子代理 `bc-2c930850-ace1-5eed-a38c-932d10720932`（Claude Opus 5.5）：第 1–8 题对，第 9 题部分对（答「写进根 `AGENTS.md`、保留英文锚点」，为此翻了 PR #11、#12 等历史 spec），第 10 题部分对（仓库没有规定，凭默认判断）；完整读取 3 个文件、部分读取 4 个、检索命中 17 个，其中 11 个是历史 spec 与 plan。原始回答存于 Project Context `internal/agents-md-slim/probe-baseline-cursor.md` |
| 基线问答，Codex | Codex CLI 0.166.0、`gpt-6-astra` `medium`（目录默认 `low`），无 `~/.codex/AGENTS.md`：41 秒、5 条 shell 命令，input 111,438（cached 72,192）、output 1,850；第 1–8 题对，第 9 题部分对（答「根 `AGENTS.md` 对应主题的小节」），第 10 题部分对（明确说仓库没有规定，依据 Codex 自身规则）；整份读了 workflow 与 PR #3 spec。原始回答与事件流存于 Project Context `internal/agents-md-slim/probe-baseline-codex.md`、`.jsonl` |
| 代码注释 | glibc 交叉编译告警在 `origin/dev` 与本分支都是 121 条；变体清单中指向 `custom/custom_main.c` 的 8 条 `old` 在改前改后各匹配一次（按 `mutate.sh` 的解析规则，含 `\n` 换行） |
| 体积 | 根 `AGENTS.md` 52 行、6,309 字节（原 74 行、14,883 字节）；`.cursor/AGENTS.md` 2,423 字节，`.github/AGENTS.md` 约 4.1 KB，`docs/superpowers/AGENTS.md` 1,721 字节 |
| 静态检查 | `check-headings.py` 32 个文件 0 处、`check-samples.sh` 114 个样例不符合 0 个；ShellCheck 0.9.0 对 `check-samples.sh` 0 条；actionlint 1.7.12 对 workflow 0 条 |
| 故意失败 | run [41238573929](https://github.com/yuangezhizao/luckfox_pico_lvgl_example/actions/runs/41238573929)：临时提交在根 `AGENTS.md` 末尾补到 8,370 字节，`📏 检查根 AGENTS.md 的体积` 失败，注解为「根 AGENTS.md 8370 字节，超过 8192 字节。按 … §3.1 把内容移到子目录 AGENTS.md、tests/README.md 或代码注释」，其后的编号检查被跳过；该提交随后以 `git reset --keep` 丢弃 |
| 复测，Codex | 同一模型与等级，worktree 为本分支 `76c2eca`：38 秒、3 条 shell 命令（一次 `cat` 根文件、三个子目录 `AGENTS.md`、`tests/README.md` 与 `CMakeLists.txt`），input 116,543（cached 82,560）、output 1,668；第 1–10 题都对，第 9 题按 spec §3.1 作答，第 10 题引用「优先级」；不再读 workflow 与历史 spec。题目覆盖所有方面，所以输入量与基线相近；每个会话的常驻部分由 14,883 字节降到 6,309 字节（复测时为 5,825 字节，之后按 `.gitignore` 更正一句，并按 spec Q13 加「按改动范围选检查」）。存于 Project Context `internal/agents-md-slim/probe-branch-codex.md`、`.jsonl` |
| 复测，Cursor | 子代理 `bc-4511c77b-320d-503f-bf53-b71c400b0d0c`：第 1–10 题都对，读过 9 个文件（基线为完整 3 个、部分 4 个、检索命中 17 个），没有读历史 spec 与 workflow；它报告自动附带的仍是旧根文件（§6）。存于 Project Context `internal/agents-md-slim/probe-branch-cursor.md` |
| 提交 | autosquash 并把 docs 提交移到最后，整理前后的树相同；5 个提交都有 SSH 签名、都不带 `Co-authored-by`，author time 不变；逐个提交运行 `check-headings.py` 均为 0 处，提交 3 起根文件为 6,309 字节 |
| co-author 钩子 | 本 VM 两个钩子改为 noreply 地址后，在临时仓库中提交，只追加 `Co-authored-by: 远哥制造 <yuangezhizao@users.noreply.github.com>`；全部分支的历史中没有钩子原先的工作邮箱（spec §2.7） |

## 6. 与计划的偏离

- Task 3：只比较了 glibc 交叉编译的告警条数，uClibc 工具链未在本机检出，由 CI 的 `🛠️ uClibc · Buildroot` 构建；改动只有注释。
- Task 8：worktree 中只删去含评分要点的本 plan，保留 spec：新的根文件在去向表中把「新增约定」指向 spec §3.1，删掉 spec 会使第 9 题失去依据。原计划两者都删。
- Task 8：Cursor 子代理的常驻规则仍是精简前的根文件（Cursor 在 Agent Run 启动时缓存规则，spec §2.2），复测不能排除旧根文件的影响；不受影响的比较以 Codex 为准。
- 推送：作者在执行中指示本 PR 不改代码、变体检验不会因此失败，该推送时直接推送，不必攒着；原约束为只改文档的修订攒到一起推送。

## 附录 A：未入库的命令

### A.1 冷启动问答的问题与评分要点

提示词（写入 `/tmp/probe/prompt.md`）：

```text
你在一个 Git 仓库的根目录。这是只读问答：不要修改任何文件，不要构建，不要运行测试，不要访问网络，不要执行任何 git 写操作。可以读取仓库中的文件，可以运行只读命令（cat、rg、ls、git log、git show）。逐题作答，每题 1–4 句，并写出依据的文件路径。最后列出本次读过的全部文件。

1. 要得到能在板子上运行的可执行文件，用什么命令构建？
2. 构建出的可执行文件能在这台 x86_64 VM 上运行吗？没有硬件时怎样查看界面？
3. 在 Cursor Cloud Agent 的 VM 上提交和改写提交时要注意什么？
4. 一个修改了 .cursor/Dockerfile 的 PR，合并前 CI 会怎样，为什么？
5. plan 中 Task 标题和编号的规则是什么？执行中途要插入一个 Task 怎么办？
6. 程序怎样判断板子是否带 WiFi？为什么不只看 /proc/device-tree/model？
7. 运行原生测试的命令是什么？32 位换算测试在什么情况下会被跳过？
8. 主程序实际按什么优化级别编译？为什么？
9. 想给以后的 agent 新增一条项目约定，应该写在哪里？
10. 如果某个技能要求先做 brainstorming 再动手，而作者让你直接改代码，你怎么做？
```

评分要点（取自 `origin/dev` 的 `AGENTS.md` 与代码）：

| 题 | 要点 |
|---|---|
| 1 | 板上用 uClibc 产物：设 `LUCKFOX_SDK_PATH`（可稀疏检出 `tools/linux/toolchain`），`env -u GLIBC_COMPILER`；glibc 产物 ABI 不兼容；不设变量时 `CMakeLists.txt` 提前 `return()` |
| 2 | 不能：32 位 ARM，且需要 `/dev/dri/card0`、`/dev/fb0`、触摸屏；qemu 只能冒烟到 `cannot open /dev/dri/card0`；看界面用 `tests/` 的 `screenshots` 或 `preview` |
| 3 | `env -u GIT_AUTHOR_*…`；钩子追加 `Co-authored-by`，reword、autosquash 时 `core.hooksPath=/dev/null`，不用 `--no-verify`；保留 author time；`cursor` 账户创建 PR 返回 403；输出前给令牌打码 |
| 4 | 镜像标签取 Dockerfile 内容哈希，PR 上找不到 dev 签名的镜像，`build-image` 失败、容器 job 被跳过，合并后 dev push 重建才恢复 |
| 5 | `### Task N：…` 全角冒号，数字从 0 或 1 起连续，放在单独的 Task 章；中途插入时按位置插入、后面顺延并同步引用 |
| 6 | 按 `/sys/bus/sdio/devices/*/uevent` 中 `SDIO_ID=C8A1:C18D`；SDK `824b817f` 起两款共用设备树，`model` 恒为 `Luckfox Pico Ultra`；旧固件 `model` 为 `Luckfox Pico Ultra W` 时直接视为带 WiFi |
| 7 | `cmake -S tests -B build-tests && cmake --build build-tests -j && ctest --test-dir build-tests --output-on-failure -j`；缺 `qemu-arm` 或 `arm-linux-gnueabihf-gcc` 时跳过，CI 用 `-DLUCKFOX_TESTS_ARM32=ON` 强制 |
| 8 | `-O0`：`-O3` 的 `add_compile_options()` 在 `add_executable()` 之后，不作用于主程序 |
| 9 | 基线：`AGENTS.md`；本分支：按 spec §3.1 判断，根文件、子目录文件、代码注释、技能或 spec |
| 10 | 作者的当次指令优先，直接改；用户规则 2、7 的停止点仍适用 |

### A.2 worktree

```bash
mkdir -p /tmp/probe
git -C /workspace worktree add --detach /tmp/probe/dev origin/dev
# 本分支（Task 8）：HEAD 中删去含评分要点的本 plan，保留 spec（§6），不提交
git -C /workspace worktree add --detach /tmp/probe/new HEAD
rm /tmp/probe/new/docs/superpowers/plans/2026-11-01-lvgl-example-agents-md-slim.md
# 结束后
git -C /tmp/probe/dev status --short; git -C /workspace status --short
git -C /workspace worktree remove --force /tmp/probe/new; git -C /workspace worktree remove /tmp/probe/dev
```

### A.3 Codex

```bash
# 安装（每台新 VM 一次；Node 装在 /opt/node，PATH 见 /etc/profile.d/node-lts.sh）
V=v26.13.0; curl -fsSLO https://nodejs.org/dist/$V/node-$V-linux-x64.tar.xz
curl -fsSL https://nodejs.org/dist/$V/SHASUMS256.txt | rg " node-$V-linux-x64.tar.xz$" | sha256sum -c -
sudo tar -C /opt -xJf node-$V-linux-x64.tar.xz && sudo ln -sfn /opt/node-$V-linux-x64 /opt/node
export PATH=/opt/node/bin:$PATH; sudo -E env PATH=$PATH npm i -g @openai/codex@latest
sudo apt-get update && sudo apt-get install -y bubblewrap
# 登录（在 tmux 会话 codex-login 中，设备码交作者）
codex login --device-auth; codex login status
# 问答：--yolo 即 --dangerously-bypass-approvals-and-sandbox；不放 ~/.codex/AGENTS.md
codex debug models   # gpt-6-astra：默认 low，支持 low、medium、high、xhigh、max、ultra
cd /tmp/probe/dev && codex exec --yolo -m gpt-6-astra -c model_reasoning_effort=medium --ephemeral --json \
  -o /tmp/probe/codex-dev.md "$(cat /tmp/probe/prompt.md)" > /tmp/probe/codex-dev.jsonl
```

### A.4 体积与告警条数

```bash
wc -c AGENTS.md .cursor/AGENTS.md .github/AGENTS.md docs/superpowers/AGENTS.md
# 各节字节数
python3 - <<'PY'
import re
t = open('AGENTS.md', encoding='utf-8').read()
for s in re.split(r'(?m)^(?=#{2,3} )', t):
    print(len(s.encode()), s.splitlines()[0] if s.strip() else '')
PY
# 增长过程
for c in $(git log --format=%h --reverse -- AGENTS.md); do echo "$c $(git log -1 --format=%ad --date=short $c) $(git show $c:AGENTS.md | wc -c)"; done
# 告警条数（只改注释前后应相同）
export GLIBC_COMPILER=/usr/bin/arm-linux-gnueabihf-
cmake -S . -B /tmp/b-glibc >/dev/null && make -C /tmp/b-glibc -j 2>&1 | rg -c 'warning:'
```

### A.5 co-author 钩子

```bash
H=~/.cursor/agent-hooks/$(printf %s /workspace | base64 | tr -d =)
# 不打印邮箱本身：只看域名，再在全部分支的历史中计数
E=$(sed -n "6s/.*<\(.*\)>.*/\1/p" $H/commit-msg.cursor.co-author); echo "${E#*@}"
git fetch -q origin; git log --all --format='%B%n%ae%n%ce' | rg -cF "$E"
# 改为 noreply 地址（只对本次 Agent Run 有效），在临时仓库中验证
for f in commit-msg.cursor.co-author prepare-commit-msg.cursor.co-author; do
  sed -i "6s/.*/'远哥制造 <yuangezhizao@users.noreply.github.com>'/" $H/$f
done
git init -q /tmp/hooktest && git -C /tmp/hooktest -c core.hooksPath=$H -c user.name=t -c user.email=t@example.invalid commit -q --allow-empty -m test
git -C /tmp/hooktest log -1 --format=%B; rm -rf /tmp/hooktest
```

## 附录 B：逐句对照表

根 `AGENTS.md`（`origin/dev` `de8dddb`）按节逐句列出新位置。「根」指本 PR 之后的根 `AGENTS.md`；同一句出现在多处时都列出。

| 原节 | 原句（摘要） | 新位置 |
|---|---|---|
| 交互约定 | 始终中文回复 | 根「交互约定」 |
| Cursor Cloud specific instructions | 保留英文标题作为锚点的说明 | 删除：现行 Cursor 文档只把该标题作为 Cloud 专属小节的示例（spec §2.6 第 7 条、Q3）；英文标题保留，只放 Cloud 专属内容 |
| 项目简介 | C + LVGL 8.3 + lv_drivers 8.1、CMake、面向 Luckfox Pico Ultra 的交叉编译 GUI 应用 | 根「项目与构建」 |
| 项目简介 | 库依赖 vendored 于 `lib/` | 根「项目与构建」；`.cursor/AGENTS.md`「当前活动」 |
| 项目简介 | 交叉工具链与 native 依赖由 `.cursor/Dockerfile` 安装；Dockerfile 模式 | `.cursor/AGENTS.md`「当前活动」 |
| 项目简介 | 构建命令见 `README.md` | 根直接给出两条构建命令 |
| Cloud Agent 环境 | 当前活动、备选、resolution order、grilling、`ubuntu` 用户与 `start`、`dpkg-query` | `.cursor/AGENTS.md` 对应各条，原文 |
| 提交与 GitHub 操作 | 身份与 `env -u …` | 根「Cursor Cloud specific instructions」，原文 |
| 提交与 GitHub 操作 | `Co-authored-by` 尾注由 `commit-msg` 追加 | 根同节，按 spec §2.7 改为：钩子每次 Run 生成，追加发起人的 Cursor 账户，`prepare-commit-msg` 在 `--no-verify` 时也运行 |
| 提交与 GitHub 操作 | 不用 `--no-verify`；reword、autosquash 加 `core.hooksPath=/dev/null`；`GIT_EDITOR` 加引号 | 根同节；补「只改说明的 `--amend`」与「改了内容的 `--amend` 仍要经过钩子」 |
| 提交与 GitHub 操作 | 保留作者时间 | 根同节，原文 |
| 提交与 GitHub 操作 | `cursor` 账户能推送，PR 操作 403，需 `gh auth login`，`hosts.yml` 会被改写 | 根同节，按 spec §2.7 补「创建、更新 PR 用 Cloud Agent 自带的 PR 管理工具」，403 限定为 `gh` |
| 提交与 GitHub 操作 | 打码 | 根同节，原文 |
| 构建 | 只能交叉编译，未导出变量时 `return()` | 根「项目与构建」 |
| 构建 | `GLIBC_COMPILER` 对应 glibc、`LUCKFOX_SDK_PATH` 对应 uClibc，稀疏检出命令（约 233 MB） | 根「项目与构建」两条子项 |
| 构建 | 构建目录放仓库外（`.gitignore` 只忽略 `build/`、`install/`） | 根「项目与构建」；按 `.gitignore` 补上 `build-tests/`（spec §2.8） |
| 构建 | glibc 构建命令块、`make install` 的产物目录 | 根「项目与构建」glibc 子项（命令改为行内） |
| 构建 | 工具链由 Dockerfile 安装（不再依赖 snapshot），本地补装命令 | 根 glibc 子项；「不再依赖 snapshot」是过程记录，删去 |
| GitHub Actions | CI 编两条（glibc 整包、uClibc 只出可执行文件） | `.github/AGENTS.md`「交叉编译」 |
| GitHub Actions | 板上用 uClibc，glibc ABI 不兼容 | 根「项目与构建」；`.github/AGENTS.md`「交叉编译」 |
| GitHub Actions | CI 成功不能代替真机 | 根「项目与构建」；`.github/AGENTS.md` 开头 |
| GitHub Actions | uClibc gcc 不在 Dockerfile，CI sparse checkout，不引入 submodule | `.github/AGENTS.md`「交叉编译」；`.cursor/AGENTS.md`「当前活动」 |
| GitHub Actions | 镜像只在 dev 构建并 attest，PR 只复用 dest-lock 通过的 digest | `.github/AGENTS.md`「CI 镜像」 |
| GitHub Actions | 改 Dockerfile 的 PR 上 `build-image` 必然失败 | 根「测试与检查」；`.github/AGENTS.md`「CI 镜像」；`.cursor/AGENTS.md`「CI」 |
| GitHub Actions | 不换成 `luckfox-pico-ci`；`USER ubuntu` 与 `--user 0` | `.github/AGENTS.md`「CI 镜像」 |
| GitHub Actions | `mutation-tests` 的触发、退出码、`SKIPPED`、失败输出 | `.github/AGENTS.md`「各 job」 |
| GitHub Actions | 新推送取消 CI，文档修订攒到一起推送 | 根「测试与检查」；`.github/AGENTS.md`「各 job」 |
| GitHub Actions | 逐提交测试只在本地执行 | `.github/AGENTS.md`「各 job」 |
| GitHub Actions | `native-tests` 的 preview 核对与 artifact | `.github/AGENTS.md`「各 job」；`tests/README.md` §7.1（已有） |
| GitHub Actions | `🐚 ShellCheck` job，新增或修改脚本先本地跑 | `.github/AGENTS.md`「各 job」；根「测试与检查」 |
| GitHub Actions | `📝 文档检查` job | `.github/AGENTS.md`「各 job」 |
| Lint | 无 C linter、无 `-Werror`；shell 由 ShellCheck 检查 | 根「项目与构建」「测试与检查」 |
| Lint | 告警段在 `add_executable()` 之前、`add_compile_options()` 只作用于其后的 target；`-std=gnu99` 的原因 | `CMakeLists.txt` 告警段注释 |
| Lint | glibc 121、uClibc 120 条基线，`-Wundef` 重复报 | `.github/AGENTS.md`「编译告警」，注明日期并给出现场统计命令 |
| Lint | `-O3` 一行不生效、主程序 `-O0` | `CMakeLists.txt` 该行注释 |
| Lint | LVGL、lv_drivers 不受影响，`C_FLAGS` 为空 | `CMakeLists.txt` 告警段注释；`.github/AGENTS.md`「编译告警」 |
| 测试 | 独立主机工程与命令 | 根「测试与检查」；`tests/README.md` §1（已有） |
| 测试 | 32 位测试需 `qemu-arm` 与交叉 gcc，缺任一跳过，两份 Dockerfile 提供 | `tests/README.md` §5（已有） |
| 测试 | CI 以 `-DLUCKFOX_TESTS_ARM32=ON` 强制运行 | `.github/AGENTS.md`「各 job」 |
| 测试 | 常规 UI 测试不调用 `custom_init()` | `tests/README.md` §6（已有） |
| 测试 | 用例按页面放、分类与改坏检验见 `tests/README.md` | 根「测试与检查」；`tests/README.md` §2、§8（已有） |
| 运行 / 验证 GUI | 32 位 ARM ELF，`Exec format error` | 根「项目与构建」（简述）；`tests/README.md` §7（新增段） |
| 运行 / 验证 GUI | `/dev/dri/card0`、`drmModeGetResources()`、正方形屏、`/dev/fb0`、evdev | 根「项目与构建」（简述）；`tests/README.md` §7（新增段，原文） |
| 运行 / 验证 GUI | 部署到物理板运行 | 根「项目与构建」 |
| 运行 / 验证 GUI | uClibc 产物 qemu 冒烟命令、`LD_LIBRARY_PATH` 须为绝对路径、预期输出 | `tests/README.md` §7（新增段，原文）；根指向 |
| 运行 / 验证 GUI | `screenshots`、`preview` 目标 | 根「项目与构建」；`tests/README.md` §7（已有） |
| 运行 / 验证 GUI | `app_env.c` 提供 5 个全局量的原因与 spec §6.1 | `tests/README.md` §2（补充） |
| 运行 / 验证 GUI | preview 脚本与假 `wpa_cli` | `tests/README.md` §7.1（已有） |
| 运行 / 验证 GUI | 可选手段，权威验证是真机 | 根「项目与构建」；`tests/README.md` §7（已有） |
| 运行 / 验证 GUI | 板型判据：共用设备树、`SDIO_ID=C8A1:C18D`、旧固件例外、PR #5 spec 链接 | `custom/custom_main.c` 判定函数注释；与 `insmod_wifi.sh` 同 ID 一句即已有的常量注释 |
| 文档 | 标题编号、Task、附录与 QA、检查命令、历史补号的经过 | `docs/superpowers/AGENTS.md` 对应各条；插入 Task 一句去掉「不确定时用 grilling 询问」（spec Q5）；检查命令另在根「测试与检查」 |
