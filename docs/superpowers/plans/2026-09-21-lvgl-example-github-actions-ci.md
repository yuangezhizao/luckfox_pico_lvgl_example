# Luckfox Pico LVGL Example — GitHub Actions 交叉编译 CI 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为本仓新增 GitHub Actions 交叉编译流水线，按 README 两条路径分别编出 glibc / Ubuntu 与 uClibc / Buildroot 的 `luckfox_lvgl_demo`，做 ABI 门禁后上传 artifact。

**Architecture:** 同构 luckfox-pico #2+#3：本仓 `.cursor/Dockerfile` → GHCR digest → `build-demo` 的 `container:`；`strategy.matrix.include` 两行切 `libc`。uClibc gcc 不进镜像，CI 时 sparse checkout `yuangezhizao/luckfox-pico@dev` 的 `tools/linux/toolchain/`。不搬 `./build.sh`、三板固件、`--user 0`、gitlink 注释、产物清单 `if: always()`、`df -h`、buildroot 缓存。

**Tech Stack:** GitHub Actions（`ubuntu-24.04`）、GHCR、`actions/attest@v4`、CMake、`arm-linux-gnueabihf-`（镜像 apt）、Rockchip `arm-rockchip830-linux-uclibcgnueabihf-`（SDK git）。

**Spec:** [`docs/superpowers/specs/2026-09-21-lvgl-example-github-actions-ci-design.md`](../specs/2026-09-21-lvgl-example-github-actions-ci-design.md)

## Global Constraints

每个 Task 默认包含 spec 全文。硬约束：

- 设计以 spec 为准：FR1–FR8、NFR1–NFR5、D1–D12、§5 结构、§6 判据、§9 安全边界
- 不改 `CMakeLists.txt`；不补 uClibc 的 `make install`；不做 native 渲染；不在 CI 执行 ARM 二进制；不引入 submodule；不把本仓 `container:` 换成 `luckfox-pico-ci`
- 镜像名 `ghcr.io/<小写 owner>/luckfox-lvgl-demo-ci`；tag = `sha256sum .cursor/Dockerfile` 前 32 位；下游只用 `@sha256:…` digest
- glibc 行只导出 `GLIBC_COMPILER=/usr/bin/arm-linux-gnueabihf-`；uClibc 行只导出 `LUCKFOX_SDK_PATH=$GITHUB_WORKSPACE/luckfox-pico`；两行独立 `build/`
- glibc 产物：`install/luckfox_lvgl_demo/` 整包，SHA256SUMS 只含该目录顶层文件；uClibc 产物：只上传 `build/luckfox_lvgl_demo` 与其 SHA256SUMS（若 cmake 实际路径不同，以该行实测为准并回填「与计划的偏离」）
- 板上验收对象是 **uClibc** artifact；不得把 glibc 产物说成可在当前 Buildroot 上运行
- 所有 `checkout`：`persist-credentials: false`；不加 `container.options: --user 0`
- 不把凭据写入 workflow / Dockerfile；只用作业内 `GITHUB_TOKEN`
- 实现提交序：`ci(github-actions)`（workflow + `AGENTS.md`）在前；全部 `docs/superpowers/` 修改在最后单个提交
- 已完成 Task 只保留落地文件与勾选步骤，不重复已写入仓库的正文（workflow / `AGENTS.md` / 提交说明以 git 为准）
- 未在真机跑的步骤不得标为通过；未跑 CI 前不得把 FR8 写成已通过
- 分支仅 `cursor/lvgl-github-actions-ci-3efc`；不另开 PR；不 force-push `dev`

## File Structure

| 文件 | 责任 |
|---|---|
| `.github/workflows/build-luckfox-lvgl-demo.yml` | 流水线全文：`build-image` + `build-demo` matrix |
| `AGENTS.md` | 一条 CI 口径（两条 libc、板上用 uClibc、CI 不能代替真机） |
| `docs/superpowers/specs/2026-09-21-lvgl-example-github-actions-ci-design.md` | 设计口径 |
| `docs/superpowers/plans/2026-09-21-lvgl-example-github-actions-ci.md` | 本文件：Task、完成情况、验证证据 |

---

### Task 1: 新增 workflow（已完成）

**落地文件（以仓库为准，不在此重复 YAML / RUN 正文）：**

- Create: `.github/workflows/build-luckfox-lvgl-demo.yml`（`build-image` dest-lock + `build-demo` matrix 两行）

**Interfaces:** 消费 spec §5、D11/D12、FR1–FR7；产出 `image=ghcr.io/<owner>/luckfox-lvgl-demo-ci@sha256:<64hex>`；artifact 名 `luckfox_lvgl_demo_${{ matrix.libc }}-${{ matrix.rootfs }}_on-${{ matrix.os }}`。

- [x] **Step 1:** 新增 workflow（触发、权限、`persist-credentials: false`、PR 不构建/不推镜像）
- [x] **Step 2:** 本地 `python3` + PyYAML 解析该文件

```bash
python3 -c 'import yaml,pathlib; yaml.safe_load(pathlib.Path(".github/workflows/build-luckfox-lvgl-demo.yml").read_text())'
```

Expected: exit 0。实测见「验证证据」。

### Task 2: `AGENTS.md` 增加 CI 口径（已完成）

**落地文件（以仓库为准，不在此重复小节正文）：**

- Modify: `AGENTS.md`（`### 构建` 与 `### Lint` 之间的 `### GitHub Actions（交叉编译门禁）`）

**Interfaces:** 消费 spec §8、§9、Q2；恰好一段 CI gotcha，不改构建命令。

- [x] **Step 1:** 插入 CI 口径（两条 libc、板上 uClibc、CI ≠ 真机、sparse 工具链、镜像只在 dev 构建）
- [x] **Step 2:** 范围：`GitHub Actions` 标题仅一处；构建节原有 glibc 命令仍在

### Task 3: 本地 glibc 冒烟（已完成；代理证明，不是 FR8）

**Files:** 无仓库文件。`build/` 与 `install/` 已被 gitignore。

**Interfaces:** 消费 glibc 路径与 ABI 断言；不设置 `LUCKFOX_SDK_PATH`。不要执行该 ARM 二进制。

- [x] **Step 1:** Cloud Agent 容器内 README Ubuntu 路径

```bash
set -euo pipefail
unset LUCKFOX_SDK_PATH || true
export GLIBC_COMPILER=/usr/bin/arm-linux-gnueabihf-
rm -rf build install
mkdir -p build
cd build && cmake .. && make -j"$(nproc)" && make install
test -s ../install/luckfox_lvgl_demo/luckfox_lvgl_demo
readelf -h ../install/luckfox_lvgl_demo/luckfox_lvgl_demo | grep ELF32
readelf -h ../install/luckfox_lvgl_demo/luckfox_lvgl_demo | grep ARM
readelf -d ../install/luckfox_lvgl_demo/luckfox_lvgl_demo | grep 'NEEDED.*libc.so.6'
! readelf -d ../install/luckfox_lvgl_demo/luckfox_lvgl_demo | grep 'NEEDED.*libc.so.0'
ls ../install/luckfox_lvgl_demo/lib/*.so*
```

Expected: 全过。实测见「验证证据」glibc 行（CI artifact 同口径）。

### Task 4: 实现提交（已完成）

**Files:** Task 1–2 的仓库文件。提交说明以 `git log` 为准，不在此重复。

- [x] **Step 1:** 未改 CMake、无 submodule、无写入凭据
- [x] **Step 2:** `ci(github-actions)` 提交（workflow + `AGENTS.md`）在 docs 提交之前

### Task 5: CI 验收与 FR8（交叉编译已完成；真机未做）

**Files:** sparse 未扩（`tools/linux/toolchain/` 够用）。

**Interfaces:** 消费已推送的 workflow；产出 run URL、digest、两行 artifact、FR8 结论。`pull_request` 只 dest-lock 复用，不构建镜像。

- [x] **Step 1:** 推送到 `cursor/lvgl-github-actions-ci-3efc`（不 force-push `dev`）
- [x] **Step 2:** `build-image` digest / attestation（首测 PR 新建；现行口径下 signer 非 dev，见偏离）
- [x] **Step 3:** glibc 行 artifact + `libc.so.6`
- [x] **Step 4: FR8** `cmake`/`make` 成功且 `NEEDED libc.so.0`、无 `libc.so.6`；sparse 未扩；日志有 `luckfox-pico SHA=` 与 `gcc -print-sysroot=`

判定（失败时用，不是「够用」的预先声明）：

| 结果 | 处理 |
|---|---|
| `cmake`/`make` 成功且 ABI 为 `libc.so.0` 或 `ld-uClibc.so.1`、不含 `libc.so.6` | 当前 sparse 树足够 |
| 找不到 `ld` / `as` / `cc1` / `sysroot` / `specs`，或 gcc 缺 loader | **不是**板上 ABI。扩 sparse 或关掉该前缀 sparse |
| `NEEDED libc.so.6` | 环境变量隔离坏了。修 workflow，不要扩 sparse |
| 执行 ARM 二进制得到 `Exec format error` | 与 FR8 无关；不要在 CI 里跑该文件 |

- [x] **Step 5: 两行隔离** 不以日志 `export VAR=` 字面量为据；glibc 编译器 `/usr/bin/arm-linux-gnueabihf-gcc` + `libc.so.6`；uClibc 为 Rockchip gcc + `libc.so.0`
- [ ] **Step 6: 真机** 未部署到 Ultra W + 480×480 Buildroot。只传 uClibc artifact。

### Task 6: 回填证据（已完成）

**落地文件（以本文件与 spec 为准，不重复提交说明）：**

- Modify: 本 plan「验证证据」「与计划的偏离及原因」「执行记录」
- Modify: spec 结构口径随现行 dest-lock / PR 不构建镜像更新

**Interfaces:** 消费 Task 5 实测；不改 D1–D12 设计结论（D4 触发理由随 dest-lock 口径更新）。未跑格子写「未做」。

- [x] **Step 1:** 回填验证证据
- [x] **Step 2:** `docs(superpowers)` 为最后单个提交

---

## Execution Handoff

无剩余施工。Task 1–6 已完成（真机 / dev 复用 / `**.md` ignore / `schedule` 见验证证据「未做」）。

## 完成情况总结

| 交付物 | 状态 |
|---|---|
| Task 1 workflow | ✅ |
| Task 2 `AGENTS.md` | ✅ |
| Task 3 本地 glibc 冒烟 | ✅ |
| Task 4 实现提交 | ✅ |
| Task 5 CI / FR8 | ✅（交叉编译与 ABI；真机未做） |
| Task 6 回填证据 | ✅ |

**整体结论：** 规格已实施。真机、dev 上 provenance 复用、`**.md` ignore、schedule 仍未做。

---

## 验证证据

通过标准见 spec §6。

| 项 | 期望 | 实测 |
|---|---|---|
| YAML 可解析 | `python3`+PyYAML 退出 0 | 本地 `PARSE_OK`；CI 已解析并跑 |
| `pull_request(dev)` 跑起来 | 草稿 PR 有 `build-image`；dev 签名存在时两行 `build-demo` | 首测 run [35520899952](https://github.com/yuangezhizao/luckfox_pico_lvgl_example/actions/runs/35520899952) `success`（当时 PR 新建镜像）。现行口径下无 dev 签名则 `build-image` 红：run [35523562987](https://github.com/yuangezhizao/luckfox_pico_lvgl_example/actions/runs/35523562987) |
| `build-image` digest | `sha256:` + 64hex | 首测 `sha256:6d107c00148c66caa5261fdfb3de3155484dd2b6b01235f93f96fff952ea633e` |
| 新建 attestation | dev 上 `rebuilt=true` 时有 attest | 首测 attestation [48769532](https://github.com/yuangezhizao/luckfox_pico_lvgl_example/attestations/48769532)（signer=`refs/pull/4/merge`，不能当 dev 复用） |
| 复用 verify（signer=dev） | 合并 `dev` 且重签之后 | 未做 |
| glibc artifact | ELF32 ARM、`libc.so.6`、含 `lib/*.so*` | `luckfox_lvgl_demo_glibc-ubuntu_on-ubuntu-24.04`（ID `10608795039`，1942471 bytes）；文件清单见 spec Q9 |
| uClibc artifact | ELF32 ARM、`libc.so.0` 或 `ld-uClibc.so.1`、无 `libc.so.6` | `luckfox_lvgl_demo_uclibc-buildroot_on-ubuntu-24.04`（ID `10608123167`，626832 bytes）；仅 `luckfox_lvgl_demo` + `SHA256SUMS`，见 spec Q9 |
| FR8 sparse 够用 | 该行 `cmake`/`make` 成功，或已按报错扩路径后成功 | `cmake`/`make` 成功；sparse 未扩；`luckfox-pico SHA=d4cd3f7523440fad218388e7602ed63d45d58c81` |
| 环境变量隔离 | 各行只生效一个编译器（不以日志 `export` 字面量为据） | glibc：`/usr/bin/arm-linux-gnueabihf-gcc` + `libc.so.6`；uClibc：Rockchip gcc 8.3.0 + `libc.so.0` |
| `**.md` ignore | 纯 Markdown push 不触发 | 未做 |
| `schedule` 跳过 `build-demo` | 月度或等价手动验证 | 未做 |
| 真机 Ultra W + 480P Buildroot | 只跑 uClibc 二进制；板上 `/usr/lib` 应有 `libdrm.so.2` / `libcjson.so.1` / `libc.so.0`（`luckfox_pico_w_defconfig` 已开 `BR2_PACKAGE_LIBDRM` / `CJSON`）；不把本仓 `lib/uclibc/` 打进 artifact | 未做；口径见 spec Q9 |

## 与计划的偏离及原因

- uClibc 工具链 `bin/` 用完整 `ls -la`，不用 `head`（`pipefail` + `ls | head` 会 SIGPIPE；也要看全目录）。
- 隔离检查不以 job 日志里的 `export VAR=` 字面量为据；以编译器路径与 `NEEDED` ABI 为准。
- `pull_request` 只 dest-lock 复用，失败或 tag 不存在均 exit 1，不构建、不覆盖 GHCR；可信镜像只在 dev 上构建（对齐 pico dest-lock / fail loud）。事件名用内置 `GITHUB_EVENT_NAME`，不另设 `EVENT_NAME`；inspect 失败不得走 pico 那种可信重建。
- `paths-ignore: '**.md'` 只写在 `push(dev)` 上（对齐 pico）；`pull_request` 无 ignore，每次都跑 dest-lock / 编译门禁。
- `persist-credentials: false` 只保留键值，不加 pico 的 gitlink 注释。
- glibc SHA256SUMS 用 pico 同款 `find -maxdepth 1`，只校验 `install/luckfox_lvgl_demo/` 顶层文件；`lib/` 仍随包上传。uClibc 在 `build/` 内对 `luckfox_lvgl_demo` 单文件校验，相对名为 `luckfox_lvgl_demo`（与 artifact 根一致）；不在 `build/` 上套 `find -maxdepth 1`。
- 产物清单步骤无 `if: always()`。
- 提交序：`.github/` + `AGENTS.md` 为第一提交；全部 `docs/superpowers/` 为最后单个提交。
- 首测 run 35520899952 / 35522203019 的 attestation signer 为 `refs/pull/4/merge`，不是 dev；现行口径下不能当 dev 可信镜像复用。run [35523562987](https://github.com/yuangezhizao/luckfox_pico_lvgl_example/actions/runs/35523562987) dest-lock 失败即退出、未 `docker buildx --push`。

## 执行记录

| ID | 结论 | 依据 |
|---|---|---|
| P1 | `set -euo pipefail` 下 `ls | head` 会 SIGPIPE；改为完整 `ls -la`（看全 `bin/`） | 本地 `yaml.safe_load` PARSE_OK |
| P2 | `AGENTS.md`「作者当前系统」与 spec Q2 一致，不改人称 | 文内未改 |
| P3 | Actions 会回显整段 `run:`，未执行分支的 `export` 会出现在日志里 | 隔离以编译器路径 / ABI 为准 |
| P4 | spec §2 写「落地前没有 `.github/`」 | 与已落地状态一致 |
| P5 | 「验证证据」表填实测 run / digest / ABI；未跑项写未做 | 见上表 |
| P6 | PR 不构建、不覆盖 GHCR；无 dev 签名则 dest-lock `exit 1`。`paths-ignore` 只在 `push` 上 | run 35523562987：tag 已在、signer=`refs/pull/4/merge`、attest 步 skipped |
