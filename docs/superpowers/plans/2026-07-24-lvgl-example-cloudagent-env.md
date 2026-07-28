# Luckfox Pico LVGL Example — Cloud Agent Dockerfile 环境 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为本仓新增一套 Dockerfile 形式的 Cursor Cloud Agent 环境（`.cursor/environment.json` + 自建 `.cursor/Dockerfile` + 官方镜像备选 `.cursor/Dockerfile.luckfox_pico`），新增中文 `AGENTS.md`，把 PR #1 改造为与参考 PR 同构的中文正文（含重做的完整桌面演示），并新增 superpowers spec/plan，全部提交进现有 PR #1。

**Architecture:** 双 Dockerfile——默认自建 `ubuntu:24.04`、备选官方 `luckfoxtech/luckfox_pico:1.0`，均 apt 装齐「交叉编译 + native 渲染验证」依赖；`environment.json` 以 `build.dockerfile` 模式默认引用自建版；用真实 `docker build` + 容器内交叉编译（bind-mount 注入源码）+ VM 侧 native 桌面渲染做代理证明；产物分 3 个 commit 提交进现有分支。

**Tech Stack:** Docker、Ubuntu 24.04 / 22.04、`arm-linux-gnueabihf` 交叉工具链、CMake、LVGL 8.3 / lv_drivers 8.1（vendored）、`libdrm-dev`/`libcjson-dev`/`libsdl2-dev`、ffmpeg x11grab。

## Global Constraints

设计细节一律以 spec 为准（[`../specs/2026-07-24-lvgl-example-cloudagent-env-design.md`](../specs/2026-07-24-lvgl-example-cloudagent-env-design.md)）：依赖清单见 spec §5.1、`environment.json` 见 §5.2、官方与经验约束见 §5.3、官方镜像备选见 §5.4、诚实红线见 §5.1 与 §10、`AGENTS.md` 要求见 FR3 与 Q6。下列为执行期的硬约束：

- **分支纪律**：仅在 `cursor/setup-dev-environment-6a48` 上提交；**绝不** `reset --hard dev` + 强推、不 force-push 成 `dev`、不删除远端分支（防 PR #1 被自动关闭）。重整时仅对本 feature 分支 `--force-with-lease`。
- **digest 绝不编造**：两个基础镜像的 digest 均须 `docker buildx imagetools inspect` 实测取值后再 pin。
- **grilling 不进 git**：commit 前把 `.git/info/exclude` 的 `.cursor/` 收窄为目录锚定 `.cursor/skills/`，放行 `.cursor/*` 环境配置。
- **3 提交**：提交1 `AGENTS.md` → 提交2 `.cursor/*` → 提交3 spec + plan（最后一次）。Author 保留原始时间、Committer 更新为当前。message 内容见 `git log`、写法口径见 spec Q10（**此处不再内嵌副本，避免与实际提交漂移**）。下文各 Task 按**执行序**称 commit A/B/C：A（`.cursor/*`）＝提交2、B（`AGENTS.md`）＝提交1、C（spec+plan）＝提交3。注：提交1 的 Author 时间（07-23）继承自分支上最初那次英文 `AGENTS.md` 提交，早于提交2 的 07-24，故按 Author 时间看前两者与执行序对调；这是「Author 保留原始时间」的既定纪律所致。
- **验证**：应尽力真实测试；受限则据实记录代理证明与受限原因。`docker build` + 容器内编译只是代理证明，端到端「Cloud Agent 从该 `.cursor` 启动」需另起新 agent 验证。
- **Dashboard**：按官方 resolution order，提交 repo 级 `environment.json` 后即优先生效，**本会话不涉及任何 Dashboard 操作**，仅作提示记录。

---

### Task 1: 收窄 `.git/info/exclude`（前置，放行 `.cursor` 环境配置进 git）

**Files:** Modify `/workspace/.git/info/exclude`（git 本地私有文件，不被跟踪，本 task 无 commit）

- [x] **Step 1: 把 `.cursor/` 收窄为目录锚定 `.cursor/skills/`**

只替换规则行、保留原注释（`# Cloud agent skills installed locally under .cursor/; must NOT be committed to git.`），结果为该注释 + `.cursor/skills/` 一行。

- [x] **Step 2: 验证追踪状态**

```bash
cd /workspace
git check-ignore -v .cursor/skills/grilling/SKILL.md   # exit=0，仍被忽略
git check-ignore -v .cursor/environment.json .cursor/Dockerfile .cursor/Dockerfile.luckfox_pico   # exit=1，可追踪
```

---

### Task 2: 在本 VM 安装 Docker（真实测试所需）

**Files:** 无（仅环境安装，不进 git）

- [x] **Step 0: 幂等检查**

`command -v docker && sudo docker info >/dev/null 2>&1` —— 若 daemon 已可用则跳过 Step 1–2，且**不要**盲目覆写正在运行 daemon 的 `/etc/docker/daemon.json`；否则执行 Step 1–2。

- [x] **Step 1: 安装 docker-ce 及插件**

依据官方《Cloud 环境设置》的「运行 Docker」一节（见 spec §12）：加 `download.docker.com` 的 apt keyring 与源，装 `docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin`。

- [x] **Step 2: 为嵌套容器配置 fuse-overlayfs + iptables-legacy**

```bash
sudo apt-get install -y fuse-overlayfs iptables
printf '%s\n' '{' '  "storage-driver": "fuse-overlayfs"' '}' | sudo tee /etc/docker/daemon.json
sudo update-alternatives --set iptables /usr/sbin/iptables-legacy || true
sudo service docker start || (sudo dockerd > /tmp/dockerd.log 2>&1 &)   # 括号必需：否则 & 会作用于整个 || 列表
```

- [x] **Step 3: 冒烟验证**

`sudo docker run --rm hello-world`。若嵌套容器导致失败，记录受限原因并转 Task 4 的兜底路径（VM 直接验证依赖清单）。

---

### Task 3: 编写两个 Dockerfile + `.cursor/environment.json`

**Files:** Create `.cursor/Dockerfile`、`.cursor/Dockerfile.luckfox_pico`、`.cursor/environment.json`

- [x] **Step 1: 取两个基础镜像的真实 digest**

`docker buildx imagetools inspect <image>` 取 index digest 填入 `FROM`。实测：ubuntu `sha256:4fbb8e6a…`（24.04.4 LTS）、官方镜像 `sha256:915d4458…`（单 amd64 manifest），两者均与参考项目对应 Dockerfile 的 pin 值一致。

- [x] **Step 2: 写两个 Dockerfile**

完整内容以仓库实际文件为准（此处不复制，避免副本漂移）；结构与依赖分类见 spec §5.1、§5.4 与 NFR2，诚实红线见 spec §5.1。要点差异：自建版设 `ENV TZ=Asia/Shanghai` 且 apt 含 `git`；备选版不设 `ENV TZ`（官方镜像已配时区）、`git` 不列入 apt（镜像自带）。

- [x] **Step 3: 写 `.cursor/environment.json`**

即 spec §5.2 的 `build` 块。

- [x] **Step 4: 轻量 schema 校验**

`curl -fsSL https://cursor.com/schemas/environment.schema.json`，人工核对 `build.dockerfile`（required）与 `build.context` 字段合法、无未知字段。

- [x] **Step 5: 确认可追踪**

`git status --porcelain .cursor/` 应显示三个 `??`（未被 exclude 忽略），暂不 commit。

---

### Task 4: 两镜像真实构建/编译 + native 渲染验证 → commit A（`.cursor/*`）

**Files:** Verify + Commit `.cursor/environment.json`、`.cursor/Dockerfile`、`.cursor/Dockerfile.luckfox_pico`

- [x] **Step 1: `docker build` 两个 Dockerfile**

```bash
sudo docker build -f .cursor/Dockerfile -t luckfox-lvgl-env:local .cursor
sudo docker build -f .cursor/Dockerfile.luckfox_pico -t luckfox-lvgl-env:luckfox .cursor
```
注：真实 Cloud Agent 按 `environment.json` 用 `context:".."`；此处传 `.cursor` 仅为缩小上下文加速——因两个 Dockerfile 都不 `COPY` 源码，产物等价。

- [x] **Step 2: 两镜像容器内交叉编译（bind-mount 注入源码，各跑一次）**

```bash
sudo docker run --rm -v /workspace:/workspace -w /workspace luckfox-lvgl-env:local bash -lc '
  export GLIBC_COMPILER=/usr/bin/arm-linux-gnueabihf-
  rm -rf build && mkdir -p build && cd build && cmake .. && make -j && make install
  readelf -h /workspace/install/luckfox_lvgl_demo/luckfox_lvgl_demo | grep -E "Class|Machine"'
```
Expected: `Class: ELF32` + `Machine: ARM`。产物固定在源码根 `/workspace/install/…`（`CMAKE_INSTALL_PREFIX` 所定），故用绝对路径。自建镜像未装 `file`（官方镜像自带），故统一用 `readelf`。编后如属主被改，在宿主 `sudo chown -R "$(id -u):$(id -g)" build install` 复原（构建只写 gitignore 的 `build/`、`install/`）。

- [x] **Step 3: native 渲染验证（复现 Main/GIF 双屏）**

由本会话自建临时 harness，配方见 spec §6.1；harness 非交付物、不入 git。实现要点：出图用 header-only `stb_image_write.h` 直接写 PNG（便于 PR 内联，避免 PPM 无法显示）；stb 头、harness 源码与产物均落在挂载区 `/workspace/build/`（gitignore），以便容器 `--rm` 后仍持久化到宿主。若 docker 受限则改在 VM 直接跑（VM 已具备全部依赖）并如实记录。

- [x] **Step 4: 记录验证证据**

把 digest、build 结果、`readelf` 输出、构建结论、渲染产物路径与实测日期记入本文末「验证证据回填」（最终汇总在 Task 7 Step 1 统一完成）。

- [x] **Step 5: commit A + push**

`git add .cursor/` 后提交；先 `git status --porcelain` 确认不含 `.cursor/skills/`。

---

### Task 5: 新增中文 `AGENTS.md` → commit B

**Files:** Create `/workspace/AGENTS.md`（相对 base `dev` 为新增；执行当时分支上已有英文稿，故实际操作是改写）

- [x] **Step 1: 通读现有英文稿，识别待改写小节**

`## Cursor Cloud specific instructions` 及其下 `What this project is / Build / Lint / Tests / Running`。

- [x] **Step 2: 中文化并按 spec FR3 / Q6 调整**

格式对齐参考（`# AGENTS` + `## 交互约定` + 英文锚点小节 + 中文 `###` 正文）；工具链叙述统一为「由 `.cursor/Dockerfile` 提供」；保留构建命令、`return()` gotcha、硬件运行注意等核心事实。lint 小节与 DRM 退出条件**按实测事实撰写**（具体要求见 spec FR3 ②③）。native 渲染段只留 gotcha、配方移入 spec §6.1（判据见 spec Q12）；`AGENTS.md` 作为长期文档不写 run 级指代。另新增 **Cloud Agent 环境**小节（活动/备选 Dockerfile 与切换方式）。

- [x] **Step 3: 校对**

确认代码块、`$GLIBC_COMPILER`、`/dev/dri/card0` 等未被翻译破坏，与两个 Dockerfile 无矛盾。

- [x] **Step 4: commit B + push**

---

### Task 6: 重写 PR #1 标题与中文正文

**Files:** 无（用 ManagePullRequest 更新 PR，不改代码）

- [x] **Step 1: 演示重做**

演示为完整桌面录制（`luckfox_desktop_*`）；方式与产物见 spec §7、根因见 spec Q8。引用方式：mp4 与 Main 截图引 owner 上传的 GitHub 附件，狐狸动图因上传会被转码成静态 PNG 而保留 run artifact（见 spec §7、Q11）。

- [x] **Step 2: 组织中文正文**

章节形态见 spec §8。

- [x] **Step 3: 更新 PR（标题 + 正文）**

用 ManagePullRequest `update_pr`；**不** reset/force-push、**不**改 base、**不**关闭 PR。

---

### Task 7: 回填验证证据 → commit C（spec + plan）→ 最终确认

**Files:** Modify 本 plan（回填证据 + 勾选 checkbox）；Commit spec + plan

- [x] **Step 1: 汇总「验证证据回填」并勾选 checkbox**

- [x] **Step 2: commit C（最后一次）+ push**

- [x] **Step 3: 最终确认**

```bash
git diff --name-only origin/dev...HEAD   # 应恰为 6 文件
git status --porcelain                   # 确认 grilling 未被追踪
gh pr view 1 --json state,files          # PR open、文件集正确
```

---

## 验证证据回填

实测时间线：2026-07-24 首测（自建版）/ 07-25 双镜像 / 07-26 复验 / 07-28 对当前提交版复测。

- **基础镜像 digest**：ubuntu `sha256:4fbb8e6a8395de5a7550b33509421a2bafbc0aab6c06ba2cef9ebffbc7092d90`（24.04.4 LTS）；官方镜像 `sha256:915d44588085826cbeda4b969dbbe7d5e54bf779ba36cda3c5072ee9533e0417`（与参考 pin 一致，单 amd64 manifest）。
- **两镜像 `docker build` 均成功** → `luckfox-lvgl-env:local` / `:luckfox`。对当前提交版的复测（07-28）：`docker build` 为**全 layer 缓存命中**（即该 recipe 的 layer 已存在，非当天重新解析下载 apt 包），容器内交叉编译为**实跑**。
- **两镜像容器内交叉编译均成功**：`readelf -h` 均为 `Class: ELF32`、`Machine: ARM`。产物体积两镜像**不同**（gcc 大版本不同所致）：自建 `local` = 3,166,112 字节、备选 `luckfox` = 3,165,940 字节。
- **依赖版本实测**：
  - 自建 `local`（Ubuntu 24.04.4）：gcc-arm-linux-gnueabihf `4:13.2.0-7ubuntu1`（编译器 `--version` 口径 13.3.0）、cmake `3.28.3-1build7`、build-essential `12.10ubuntu1`、libdrm-dev `2.4.125-1ubuntu0.1~24.04.2`、libcjson-dev `1.7.17-1`、libsdl2-dev `2.30.0+dfsg-1ubuntu3.1`。
  - 备选 `luckfox`（Ubuntu 22.04.3）：gcc-arm-linux-gnueabihf `4:11.2.0-1ubuntu1`（编译器 11.4.0）、cmake `3.22.1-1ubuntu1.22.04.2`、build-essential `12.9ubuntu3`、libdrm-dev `2.4.113-2~ubuntu0.22.04.1`、libcjson-dev `1.7.15-1ubuntu0.1`、libsdl2-dev `2.0.20+dfsg-2ubuntu1.22.04.1`。
- **官方镜像原始预装实测**（`docker run --rm luckfoxtech/luckfox_pico:1.0 dpkg-query`，07-28）：已装 `git 1:2.34.1-1ubuntu1.10`、`cmake 3.22.1-1ubuntu1.22.04.1`、`ca-certificates`、`pkg-config 0.29.2`、`file`、`tzdata 2023c`；未装 `sudo`，未装 `build-essential` 元包（其依赖中仅缺 `dpkg-dev`，gcc/g++/make/libc6-dev/binutils 齐全）。时区已写入镜像（`/etc/localtime` 与 `/etc/timezone` 均 `Asia/Hong_Kong`），故该 Dockerfile 刻意不设 `ENV TZ`。
- **构建结论**：交叉编译成功、无错误（仅少量既有告警）。官方 SDK 镜像上也能 apt 装齐本仓所需 glibc 工具链 + native/SDL 库，`Dockerfile.luckfox_pico` 可用；但其预装 SDK 依赖对本仓无实质收益（见 spec §5.4）。
- **native 渲染**：harness 复现 Main/GIF 双屏（480×480，真实点击 GIF 按钮 `LV_SCR_LOAD_ANIM_FADE_ON` 切屏）；桌面录制（SDL + ffmpeg x11grab）在 **VM 侧** `DISPLAY=:1` 完成（docker 容器内无 X 桌面，故镜像侧只验证了 `libsdl2-dev` 可安装，见 spec §6）。
- **演示产物实测**（`ffprobe -count_frames` / `sha256sum`）：`luckfox_desktop_demo.mp4` = 6.52s / 25fps / 163 帧（`17c14b9f…`）；`luckfox_desktop_fox.gif` = 24 帧 / 2.0s（`r_frame_rate=12/1`，`avg_frame_rate` 元数据报 12.5）（`6fe289fd…`；源 LVGL animimg 为 20 帧，ffmpeg 抽帧后 24）；`luckfox_desktop_main.png` = 1280×800（`a00132a3…`）。07-28 另比对了 owner 上传的三个 GitHub 附件：mp4 与本地 **sha256 完全相同**（GitHub 原样存储未转码）、Main 截图内容一致但已被重编码（891,416 字节 PNG，故哈希不同）、**狐狸动图被转码为静态 PNG**（960×600、1 帧，动画丢失），故正文仅前两者改引附件。
- **平台自动装包清单**：Aptfile `sha256=819987b7…`，07-26 复验未变、逐项一致（google-chrome 不在 Aptfile，由平台单独脚本安装）。
- **局限**：以上均为「配置可构建、依赖可用」的代理证明；端到端「Cloud Agent 从该 `.cursor` 启动」需另起新 agent 验证（本会话未做）。

## 执行结果

- 核心决策见 spec §4 决策表（D1–D7）与 §11 QA。
- 分 3 提交合入现有 PR #1（`docs(agents)` → `chore(cloud-env)` → `docs(superpowers)`）；验证见上方「验证证据回填」，演示见 PR 正文；提交纪律与 grilling 排除见本文「Global Constraints」。
