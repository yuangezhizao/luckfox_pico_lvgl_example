# Luckfox Pico LVGL Example — Cloud Agent 环境（Dockerfile 配置即代码）设计规格

- **日期**：2026-07-24
- **分支**：`cursor/setup-dev-environment-6a48`（本仓库 PR #1 的 head 分支）
- **关联 PR**：[`yuangezhizao/luckfox_pico_lvgl_example#1`](https://github.com/yuangezhizao/luckfox_pico_lvgl_example/pull/1)
- **参考 PR**：[`yuangezhizao/luckfox-pico#1`](https://github.com/yuangezhizao/luckfox-pico/pull/1)（已 merged 的 Cloud Agent「Dockerfile 配置即代码」PR，本设计对齐其形态与格式，内容则全部填充本仓库的真实工作）
- **关联计划**：[`docs/superpowers/plans/2026-07-24-lvgl-example-cloudagent-env.md`](../plans/2026-07-24-lvgl-example-cloudagent-env.md)

## 1. 概述与目标

为本仓新增 **Dockerfile 形式的 Cursor Cloud Agent 环境**（`.cursor/environment.json` + `.cursor/Dockerfile` + 官方镜像备选 `.cursor/Dockerfile.luckfox_pico`），把开发环境「配置即代码」，不依赖任何人的个人快照，让任何人从本分支起 Cloud Agent 都能得到可复现的「交叉编译 + native 渲染验证」环境。同时新增中文 `AGENTS.md`、把 PR #1 改造为与参考 PR 同构的中文结构化形态、走完整 superpowers 流程新增 spec/plan，并把 GUI 演示重做为完整桌面 native 渲染录制（见 §7、Q8）。

核心原则是「**同构而非逐字复制**」：结构与格式对齐参考 PR，但每一处内容都如实描述本仓真实做过的工作，绝不搬运参考 PR 中本仓未发生的事（buildroot 编固件、两块板实测等），也不照搬参考 Dockerfile 顶部注释里对本仓为假的技术陈述（见 §5.1 诚实红线）。

## 2. 背景与现状

本仓是 GUI-Guider + LVGL 8.3 + lv_drivers 8.1 的嵌入式 GUI 应用，CMake 交叉编译为 32-bit ARM，目标板 Luckfox Pico Ultra，依赖均 vendored 于 `lib/`。改造前 PR #1（标题 `docs: add AGENTS.md with Cursor Cloud dev environment setup`）净改动只有一个英文 `AGENTS.md`，环境依赖 VM snapshot 预装的工具链、并非配置即代码。参考 PR 针对的是 Luckfox 官方 SDK（buildroot 编整套固件），依赖清单与验证方式均不适用于本仓，只借鉴其结构与格式。

## 3. 需求

功能性需求：
- FR1：提供 `.cursor/Dockerfile`，从裸 `ubuntu:24.04` 自装「交叉编译 + native 渲染验证」全部依赖。
- FR2：提供 `.cursor/environment.json`，以 `build.dockerfile` 模式引用该 Dockerfile。
- FR2b：提供**备选** `.cursor/Dockerfile.luckfox_pico`（官方镜像 `luckfoxtech/luckfox_pico:1.0`），依赖能力与自建版对齐；默认不启用，切换需手动改 `build.dockerfile`（见 §5.4、Q7）。
- FR3：**新增**中文 `AGENTS.md`（相对 base `dev` 为新增），格式对齐参考 PR（见 Q6）。内容须满足：①依赖/工具链叙述与 Dockerfile 配置即代码自洽——工具链与 native 验证依赖一律叙述为「由 `.cursor/Dockerfile` 安装」，不得出现依赖 VM snapshot 的说法；②lint 小节如实写明本仓**没有生效的告警关卡**——`CMakeLists.txt` 两处 `add_compile_options()` 位于 `add_executable()` 之后，`-Wall -Wextra` 等标志（连同 `-O3`/`-fPIC`/`-std=c99`）全部未进编译命令行；③运行小节须分清两层失败：本 VM 上产物是 32 位 ARM ELF、无 binfmt_misc/qemu，直接执行即 `Exec format error`、根本进不到 `main()`；而在架构可执行的环境里，DRM 退出点为 open `/dev/dri/card0` 失败、`drmModeGetResources()` 失败、首个 mode 非正方形三者——不得笼统写成「未检测到显示器就退出」，也不得断言本 VM 会停在 DRM 检查点；④native 渲染段只留 gotcha、操作配方移入 §6.1（见 Q12）。
- FR4：新增 superpowers 产物（本 spec + plan）并作为 PR diff 一部分提交。
- FR5：把 PR #1 标题与正文改写为与参考 PR 同构的中文结构化形态，演示重做为完整桌面录制（见 §7、Q8）。

非功能性需求：
- NFR1：Dockerfile 依赖清单应尽力通过真实测试（`docker build` + 容器内交叉编译 + native 渲染）；环境受限时据实记录代理证明与受限原因。
- NFR2：格式对齐参考 `.cursor/Dockerfile`（顶部说明注释块、FROM tag+digest 双锁定、`ARG DEBIAN_FRONTEND` + `ENV TZ`、平台自动装包 ASCII 框、带详解注释的 apt、结尾 `git safe.directory`）。
- NFR3：不造假、不越权、如实记录；grilling 技能文件不进 git。
- NFR4：绝不 reset/force-push 远端分支到 `dev`、不删分支，避免 PR #1 被自动关闭。

## 4. 设计决策（含理由）

| 决策 | 结论 | 理由 |
|---|---|---|
| D1 范围 | 甲：同构套用（结构/流程产物对齐参考 PR，内容为本仓真实工作） | 聚焦 PR 呈现层 + 真实新工作，既对齐参考又不造假 |
| D2 PR diff | 新增并提交 spec/plan + 真正新增 `.cursor/` 环境配置 | 「内容对齐」的必然结果；且用户明确要为本仓新增 Dockerfile 形式 Cloud Agent 环境 |
| D3 支撑范围 | ii：交叉编译 + native 渲染验证（硬需 `libdrm-dev`/`libcjson-dev`，`libsdl2-dev` 供 SDL 桌面渲染，`pkg-config` 为可选便利） | 让新环境能复现完整桌面演示，验证最有说服力 |
| D4 基础镜像 | 双 Dockerfile：**默认**自建 `ubuntu:24.04` + **备选**官方 `luckfoxtech/luckfox_pico:1.0`，均 tag+digest 双锁定 | 自建版贴合默认 Cloud Agent 且已实测；官方镜像备选与参考项目 `.cursor/` 对齐并提供可切换选项（见 §5.4、Q7） |
| D5 用户/权限 | 不写 `USER`/`useradd`/sudoers NOPASSWD，仅 apt 装 `sudo` + `git safe.directory` | 参考 Dockerfile（已 merged、生产可用）也未写这些，非必需，Cursor 平台自行处理运行用户；本仓为全新文件，故这三行从一开始就不写入（非「移除」） |
| D6 environment.json | 仅 `build` 块（`dockerfile` + `context`），省略 install/start/terminals | 依赖已在镜像内、无常驻服务；与参考逐字一致 |
| D7 注入复盘评论 | 默认不设 + 据实兜底 | 本仓若未遇提示注入则不造；若真遇到则如实写完整复盘 |

## 5. 环境配置设计

### 5.1 `.cursor/Dockerfile`（默认，自建 `ubuntu:24.04`）

基础镜像 tag + digest 双锁定（digest 用 `docker buildx imagetools inspect` 取 index digest 实测填入、绝不编造）。依赖清单从裸系统自装；base digest 锁定 + 清单固化，但 apt 包版本仍随上游漂移、非严格字节级可复现。

| 类别 | 安装包 | 用途 |
|---|---|---|
| 基础工具 | `git` `sudo` `ca-certificates` | `git` 构建期硬需（结尾的 `git config` 依赖它；备选版由官方镜像自带、不列入 apt）/ `sudo` 提权 / `ca-certificates` 运行期 https 保底（Ubuntu apt 源走 http，apt 本身并不需要它） |
| ARM 交叉工具链 | `gcc-arm-linux-gnueabihf` `g++-arm-linux-gnueabihf` | 编 32-bit ARM 目标（CMakeLists 走 `$GLIBC_COMPILER` 前缀） |
| 构建系统 | `cmake` `build-essential` | CMake 构建；`build-essential` 同时提供 native x86 `gcc/g++/make`（harness 需要） |
| native 渲染（硬需） | `libdrm-dev` `libcjson-dev` | x86 版；`custom/*.c` 源码调用 DRM/cJSON API，链接期需其符号，但运行时不触达该路径、无需真实 DRM 设备 |
| 桌面渲染 | `libsdl2-dev` | SDL2 开窗渲染所需依赖（两个 Dockerfile 均含；验证范围见 §6） |
| 便利工具（可选） | `pkg-config` | 本仓实际构建路径未用到（根 `CMakeLists.txt` 无 `find_package`/`pkg_check_modules`；vendored 的 `lib/lv_drivers/wayland/CMakeLists.txt` 虽用了，但其父级从不 `add_subdirectory` 该目录），非硬需 |

**⚠️ 顶部注释须适配本仓、勿照搬参考的反转陈述（诚实红线）**：①本仓 ARM 交叉工具链**由本 Dockerfile apt 安装**，与参考「工具链随 SDK 仓库内置、无需在镜像内安装」**相反**；②本仓**无 `build.sh`**、构建走 `cmake`，不得出现参考的「`build.sh` 选板 / 加入 PATH」话术；③参考的 buildroot 等 SDK 专属话术不照搬。只复用参考的格式骨架；「平台自动装包 ASCII 框」按本仓 Aptfile 整理（其 sha256 与参考相同，非逐字节复制）。

### 5.2 `.cursor/environment.json`

```json
{
  "build": {
    "dockerfile": "Dockerfile",
    "context": ".."
  }
}
```

两个路径均相对 `environment.json` 所在目录（即 `.cursor`）解析；`".."` 按官方「Important path behavior」与 `.`、`./` 一道被特殊处理为仓库根。与参考逐字一致。注：`context:".."` 会把整个仓库根作为构建上下文（无 `.dockerignore`，实测约 81 MB）；因两个 Dockerfile 都不 `COPY` 源码，故无正确性问题，只是上下文偏大。保留 `".."` 是为与参考对齐（D6）；若要最小化上下文，官方规则是省略 `context` 即默认 `.cursor`，本仓有意不改。

### 5.3 约束要点

- **不要 `COPY` 项目源码**（官方明文要求；Cursor 自行 checkout 正确 commit）。
- 镜像自带 `git` 与 `sudo` 是**经验做法、非官方明文要求**——官方 setup 文档的 Dockerfile 一节只要求不 `COPY` 源码，未提 git/sudo；本仓沿用参考项目做法保底装齐。
- 结尾 `git config --system --add safe.directory '*'`：`'*'` 全局关闭仓库所有权检查（对齐参考、隔离容器内风险低）；未收窄到 `/workspace` 因 Cursor 实际 checkout 路径不固定——已知安全权衡。
- 按官方 resolution order（repo `.cursor/environment.json` → personal → team saved environment），本 PR 提交了 repo 级配置即命中第一档、优先生效，故**通常无需任何 Dashboard 操作**，也无需走「Set up agent」向导（官方将 agent-driven setup 标为 recommended、手工 Dockerfile 为 advanced）。
- schema：`https://cursor.com/schemas/environment.schema.json`（`www.` 版 308 重定向到此）。

### 5.4 `.cursor/Dockerfile.luckfox_pico`（官方镜像备选）

形式对齐参考项目同名文件，依赖适配本仓。基础镜像 `luckfoxtech/luckfox_pico:1.0`（tag+digest 双锁定，digest 与参考一致；Docker Hub 该仓库仅此一个 tag、2023-11-11 后未更新），基于 Ubuntu 22.04、已预装 SDK 的 host 依赖。

**⚠️ 关键取舍（诚实红线）**：官方镜像面向 **SDK**，预装的是编译 SDK 所需的 host 依赖；SDK 的 uClibc 工具链 `arm-rockchip830` 随 SDK 仓库内置，本镜像与本仓中均无。本仓用的是 glibc 工具链 `gcc-arm-linux-gnueabihf` + native 渲染库，官方镜像未预装，故仍需 apt 补装——真差额为交叉工具链、`libdrm-dev`/`libcjson-dev`/`libsdl2-dev`、`sudo` 与 `build-essential` 元包；而 `cmake`/`ca-certificates`/`pkg-config` 已预装、仍列入 apt 只为保证存在性并与自建版口径一致（apt 会顺带升级它们，并非无操作），`git` 则完全不列、直接用镜像自带。因此**本备选相对自建版无实质构建收益**，仅为「与参考对齐 + 提供可切换选项」而备。格式上保留与自建版逐字一致的 ASCII 框，但**不设 `ENV TZ`**（官方镜像已把时区写进镜像本身）。

## 6. 构建与验证策略

- 真实测试：按官方《Cloud 环境设置》的「运行 Docker」一节（`docker-ce` + `fuse-overlayfs` + `iptables-legacy`，见 §12）在本 VM 装 docker，对两个 Dockerfile 各做 `docker build` + 容器内交叉编译（bind-mount 注入工作区，`readelf` 确认 ELF32/ARM）。因不 `COPY` 源码，容器内编译必须走 bind-mount；注意编后按需 `sudo chown` 复原属主。
- **验证范围须如实界定**：`libsdl2-dev` 在容器内**只验证了能装上**；SDL 开窗与 x11grab 录屏是在 **VM 侧** `DISPLAY=:1` 完成的（docker 容器内没有 X 桌面）。故「把 SDL 桌面渲染能力固化进镜像」意为「保证依赖齐备」，而非「已在镜像内跑通开窗」，PR 正文与 plan 均须照此措辞。
- **局限声明**：`docker build` + 容器内编译只是「配置可构建、依赖可用」的**代理证明**；「Cloud Agent 真能从该 `.cursor` 端到端启动」需另起新 agent 验证，本会话未做、不过度承诺。
- 若 docker 在嵌套容器中受限，以「VM 直接验证依赖清单 + 记录受限原因」兜底并如实说明。验证证据（产物、构建结论、渲染截图、实测日期）在 plan 中回填。
- native 渲染 harness 由本会话参照 LVGL 移植方式自建，**非交付物、不入 git**；配方见 §6.1。

### 6.1 native 渲染验证配方（自 `AGENTS.md` 移入，见 Q12）

无硬件时在 x86 本机渲染 GUI-Guider 屏幕的完整做法。LVGL 本身可移植（`LV_COLOR_DEPTH` 为 32 / ARGB8888），难点只在补齐 `src/main.c` 提供的运行环境：

- **编译单元**：`lib/lvgl/src/**/*.c` + `generated/**/*.c` + `custom/*.c`，外加自写 harness（`**` 需先 `shopt -s globstar`；`lib/lvgl/src` 下无顶层 `.c`，故递归必需）。
- **harness 不可省略**：上述源文件没有 `main()`，且 `custom/*.c` 与 `generated/*.c` 合计 `extern` 引用了 `guider_ui`/`SCALE`/`QUIT_FLAG`/`WIFI_ENABLE`/`MUSIC_ENABLE` 这 5 个仅在 `src/main.c` 定义的全局量（`QUIT_FLAG` 只被 `custom/custom_main.c` 引用），harness 须自带 `main()` 并定义这 5 个（`SCALE=1.0` 对应 480×480），否则链接期必报 undefined reference。直接改编 `src/main.c` 不可行——它会拉入 `lv_drivers` 的 DRM/fbdev/evdev 硬件驱动。
- **显示后端二选一**：headless（全屏内存缓冲区 + `flush_cb` + 导出图片），或 SDL 开窗。走 `lv_drivers/sdl` 需把 `lib/lv_drv_conf.h` 的 `USE_SDL` 从默认 0 置 1（该宏由 `#ifndef` 包裹，故 `-DUSE_SDL=1` 亦可）；也可绕开 lv_drivers、直接调用 SDL2 自实现 `flush_cb`——本仓 PR #1 的演示即用后者。两条 SDL 路径都需 `libsdl2-dev`，并可配 `DISPLAY` + `ffmpeg x11grab` 录屏。
- **链接期依赖**：`custom/*.c` 用到 DRM/cJSON 符号，harness 须链系统 x86 版（`-ldrm -lcjson`，另加 `-lpthread -lm`）；但 native 渲染运行时并不触达该硬件路径，无需真实 DRM 设备。
- **include 路径**：以 `CMakeLists.txt` 全局 `include_directories()`（glibc 分支 13 条）为基准，另需手工补 `lib/lvgl/src`——该路径由 `target_include_directories` 提供、不在全局列表内。
- **导航**：注册 pointer indev、脚本化 press/release 即可驱动切屏（如 Main → GIF 按钮）。

## 7. 演示方案（完整桌面 native 渲染录制）

- **方式**：由本会话临时 harness **直接调用 SDL2 自实现显示/输入驱动**（未使用 `lv_drivers/sdl`），在 XFCE 桌面（`DISPLAY=:1`）开窗运行 GUI，用 `ffmpeg x11grab` 实时录制整个桌面；tick 由 `custom_tick_get()` 驱动，动画与切屏按真实时间播放。
- **产物**：`luckfox_desktop_demo.mp4`（Main → 点击 GIF 按钮 → `LV_SCR_LOAD_ANIM_FADE_ON` 切屏 → 狐狸动画，6.5s / 163 帧）、`luckfox_desktop_fox.gif`（24 帧 / 960×600）、`luckfox_desktop_main.png`（1280×800）。
- **PR 正文引用方式**：mp4 与 Main 截图改引 owner 上传的 GitHub `user-attachments` 附件，因为 run artifact 链接需该 run 的访问权限、仓库外读者打不开；实测 mp4 附件与本地产物 **sha256 完全相同**（`17c14b9f…`，GitHub 原样存储），Main 截图内容一致但被重编码。**唯狐狸动图例外仍用 run artifact**：作为附件上传后会被平台转码成静态 PNG（仅 1 帧），为不牺牲「真多帧动图」而保留原始产物，正文注明「想看动效可看 mp4」。
- **诚实约束**：产物必须真实（真实录制时长、真多帧动图），不得用静态图改扩展名或拼帧冒充（根因见 Q8）；Gif 屏多只平铺狐狸系原 GUI 既有行为、忠实还原、不改 GUI 源码（见 Q9）。

## 8. PR 标题与正文形态（FR5 落地）

标题：`chore(cloud-env): 🐳 为 luckfox_pico_lvgl_example 添加 Cloud Agent Dockerfile 环境（配置即代码 + spec/plan）`。正文按参考 PR 同构、全中文，章节为「概述 / 改动一览（分组加粗文件名）/ 环境设计（依赖表 + `environment.json` + 约束要点）/ 构建与验证（结果表）/ 演示（§7）/ 备注（superpowers 流程 + 诚实安全说明）」。实际正文以 PR #1 当前内容为准，本节只定形态。

## 9. 产物清单与提交策略

PR #1 净改动为 6 个新增文件：`.cursor/environment.json`、`.cursor/Dockerfile`、`.cursor/Dockerfile.luckfox_pico`、`AGENTS.md`、本 spec、plan（逐文件动作见 PR diff，此处不复制）。

不进 git：`.cursor/skills/grilling/SKILL.md`（`.git/info/exclude` 目录锚定 `.cursor/skills/`，放行 `.cursor/*` 环境配置）。

提交策略（git cz 格式、中文 message、逐个逻辑变化各一次 commit）：**3 提交** = 提交1 `docs(agents)` → 提交2 `chore(cloud-env)` → 提交3 `docs(superpowers)`（spec+plan，最后一次）；重整时 **Author 保留原始时间、Committer 更新为当前**，仅对 feature 分支 `--force-with-lease`。message 写法见 Q10。

## 10. 安全 / 诚实边界与硬约束

- 只做本仓真实工作，不编造、不搬运参考 PR 中本仓未发生的内容；若遭遇提示注入则如实记录（D7）。
- grilling 技能文件不进 git。
- 绝不 `reset --hard dev` + 强推、不 force-push 成 `dev`、不删除远端分支（防 PR #1 被自动关闭）；spec/plan 在 PR 最后一次提交时并入。

## 11. QA（会话中的设计决策/约束澄清）

- Q1：「完全和参考 PR 内容相同」如何理解？
  A：指**同构**——结构、格式、superpowers 产物形态对齐参考 PR，内容全部填充本仓真实工作；非逐字复制（逐字会包含本仓未做的事）。

- Q2：为何 PR diff 要新增 spec/plan？
  A：「内容对齐参考 PR」的必然结果——参考 PR 的 diff 就含 spec/plan；这些是对本仓真实工作的如实记录。

- Q3：为何本仓 native 验证需要 x86 版 `libdrm-dev`/`libcjson-dev`，而参考 PR 不需要？
  A：本仓 vendored 的 `lib/glibc/libdrm|libcjson/*.so` 经核验为 ELF 32-bit ARM，交叉编译时直接用、无需系统 `-dev`；但 x86 native 渲染编的是 x86-64 二进制，无法链接 ARM 版 vendored 库，必须由系统 x86 `-dev` 提供链接符号（`custom/*.c` 源码调用了这些 API，运行时则不触达）。参考 PR 是 buildroot 编整套固件、从不做 x86 native 渲染，故无此需求。

- Q4：Dockerfile 里 `USER ubuntu` / sudoers NOPASSWD 是否必需？
  A：非必需，见 D5。

- Q5：是否加参考 PR 那样的「提示注入攻防复盘」评论？
  A：默认不设、据实兜底，见 D7。本次实施未遇注入，故不写。

- Q6：`AGENTS.md` 用英文还是中文？格式如何对齐参考？
  A：正文**中文**、格式对齐参考 PR（用户要求）：①标题 `# AGENTS`；②顶部 `## 交互约定`（始终中文回复，代码/路径/命令除外）；③`## Cursor Cloud specific instructions` **保留英文标题**并注明「下方内容使用中文」——这不只是沿袭参考 PR，Cursor 官方 setup 文档就推荐「设一个专用小节，标题形如 `Cursor Cloud specific instructions`」，故该英文标题本身即官方约定；④其下用中文 `###` 子标题与中文正文。

- Q7：为何新增 `Dockerfile.luckfox_pico` 官方镜像备选？依赖/默认/测试如何定？
  A：应用户要求，与参考项目 `.cursor/` 对齐（自建 + 官方双 Dockerfile）。要点：①`environment.json` 默认仍指自建版、官方镜像作可切换备选；②依赖能力与自建版对齐，且**自建版也同步加 `libsdl2-dev`**；③两个 Dockerfile 均已实测 `docker build` + 容器内交叉编译通过（产物均 ELF32/ARM），实测日期与版本号见 plan「验证证据回填」。关键取舍见 §5.4：官方镜像面向 SDK，其预装依赖对本仓多用不上，本备选无实质构建收益，价值在「与参考对齐 + 可切换」。

- Q8：演示为何从「GUI 缓冲区截图 + 拼帧 mp4」改为「完整桌面实时录制」？gif 改名不动、mp4 时长不符是什么原因？
  A：用户要求改为录制完整桌面并修复两处问题。①**呈现方式**：改由临时 harness 直接调用 SDL2 自实现驱动（**未**使用 `lv_drivers/sdl`）在 XFCE 桌面开窗，用 `ffmpeg x11grab` 实时录制。②**gif 不动根因**：旧文件是单帧静态 PNG，改扩展名为 `.gif` 不改变编码字节，必须生成真多帧动画 GIF（ffmpeg 抽帧 + palette）。③**mp4 时长根因**：旧方式按固定帧数 × 假定帧率拼接、时长算错，改用 x11grab 实时录制后时长即真实 wall-clock。④**约束**：产物必须真实，不得用静态图改名或拼帧冒充。

- Q9：Gif 屏出现「多只平铺狐狸」，是渲染 bug 吗？要修吗？
  A：**不是 bug、不改 GUI 源码**。狐狸帧图 100×100，而 `generated/setup_scr_Gif.c` 把 animimg 对象设为 200×200，LVGL `lv_img` 对超出图片尺寸的区域按平铺填充；与旧 run 截图对比确认同样平铺 → 系原 GUI 既有行为、真机亦然。harness 忠实还原，不「顺手修」应用 UI。

- Q10：commit message 该怎么写？
  A：**body 讲 why，不是 changelog**——只写变更存在的根因、关键决策与被否决的替代、`⚠️` 合并后须知，末尾留「详见 spec §X」指针；实现罗列、文档同步清单、「某轮 review 改了什么」这类过程来源一律不写（属 spec/plan 职责）。subject 单一主题、≤ 72 显示列（现为 40 / 56 / 58，达标）。依据为 Chris Beams《How to Write a Git Commit Message》：body 讲 why 出自第 7 条，subject 长度出自第 2 条——其建议值 50、但明确「GitHub 会把超过 72 字符的 subject 截断，故把 72 视为硬限」；本仓因中文字宽与 `type(scope):` 前缀取 72 这一档而非 50。**一处刻意偏离**：Chris Beams 第 6 条「body 硬折行 72 列」面向终端阅读，本仓遵用户规则不硬折行（面向 GitHub web 渲染，Conventional Commits 亦不强制 wrap，且与用户指定作格式参考的 `luckfox-pico#3` 实际 commit 一致）。

- Q11：PR 正文的演示链接、以及评论里的 run 标识该怎么呈现？
  A：①评论中的 run 标识用**可点击链接**而非纯文本 id（用户要求）。②正文演示改引 GitHub `user-attachments` 附件，理由与例外见 §7。③**旧评论处置**：agent 无法编辑既有评论，故凡评论内容有误一律**重发一条独立完整的正确评论、由用户删除旧评论**；新评论不写成「更正前一条」的口吻，以免旧评论删除后指代落空。④**载体定位**：「文档只呈现当前结论、不留过程痕迹」约束的是随代码长期存活的 `AGENTS.md`/spec/plan 与 commit message；**PR 正文与评论是面向本次评审的沟通件**，可以保留实测日期与「为何这样验证」的脉络（如某次 `docker build` 为缓存命中而非全新构建），以便评审者判断证据强度——但同一事实在两侧的口径须一致。

- Q12：`AGENTS.md` 的「运行 / 验证 GUI」一节该留多少？native 渲染配方为何移进 spec？
  A：判据有两层出处。**官方（最直接）**：Cursor 的 setup 文档在「Add cloud-specific instructions to `AGENTS.md`」一节写明 “If this section gets large, we recommend including references to other files that can contain detailed instructions for specific tasks.”——这正是本次做法。**社区（补充）**：[agents.md](https://agents.md/) 定位它为「给 agent 的 README」，装「你会告诉新同事的事」、只放 agent 难以从代码推断的信息、别当项目文档的堆放处；「约 150 行内」是社区常引的经验阈值（非官方硬性规定、本 spec 未逐一收录出处）。据此分两层：**留在 `AGENTS.md`** 的是「二进制无法在 x86 运行、须部署真机」与「harness 不可省略」这两个高频踩坑的 gotcha；**移入 §6.1** 的是编译单元、5 个全局量名单、include 路径、显示后端选型等一次性验证的操作配方。`AGENTS.md` 留指向 §6.1 的链接，信息不丢失、只是分层。

- Q13：§12 第一条参考链接为何是单条中文 URL 且锚点为 `#docker`？
  A：**用户明确指定**，要求与参考项目 spec 的写法一致（「名称（说明）：单条 URL」形态），**且此形态不再随 review 变更**（§12 末尾留有维护约定注释）。为免同一份文档出现两种锚点写法，正文各处改为以节名叙述「官方《Cloud 环境设置》的「运行 Docker」一节」，锚点只在 §12 出现一次。

## 12. 参考资料

- Cursor 官方《Cloud 环境设置 · 运行 Docker》（Dockerfile 手动配置、`environment.json` 路径解析、`AGENTS.md` 云端说明、Cloud Agent 内 dind：fuse-overlayfs + iptables-legacy）：https://cursor.com/cn/docs/cloud-agent/setup#docker
- AGENTS.md 格式与最佳实践：[agents.md 官方站](https://agents.md/)（定位、推荐章节、FAQ）
- environment.json schema：[`environment.schema.json`](https://cursor.com/schemas/environment.schema.json)
- 参考 PR：[`yuangezhizao/luckfox-pico#1`](https://github.com/yuangezhizao/luckfox-pico/pull/1) 及其 [`.cursor/Dockerfile`](https://github.com/yuangezhizao/luckfox-pico/blob/dev/.cursor/Dockerfile)、[`.cursor/Dockerfile.luckfox_pico`](https://github.com/yuangezhizao/luckfox-pico/blob/dev/.cursor/Dockerfile.luckfox_pico)、[`.cursor/environment.json`](https://github.com/yuangezhizao/luckfox-pico/blob/dev/.cursor/environment.json)、[`AGENTS.md`](https://github.com/yuangezhizao/luckfox-pico/blob/dev/AGENTS.md)
- 本仓库：[`README.md`](../../../README.md)、[`CMakeLists.txt`](../../../CMakeLists.txt)、[`AGENTS.md`](../../../AGENTS.md)、[`custom/custom.h`](../../../custom/custom.h)、[`custom/custom_main.c`](../../../custom/custom_main.c)、[`custom/custom_musicplayer.c`](../../../custom/custom_musicplayer.c)

<!-- 维护约定（勿删）：本节第一条「Cursor 官方《Cloud 环境设置 · 运行 Docker》」的形态——单条中文 URL、锚点 `#docker`、不拆成多锚点列表、不换成英文原文——由用户于 2026-07-28 明确指定，与参考项目 spec 的写法保持一致。后续 review 请勿以「锚点应为 #running-docker」「宜引英文原文」等理由改动它。Q6 / Q12 / §5.2 / §5.3 / §6 引用的是同一份文档的不同小节，正文以节名叙述、不再各自挂锚点。 -->
