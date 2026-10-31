# AGENTS

## 交互约定

- 与本仓库交互时，请始终使用中文回复（代码、路径、命令除外）。

## 优先级

- 作者在对话中的当次指令优先于本文件，本文件优先于技能中的流程要求。superpowers 等技能的流程在本仓库只作可选参考，作者已明确同意不必为满足技能流程而停下或补做步骤；spec 与 plan 仍按 [`docs/superpowers/AGENTS.md`](docs/superpowers/AGENTS.md) 的格式写。
- 因本文件或技能而停下、请求确认或偏离任务时，说明是哪个文件的哪一句。

## 项目与构建

- 面向 `Luckfox Pico Ultra`（含 Ultra W）ARM 开发板的嵌入式 GUI 例程：C + LVGL 8.3 + lv_drivers 8.1，CMake 构建，库依赖 vendored 于 `lib/`。
- 只能交叉编译，产物是 32 位 ARM：不导出 `GLIBC_COMPILER` 或 `LUCKFOX_SDK_PATH` 时 `CMakeLists.txt` 提前 `return()`，不产生任何构建目标。
- 板上（SDK 编的 Buildroot）只能运行 **uClibc** 产物；glibc 产物 ABI 不兼容（`libc.so.6` vs `libc.so.0`）。构建目录放仓库外（`.gitignore` 只忽略 `build/`、`build-tests/`、`install/`）。
  - uClibc：无需完整 SDK，按 CI 做法稀疏检出工具链（约 233 MB）：`git clone --depth 1 --filter=blob:none --sparse -b dev https://github.com/yuangezhizao/luckfox-pico.git /tmp/luckfox-pico && git -C /tmp/luckfox-pico sparse-checkout set tools/linux/toolchain`，再 `env -u GLIBC_COMPILER LUCKFOX_SDK_PATH=/tmp/luckfox-pico cmake -S . -B /tmp/build-uclibc && make -C /tmp/build-uclibc -j`。
  - glibc：`export GLIBC_COMPILER=/usr/bin/arm-linux-gnueabihf-` 后 `mkdir -p build && cd build && cmake .. && make -j && make install`，可部署目录写入 `install/luckfox_lvgl_demo/`（二进制 + 打包的 `lib/`）。工具链由 `.cursor/Dockerfile` 安装，其他环境 `sudo apt-get install -y gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf`。
- 产物不能在 x86 VM 上运行：除了架构不同，打不开 `/dev/dri/card0` 或屏幕不是正方形时启动即 `exit()`；缺 `/dev/fb0` 或 evdev 触摸屏时只报错、不退出。没有硬件时用 `tests/` 的 `screenshots`、`preview` 目标看界面；uClibc 产物的 qemu 冒烟见 [`tests/README.md`](tests/README.md)。权威验证是物理 Luckfox Pico Ultra / Ultra W，CI 通过只证明交叉编译与 ABI 门禁。
- C 代码没有 linter，告警只记录、不作门禁（无 `-Werror`）。

## 测试与检查

- 原生测试（主机 gcc + ASan/UBSan，headless）：`cmake -S tests -B build-tests && cmake --build build-tests -j && ctest --test-dir build-tests --output-on-failure -j`；分类、命名、32 位测试、改坏检验与 preview 脚本见 [`tests/README.md`](tests/README.md)。
- 新增或修改 shell 脚本先在本地跑 `shellcheck -x -P SCRIPTDIR <脚本>`，CI 要求 0 条（含 style 级）。
- 改了 spec 或 plan 运行 `python3 tests/tools/docs/check-headings.py`，有问题时 CI 失败。
- 按改动范围选检查：只改文档跑 `check-headings.py`；改 shell 脚本跑 ShellCheck；改 `custom/`、`generated/` 或用例跑 ctest，改界面时再跑 [`tests/README.md`](tests/README.md) §7.1 的核对脚本；改 `CMakeLists.txt` 或 `lib/` 跑两条交叉编译。plan 另有要求时以 plan 为准。CI 只在指向 `dev` 的 PR 每次推送、以及 `dev` 上不只改 `.md` 的推送时运行全部检查与变体检验，没开 PR 的分支推送不触发。
- 修改 `.cursor/Dockerfile` 的 PR 在合并前 `build-image` 必然失败、容器 job 被跳过，合并后 dev push 重建镜像才恢复。同一 PR 的新推送会取消正在运行的 CI（`🧬 变体检验` 最耗时），只改文档的修订尽量攒到一起推送。

## 提交约定

- 标题按 [cz-conventional-emoji v1.1.8](https://github.com/gaoac/cz-conventional-emoji/blob/v1.1.8/engine.js#L21) 的格式 `<type>(<scope>): <emoji> <描述>`：先选与改动相符的 gitmoji，type 由 emoji 决定（取该文件的 `GITMOJI_TYPE_MAP`，未列出的取 gitmoji 名称），如 ✨ `feat`、🐛 `fix`、📝 `docs`、✅ `test`、👷 `ci`、🔧 `chore`、🔨 `hammer`、🚸 `ux`、💡 `comments`；scope 必填，取改动所在的模块，常用的见 `git log`；描述用一句动宾短语，不加句号。
- 一个提交只做一件事，一句话说不清就拆开。
- 正文写 diff 看不出的内容：要解决的问题与根因、为什么选这个做法（必要时写被否决的替代）、副作用与合并后须知；不复述改了哪些文件，不写过程（谁发现的、第几次修改），引用外部讨论时概括要点；简单改动可以不写正文。
- 正文不硬折行，这与 Git 按 72 列折行的惯例不同。
- 关联的 spec 写成文末脚注 `Refs: docs/superpowers/specs/<文件>.md §X`，它不能代替正文中的原因。
- spec 与 plan 放在 PR 的最后一个提交。

## 去向表

| 要做的事 | 先读 |
|---|---|
| 改 Cloud Agent 环境：`.cursor/` 下的 Dockerfile、`install.sh`、`start.sh`、`environment.json` | [`.cursor/AGENTS.md`](.cursor/AGENTS.md) |
| 改 workflow、排查 CI 失败 | [`.github/AGENTS.md`](.github/AGENTS.md) |
| 写或改 `docs/superpowers/` 下的 spec、plan | [`docs/superpowers/AGENTS.md`](docs/superpowers/AGENTS.md) |
| 写测试，做 native 渲染、preview 截图或 qemu 冒烟 | [`tests/README.md`](tests/README.md) |
| 改板型（WiFi）判定、`CMakeLists.txt` 的告警段 | 代码旁的注释 |
| 新增给 agent 的约定 | spec [`2026-11-01-lvgl-example-agents-md-slim-design.md`](docs/superpowers/specs/2026-11-01-lvgl-example-agents-md-slim-design.md) §3.1 的放置原则、§4.1 的验证方法；本文件不超过 8 KiB，CI 检查 |

## Cursor Cloud specific instructions

只适用于 Cursor Cloud Agent 的 VM；环境配置见 [`.cursor/AGENTS.md`](.cursor/AGENTS.md)。

- **身份**：提交身份为仓库配置的 `Cursor Agent <cursoragent@cursor.com>`，带 SSH 签名。环境会注入 `GIT_AUTHOR_*`、`GIT_COMMITTER_*`，提交、rebase 前要加 `env -u GIT_AUTHOR_NAME -u GIT_AUTHOR_EMAIL -u GIT_COMMITTER_NAME -u GIT_COMMITTER_EMAIL`。
- **`Co-authored-by` 尾注**：`.git/config` 的 `core.hooksPath` 指向 `~/.cursor/agent-hooks/<id>/`，其中的钩子由 Cursor 在每次 Agent Run 启动时生成，`commit-msg` 与 `prepare-commit-msg` 会把发起本次 Run 的 Cursor 账户追加为 `Co-authored-by`（`prepare-commit-msg` 在 `--no-verify` 时也运行）。本仓允许带该尾注，但地址只能是 `远哥制造 <yuangezhizao@users.noreply.github.com>`，不能写进工作邮箱。
  - 每次 Run 第一次提交前，查看 `commit-msg.cursor.co-author` 与 `prepare-commit-msg.cursor.co-author` 中的 `CO_AUTHORS`，不是该地址就改成它；只对本次 Run 有效，新 Run 会重新生成。
  - 内容提交照常经过钩子，`pre-commit` 会做密钥扫描，不要用 `--no-verify`；推送前用 `git log --format=%B origin/dev..HEAD | grep -i co-authored-by` 确认没有其他地址。
  - `GIT_EDITOR` 由 shell 解释，参数中有 `(` 时要加引号。
- **保留作者时间**：rebase（含 fixup、reword）和 `--amend` 都要保留 author time，committer time 可以变化。不要用 `--reset-author`、`--ignore-date`、`--date`，也不用 `--committer-date-is-author-date`。
- **PR 与 GitHub 账户**：推送可以使用 Cloud Agent 自带的 `cursor` 账户凭据；创建、更新 PR 用 Cloud Agent 自带的 PR 管理工具。在本环境用 `gh`（`cursor` 账户）创建 PR、改 PR 正文、发评论会返回 HTTP 403 `Resource not accessible by integration`，需要 `gh` 写操作时请作者 `gh auth login` 登录本人账户；环境刷新令牌时会改写 `~/.config/gh/hosts.yml`，登录随时可能失效。
- **打码**：`git remote -v`、push 报错可能显示带令牌的地址，输出前要先打码。
