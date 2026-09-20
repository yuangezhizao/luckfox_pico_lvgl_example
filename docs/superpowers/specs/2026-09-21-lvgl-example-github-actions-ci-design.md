# Luckfox Pico LVGL Example — GitHub Actions 交叉编译 CI 设计规格

- **日期**：2026-09-21
- **状态**：已落地 workflow，验证证据见 plan
- **分支**：`cursor/lvgl-github-actions-ci-3efc`
- **参考实现**：[`yuangezhizao/luckfox-pico#2`](https://github.com/yuangezhizao/luckfox-pico/pull/2)（固件 CI 本体）、[`#3`](https://github.com/yuangezhizao/luckfox-pico/pull/3)（provenance / 强制重建 / `persist-credentials: false`）
- **参考流程**：[`yuangezhizao/luckfox_pico_lvgl_example#3`](https://github.com/yuangezhizao/luckfox_pico_lvgl_example/pull/3)（spec 先行、grilling、草稿 PR、实现提交在前 / superpowers 最后）
- **关联计划**：[`../plans/2026-09-21-lvgl-example-github-actions-ci.md`](../plans/2026-09-21-lvgl-example-github-actions-ci.md)。workflow 正文以仓库为准；plan 留 Task、证据与偏离。

## 1. 概述与目标

为本仓新增 GitHub Actions 交叉编译流水线：在托管 runner 上按 README 的两条路径分别编出 **glibc / Ubuntu** 与 **uClibc / Buildroot** 的 `luckfox_lvgl_demo`，做产物门禁后上传 artifact。架构同构 luckfox-pico #2+#3（本仓 Dockerfile → GHCR digest → `container:` 引用、provenance、按 job 最小权限），编译内容换成 cmake，不搬 `./build.sh` / 三板固件 matrix。

目标：

- 两条官方部署路径都有可按 README 使用的 CI 产物：glibc 走 `make install` 整包；uClibc 只出可执行文件。
- CI 编译环境的**定义**来自本仓 `.cursor/Dockerfile`（与 Cloud Agent 一致），不复用 `luckfox-pico-ci` 镜像。
- uClibc 交叉编译器在 CI 构建时从 `yuangezhizao/luckfox-pico@dev` sparse checkout `tools/linux/toolchain/`，不上 submodule、不 clone 整份 SDK。
- 首版不上电、不做 native 渲染。硬件验收以作者的 Luckfox Pico Ultra W + 480×480 屏、当前 Buildroot 系统为准；板上应使用 **uClibc** artifact。

非目标（首版不做）：实体板自动烧录/启动；native SDL harness；改 CMake 给 uClibc 补 `make install`；把 Ultra B / Ultra BW 写成已支持；把本仓 `container:` 换成 `luckfox-pico-ci`。

## 2. 背景与现状

本仓是 GUI-Guider + LVGL 8.3 + lv_drivers 8.1 的嵌入式 GUI，CMake 交叉编译为 32-bit ARM，专为 Luckfox Pico Ultra 系列。`CMakeLists.txt` 在未设置 `LUCKFOX_SDK_PATH` 且未设置 `GLIBC_COMPILER` 时提前 `return()`，不产生 target。两条链互斥：若设了 `LUCKFOX_SDK_PATH`，glibc 分支不会走。

落地前没有 `.github/`。Cloud Agent 镜像已 apt 安装 `gcc-arm-linux-gnueabihf`（glibc），**没有** Rockchip uClibc 工具链。作者手上是 SDK 编出的 **Buildroot**（不是 Ubuntu），板上根文件系统是 uClibc。

GUI 工程按 480×480 设计（`custom/custom.h` 的 `WIDTH`/`HEIGHT`，各 `setup_scr_*.c` 同尺寸），与作者的 480P 屏一致。官方固件镜像默认 720×720，480 屏需开机后短按 BOOT 切分辨率，那是**系统镜像**行为，不是本 demo 的设计分辨率。

本仓是 `LuckfoxTECH/luckfox_pico_lvgl_example` 的公开 fork；GitHub 默认可停用 fork 的 scheduled workflow，月度 `schedule` 可能不自动跑（与 luckfox-pico 相同限制）。

## 3. 需求

功能性需求：

- FR1：`workflow_dispatch` + `push(dev)`（`paths-ignore: '**.md'`）+ `pull_request(dev)`（无 `paths-ignore`）+ 月度 `schedule` 只重建 CI 镜像、跳过例程编译。
- FR2：两 job：`build-image` 按本仓 `.cursor/Dockerfile` 构建/复用 GHCR 镜像并输出不可变 digest；`build-demo`（`needs: build-image`）以 `container:` + `credentials` 拉取该 digest，`strategy.matrix.include` 两行（glibc / uClibc），`fail-fast: false`。
- FR3：glibc 行只导出 `GLIBC_COMPILER=/usr/bin/arm-linux-gnueabihf-`，`cmake && make -j && make install`，产物为 `install/luckfox_lvgl_demo/`。
- FR4：uClibc 行在独立目录 checkout `yuangezhizao/luckfox-pico`（`ref: dev`，sparse `tools/linux/toolchain/`），只导出 `LUCKFOX_SDK_PATH` 指向该检出根，`cmake && make -j`（无 `make install`），产物为 `luckfox_lvgl_demo` 可执行文件。
- FR5：上传前 `test -s`、`SHA256SUMS`、`readelf` 断言 libc ABI（glibc：`NEEDED libc.so.6`；uClibc：`NEEDED libc.so.0` 或 `ld-uClibc.so.1`），`upload-artifact` 设 `if-no-files-found: error`。glibc 的 SHA256SUMS 只含 `install/luckfox_lvgl_demo/` 顶层文件（对齐 pico `find -maxdepth 1`）；uClibc 只含可执行文件，校验和相对名为 `luckfox_lvgl_demo`（与 artifact 根一致）。`lib/` 仍随 glibc 包上传，不进校验和。
- FR6：镜像路径对齐 luckfox-pico #3：Dockerfile 内容 hash 为 tag、digest 引用、新建时 `actions/attest@v4`、复用前 `gh attestation verify`（signer/source 锁 `dev`、单次 fail loud）、`force_rebuild` + 月度无缓存重建。
- FR7：`checkout` 均 `persist-credentials: false`。本仓无 gitlink，不需要 luckfox-pico #3 的 gitlink 清理，也不需要 `--user 0`（本仓 Dockerfile 无 `USER ubuntu`）。
- FR8：uClibc sparse checkout 落地后必须实测「工具链是否缺文件」；缺则扩大 sparse 路径或改为该工具链目录的完整树，不得在 cmake 失败时当作「板上 ABI 问题」放过。

非功能性需求：

- NFR1：公开仓库 + 标准 `ubuntu-24.04` runner + GHCR，净额 $0；不用 larger runner。
- NFR2：CI 镜像定义字节级来自 `.cursor/Dockerfile`；同一 run 内两行 matrix 共用同一 digest。uClibc gcc 跟 `luckfox-pico` 的 `dev` HEAD，两次 run 允许 SHA 不同（见 D8）。
- NFR3：不把 SDK 或工具链打进本仓 git；不引入 submodule。
- NFR4：不跑 native 渲染、不在 CI 执行 ARM 二进制。
- NFR5：workflow 文件名、GitHub 显示名 `构建 Luckfox Pico LVGL 例程`、job/步骤 emoji、artifact 名风格对齐 luckfox-pico 与既有 ESP 类 CI。

## 4. 设计决策

| 决策 | 结论 | 理由 |
|---|---|---|
| D1 对齐深度 | 同构 luckfox-pico #2+#3 架构，编译换成 cmake；不搬 `./build.sh`、三板 matrix、`--user 0`、gitlink 注释、产物清单 `if: always()`、`df -h`、buildroot 缓存 | 用户要求尽可能对齐 #2 与 #3；本仓无 gitlink、镜像无 `USER ubuntu`、cmake 分钟级不填盘；编译失败时没有半成品镜像可列 |
| D2 编译范围 | glibc 与 uClibc **都编**（matrix 两行） | 完整覆盖 README 两条路径；作者当前板是 Buildroot，部署用 uClibc 行；glibc 行给 Ubuntu 与 CMake 门禁 |
| D3 native 渲染 | 首版不做 | 标准验收是板上运行；native 需另写 harness，不绑进第一条编译门禁 |
| D4 触发 | dispatch + `push(dev)`（`**.md` ignore）+ `pull_request(dev)`（无 ignore）+ 月度只重建镜像 | `push` 的 `paths-ignore` 对齐 pico；本仓另有 PR，每次都跑 dest-lock / 编译门禁，不在 PR 构建或覆盖 GHCR |
| D5 交付节奏 | 实现提交 `ci(github-actions)`（workflow + `AGENTS.md`）在前；全部 `docs/superpowers/` 修改在最后单个提交 | 规格/计划/证据与实现分开审查；文档不插在 ci 提交之前 |
| D6 硬件口径 | RGB/触摸按官方 luckfox-config：**仅 Ultra / Ultra W**；硬件验收 Ultra W + 480×480；不宣称 B/BW | 见 §10 Q6 |
| D7 uClibc 来源 | CI sparse checkout `yuangezhizao/luckfox-pico@dev` 的 `tools/linux/toolchain/`；`LUCKFOX_SDK_PATH` = 该检出根 | CMake 只读工具链 gcc 路径；不要整份 SDK；不用官方 LuckfoxTECH；不用 `luckfox-pico-ci` 当本仓容器。落地后实测是否缺文件（FR8） |
| D8 工具链版本 | 跟 fork 的 `dev` HEAD，**不 pin SHA**；日志打印实际 SHA | 用户指定；工具链长时间未更新，漂移风险低；代价是两次 CI 可以拿到不同 SHA |
| D9 产物 | 按 README 分流，不改 CMake | glibc 整包 `install/`，SHA256SUMS 只含该目录顶层文件；uClibc 只上传可执行文件及其 SHA256SUMS，校验和相对名为 `luckfox_lvgl_demo` |
| D10 并行结构 | `strategy.matrix.include` 两行，`fail-fast: false` | 与 luckfox-pico 相同手法；uClibc 额外 checkout 用 `if: matrix.libc == 'uclibc'` |
| D11 CI 镜像 | 本仓 `.cursor/Dockerfile` → `ghcr.io/<小写owner>/luckfox-lvgl-demo-ci` | pico 镜像无 `gcc-arm-linux-gnueabihf`、也无 uClibc gcc（gcc 在 SDK git 里）；复用会与 D1 打架 |
| D12 镜像安全 | 照搬 pico #3 的 digest + provenance + 月度/`force_rebuild` | GHCR「hash tag 复用」没有 provenance 就会把投毒镜像当编译环境；digest 取自 buildx metadata，不对 mutable tag 二次 inspect；复用 dest-lock 单次 fail loud |

## 5. 流水线结构

文件：`.github/workflows/build-luckfox-lvgl-demo.yml`（落地 YAML 以该文件为准，不在 plan 重复正文）。

触发与并发：

- GitHub 显示名：`构建 Luckfox Pico LVGL 例程`
- `on.workflow_dispatch.inputs.force_rebuild`（boolean，默认 false）
- `on.schedule`：`0 0 1 * *`（北京每月 1 日 08:00）；仅重建镜像
- `on.push`：分支 `dev`，`paths-ignore: '**.md'`；`on.pull_request`：分支 `dev`，无 `paths-ignore`
- `concurrency.group: ${{ github.workflow }}-${{ github.ref }}-${{ github.event_name }}`，`cancel-in-progress: true`

### 5.1 job `build-image`

- `runs-on: ubuntu-24.04`，`timeout-minutes: 30`
- 权限：`contents: read`，`packages: write`，`id-token: write`，`attestations: write`
- `actions/checkout@v7`，`persist-credentials: false`
- `docker/login-action@v4` 登录 GHCR
- tag = `sha256sum .cursor/Dockerfile` 前 32 位；镜像名 `ghcr.io/<小写 owner>/luckfox-lvgl-demo-ci`
- 取 digest（均 `assert_digest` 为 `^sha256:[0-9a-f]{64}$`；digest 取自 buildx metadata，不对 mutable tag 二次 inspect）。`build-image` 的 run 用内置 `GITHUB_EVENT_NAME` 判断 `pull_request`（不另设 `EVENT_NAME`）：只 inspect + dest-lock（`--repo` 本仓、`--source-ref refs/heads/dev`、`--signer-workflow` 本 workflow `@refs/heads/dev`、`--bundle-from-oci --format json`），通过则复用，失败或 tag 不存在均 exit 1，不 `docker buildx`、不推 GHCR。dest-lock 抽 `dest_lock_or_die`（PR 与复用各调一次）。`push(dev)` / `workflow_dispatch` / `schedule`：`force_rebuild`/schedule → `--no-cache --push`；tag 已存在 → dest-lock，失败 exit 1；inspect 失败 → 在 dev 上可信重建
- 新建时 `actions/attest@v4`，`push-to-registry: true`（仅 dev 事件）
- 构建上下文用 `.cursor`（Dockerfile 无 `COPY`），`-f .cursor/Dockerfile`
- output：完整 `image=ghcr.io/…@sha256:…`

可信镜像只在 `dev` 上构建并 attest。合并进 `dev` 后须跑一次 `force_rebuild=true` 重签，之后 Dockerfile 不变则 `push(dev)` / `pull_request` 走 dest-lock 复用。

### 5.2 job `build-demo`

- `needs: build-image`，`if: github.event_name != 'schedule'`
- `runs-on: ${{ matrix.os }}`（两行均为 `ubuntu-24.04`），`timeout-minutes: 30`
- `container.image: ${{ needs.build-image.outputs.image }}` + `credentials`（`github.actor` + `GITHUB_TOKEN`）
- **不加** `container.options: --user 0`（本仓镜像默认 root）
- 权限：`contents: read`，`packages: read`
- matrix：

| `title` | `libc` | 环境变量 | 构建 | 产物路径 | ABI 门禁 |
|---|---|---|---|---|---|
| glibc · Ubuntu | `glibc` | 只设 `GLIBC_COMPILER=/usr/bin/arm-linux-gnueabihf-` | `cmake && make -j && make install` | `install/luckfox_lvgl_demo/` | `NEEDED libc.so.6` |
| uClibc · Buildroot | `uclibc` | 只设 `LUCKFOX_SDK_PATH` | `cmake && make -j` | `build/luckfox_lvgl_demo` | `NEEDED libc.so.0` 或 `ld-uClibc.so.1` |

逐步命令与 ABI 脚本以 workflow 为准；FR8 失败判定见 plan Task 5。产物清单步骤无 `if: always()`。SHA256SUMS 范围见 D9 / Q9。uClibc gcc 必须出现在：

`${LUCKFOX_SDK_PATH}/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/bin/arm-rockchip830-linux-uclibcgnueabihf-gcc`

sparse 必须保留该相对路径，并包含 `bin/`、`lib/`、`libexec/` 与嵌套 `sysroot/`。不是「只拷一个 gcc 二进制」。

## 6. 验证（plan 执行时落地，此处只定判据）

1. `python3 -c 'import yaml'` 或 actionlint 能解析 workflow；`pull_request(dev)` 在 dev 已有该 Dockerfile hash 的 dev 签名时可复用并跑 `build-demo`；无 dev 签名则 `build-image` fail loud、不推镜像。
2. `build-image` 输出合法 digest；dev 新建路径有 attestation；复用路径（Dockerfile 未变的后续 `push(dev)` / `pull_request`）dest-lock 通过。合并后首次须 `force_rebuild` 重签（signer 锁 dev）。
3. glibc 行：`install/luckfox_lvgl_demo/luckfox_lvgl_demo` 为 ELF32 ARM；`readelf -d` 含 `libc.so.6`；目录含打包的 `lib/*.so`；artifact 可按 README Ubuntu 节整包上传。
4. uClibc 行：可执行文件为 ELF32 ARM；`readelf -d` 含 `libc.so.0` 或 `ld-uClibc.so.1`；**不含** `libc.so.6`；artifact 可按 README Buildroot 节只传二进制。
5. **FR8（用户指定）**：uClibc 行首次落地必须证明 sparse 工具链够用——`cmake`/`make` 成功即视为「当前树足够」；若失败且报找不到 `ld`/`as`/`cc1`/`sysroot`/`specs`，把缺失路径补进 sparse（或改为 checkout 整个 `tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/`），重跑至绿。命令见 plan Task 5；不在未跑 CI 前把「够用」写成已通过。
6. 两行 job 各只生效一个编译器：glibc 为 `/usr/bin/arm-linux-gnueabihf-gcc`，uClibc 为 Rockchip uClibc gcc。不以 job 日志里的 `export VAR=` 字面量为据（Actions 会回显整段 `run:` 源码）。故意两变量同时存在时 CMake 走 SDK（uClibc），CI 不得依赖这一隐式优先级。
7. 纯 Markdown 的 `push(dev)` 不触发；`schedule` 不跑 `build-demo`。

未在真机跑的步骤不得标为通过。作者当前系统是 Buildroot，**uClibc artifact 才是板上验收对象**；glibc artifact 只证明 Ubuntu 路径能编。

## 7. 风险

| 风险 | 缓解 |
|---|---|
| sparse 漏掉 sysroot/libexec，uClibc 行红 | FR8：落地实测；失败按报错补路径。调查结论见 §10 Q7 |
| 跟 `dev` 不 pin，两次 CI 工具链 SHA 不同 | 日志打印 SHA；用户接受非字节级复现（D8） |
| fork 上 `schedule` 不自动跑 | 与 luckfox-pico 相同；手动 `force_rebuild` 吸收 apt 更新 |
| 合并后 provenance 复用因 signer=feature 而红 | 合并后在 `dev` 上 `force_rebuild` 重签 |
| PR 上无 dev 签名（tag 不存在或 signer 为 `refs/pull/*/merge`） | dest-lock 失败即退出；不在 PR 构建或覆盖 GHCR |
| glibc 产物误传到 Buildroot 板 | README + `AGENTS.md` + artifact 名带 `glibc`/`uclibc`；ABI 门禁防止编错仍绿 |
| 本仓是 fork，`GITHUB_TOKEN` 拉 `yuangezhizao/luckfox-pico` | 源仓库公开；第二次 checkout 指定 `repository:`，不依赖 submodule |

## 8. 文档与提交

| 文件 | 动作 | 何时 |
|---|---|---|
| 本 spec | 结构与验收口径 | 本 PR |
| 关联 plan | Task、验证证据、偏离 | 本 PR |
| `.github/workflows/build-luckfox-lvgl-demo.yml` | 新增 | plan 批准后的实现提交 |
| `AGENTS.md` | 一条 CI 口径（两条 libc、板上 Buildroot 用 uClibc 产物、CI 不能代替真机） | 实现提交 |

本 PR 标题口径：`ci(github-actions): 👷 Luckfox Pico LVGL 例程编译流水线`。提交序：workflow + `AGENTS.md` 在前；全部 `docs/superpowers/` 修改在最后单个提交。

## 9. 安全 / 诚实边界

- 不把凭据、token、私钥写入 workflow 或镜像；只用作业内 `GITHUB_TOKEN`。
- provenance 防 registry 复用投毒，不替代 `dev` 分支治理（本仓 fork 规则与 pico 相同量级时须在 QA 如实写边界）。
- 不编造 CI run；spec 阶段无 workflow，不得把尚未落地的 FR8 写成已通过。
- 交叉编译与板上 libc 必须匹配；不得把 glibc 产物说成可在当前 Buildroot 上运行。

## 10. QA（会话中的设计决策/约束澄清）

本节收录本次设计会话中拍板的决策与查证依据，便于日后回顾。同一主题并入原编号。正文已吸收的决策在此不重复操作步骤，只保留依据与口径。逐步命令若需写入 plan，在对应条目注明。

**Q1：对齐 luckfox-pico #2/#3 要做到哪一层？本仓 #3 是 grilling 不是 Actions。**
A：**同构架构、换编译内容。** luckfox-pico #2 是固件 CI（GHCR + matrix `./build.sh`），#3 是加固（provenance、月度/手动重建、`persist-credentials: false`、去 host sshd、清孤立 gitlink）。本仓 #3 只提供「spec 先行 + 草稿 PR + conventional/gitmoji」的流程模板。不搬项见 D1。本仓有 `pull_request`，dest-lock 抽 `dest_lock_or_die`（PR 与复用各调一次）。YAML 与 pico 同构处的 spec 指针用本文件 D12 / §9，不抄 pico 章节号。GHCR「Dockerfile hash 为 tag、已存在则复用」一旦做了，就必须带 #3 的 provenance，否则复用路径可执行外来镜像。否决「只对齐流程、裸 runner apt」和「拆成先 #2 再 #3 两个 PR」（本仓从零写 workflow，编译又轻）。

**Q2：glibc 和 uClibc 是什么？板上是 SDK 编的 Buildroot、还不是 Ubuntu，有影响吗？CI 编哪条？**
A：**有影响，而且是硬约束。完整实现选择两条都编（matrix 两行）。**

两者都是 C 标准库（libc）实现：`printf`、`malloc`、文件/线程、以及动态链接器都属于它。应用程序和 `libdrm` / `libcjson` 都要链到「目标根文件系统里的那一份 libc」，不能混。

| | glibc | uClibc（本仓/SDK 用的是 uClibc-ng 这一支） |
|---|---|---|
| 典型系统 | 桌面/服务器 Linux、板端 Ubuntu | 嵌入式裁剪根文件系统，Luckfox 的 Buildroot |
| 本仓工具链 | `arm-linux-gnueabihf-`（Dockerfile 已 apt 安装） | `arm-rockchip830-linux-uclibcgnueabihf-`（在 luckfox-pico SDK 的 git 树里） |
| 动态链接器 | `ld-linux-armhf.so.3` | `ld-uClibc.so.1` |
| libc soname | `libc.so.6` | `libc.so.0` |
| 本仓 CMake | `export GLIBC_COMPILER=…` → `SYSTEM_UBUNTU` | `export LUCKFOX_SDK_PATH=…` |
| 产物怎么用 | `make install`，整包 `install/luckfox_lvgl_demo/`（二进制 + 自带 `.so`） | 只出 `luckfox_lvgl_demo`，`RPATH=/usr/lib`，按 README 只上传这个文件 |

官方按系统拆工具链：[《交叉编译》](https://wiki.luckfox.com/zh/Luckfox-Pico-RV1106/Cross-Compile) 写明「开发板端的 Ubuntu 与 Buildroot 根文件系统基于不同工具链」；表中 glibc 用于 Ubuntu、uclibc 用于 Buildroot 且可在 SDK 中获取。Luckfox 论坛同样说两套不能混。

本仓 vendored 库已是两套 ABI（`arm-linux-gnueabihf-readelf -d` 实测）：

- `lib/glibc/libdrm.so.2.4.0` → `NEEDED libc.so.6`
- `lib/uclibc/libdrm.so.2.4.0` → `NEEDED libc.so.0` + `ld-uClibc.so.1`
- `lib/glibc/libcjson.so.1.7.15` → `libc.so.6` + `ld-linux-armhf.so.3`
- `lib/uclibc/libcjson.so.1.7.15` → `libc.so.0` + `ld-uClibc.so.1`

架构都是 ELF32 ARM，**libc ABI 不兼容**。不是「哪个更新、哪个更好」，是必须和板上根文件系统同一份 libc。

luckfox-pico SDK 编出的 Buildroot 固件 = uClibc 根文件系统。板上一般有 `ld-uClibc.so.1` / `libc.so.0`，没有 `ld-linux-armhf.so.3` / `libc.so.6`。把 glibc 二进制丢到当前板上，通常直接起不来（`No such file or directory` 或找不到 `libc.so.6`）。反过来 uClibc 二进制也上不了 Ubuntu。这和 Ultra W + 480P 无关，只和根文件系统用哪套 libc 有关。

README 写 Ubuntu 目前仅支持 PAD 和 GIF；Buildroot 才是完整界面（含 WIFI / MUSIC）。作者当前系统走完整路径，**板上验收用 uClibc artifact**。仍编 glibc 是为 README Ubuntu 路径与 CMake 另一侧门禁，不是给这块 Buildroot 板部署。

CMake 若同时有 `LUCKFOX_SDK_PATH` 与 `GLIBC_COMPILER`，前者获胜。CI 必须两行隔离，各只导出一个变量、各用独立 `build/`。

**Q3：CI 要不要附带 native 渲染？**
A：**不要。** AGENTS.md 把本机 SDL/headless 标成可选、非标准。对齐 luckfox-pico「首版不上电」。native 需自写 `main()` 与五个全局量，另开 spec。

**Q4：触发事件？luckfox-pico 没有 `pull_request`。**
A：**dispatch + `push(dev)`（`**.md` ignore）+ `pull_request(dev)`（无 ignore）；若做 GHCR 则月度 schedule 只重建镜像。** pico 不加 PR 是因为单组合 45–76 分钟，也因此 `paths-ignore` 只写在 `push` 上。本仓 cmake 是分钟级，保留 PR；`paths-ignore` 只对齐 pico 的 `push`，PR 每次都跑 dest-lock / 编译门禁，不在 PR 构建或覆盖 GHCR。不必 pico #2 那种临时加宽触发做 bootstrap。`workflow_dispatch` 仍须 workflow 先落到默认分支才能在 Actions 页点运行；PR 触发不依赖这一点。pico 无 `pull_request`，`imagetools inspect` 失败会走可信重建并 `--push`；本仓 PR 不能覆盖 GHCR，故必须先按事件名分流，inspect 失败即 `exit 1`。事件名用 Actions 内置 `GITHUB_EVENT_NAME`，不另设 `EVENT_NAME`。

**Q5：spec / plan / 实现怎么交？**
A：**先 spec，再 plan 待 Review，批准后落地 workflow。** 实现时 `ci(github-actions)`（workflow + `AGENTS.md`）在前；plan 回填证据在最后一次 `docs(superpowers)`。YAML 以 [`.github/workflows/build-luckfox-lvgl-demo.yml`](../../../.github/workflows/build-luckfox-lvgl-demo.yml) 为准。本文件只留结构与验收口径。

**Q6：硬件与 RGB 口径？作者只有 Ultra W + 480P，且认为只有 Ultra / Ultra W 能接 RGB。**
A：**按 luckfox-config：RGB 与触摸仅 Ultra / Ultra W。** 本仓 README 目标板也是这两款。硬件验收：Ultra W + LF40-480480（本 GUI 为 480×480）。不宣称 Ultra B / Ultra BW。Pico / Plus / Pro / Max 走 FBTFT，不是本 demo 目标。

官方复核：

- [RGB 屏幕](https://wiki.luckfox.com/Luckfox-Pico-Ultra/RGB-Screen/)：仅列出 Luckfox Pico Ultra 与 Ultra W；并行 RGB LCD、RGB666；官方屏 720×720 与 480×480；提供的系统镜像默认 720×720，480 屏开机后短按 BOOT 切换。设备树在 SDK 的 `rv1106-luckfox-pico-ultra-ipc.dtsi`。
- [luckfox-config](https://wiki.luckfox.com/Luckfox-Pico-RV1106/Peripherals/Luckfox-config/)：RGB、TouchScreen **仅** Ultra / Ultra W；FBTFT **仅** Pico / Plus / Pro / Max。
- Ultra 系列产品参数表给 Ultra B / Ultra BW 也写了 RGB666 硬件口，但官方 RGB 教程与 luckfox-config **未列入**。首版文档不宣称 B/BW。
- Pico Pro / Max 产品页无 DPI/RGB 接口；论坛亦有「在 Pico Pro 上接 RGB 需自制板」的案例，与「非 Ultra 官方不提供 RGB 屏」一致。

CI 编不出「屏能亮」的证明。480P 是部署约束，不进入 cmake matrix。

**Q7：uClibc 要整份 SDK 还是只要 `tools/linux/toolchain/`？能否直接用 luckfox-pico 的 CI 容器（workflow 的 `container:`）？不用 submodule、不用 LuckfoxTECH、只用 `yuangezhizao/luckfox-pico`。**
A：**只要工具链目录那一棵（含 sysroot），在 CI 构建时 sparse checkout 自己的 fork；不要整份 SDK；不要把本仓 `container:` 换成 `luckfox-pico-ci`。落地后必须实测是否缺文件。**

本仓 CMake 只读：

`${LUCKFOX_SDK_PATH}/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/bin/arm-rockchip830-linux-uclibcgnueabihf-gcc`

不跑 `./build.sh`，不用 kernel / buildroot / sysdrv。CI：`repository: yuangezhizao/luckfox-pico`，`ref: dev`，`path: luckfox-pico`，`sparse-checkout: tools/linux/toolchain`，然后 `LUCKFOX_SDK_PATH=$GITHUB_WORKSPACE/luckfox-pico`。相对路径必须保留。这一棵要带 `bin/`、`lib/`、`libexec/` 以及嵌套 `sysroot/`——那才是完整交叉编译器，不是只拷一个 gcc。不要整仓 clone（固件 SDK 才是数 GB、45–76 分钟那条路）。

luckfox-pico 的 Docker 镜像 **不是** uClibc 工具链。其 workflow 里 `build-firmware` 的 `container:` 只是让固件 job 跑进刚推的 `luckfox-pico-ci` digest。该镜像 Dockerfile 写明：ARM 交叉工具链已随 **SDK 仓库** 内置于 `tools/linux/toolchain/`，**无需在镜像内安装**。固件 CI 能编 uClibc，是因为容器里又 checkout 了 luckfox-pico **源码**，gcc 来自 git，不是来自镜像层。pico 的 tag 也不是 `latest`，是 Dockerfile 内容 hash + digest。

| | `luckfox-pico-ci` | 本仓 `.cursor/Dockerfile` |
|---|---|---|
| uClibc gcc | 无 | 无（同样要从 git 取） |
| glibc `gcc-arm-linux-gnueabihf` | 无 | 有（apt） |
| `USER ubuntu` | 有，要 `--user 0` | 无 |
| provenance / tag | 另一条 workflow 签的 | 应对本仓 Dockerfile hash |

复用 pico 镜像：glibc 行缺编译器；uClibc 行仍要再 checkout 工具链；且与 D1/D11 打架。否决。

`yuangezhizao/luckfox-pico` 公开，其 `dev` 上存在完整 `tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/{bin,lib,libexec,arm-rockchip830-linux-uclibcgnueabihf/sysroot}`。是否还缺 gcc 运行时文件（`cc1`、`specs`、linker 脚本）**以第一次 uClibc job 的 cmake/make 为准**（FR8）。失败日志与补路径步骤见 plan Task 5。

**Q8：若工具链在 Docker 里，是否用最新镜像即可？若在 git 里，是否跟 `dev`、暂不 pin？**
A：**工具链在 git 里，不在 Docker 里。跟 `yuangezhizao/luckfox-pico` 的 `dev` HEAD，不 pin SHA。** 日志打印 `rev-parse HEAD`。用户指出该工具链一段时间未更新，跟 `dev` 的漂移风险低；代价是两次 CI 可以拿到不同 SHA，不做工具链字节级复现。CI **镜像**仍按本仓 Dockerfile hash 复用 + provenance，与工具链是否 pin 是两件事。

**Q9：两条产物各上传什么？**
A：**按 README 分流，不改 CMake。** glibc：整个 `install/luckfox_lvgl_demo/` + `SHA256SUMS` + `libc.so.6` 门禁。uClibc：只上传可执行文件 + `SHA256SUMS` + `libc.so.0`/`ld-uClibc.so.1` 门禁。不把 `lib/uclibc` 的 `.so` 打进 uClibc 包。也不允许两条都只传二进制（glibc 在 Ubuntu 上会缺随包 `.so`）。glibc SHA256SUMS 只列出 `install/luckfox_lvgl_demo/` 顶层文件（对齐 pico `find -maxdepth 1`，现行为可执行文件本身），不含 `lib/` 下 so；uClibc 在 `build/` 内对可执行文件单文件校验，相对名为 `luckfox_lvgl_demo`（与 artifact 根一致），不是 `build/luckfox_lvgl_demo`。`lib/` 仍随 glibc artifact 上传。

RPATH 是写进 ELF 的运行时搜库路径，动态链接器按它找 `.so`。glibc 行 `CMAKE_INSTALL_RPATH=$ORIGIN/lib`，用二进制旁边随包的 `lib/`；uClibc 行 `CMAKE_INSTALL_RPATH=/usr/lib`，用板上根文件系统里的库。

`lib/uclibc/` 是本仓 vendored 的 `libdrm` / `libcjson`（uClibc ABI），给链接期用，不是 luckfox-pico SDK / sysroot 里的 so。uClibc 二进制实测 `NEEDED`：`libdrm.so.2`、`libcjson.so.1`、`libc.so.0`，`RPATH=/usr/lib`。官方 Ultra W Buildroot（`luckfox_pico_w_defconfig`）已 `BR2_PACKAGE_LIBDRM=y`、`BR2_PACKAGE_CJSON=y`，板上 `/usr/lib` 应有同名库；按 README 只传二进制即可，不把本仓 `lib/uclibc/` 打进 artifact。把 so 和二进制放一起也找不到（RPATH 不是 `$ORIGIN`）。真机未跑；若板上缺库，应装进 `/usr/lib` 或改 CMake RPATH（本 PR 不改 CMake）。

workflow 上传路径：glibc `install/luckfox_lvgl_demo/` 整目录；uClibc `build/luckfox_lvgl_demo` 与 `build/SHA256SUMS`（摊平后包根为 `luckfox_lvgl_demo` + `SHA256SUMS`）。run [35520899952](https://github.com/yuangezhizao/luckfox_pico_lvgl_example/actions/runs/35520899952) 实测已上传：

- glibc：`luckfox_lvgl_demo`、`SHA256SUMS`、`lib/libcjson.so{,.1,.1.7.15}`、`lib/libcjson_utils.so{,.1,.1.7.15}`、`lib/libdrm.so{,.2,.2.4.0}`、`lib/libdrm_amdgpu.so{,.1,.1.0.0}`、`lib/libdrm_etnaviv.so{,.1,.1.0.0}`、`lib/libdrm_freedreno.so{,.1,.1.0.0}`、`lib/libdrm_nouveau.so{,.2,.2.0.0}`、`lib/libdrm_radeon.so{,.1,.1.0.1}`
- uClibc：`luckfox_lvgl_demo`、`SHA256SUMS`

无 `dev` 签名时 dest-lock 失败，`build-demo` 不跑，后续 PR run 不再出 artifact。

**Q10：两个隔离 job 还是 matrix？作者想用 matrix，因 luckfox-pico 用过。**
A：**用 `strategy.matrix.include` 两行，`fail-fast: false`。** pico 已按行切换 `board`/`required_extra`。本仓按行切换 `libc`；uClibc 多出来的 sparse checkout 写成 `if: matrix.libc == 'uclibc'`，与按行切换字段是同一手法。两行共用本仓 GHCR 容器。拆成两个独立 job 会把 checkout / cmake / 校验 / 上传抄两遍，没有好处。YAML 以仓库 workflow 为准。

## 11. 参考资料

- 官方中文《交叉编译》：https://wiki.luckfox.com/zh/Luckfox-Pico-RV1106/Cross-Compile
- 官方英文 RGB 屏幕：https://wiki.luckfox.com/Luckfox-Pico-Ultra/RGB-Screen/
- 官方 luckfox-config（RGB/触摸仅 Ultra / Ultra W）：https://wiki.luckfox.com/Luckfox-Pico-RV1106/Peripherals/Luckfox-config/
- luckfox-pico 固件 CI 规格：https://github.com/yuangezhizao/luckfox-pico/blob/dev/docs/superpowers/specs/2026-07-17-luckfox-github-actions-ci-design.md
- luckfox-pico 固件 CI workflow：https://github.com/yuangezhizao/luckfox-pico/blob/dev/.github/workflows/build-luckfox-pico-firmware.yml
- 本仓 `CMakeLists.txt`、`README.md` / `README_CN.md`、`.cursor/Dockerfile`、`custom/custom.h`
