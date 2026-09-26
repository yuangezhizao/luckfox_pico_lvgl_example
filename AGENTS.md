# AGENTS

## 交互约定

- 与本仓库交互时，请始终使用中文回复（代码、路径、命令除外）。

## Cursor Cloud specific instructions

> 说明：保留该英文章节标题作为工具约定锚点；下方内容使用中文。

### 项目简介
这是一个交叉编译的嵌入式 GUI 应用（C + LVGL 8.3 + lv_drivers 8.1，使用 CMake 构建），面向 `Luckfox Pico Ultra` ARM 开发板。项目库依赖（LVGL/lv_drivers/libdrm/libcjson）已 vendored 于 `lib/`；交叉工具链与 native 渲染验证依赖（`libdrm-dev`/`libcjson-dev` 等）由 `.cursor/Dockerfile` 安装。现在通过 `.cursor/environment.json`（引用 `.cursor/Dockerfile`）以 Dockerfile 模式提供可复现环境。构建命令见 `README.md`。

### Cloud Agent 环境（Dockerfile 模式 / 配置即代码）
- **当前活动**：`.cursor/environment.json` → `.cursor/Dockerfile`（自建 `ubuntu:24.04`，apt 安装 ARM 交叉工具链、CMake 与 native/SDL 验证依赖）。
- **备选**：`.cursor/Dockerfile.luckfox_pico`（Luckfox 官方镜像 `luckfoxtech/luckfox_pico:1.0`）；切换只需把 `environment.json` 的 `build.dockerfile` 改为指向它。
- 按官方 resolution order，repo 级 `.cursor/environment.json` 优先于 personal / team saved environment，故通常无需任何 Dashboard 操作。
- grilling：`environment.json` 的 `install` 在创建 Environment Build 时把固定 commit `85f83d3` 的技能写入 `$HOME/.cursor/skills/grilling/SKILL.md`（不进 git）。新 Agent 须从捕获了该 install 的 Build 启动。

### 构建（标准开发流程）
这是一个仅支持交叉编译的项目：产出的二进制是 32 位 ARM。`CMakeLists.txt` 会提前调用 `return()`，除非你先导出下列变量之一，否则不会产生任何构建目标：
- `GLIBC_COMPILER` -> Ubuntu/glibc 目标（可在本环境 / Cloud Agent 容器中使用），或
- `LUCKFOX_SDK_PATH` -> Buildroot/uClibc 目标（需要数 GB 的 Luckfox SDK，本环境 / Cloud Agent 容器中没有）。

走 glibc 路径（工具链由 `.cursor/Dockerfile` 配置即代码提供）：
```
export GLIBC_COMPILER=/usr/bin/arm-linux-gnueabihf-
mkdir -p build && cd build && cmake .. && make -j && make install
```
`make install` 会把可部署目录写入 `install/luckfox_lvgl_demo/`（二进制 + 打包的 `lib/`）。`build/` 与 `install/` 均已被 gitignore。

交叉工具链由 `.cursor/Dockerfile` 安装（不再依赖 snapshot）；如需本地补装可 `sudo apt-get install -y gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf`。

### GitHub Actions（交叉编译门禁）
- CI 编两条：glibc（只设 `GLIBC_COMPILER=/usr/bin/arm-linux-gnueabihf-`，`make install` 整包 `install/luckfox_lvgl_demo/`）与 uClibc（只设 `LUCKFOX_SDK_PATH` 指向 CI 内 `yuangezhizao/luckfox-pico` 检出根，只出可执行文件）。
- 作者当前系统是 SDK 编的 Buildroot，板上使用 **uClibc** 产物；glibc 产物 ABI 不兼容（`libc.so.6` vs `libc.so.0`），不能在这块板上运行。
- CI 成功只证明交叉编译与 ABI 门禁通过，不能代替真机点亮；权威验证仍是物理 Luckfox Pico Ultra / Ultra W。
- uClibc gcc 不在 `.cursor/Dockerfile` 里；CI 从 `yuangezhizao/luckfox-pico@dev` sparse checkout `tools/linux/toolchain/`，不引入 submodule。
- CI 镜像只在 `dev` 上构建并 attest；`pull_request` 只复用 dest-lock（signer/source=`dev`）通过的 digest，失败即退出，不覆盖 GHCR。不把 `container:` 换成 `luckfox-pico-ci`（其中无 `gcc-arm-linux-gnueabihf`，uClibc gcc 也不在镜像层）。

### Lint（代码检查）
没有独立的 linter，也**没有生效的编译告警关卡**——注意这是个容易踩的坑：`CMakeLists.txt` 虽声明了 `-Wall -Wextra` 等一长串 `-W...` 标志，但两处 `add_compile_options()` 都写在 `add_executable()` **之后**，而它只对其后创建的 target 生效，故实测这些标志根本没进编译命令行（`build/**/flags.make` 的 `C_FLAGS` 为空；同一批里的 `-O3`/`-fPIC`/`-std=c99` 一并失效）。当前构建只有 gcc 默认级别的少量既存告警、没有错误；若要恢复告警关卡，需把这两处 `add_compile_options()` 上移到 `add_executable()` 之前。

### 测试
本仓库没有自动化测试套件。

### 运行 / 验证 GUI（重要陷阱）
构建出的二进制无法在本 x86 云端 VM 上运行——它是 32 位 ARM ELF，在没有 binfmt_misc/qemu 的环境里直接执行会得到 `Exec format error`，连 `main()` 都进不去。即便架构可执行，它也面向真实硬件：启动时 `luckfox_get_drm_info()` 打开 `/dev/dri/card0`，打开失败或 `drmModeGetResources()` 失败即 `exit()`（本 VM 无该设备节点）；且只支持正方形屏，已连接显示器的首个 mode 非正方形同样 `exit()`。它还需要 `/dev/fb0` 和一个 evdev 触摸屏。请部署到物理 Luckfox Pico Ultra 上运行。

没有硬件时可以选做 native 渲染验证：LVGL 是可移植的（`LV_COLOR_DEPTH` 为 32 / ARGB8888），把 `lib/lvgl/src` + `generated/` + `custom/` 在本机原生编译即可渲染 GUI-Guider 屏幕，headless 出图或 SDL 开窗都行。**最容易踩空的前提**：这批源文件里没有 `main()`，且 `custom/*.c`、`generated/*.c` 还 `extern` 引用了仅在 `src/main.c` 定义的 5 个全局量，所以必须自写 harness 提供 `main()` 与这 5 个定义，否则链接期必报 undefined reference；直接改编 `src/main.c` 也不行——它会拉入 `lv_drivers` 的 DRM/fbdev/evdev 硬件驱动。完整配方（编译单元、include 路径、显示后端选型、链接期依赖）见 [`docs/superpowers/specs/2026-07-24-lvgl-example-cloudagent-env-design.md`](docs/superpowers/specs/2026-07-24-lvgl-example-cloudagent-env-design.md) §6.1。此为**可选的临时手段、非标准流程**，权威验证仍以部署到物理 Luckfox Pico Ultra 为准。

板型判据另有一坑：SDK `824b817f` 起 Ultra 与 Ultra W 共用设备树，`/proc/device-tree/model` 恒为 `Luckfox Pico Ultra`，不能据此区分是否带 WiFi；例程按 `/sys/bus/sdio/devices/*/uevent` 中的 `SDIO_ID=C8A1:C18D`（板载 AIC8800DC，与 SDK `insmod_wifi.sh` 加载驱动所用 ID 相同）判定，仅旧固件 `model` 为 `Luckfox Pico Ultra W` 时直接视为带 WiFi。背景见 [`docs/superpowers/specs/2026-09-25-lvgl-example-ultra-w-wifi-detect-design.md`](docs/superpowers/specs/2026-09-25-lvgl-example-ultra-w-wifi-detect-design.md) §2。
