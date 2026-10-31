# Cursor Cloud Agent 环境

改 `.cursor/` 下的环境配置（`environment.json`、两个 Dockerfile、`install.sh`、`start.sh`）前读本文件。提交与 GitHub 操作的约定在根 `AGENTS.md` 的「Cursor Cloud specific instructions」。

- **当前活动**：`.cursor/environment.json` → `.cursor/Dockerfile`（Dockerfile 模式，配置即代码）：自建 `ubuntu:24.04`，apt 安装 ARM 交叉工具链、CMake、native/SDL 验证依赖（`libdrm-dev`、`libcjson-dev` 等）、`qemu-user` 与 preview 脚本工具（`imagemagick`、`xdotool`、`xvfb`）、文档检查依赖 `python3-markdown-it`，另装通用 / 诊断 CLI 与 Tailscale，末尾 `USER ubuntu`。项目库依赖（LVGL、lv_drivers、libdrm、libcjson）vendored 于 `lib/`，不由 Dockerfile 安装；uClibc gcc 也不在 Dockerfile 中（取法见根 `AGENTS.md`「项目与构建」）。
- **备选**：`.cursor/Dockerfile.luckfox_pico`（Luckfox 官方镜像 `luckfoxtech/luckfox_pico:1.0`，该 FROM 无 `ubuntu` 账户，`USER` 前 `useradd` 创建）；切换只需把 `environment.json` 的 `build.dockerfile` 改为指向它。
- 按官方 resolution order，repo 级 `.cursor/environment.json` 优先于 personal / team saved environment，故通常无需任何 Dashboard 操作。
- **grilling**：`install`（`sudo -n -E bash .cursor/install.sh`）在创建 Environment Build 时把固定 commit `85f83d3` 的技能写入 `$HOME/.cursor/skills/grilling/SKILL.md`（不进 git），并 `chown` 给 `ubuntu`。新 Agent 须从捕获了该 install 的 Build 启动。
- **用户与远程接入**：默认用户 `ubuntu`（免密 sudo）；`start`（`sudo -n -E bash .cursor/start.sh`）每次 Agent Run 以 kernel 模式拉起 Tailscale 与 sshd，主机名 `cursor-agent-for-luckfox-pico-lvgl-example-<bcId 前 8 位>`，需要用户级 Secrets `TAILSCALE_AUTHKEY`、`SSH_AUTHORIZED_KEYS`（缺失时 `start` 失败，Agent 仍可编码，只是无远程接入）。
- **核对镜像内容**：要对锁定 digest 做 `dpkg-query` 时，按 Cursor 文档在当前 VM 临时安装 Docker，禁止写入 Dockerfile；遇 `/etc/fuse.conf` 的 conffile 提问用 `dpkg --force-confold --configure -a` 保留现有文件。
- **CI**：CI 镜像标签取 `.cursor/Dockerfile` 内容哈希，修改它的 PR 在合并前 `build-image` 必然失败，详见 `.github/AGENTS.md`。新增 apt 包时两份 Dockerfile 一并考虑。
