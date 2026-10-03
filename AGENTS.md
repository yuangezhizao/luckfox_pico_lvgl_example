# AGENTS

## 交互约定

- 与本仓库交互时，请始终使用中文回复（代码、路径、命令除外）。

## Cursor Cloud specific instructions

> 说明：保留该英文章节标题作为工具约定锚点；下方内容使用中文。

### 项目简介
这是一个交叉编译的嵌入式 GUI 应用（C + LVGL 8.3 + lv_drivers 8.1，使用 CMake 构建），面向 `Luckfox Pico Ultra` ARM 开发板。项目库依赖（LVGL/lv_drivers/libdrm/libcjson）已 vendored 于 `lib/`；交叉工具链与 native 渲染验证依赖（`libdrm-dev`/`libcjson-dev` 等）由 `.cursor/Dockerfile` 安装。现在通过 `.cursor/environment.json`（引用 `.cursor/Dockerfile`）以 Dockerfile 模式提供可复现环境。构建命令见 `README.md`。

### Cloud Agent 环境（Dockerfile 模式 / 配置即代码）
- **当前活动**：`.cursor/environment.json` → `.cursor/Dockerfile`（自建 `ubuntu:24.04`，apt 安装 ARM 交叉工具链、CMake、native/SDL 验证依赖与 `qemu-user`）。
- **备选**：`.cursor/Dockerfile.luckfox_pico`（Luckfox 官方镜像 `luckfoxtech/luckfox_pico:1.0`）；切换只需把 `environment.json` 的 `build.dockerfile` 改为指向它。
- 按官方 resolution order，repo 级 `.cursor/environment.json` 优先于 personal / team saved environment，故通常无需任何 Dashboard 操作。
- grilling：`environment.json` 的 `install` 在创建 Environment Build 时把固定 commit `85f83d3` 的技能写入 `$HOME/.cursor/skills/grilling/SKILL.md`（不进 git）。新 Agent 须从捕获了该 install 的 Build 启动。

### 构建（标准开发流程）
这是一个仅支持交叉编译的项目：产出的二进制是 32 位 ARM。`CMakeLists.txt` 会提前调用 `return()`，除非你先导出下列变量之一，否则不会产生任何构建目标：
- `GLIBC_COMPILER` -> Ubuntu/glibc 目标（可在本环境 / Cloud Agent 容器中使用），或
- `LUCKFOX_SDK_PATH` -> Buildroot/uClibc 目标（板上实际运行的产物）。无需完整 SDK：按 CI 做法稀疏检出 `yuangezhizao/luckfox-pico@dev` 的 `tools/linux/toolchain`（约 233 MB）即可，`git clone --depth 1 --filter=blob:none --sparse -b dev https://github.com/yuangezhizao/luckfox-pico.git /tmp/luckfox-pico && git -C /tmp/luckfox-pico sparse-checkout set tools/linux/toolchain`，再 `env -u GLIBC_COMPILER LUCKFOX_SDK_PATH=/tmp/luckfox-pico cmake -S . -B /tmp/build-uclibc && make -C /tmp/build-uclibc -j`；构建目录放仓库外（`.gitignore` 只忽略 `build/`、`install/`）。

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
- CI 成功只证明交叉编译与 ABI 门禁通过，不能代替真机点亮；权威验证仍是物理 Luckfox Pico Ultra / Luckfox Pico Ultra W。
- uClibc gcc 不在 `.cursor/Dockerfile` 里；CI 从 `yuangezhizao/luckfox-pico@dev` sparse checkout `tools/linux/toolchain/`，不引入 submodule。
- CI 镜像只在 `dev` 上构建并 attest；`pull_request` 只复用 dest-lock（signer/source=`dev`）通过的 digest，失败即退出，不覆盖 GHCR。镜像标签取 `.cursor/Dockerfile` 内容哈希，所以修改该文件的 PR 在合并前 `build-image` 必然失败（找不到 dev 签名镜像），合并后 dev push 重建镜像才恢复。不把 `container:` 换成 `luckfox-pico-ci`（其中无 `gcc-arm-linux-gnueabihf`，uClibc gcc 也不在镜像层）。

### Lint（代码检查）
没有独立的 linter，也没有告警关卡（无 `-Werror`）。`CMakeLists.txt` 的告警段 `add_compile_options()` 位于 `add_executable()` 之前（`add_compile_options()` 只作用于其后创建的 target），并用 `-std=gnu99`（`-std=c99` 会关掉 POSIX/GNU 扩展声明而编译失败），主程序以 `-Wall -Wextra …` 编译：glibc 132 条、uClibc 131 条告警为既存基线，只记录不作门禁。另一处 `add_compile_options(-fPIC -Wall -O3 -g0)` 仍在 `add_executable()` 之后、未生效，主程序实际按 `-O0` 编译；LVGL 与 lv_drivers 子目录不受这两处影响，`C_FLAGS` 为空。

### 测试
`tests/` 是独立的主机原生 CMake 工程（gcc + ASan/UBSan、headless），与交叉编译互不影响：`cmake -S tests -B build-tests && cmake --build build-tests -j && ctest --test-dir build-tests --output-on-failure -j`。32 位换算测试需 `qemu-user`（`qemu-arm`）与 `arm-linux-gnueabihf-gcc` 同时存在，缺任一时自动跳过，两者都由 `.cursor/Dockerfile` 与 `.cursor/Dockerfile.luckfox_pico` 提供；CI 的 `native-tests` job 以 `-DLUCKFOX_TESTS_ARM32=ON` 强制运行。常规 UI 测试不调用 `custom_init()`（它会拉起 `mpv` 并做硬件相关初始化；`mpv` 的启动与连接由 `cases/music/` 的用例以假 `mpv` 单独测试）。用例按页面放在 `tests/cases/<页面>/`，分类、命名、新增步骤与改坏检验见 `tests/README.md`。

### 运行 / 验证 GUI（重要陷阱）
构建出的二进制无法在本 x86 云端 VM 上运行——它是 32 位 ARM ELF，在没有 binfmt_misc/qemu 的环境里直接执行会得到 `Exec format error`，连 `main()` 都进不去。即便架构可执行，它也面向真实硬件：启动时 `luckfox_get_drm_info()` 打开 `/dev/dri/card0`，打开失败或 `drmModeGetResources()` 失败即 `exit()`（本 VM 无该设备节点）；且只支持正方形屏，已连接显示器的首个 mode 非正方形同样 `exit()`。它还需要 `/dev/fb0` 和一个 evdev 触摸屏。请部署到物理 Luckfox Pico Ultra 上运行。uClibc 产物可用 `qemu-user` 冒烟（已在 `.cursor/Dockerfile` 中）：在仓库根目录执行 `qemu-arm -L "$(<uClibc gcc> -print-sysroot)" -E LD_LIBRARY_PATH=$PWD/lib/uclibc/libdrm:$PWD/lib/uclibc/libcjson <产物>`（`LD_LIBRARY_PATH` 须为绝对路径，相对路径会报 `can't load library 'libdrm.so.2'`）能加载 uClibc 与 vendored 库，输出含 `cannot open /dev/dri/card0`、`exit=1`，只证明 ABI 与动态库可用，看不到界面。

没有硬件时可用 `tests/` 的手动目标做 native 渲染：`cmake --build build-tests --target screenshots` 输出 480/720 主屏截图，`--target preview`（需 SDL2）后 `DISPLAY=:1 ./build-tests/preview` 开窗、鼠标当触摸。二者与测试共用 `tests/support/`：`generated/`、`custom/` 没有 `main()`，还 `extern` 引用仅在 `src/main.c` 定义的 5 个全局量，而 `src/main.c` 会拉入 DRM/fbdev/evdev 硬件驱动，所以由 `tests/support/app_env.c` 提供这 5 个定义与内存帧缓冲显示；编译单元与 include 路径的来历见 [`docs/superpowers/specs/2026-07-24-lvgl-example-cloudagent-env-design.md`](docs/superpowers/specs/2026-07-24-lvgl-example-cloudagent-env-design.md) §6.1。此为可选手段，权威验证仍以部署到物理 Luckfox Pico Ultra 为准。

板型判据另有一坑：SDK `824b817f` 起 Luckfox Pico Ultra 与 Luckfox Pico Ultra W 共用设备树，`/proc/device-tree/model` 恒为 `Luckfox Pico Ultra`，不能据此区分是否带 WiFi；例程按 `/sys/bus/sdio/devices/*/uevent` 中的 `SDIO_ID=C8A1:C18D`（板载 AIC8800DC，与 SDK `insmod_wifi.sh` 加载驱动所用 ID 相同）判定，仅旧固件 `model` 为 `Luckfox Pico Ultra W` 时直接视为带 WiFi。背景见 [`docs/superpowers/specs/2026-09-25-lvgl-example-luckfox-pico-ultra-w-wifi-detect-design.md`](docs/superpowers/specs/2026-09-25-lvgl-example-luckfox-pico-ultra-w-wifi-detect-design.md) §2。
