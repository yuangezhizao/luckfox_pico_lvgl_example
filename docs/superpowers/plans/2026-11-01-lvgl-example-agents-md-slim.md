# Luckfox Pico LVGL Example — AGENTS.md 精简与 harness 审视实施计划

> **For agentic workers:** 作者同意本 PR 不必严格遵循 superpowers 的执行流程（spec Q4、Q11）；按下面的 Task 顺序执行，复选框（`- [ ]`）用于跟踪。

**Goal:** 根 `AGENTS.md` 降到 8 KiB 以内，只留几乎每个任务都用得到、读代码推不出来的内容；移出的内容按「离用到的地方最近」放进子目录 `AGENTS.md`、`tests/README.md` 与代码注释；写明指令优先级；CI 守住体积预算；仓库外的 harness 能改的改、不能改的写明改法。

**Architecture:** 推送 spec 与 plan、开草稿 PR（Task 1）；在 `origin/dev` 上做基线冷启动问答（Task 2）；代码注释（Task 3）；子目录说明文件（Task 4）；重写根文件与逐句对照表（Task 5）；CI 体积检查（Task 6）；仓库外 harness（Task 7）；本分支冷启动问答（Task 8）；整理与收尾（Task 9）。

**Tech Stack:** Markdown、GitHub Actions、bash、Python 3（markdown-it-py）、Codex CLI 0.166.0、pi 1.4.0、Node.js v26.13.0。

**Spec:** [`docs/superpowers/specs/2026-11-01-lvgl-example-agents-md-slim-design.md`](../specs/2026-11-01-lvgl-example-agents-md-slim-design.md)

---

## 1. Global Constraints

- 分支 `cursor/agents-md-slim-d7ec`，起点 `origin/dev` `de8dddb`（PR #16 合入）；PR 指向 `dev`，开发时为草稿，执行者不合并。
- 不改 C 代码与 CMake 的行为，只加注释；不改用例；本地不运行 `ctest` 与 `mutate.sh`，由 CI 运行（spec §4 第 6 条）。
- 提交身份与尾注按 `AGENTS.md`「Cursor Cloud specific instructions」：`env -u GIT_AUTHOR_NAME -u GIT_AUTHOR_EMAIL -u GIT_COMMITTER_NAME -u GIT_COMMITTER_EMAIL`；内容提交照常经过钩子；保留 author time。本 PR 的提交都在 Cursor Cloud Agent 中开发，按 spec Q20 都带 `Co-authored-by: 远哥制造 <yuangezhizao@users.noreply.github.com>`。
- 提交信息按根 `AGENTS.md`「提交约定」（spec Q15–Q19）：标题按 cz-conventional-emoji v1.1.8 的格式，正文写原因、不写过程、不硬折行，关联 spec 用 `Refs:` 脚注。
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

按根 `AGENTS.md`「提交约定」写成后，各提交在整理历史时重写了说明，author time 保持最初创建时的值：

1. `comments(main): 💡 把板型判据的背景写进代码注释`：`custom/custom_main.c`（💡 在 cz-conventional-emoji 中对应 `comments`）。
2. `comments(cmake): 💡 把告警段的作用范围与实际优化级别写进 CMakeLists.txt 注释`：`CMakeLists.txt`。
3. `docs(agents): 📝 新增子目录 AGENTS.md，并把测试与 GUI 验证说明并入 tests/README.md`：`.cursor/AGENTS.md`、`.github/AGENTS.md`、`docs/superpowers/AGENTS.md`、`tests/README.md`、`tests/tools/docs/` 两个脚本的注释。
4. `docs(agents): 📝 精简根 AGENTS.md`：根 `AGENTS.md`。
5. `ci(github-actions): 👷 新增根 AGENTS.md 的体积检查`：新 workflow `agents-md-size.yml`，`.github/AGENTS.md` 的说明，主 workflow 中 `📝 文档检查` 的注释。
6. `docs(superpowers): 📝 历史 spec 与 plan 中删除合并后核对 dev CI 的步骤`：PR #14–#16 的 plan 与 PR #14、#16 的 spec。
7. `docs(superpowers): 📝 新增 AGENTS.md 精简 spec 与 plan`：本 spec 与 plan，最后一个提交。

## 4. Tasks

### Task 1：spec 与 plan，推送并开草稿 PR ✅

落地：本 spec 与 plan；草稿 PR #17（由 Cloud Agent 自带的 PR 管理工具创建），正文开头为 spec §1.1。

### Task 2：基线冷启动问答 ✅

落地：`origin/dev` 上 Cursor 子代理与 Codex 两路问答，评分见 §5。Cursor 子代理在 Task 5 之前运行；Codex 由作者设备码登录后运行。

### Task 3：代码旁注释 ✅

落地：`custom/custom_main.c` 板型判定函数的注释；`CMakeLists.txt` 两处 `add_compile_options()` 的注释。告警条数与变体清单的核对见 §5。提交 1、2。

### Task 4：子目录说明文件 ✅

落地：`.cursor/AGENTS.md`、`.github/AGENTS.md`、`docs/superpowers/AGENTS.md`；`tests/README.md` §2 补 `app_env.c` 的来历、§7 补产物不能在 VM 上运行的原因、`/dev/fb0` 与触摸屏缺失时的实际行为与 qemu 冒烟；`tests/tools/docs/` 两个脚本注释中的规则出处。体积检查的说明随提交 5 加入 `.github/AGENTS.md`，使每个提交单独看都自洽；子目录文件之间互相提到的文档路径写成 Markdown 链接；按云端实测在 `.cursor/AGENTS.md` 补「规则缓存」一条，含本地预览与从分支做草稿 Build 的验证方法（spec §2.2、§4.1.3）。提交 3。

### Task 5：根 `AGENTS.md` ✅

落地：按 spec §3.2 重写，「提交与 GitHub 操作」按 spec §2.7 更新；对照 SDK 仓库后更正 `.gitignore` 一句、加「按改动范围选检查」（spec §2.8、Q13）；指向三个子目录 `AGENTS.md` 与 `tests/README.md` 的路径写成 Markdown 链接，便于点击；按 spec Q15 新增「提交约定」：git cz 格式与 spec、plan 放在最后一个提交；合并前按 Codex 审查改正硬件缺失时退出行为的一句（Task 9）；逐句去向见附录 B。提交 4。

### Task 6：CI 体积检查 ✅

落地：单独的 workflow `.github/workflows/agents-md-size.yml`（job「📏 检查根 AGENTS.md 的体积」）。最初是 `📝 文档检查` 中的一步，合并前按 Codex 审查移出（Task 9）。故意失败的临时提交见 §5，已丢弃。提交 5。

### Task 7：仓库外 harness

- [x] Project Context `workflows/codex-dev.md`「给 Codex 的公共指令要点」加一条（spec §3.6），已告知协调者。
- [x] 本 VM 的 co-author 钩子改为 noreply 地址（spec §2.7、§3.6）。
- [x] 本 VM 安装 Codex 与 pi，作者设备码登录（spec §3.6、附录 A.3）。
- [x] exec-daemon 的说明整理为 Project Context `docs/harness/exec-daemon.md`，待作者提交到 `yuangezhizao/Cursor-Cloud-Agents`（spec Q21）。
- [ ] 作者按 spec §3.6.1 修改 Cursor User Rules 后，由协调者同步 `internal/user-rules.md`。

### Task 8：本分支冷启动问答 ✅

落地：本分支上 Cursor 子代理与 Codex 两路复测，与基线的比较见 §5。之后补了行为测试、pi（Astra 与 Claude）与从本分支构建的草稿 Build 上的 Cursor，并在根文件每次实质改动后重测，全部轮次见 spec §4.2。

### Task 9：整理提交、精简 plan、PR 正文、合并前确认与 Codex 审查 ✅

- [x] 提交顺序为 §3，spec 与 plan 在最后；本 plan 删去已完成 Task 的步骤，只留落地记录、§5、§6 与附录。
- [x] PR 正文按 PR #14–#16 的格式更新，开头用 spec §1.1；CI 全部通过后改为 ready，不合并。作者要求暂时跳过变体检验，其余 6 个 job 通过后改为 ready（run 41239963455）。
- [x] Codex 审查（P2）：「CI 每次推送都会运行全部检查与变体检验」不准确，复现属实：workflow 的 `push` 只监听 `dev` 且 `paths-ignore: '**.md'`，`pull_request` 只针对 `dev`，本 PR 开 PR 前推送分支时也没有触发。改为指向 `dev` 的 PR 每次推送与 `dev` 上不只改 `.md` 的推送，并写明没开 PR 的分支推送不触发；以 fixup 并入提交 4。
- [x] 提交的说明按根 `AGENTS.md`「提交约定」重写（spec Q18、Q19）：原第 1 个提交 `docs(custom): 💡 …` 按适配器改为 `comments` 类型，并因 scope 必填、同时改了 `custom/` 与 `CMakeLists.txt`，拆为 `comments(main)` 与 `comments(cmake)` 两个提交，都沿用原提交的 author time；各提交正文写原因、不硬折行，关联 spec 改为 `Refs:` 脚注；author time 保持最初创建时的值。
- [x] 按 spec Q20 给全部提交补上 `Co-authored-by: 远哥制造 <yuangezhizao@users.noreply.github.com>`（`git rebase -x` 逐个 `--amend --trailer`），author time 不变。
- [x] 按作者要求在 PR 中评论 Q22 的开场提示词与 Q21 的落实。
- [x] 作者对 `972ef96` 再次请求 Codex 审查，结果为未发现重大问题。
- [x] 合并前准备：spec 状态行缩短为 PR #1–#4 的写法；本 plan 已是精简后的形式，不再重写；不做完整的 A、B、C 审查：只改文档与注释，告警条数与变体清单核对不变，Codex 审查未发现问题，CI 除按作者要求不等待的变体检验外均通过，合并前已对照仓库与原始记录通读 spec、plan、PR 正文并修正；PR 保持 ready，不合并。
- [x] Codex 审查（P2，`379122d`）：只改 `AGENTS.md` 的 dev 推送因主 workflow 的 `push.paths-ignore: '**.md'` 整个不触发，体积检查不运行。核对属实：`dev` 的规则集只禁止删除与强推，不要求经过 PR；另发现改 Dockerfile 的 PR 上它随 `📝 文档检查` 一同被跳过。改为单独的 workflow `agents-md-size.yml`，只在 `AGENTS.md` 变化时触发、在 runner 上直接运行，`📝 文档检查` 中的这一步删去；提交 5 的标题与说明随之重写（spec §3.5、Q6）。该提交上主 workflow 的 `🧪 原生测试` 在 720 一轮 preview 启动时 30 s 内没有等到窗口（480 一轮通过，测试与被测代码与上一个通过的 `379122d` 相同），作者重跑后通过（run 41301359547 第 2 次）。
- [x] Codex 审查（P2，`aba3440`）：根文件「并需要 `/dev/fb0` 与 evdev 触摸屏，否则 `exit()`」不准确。核对属实：`fbdev_init()` 打开失败只 `return`，`fbdev_flush()` 在 `fbp` 为空时跳过绘制；`custom_touch_init()` 返回 -1，`src/main.c` 不检查；会 `exit()` 的 `luckfox_get_input_device_info()` 在 `main.c` 中已注释掉。`origin/dev` 的原文只说 DRM 与非方屏会 `exit()`，是精简时压缩成一句改变了意思；第 22 轮 Cursor 问答第 2 题即答成「缺任何一项都会 `exit()`」。根文件改为「打不开 `/dev/dri/card0` 或屏幕不是正方形时启动即 `exit()`；缺 `/dev/fb0` 或 evdev 触摸屏时只报错、不退出」（7,916 字节），`tests/README.md` §7 补上缺失时的实际行为；以 fixup 并入提交 3、4，提交 4、5 说明中的字节数同步。Codex 问答复测 10 题全对（spec §4.2 第 28 轮）。作者随后对 `eaf5675` 与 `90ae965` 请求的审查都未发现重大问题，安全审查也没有发现问题。
- [x] 作者认为合并后核对 dev CI、把结果评论到 PR 不是必须的步骤，要求删去：本 plan 与 PR #14–#16 plan 中最后一个 Task「合并后核对 dev CI」及其在 Architecture 行中的对应部分，PR #14 spec §3.5 的一条与 §4 第 10 条、PR #16 spec §4 第 6 条（后两条顺延为第 6、7 条）。PR #1–#13 中：PR #9 的 V5「合并后 `dev` push 的 CI」是该 spec V1–V5 验证设计的一项，只要求 dev push 自动运行、不评论到 PR，状态行、验证表、风险表与 QA 都引用它，保留；PR #7 plan §5 记的「合并后首次 dev push run 四个 job 均 success」是已完成的实测结果，保留；其余没有此类步骤。提交 6。

## 5. 验证证据

| 项 | 结果 |
|---|---|
| 起点体积 | `origin/dev` `de8dddb` 的根 `AGENTS.md` 74 行、14,883 字节、9,800 字符；21 个提交改过它，已合并的 16 个 PR 中 14 个改过（只有 #2、#15 没改）（附录 A.4） |
| 文档检查 | 加入本 spec 与 plan 后 32 个文件问题 0 处，114 个样例不符合 0 个 |
| 基线问答，Cursor | 子代理 `bc-2c930850-ace1-5eed-a38c-932d10720932`（Claude Opus 5.5）：第 1–8 题对，第 9 题部分对（答「写进根 `AGENTS.md`、保留英文锚点」，为此翻了 PR #11、#12 等历史 spec），第 10 题部分对（仓库没有规定，凭默认判断）；完整读取 3 个文件、部分读取 4 个、检索命中 17 个，其中 11 个是历史 spec 与 plan。原始回答存于 Project Context `internal/agents-md-slim/probe-baseline-cursor.md` |
| 基线问答，Codex | Codex CLI 0.166.0、`gpt-6-astra` `medium`（目录默认 `low`），无 `~/.codex/AGENTS.md`：41 秒、5 条 shell 命令，input 111,438（cached 72,192）、output 1,850；第 1–8 题对，第 9 题部分对（答「根 `AGENTS.md` 对应主题的小节」），第 10 题部分对（明确说仓库没有规定，依据 Codex 自身规则）；整份读了 workflow 与 PR #3 spec。原始回答与事件流存于 Project Context `internal/agents-md-slim/probe-baseline-codex.md`、`.jsonl` |
| 代码注释 | glibc 交叉编译告警在 `origin/dev` 与本分支都是 121 条；变体清单中指向 `custom/custom_main.c` 的 8 条 `old` 在改前改后各匹配一次（按 `mutate.sh` 的解析规则，含 `\n` 换行） |
| 体积 | 根 `AGENTS.md` 61 行、7,916 字节（原 74 行、14,883 字节）；`.cursor/AGENTS.md` 3,328 字节，`.github/AGENTS.md` 4,422 字节，`docs/superpowers/AGENTS.md` 1,721 字节；子目录文件不计入体积检查 |
| 静态检查 | `check-headings.py` 32 个文件 0 处、`check-samples.sh` 114 个样例不符合 0 个；ShellCheck 0.9.0 对 `check-samples.sh` 0 条；actionlint 1.7.12 对两个 workflow 0 条 |
| 故意失败 | run [41238573929](https://github.com/yuangezhizao/luckfox_pico_lvgl_example/actions/runs/41238573929)：临时提交在根 `AGENTS.md` 末尾补到 8,370 字节，`📏 检查根 AGENTS.md 的体积` 失败，注解为「根 AGENTS.md 8370 字节，超过 8192 字节。按 … §3.1 把内容移到子目录 AGENTS.md、tests/README.md 或代码注释」，其后的编号检查被跳过；该提交随后以 `git reset --keep` 丢弃。改为单独的 workflow 后再验证一次：run [41301267418](https://github.com/yuangezhizao/luckfox_pico_lvgl_example/actions/runs/41301267418)（`pull_request`），临时提交补到 8,208 字节，`根 AGENTS.md 体积` 失败，`AGENTS.md` 上的注解为「根 AGENTS.md 8208 字节，超过 8192 字节……」；同样丢弃。`dev` 推送的触发无法在 PR 上实测，按 `on.push.paths` 的写法判断 |
| 复测，Codex | 同一模型与等级，worktree 为本分支 `76c2eca`：38 秒、3 条 shell 命令（一次 `cat` 根文件、三个子目录 `AGENTS.md`、`tests/README.md` 与 `CMakeLists.txt`），input 116,543（cached 82,560）、output 1,668；第 1–10 题都对，第 9 题按 spec §3.1 作答，第 10 题引用「优先级」；不再读 workflow 与历史 spec。题目覆盖所有方面，所以输入量与基线相近；每个会话的常驻部分由 14,883 字节降到 7,916 字节（复测时为 5,825 字节，之后按 `.gitignore` 更正一句、按 spec Q13 加「按改动范围选检查」、限定 CI 触发范围，把指向子目录文件与 `tests/README.md` 的路径改为 Markdown 链接，按 spec Q15 加「提交约定」一节，按 Q16–Q19 写全该节，按 Q20 允许 noreply 地址的尾注，按 Q19 写明 scope 必填，并在去向表中指向 spec §4.1 的验证方法，按 Codex 审查改正硬件缺失时的退出行为）。存于 Project Context `internal/agents-md-slim/probe-branch-codex.md`、`.jsonl` |
| 复测，Cursor | 子代理 `bc-4511c77b-320d-503f-bf53-b71c400b0d0c`：第 1–10 题都对，读过 9 个文件（基线为完整 3 个、部分 4 个、检索命中 17 个），没有读历史 spec 与 workflow；它报告自动附带的仍是旧根文件（§6）。存于 Project Context `internal/agents-md-slim/probe-branch-cursor.md` |
| 中间版本问答，Codex | 根文件按 `.gitignore` 更正、加「按改动范围选检查」并限定 CI 触发范围之后（6,309 字节），同一模型与等级：41 秒、6 条命令，input 136,717（cached 98,176）、output 1,847；第 1–10 题都对。存于 Project Context `internal/agents-md-slim/behavior-and-final-probe-codex.md` |
| 行为测试，Codex | 同一个只改文档的小任务（`tests/README.md` 补一句并按约定提交），在 `origin/dev` 与本分支的独立克隆中各跑一次（去掉远端，钩子同本机）：基线 55 秒、11 条命令、input 403,754，跑了 `check-headings.py` 与 `check-samples.sh`；本分支 31 秒、8 条命令、input 232,814，只跑 `check-headings.py` 并引用「按改动范围选检查」。两边都用 `env -u …`、带签名、以 `core.hooksPath=/dev/null` 去掉尾注，都没有停下询问；提交说明都写成 `docs: 补充文档检查脚本依赖`，不是 git cz 格式（spec Q15）。存于同上 |
| 云端实测，Cursor | 在本分支上新开云端 Agent Run（子代理 `bc-70d358d6-905f-5740-a40e-288d60b30b20`，分支当时为 `70471ed`）：先记录常驻规则，再依次读 `.github/`、`docs/superpowers/`、`.cursor/` 下的普通文件，检查各子目录 `AGENTS.md` 独有的一句原文是否出现。常驻规则只有根文件，且是精简前的版本；三次读取后都没有出现子目录文件的内容；工作区中 `.github/AGENTS.md` 第 3 行确为该原文。根规则与检出的分支不一致。追加核对：该 VM 的 `request-context-cache.json` 写入于 2026-10-31 20:16:24（与 `builtAtMs` 一致），`requestContext.rules` 只有根文件一条，内容与 `de8dddb` 的 `AGENTS.md` 逐字相同；git reflog 显示 Run 启动后才从 `dev` 切到本分支；本 VM 的缓存写入时间与内容相同。即规则取自 Environment Build 时的缓存，本次不能判断 Cursor 是否按目录自动加载（spec §2.2、§3.3.1）。存于 Project Context `internal/agents-md-slim/cloud-nested-load-test.md` |
| 云端实测，草稿 Build | 从本分支触发草稿构建 `bld-20261031-847b7540-4900-4dab-ad00-9bf8aac761b5`（约 4 分 20 秒），以它起云端子代理 `bc-c09071fa-9ce8-53bb-aebc-da0063665634`：常驻规则只有根文件且为新版；读 `.github/`、`docs/superpowers/`、`.cursor/` 下的文件时，读取结果末尾各附上该目录的 `AGENTS.md`；规则缓存中 4 条规则（根文件与三个子目录文件）类型都是 `global`。即子目录文件写进缓存但按需附加、不常驻（spec §3.3.1）。存于 Project Context `internal/agents-md-slim/cloud-nested-load-test.md` |
| pi 问答与行为测试 | pi 1.4.0，`openai-codex/gpt-6-astra`、`medium`，无 `~/.pi/agent/AGENTS.md`。问答：基线 30 秒、2 次工具调用，第 1–8 题对、第 9、10 题部分对；本分支 47 秒、8 次，10 题全对，读了根文件、两个子目录 `AGENTS.md` 与 `tests/README.md`。行为测试：基线 55 秒、15 次，跑了 `check-headings.py` 与 `check-samples.sh`，提交说明 `docs: 补充文档检查脚本的依赖说明`；本分支 37 秒、10 次，只跑 `check-headings.py`，提交说明 `docs(tests): 📝 补充文档检查脚本依赖`（git cz 格式）。两边都守住身份、签名与去尾注，都没有停下询问。存于 Project Context `internal/agents-md-slim/pi-astra-tests.md` |
| 本地预览规则缓存 | 在本分支的检出目录中运行 `prebuild-request-context-cache --cloud-rules-enabled`（附录 A.6），`requestContext.rules` 为根文件与三个子目录文件共 4 条、类型都是 `global`，`cloudRule` 只含根文件（6,757 字节，当时的版本），与草稿 Build 的结果一致；运行前后本机缓存的 SHA-256 相同 |
| 7,858 字节版本复测，Codex | 根文件 7,858 字节（尾注规则按 spec Q20 改写之后），同一模型与等级：问答 10 题全对，第 3 题说明每次 Run 首次提交前核对 co-author 钩子；行为测试先核对钩子地址，只跑 `check-headings.py` 与 `git diff --check`，提交说明 `docs(tests): 📝 补充文档检查脚本依赖`（简单改动省略正文），钩子追加 noreply 地址的 `Co-authored-by`，没有停下询问。存于 Project Context `internal/agents-md-slim/behavior-and-final-probe-codex.md` |
| 7,882 字节版本复测，Codex 与 pi | scope 必填之后（`047fcd4`），Codex 与 pi 都用 `gpt-6-astra` `medium`：问答都是 10 题全对；行为测试都先核对钩子地址，只跑 `check-headings.py` 与 `git diff --check`，提交说明 `docs(tests): 📝 补充文档检查脚本依赖`，带 scope 与 noreply 尾注（spec §4.2 第 18–21 轮）。存于 Project Context `internal/agents-md-slim/behavior-and-final-probe-codex.md` |
| 云端实测，草稿 Build 2 | 从 `047fcd4` 构建 `bld-20261031-38125777-d8c4-4f38-aaa1-f3a2d8cf0d5f`，以它起两个显式指定 `claude-opus-5-5-high` 的云端子代理：问答 `bc-8c936e4d-47b1-5e92-a3b7-f73df51a3fe1` 先报告常驻规则是新版根文件，10 题全对；行为测试 `bc-fdec677c-6cea-5308-9628-82e659ea8bdd` 提交前把新 VM 钩子里的工作邮箱改为 noreply 地址，提交说明 `docs(tests): 📝 写明文档检查脚本依赖 python3-markdown-it` 带一句正文，另跑了 `check-samples.sh`（spec §4.2 第 22、23 轮）。存于 Project Context `internal/agents-md-slim/cursor-build2-tests.md` |
| pi + Claude | pi 1.4.0，`github-copilot/claude-opus-5.5` `high`：问答基线 8+2、本分支 `e27e755` 10 题全对；行为测试基线已能从 `git log` 归纳出格式并写正文，本分支核对钩子地址、格式正确、noreply 尾注，但仍多跑 `check-samples.sh`（spec §4.2 第 24–27 轮）。存于 Project Context `internal/agents-md-slim/pi-claude-tests.md` |
| 测试轮次汇总 | 本 PR 共 28 轮（Codex 10 轮；pi 10 轮，其中 `gpt-6-astra` 6 轮、Claude Opus 5.5 4 轮；Cursor 7 轮，其中 1 轮为续问、2 轮在草稿 Build 2 上；本地预览规则缓存 1 轮）。7,882 字节版本上 Codex、pi 与 Cursor，7,906 字节版本上 pi + Claude，问答都是 10 题全对，行为测试都核对了钩子地址、提交说明符合格式；Claude（Cursor 与 pi）仍多跑 `check-samples.sh`。7,916 字节的最终版本改正了硬件缺失时的退出行为，由 Codex 问答复测（第 28 轮，存于 Project Context `internal/agents-md-slim/round28-qa-codex.md`）。方法、结果与经验见 spec §4.1–§4.3 |
| 链接与图 | 四个 `AGENTS.md`、`tests/README.md` 与本 spec、plan 中的 6 个相对链接都指向存在的文件；spec 中 5 个 Mermaid 代码块用 `@mermaid-js/mermaid-cli` 都能渲染 |
| 提交 | autosquash 并把 docs 提交移到最后，整理前后的树相同；7 个提交都有 SSH 签名，按 spec Q20 都带 noreply 地址的 `Co-authored-by`，author time 不变；逐个提交运行 `check-headings.py` 均为 0 处，提交 4 起根文件为 7,916 字节 |
| co-author 钩子 | 本 VM 两个钩子改为 noreply 地址后，在临时仓库中提交，只追加 `Co-authored-by: 远哥制造 <yuangezhizao@users.noreply.github.com>`；全部分支的历史中没有钩子原先的工作邮箱（spec §2.7） |

## 6. 与计划的偏离

- Task 3：只比较了 glibc 交叉编译的告警条数，uClibc 工具链未在本机检出，由 CI 的 `🛠️ uClibc · Buildroot` 构建；改动只有注释。
- Task 8：worktree 中只删去含评分要点的本 plan，保留 spec：新的根文件在去向表中把「新增约定」指向 spec §3.1，删掉 spec 会使第 9 题失去依据。原计划两者都删。
- Task 8：Cursor 子代理的常驻规则仍是精简前的根文件（Cursor 在 Agent Run 启动时缓存规则，spec §2.2），复测不能排除旧根文件的影响；不受影响的比较以 Codex 为准。后来从本分支做草稿 Build，在新版根文件下补测了 Cursor（spec §4.2 第 10、22、23 轮）。
- Task 6：体积检查原计划放在 `📝 文档检查` 中（spec Q6），合并前改为单独的 workflow，原因见 Task 9 与 spec §3.5。
- 范围：作者要求一并删去 PR #14–#16 plan 中合并后核对 dev CI 的 Task，单独成提交 6（Task 9）。
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
| 2 | 不能：32 位 ARM；打不开 `/dev/dri/card0` 或非方屏时 `exit()`，缺 `/dev/fb0`、触摸屏只报错不退出（答成「缺任何一项都退出」算部分对）；qemu 只能冒烟到 `cannot open /dev/dri/card0`；看界面用 `tests/` 的 `screenshots` 或 `preview` |
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
# 行为测试：独立克隆，去掉远端（提交到不了 GitHub），钩子同本机；任务原文见 spec §4.1 所指的 Project Context 文件
H=$(git -C /workspace config --get core.hooksPath)
git clone -q /workspace /tmp/bt && git -C /tmp/bt remote remove origin && git -C /tmp/bt config core.hooksPath "$H"
git -C /tmp/bt checkout -q --detach <被测提交>   # 基线为 de8dddb
```

### A.3 Codex 与 pi

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
# pi：npm 默认缓存目录无写权限时加 --cache
sudo -E env PATH=$PATH npm i -g --cache /tmp/npmcache @earendil-works/pi-coding-agent@latest; pi --version
# 登录：在 tmux 会话 pi-login 中运行 pi，输入 /login，分别选 openai-codex 与 github-copilot，设备码交作者
pi auth check --provider github-copilot; pi --list-models opus
# 问答只给只读工具；行为测试把 --tools 换成 read,bash,edit,write,grep,find,ls；不放 ~/.pi/agent/AGENTS.md
cd /tmp/probe/dev && pi -p --provider openai-codex --model gpt-6-astra --thinking medium \
  --tools read,grep,find,ls,bash --name qa-dev "$(cat /tmp/probe/prompt.md)" > /tmp/probe/pi-dev.md
# 跨模型对照：--provider github-copilot --model claude-opus-5.5 --thinking high
# 会话日志 ~/.pi/agent/sessions/--tmp-probe-dev--/*.jsonl：每条回复的 model、thinkingLevel、usage
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

### A.6 本地预览规则缓存

```bash
F=/opt/cursor/.exec-daemon/request-context-cache.json
cp -p $F /tmp/rcc.bak
cd <要预览的检出目录> && /exec-daemon/node /exec-daemon/index.js prebuild-request-context-cache --cloud-rules-enabled --log-level warn
python3 -c "import json; rc=json.load(open('$F'))['requestContext']; [print(r['fullPath'], r['type'], len(r['content'].encode())) for r in rc['rules']]; print('cloudRule', len(str(rc.get('cloudRule','')).encode()))"
cp -p /tmp/rcc.bak $F   # 还原本机正在使用的缓存
```

### A.7 Cursor：从分支做草稿 Build 再起云端子代理

1. 先推送要测的提交。调用 `cursor-cloud` 的 `trigger-environment-build`，参数 `{"refs": [{"repoUrl": "<environment-info 的 repos 中本仓库的地址>", "ref": "cursor/agents-md-slim-d7ec"}]}`，返回 `buildId`；用 `list-environment-builds` 等它完成（约 4 分钟）。不激活该 Build。
2. 用 `Task` 起子代理：`environment: "cloud"`、`cloud_requested_environment_build_id: "<buildId>"`、显式的 `model`（本 PR 为 `claude-opus-5-5-high`）。问答的提示词让它先在调用任何工具前报告常驻规则的来源、是否新版根文件（用新版独有的一句判断）与自己的模型，再按 A.2 建 worktree、删 plan、答 A.1 的题；行为测试按 A.2 建克隆后做同一个任务。
3. 云端子代理检出的是 Build 时的提交，结果中核对它报告的 HEAD；新 VM 的 co-author 钩子是 Run 发起人的工作邮箱，回报中只让它写域名。
4. 结果用 `batch-fetch-details`（`includeTranscripts`）取回；平台元数据不记录子代理的模型，以 `Task` 参数与子代理自报为准（spec §4.1.3）。

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
| 提交与 GitHub 操作 | `Co-authored-by` 尾注由 `commit-msg` 追加，本仓不带 | 根同节，按 spec §2.7 改为钩子每次 Run 生成、追加发起人的 Cursor 账户，`prepare-commit-msg` 在 `--no-verify` 时也运行；按 spec Q20 改为允许带尾注、地址只能是 GitHub noreply 地址，每次 Run 第一次提交前检查钩子 |
| 提交与 GitHub 操作 | 不用 `--no-verify`；reword、autosquash 加 `core.hooksPath=/dev/null`；`GIT_EDITOR` 加引号 | 根同节保留「不用 `--no-verify`」与 `GIT_EDITOR` 一条，补推送前检查尾注地址；`core.hooksPath=/dev/null` 一条按 spec Q20 删去：它只为去掉尾注而设 |
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
| 运行 / 验证 GUI | `/dev/dri/card0`、`drmModeGetResources()`、正方形屏、`/dev/fb0`、evdev | 根「项目与构建」（简述，按 Task 9 改正为只有 DRM 与非方屏会 `exit()`）；`tests/README.md` §7（新增段，原文，并补上 `/dev/fb0` 与触摸屏缺失时的实际行为） |
| 运行 / 验证 GUI | 部署到物理板运行 | 根「项目与构建」 |
| 运行 / 验证 GUI | uClibc 产物 qemu 冒烟命令、`LD_LIBRARY_PATH` 须为绝对路径、预期输出 | `tests/README.md` §7（新增段，原文）；根指向 |
| 运行 / 验证 GUI | `screenshots`、`preview` 目标 | 根「项目与构建」；`tests/README.md` §7（已有） |
| 运行 / 验证 GUI | `app_env.c` 提供 5 个全局量的原因与 spec §6.1 | `tests/README.md` §2（补充） |
| 运行 / 验证 GUI | preview 脚本与假 `wpa_cli` | `tests/README.md` §7.1（已有） |
| 运行 / 验证 GUI | 可选手段，权威验证是真机 | 根「项目与构建」；`tests/README.md` §7（已有） |
| 运行 / 验证 GUI | 板型判据：共用设备树、`SDIO_ID=C8A1:C18D`、旧固件例外、PR #5 spec 链接 | `custom/custom_main.c` 判定函数注释；与 `insmod_wifi.sh` 同 ID 一句即已有的常量注释 |
| 文档 | 标题编号、Task、附录与 QA、检查命令、历史补号的经过 | `docs/superpowers/AGENTS.md` 对应各条；插入 Task 一句去掉「不确定时用 grilling 询问」（spec Q5）；检查命令另在根「测试与检查」 |
