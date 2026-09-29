# Luckfox Pico LVGL Example — 原生测试基础设施设计规格

- **日期**：2026-09-28
- **状态**：已实现，验证证据见 plan
- **分支**：`cursor/native-tests-cbb9`
- **关联计划**：[`../plans/2026-09-28-lvgl-example-native-tests.md`](../plans/2026-09-28-lvgl-example-native-tests.md)。代码以仓库为准；plan 留 Task、证据与偏离。

## 1. 概述与目标

仓库没有自动化测试，主程序声明的编译告警也从未生效。PR #5、#6 的验证 harness、断言脚本与 32 位换算测试都放在仓库外，无法回归。本 PR 只建测试基础设施：恢复主程序编译告警，新增 `tests/` 原生 CMake 工程并纳入 PR #5、#6 的既有测试，把 `qemu-user` 补进两份 Dockerfile，接入 CI。既存缺陷的修复放在下一个 PR，已定结论见 §10。

目标：

- 主程序的 `-W…` 告警在 glibc 与 uClibc 两条交叉编译中生效，编译通过，产物运行时内容不变。
- `tests/` 在 x86 主机上以 gcc + ASan/UBSan headless 构建运行，按页面分类，新增测试有固定位置与命名规则。
- PR #5 的 SDIO 判定与 `wifi_backend_release()` 判空、PR #6 的亮度断言与 32 位换算，在最新 `dev` 上全部通过，并能抓住对应改坏。
- 新增的 `native-tests` job 与两条交叉编译一起为绿：改 Dockerfile 前的 PR run 与合并后的 dev push run（§6 第 6 条）。

非目标：修任何既存缺陷或新出现的告警；启用 `-O3` 或调整 LVGL 优化级别；加 `-Werror`；调用 `custom_init()`；把 `custom/*.c` 与系统调用解耦；提交会失败的测试。

## 2. 背景与依据

### 2.1 例程的 LVGL 运行链路

- LVGL 8.3.10 与 lv_drivers 8.1 均 vendored 于 `lib/`，`LV_COLOR_DEPTH 32`（ARGB8888），`LV_MEM_CUSTOM 1`（对象内存走 `malloc`）。
- 入口 `src/main.c`：`custom_init()` → 板型与音乐目录探测 → `luckfox_get_drm_info()` 只从 `/dev/dri/card0` 读首个 mode 的分辨率并算出 `SCALE = hdisplay / 480`（480 屏 1.0、720 屏 1.5，非正方形即退出）→ `lv_init()` → `fbdev_init()` 直接写 `/dev/fb0` → 两块半屏绘制缓冲在 `main()` 栈上 → `evdev_init()` 固定读 `/dev/input/event0` → `setup_ui()`、`events_init()` → 主循环 `lv_timer_handler()` 加 `usleep(5000)`，直到 `QUIT_FLAG` 置位。
- tick 由 `LV_TICK_CUSTOM` 调 `custom/custom_main.c` 的 `custom_tick_get()`，基于 `gettimeofday`。
- 界面由 GUI Guider 导出（`generated/`，已手改，无 `.guiguider` 工程文件）；`custom/*.c` 把界面逻辑与系统调用混在一起（mpv 套接字、`popen("wpa_cli")`、背光 sysfs），并 `extern` 引用只在 `src/main.c` 定义的 5 个全局量 `guider_ui`、`QUIT_FLAG`、`SCALE`、`WIFI_ENABLE`、`MUSIC_ENABLE`。
- 原生运行的前提：`src/main.c` 会拉入 DRM/fbdev/evdev 硬件驱动，不能直接编；测试须自带 `main()` 与这 5 个全局量，编 `lib/lvgl/src`、`generated/`、`custom/`（配方见 [`2026-07-24-lvgl-example-cloudagent-env-design.md`](2026-07-24-lvgl-example-cloudagent-env-design.md) §6.1）。

### 2.2 编译告警为何未生效

- `CMakeLists.txt` 两处 `add_compile_options()` 写在 `add_executable()` 之后。`add_compile_options()` 只作用于其后创建的 target，故主程序 `flags.make` 的 `C_FLAGS` 为空，`-Wall -Wextra …`、`-fPIC -O3 -g0`、`-std=c99` 全部失效，主程序实际按 `-O0`、编译器默认标准编译。LVGL 与 lv_drivers 由更早的 `add_subdirectory()` 创建，本就不受这两处影响。
- 只上移不改标准会编译失败：`-std=c99` 关闭 POSIX/GNU 扩展声明，`S_IFDIR`、`DT_REG`、`DT_CHR`、`SA_RESTART` 未定义，`struct sigaction` 不完整，`popen`、`usleep`、`vfork` 变为隐式声明，glibc 下共 9 个 error；改为 `-std=gnu99` 后编译通过。

### 2.3 已有测试素材

| 来源 | 素材 | 依赖 | 本 PR 处置 |
|---|---|---|---|
| PR #5 plan Task 2 | SDIO 判定 harness：7 个用例（absent/empty/func1_only/ultra_w/hidden/no_newline/prefix），以 `sed` 从源码抽出 `luckfox_sdio_has_id()` | host gcc | 纳入，改为经 `unit_sdio_detect.c` `#include` 源文件 |
| PR #5 plan Task 2 | `_lv_ll_remove(&ll, NULL)` 段错误复现程序，证明 `lv_timer_del(NULL)` 会崩 | host gcc | 改写为 `wifi_backend_release()` 判空回归测试 |
| PR #5 plan Task 2 | 双重 `fclose` 演示 | host glibc | 不纳入（演示 libc 行为，不测本仓代码） |
| PR #6 plan Task 1 | 原生预览 harness（headless 出 PPM，`--sdl` 开窗） | SDL2 | 拆为 `tests/support/` 与截图、预览两个手动目标 |
| PR #6 plan Task 5 | `check.sh` 11 项亮度断言 | `strace`、root、`su nobody` | 改写为 C 内断言 |
| PR #6 plan Task 5 | `mutate.sh` 与 9 个改坏变体（M1–M7、M10、M11） | bash、python3 | 纳入为手动目标 |
| PR #6 plan Task 13 | `u32test.c` 32 位换算测试 6 项（另打印 `sizeof(long)`） | `arm-linux-gnueabihf-gcc`、`qemu-user` | 纳入，另加 `sizeof(long) == 4` 断言共 7 项，进 CI |

### 2.4 CI 镜像约束

- workflow 的 CI 镜像标签取 `.cursor/Dockerfile` 内容的 sha256 前 32 位。`pull_request` 只复用 dev 上构建并经 dest-lock 验证的镜像，标签不存在即失败退出（workflow `build-image` 步骤中 `pull_request` 分支）。
- 因此任何修改 `.cursor/Dockerfile` 的 PR，其 CI 的 `build-image` 必然失败，下游 job 全部无法运行，直到合并进 dev 重建镜像。
- `dev` 现有镜像不含 `qemu-user`，已有 `build-essential`、`cmake`、`gcc-arm-linux-gnueabihf`、`libdrm-dev`、`libcjson-dev`、`libsdl2-dev`，容器默认以 root 运行。
- 本 PR 的应对：`qemu-user` 进两份 Dockerfile，接受合并前 PR CI 失败，结论与被否决的替代见 D8。

### 2.5 `custom_init()` 在测试中不可调用

`custom_init()` 无条件调用 `music_player_thread_init()`，其 `vfork()` 子进程在 `execlp("mpv", …)` 失败时执行 `return 0;`，破坏父进程栈；主机与 CI 无 `mpv`，调用后进程段错误（最新 dev 上实测退出码 139）。修复属于下一个 PR（§10），本 PR 的测试不调用 `custom_init()`，由 `tests/support/` 自行设置 5 个全局量。

### 2.6 参考 ESP-Pocket2 的 PC 模拟器

[`yuangezhizao/ESP-Pocket2`](https://github.com/yuangezhizao/ESP-Pocket2) 的 PR5 是 ESP32 在 QEMU 里跑 RGB 帧缓冲，与本仓无关；PR6 的 `pc_simulator/` 是独立 CMake 工程，FetchContent 拉取 LVGL 9.5，用 LVGL 9 内置的 `lv_sdl_window_create`、`lv_sdl_mouse_create` 开窗并以鼠标模拟触摸，界面先解耦为可移植的 `ui_app` 并注入时钟源，构建时用 venv 跑 `LVGLImage.py` 生成图片；PR8 的 `pc_simulator_lgfx/` 基于 LovyanGFX 的 SDL 面板。

- 借鉴：独立的 PC 端 CMake 子工程，与固件构建互不干扰；SDL2 先按 config 模式找、找不到再退回（本仓退回 pkg-config：CMake 只自带 SDL1 的 `FindSDL`，没有 `FindSDL2` 模块，Ubuntu `libsdl2-dev` 同时提供 `sdl2-config.cmake` 与 `sdl2.pc`）；README 写依赖、构建运行、交互方式、与真机的对照与云端截图方法。
- 不照搬：LVGL 8.3 没有 `lv_sdl_window_create`；vendored 的 `lib/lv_drivers/sdl/sdl.c` 可用，但分辨率等参数来自与产品共用的 `lib/lv_drv_conf.h`（`USE_SDL 0`、`SDL_HOR_RES 480`、`SDL_VER_RES 320`），启用须在测试构建里覆盖该配置，不如沿用 PR #6 harness 自写的 SDL flush；FetchContent 会让测试跑的不是产品实际用的 `lib/lvgl`，且引入网络依赖；图片已是 `generated/images/*.c`，无需生成；`ui_app` 式解耦是对 `custom/*.c` 的重构，超出本 PR。
- LovyanGFX 方案不适用：本仓不用 LovyanGFX。

### 2.7 测试组织的通行做法

- LVGL 上游 `release/v8.3` 把用例平铺在 `tests/src/test_cases/test_<控件>.c`；`master` 用例增多后按类别拆为 `test_cases/{widgets,draw,libs,refr,cache,public_api,3d}/`，文件名仍为 `test_<对象>.c`；公共代码单独放 `tests/src/lv_test_*.c`，与用例分开。
- Zephyr 的测试目录镜像被测子系统（`tests/<子系统>/<组件>`），测试函数以 `test_` 开头，测试标识用点分层级（如 `kernel.semaphore.stress`）。
- CMake 推荐 `add_test(NAME …)` 签名；测试属性只能在创建该测试的目录里设置；分组用 `LABELS`，按标签筛选 `ctest -L`、按名字正则筛选 `ctest -R`。
- 两者都不给测试文件加序号或日期前缀：测试须互不依赖，`ctest -j` 并行运行，序号暗示顺序且插入新测试要重新编号；日期前缀只用于 `docs/superpowers/specs|plans` 这类按时间记录的文档，测试文件的时间由 git 历史给出。

## 3. 需求

- FR1：`CMakeLists.txt` 只把第二处 `add_compile_options(…)`（告警段）移到 `add_executable()` 之前，并把其中 `-std=c99` 改为 `-std=gnu99`；第一处 `add_compile_options(-fPIC -Wall -O3 -g0)` 原位不动。新出现的告警只记录为基线，不修。
- FR2：新增 `tests/` 原生 CMake 工程，结构、分类与命名按 §5.2。
- FR3：`tests/support/` 提供 5 个全局量定义、headless 显示与指针、脚本化触摸、stdout/stderr 捕获、假文件目录与断言宏（§5.4）。
- FR4：纳入 §5.5 所列用例，在最新 `dev` 代码上全部通过。
- FR5：手动目标 `screenshots`、`preview` 与 `tools/mutate.sh`（§5.6），不进 CI、不注册为 ctest。
- FR6：workflow 新增 `native-tests` job（§5.7）。
- FR7：`AGENTS.md` 按 §5.8 更新。
- FR8：PR #5、#6 的 plan/spec 中已入库测试的完整正文改为指向仓库的一句要点（§5.9）。
- FR9：`.cursor/Dockerfile` 与 `.cursor/Dockerfile.luckfox_pico` 补装 `qemu-user`（D8）。
- NFR1：除 FR1 的 `CMakeLists.txt` 外不改生产代码（`src/`、`custom/`、`generated/`、`lib/`）；两份 Dockerfile 只补装 `qemu-user`。
- NFR2：测试构建与运行不访问网络；CI 的 `native-tests` job 除检出本仓外不访问网络，所需工具全部来自 CI 镜像。
- NFR3：测试代码不引入 `strcpy`/`strcat`/`sprintf`/`gets`，路径格式化用 `snprintf` 并检查截断。
- NFR4：测试不依赖 root、`strace`、`su` 或真实设备节点；可在 CI 容器（root）与普通用户下运行。

## 4. 设计决策

| 决策 | 结论 | 理由 |
|---|---|---|
| D1 告警提交范围 | 只上移告警段并改 `-std=gnu99`；`-O3` 行不动，作为本 PR 第一个提交 | 一起上移会让主程序首次以 `-O3` 编译，中间提交都在未修的 UB（画布悬空 VLA、tick 32 位溢出）上优化，且改变真机性能与行为；启用 `-O3` 属性能变更，需与 LVGL 子目录优化级别一并处理并真机回归，留给后续 PR（Q3） |
| D2 测试入库 | 测试放仓库 `tests/`，随修复一起回归 | 仓库外 harness 随 VM 过期丢失、无法进 CI；否决「只在仓库外保留证据」与「纯逻辑入库、GUI 场景留仓库外」（Q2） |
| D3 断言框架 | 纯 C 极简断言宏 + CTest | 用例少、零依赖，与 PR #5、#6 写法一致；否决 vendored Unity/cmocka（Q12） |
| D4 访问 `static` 函数 | 配套的 `unit_<对象>.c` `#include` 被测 `.c` 并导出 `tst_` 前缀的包装函数，随被测源码以 `-w` 编译；测试文件只调包装函数，以 `-Werror` 编译；被测源码编成静态库 | 不改生产代码；被测源码的既存告警不影响测试代码的严格编译；静态库成员只在有未解析符号时才被链接，`unit_` 文件已定义该 `.c` 的全部符号，库内同名目标文件不会被拉入，不会重复定义；PR #6 32 位测试已用 `#include` 法（Q14） |
| D5 用例隔离 | 依赖界面或模块静态状态的用例，每个用例一个进程（同一可执行文件按参数选用例，各注册为一条 ctest） | `setup_ui()` 创建的屏幕与 `custom_brightness.c` 的静态状态无法重置 |
| D6 假背光目录 | 静态库中的 `custom_brightness.c` 以 `-include fake_fs.h -DBACKLIGHT_SYSFS_DIR=tst_backlight_root()` 编译，函数返回本进程 `mkdtemp` 所建临时目录下的 `sys/class/backlight` | `custom_brightness.c` 只把该宏当表达式用（`opendir`、`snprintf` 参数），不与字面量拼接；每个进程独立目录，`ctest -j` 并行不冲突；`-include` 让函数有原型，避免隐式声明把指针截成 `int` |
| D7 替代 `su nobody` 与 `strace` | 写入失败：控件创建后把 `brightness` 换成指向 `/dev/full` 的符号链接；写入次数：inotify 同时监听 `IN_OPEN` 与 `IN_CLOSE_WRITE`，只计后者 | root 身份下 `chmod 444` 挡不住写入，`/dev/full` 对任何用户都返回 `ENOSPC`；inotify 是纯 C、无需 ptrace；内核会把队列末尾相邻的相同事件合并，只监听 `IN_CLOSE_WRITE` 时拖动结束后读取恒得 1 次，加监听 `IN_OPEN` 使事件交替出现、不被合并（Q14） |
| D8 32 位测试进 CI | `qemu-user` 进 `.cursor/Dockerfile` 与 `.cursor/Dockerfile.luckfox_pico`，`native-tests` job 直接使用 CI 镜像；代价是本 PR 合并前 PR CI 的 `build-image` 必然失败（§2.4），两条交叉编译与原生测试都不运行，合并后 dev push 重建镜像即恢复 | 合并后 CI 与 Cloud Agent 环境都不再依赖临时安装；两份都改，因为备选镜像同样装了 `gcc-arm-linux-gnueabihf`，切换环境后 32 位测试照样可跑；否决 job 内 `apt-get install`（每次运行依赖外网与镜像源，且还要另开 PR 迁入 Dockerfile）、先单独开 PR 改 Dockerfile（多一轮合并）、「32 位测试只作手动目标」（失去 32 位回归）（Q10） |
| D9 目录与命名 | 按页面分类的 `tests/cases/<页面>/`，`test_<对象>.c`，无序号/日期前缀，CTest 名 `<类别>.<对象>.<用例>` | §2.7；§5.2（Q17） |
| D10 交互式预览 | 本 PR 加手动目标 `preview`（仅找到 SDL2 时生成） | 与截图目标共用 `tests/support/`，增量小；修缺陷时可直接开窗复现，省去临时 harness；否决另开 PR 做顶层 `pc_simulator/`（Q16） |
| D11 CI 接入 | 同一 workflow 新增 job，复用 `build-image` 输出的镜像，`schedule` 触发时跳过 | 与既有两条交叉编译共用可信镜像与触发条件 |
| D12 失败测试 | 本 PR 只提交在 dev 上通过的测试；未修缺陷的失败测试随修复 PR 提交 | 避免 CI 常红 |
| D13 `AGENTS.md` | 同步更新 | 合并后「告警未生效」「无测试套件」两处变为错误信息，每轮自动加载会误导后续会话（Q11） |
| D14 旧文档精简 | 单独一个 `docs(superpowers)` 提交，位于本 PR spec/plan 提交之前 | 与「新增本 PR spec/plan」是两个主题（Q9） |

## 5. 设计

### 5.1 第一个提交：恢复编译告警

改动：告警段 `add_compile_options(…)` 整段移到 `add_executable()` 之前，`-std=c99` 改为 `-std=gnu99`；`# debug` 注释与 `add_compile_options(-fPIC -Wall -O3 -g0)` 留在 `add_executable()` 之后，继续不生效。

改动前后在仓库外以同一路径分别构建，对比如下：

| 目标 | 告警（前 → 后） | error | 产物对比 |
|---|---|---|---|
| glibc（`arm-linux-gnueabihf-gcc` 13.3.0） | 4 → 132 | 0 | 逐字节一致（3 171 276 字节）；77 个目标文件反汇编一致 |
| uClibc（`arm-rockchip830-linux-uclibcgnueabihf-gcc` 8.3.0，工具链 `yuangezhizao/luckfox-pico@dev` `d4cd3f7`） | 2 → 131 | 0 | 相差 23 字节，全部在 `.strtab` |

- uClibc 的 23 字节是函数内 `static` 变量符号名的编号后缀（如 `start_ms.17729` → `start_ms.17724`、`last_x.17699` → `last_x.17694`）：GCC 8 按内部声明序号生成该后缀，换 `-std` 后序号整体偏移。`.text`、`.rodata`、`.data`、`.bss`、`.init_array` 内容与全部反汇编一致，节头表一致。
- 结论：两条目标的运行时加载内容不变，本提交不需要真机冒烟。
- glibc 132 条告警按类别：`-Wundef` 77、`-Wmissing-prototypes` 17、`-Wunused-variable` 15、`-Wcast-function-type` 6、`-Wstack-usage` 4、`-Wshadow` 3、`-Wreturn-type` 3、`-Wimplicit-function-declaration` 2，`-Wunused-but-set-variable`、`-Wformat-truncation`、`-Wformat-overflow`、`-Wdouble-promotion`、`-Waddress` 各 1；uClibc 131 条相同，只少 `-Waddress`。连 `-O3` 行一起上移时 glibc 为 150 条，多出的 18 条依赖优化才能检测（`-Wunused-result` 13、`-Wformat-overflow` 4、`-Wnonnull` 1），本 PR 不启用。
- 告警只作基线记录，不加 `-Werror`，不在本 PR 逐条修。

### 5.2 目录结构、分类与命名

```text
tests/
├── CMakeLists.txt            # 工程入口：主机 gcc + ASan/UBSan 公共选项、被测源码静态库、luckfox_add_test()、add_subdirectory(cases/*)
├── README.md                 # 怎么跑、类别对照表、新增测试的步骤、qemu-user 与 SDL2 依赖
├── support/                  # 公共代码，不含用例；文件不带 test_ 前缀，函数统一 tst_ 前缀
│   ├── check.h               # 断言宏（CHECK / CHECK_EQ_INT / CHECK_STR_CONTAINS）
│   ├── app_env.c/.h          # 5 个全局量、headless 显示/指针、tst_run_ms、tst_tap、tst_drag
│   ├── capture.c/.h          # stdout/stderr 捕获进临时文件，按子串计行
│   └── fake_fs.c/.h          # 按进程建临时目录、写假 sysfs/uevent 文件
├── cases/                    # 用例，按页面分类
│   ├── main/                 # 主屏与时间：板型判定、亮度、时钟、tick、日历
│   │   ├── CMakeLists.txt
│   │   ├── test_sdio_detect.c
│   │   ├── unit_sdio_detect.c        # #include custom_main.c，导出 tst_sdio_has_id()
│   │   ├── test_brightness.c
│   │   ├── test_brightness_arm32.c   # 标签 arm32：交叉编译后 qemu-arm 运行
│   │   ├── unit_brightness_arm32.c   # #include custom_brightness.c，导出 tst_bl_*()
│   │   ├── brightness.mutants        # test_brightness 的改坏变体
│   │   ├── brightness_arm32.mutants  # test_brightness_arm32 的改坏变体
│   │   └── sdio_detect.mutants       # test_sdio_detect 的改坏变体
│   └── wifi/                 # WiFi 页
│       ├── CMakeLists.txt
│       ├── test_backend_release.c
│       └── backend_release.mutants
└── tools/                    # 手动目标，不进 CI
    ├── mutate.sh             # 读 cases/**/*.mutants，逐个改坏源码副本后跑对应测试
    ├── screenshots.c         # screenshots 目标
    └── preview.c             # preview 目标
```

分类规则：类别是页面而不是源文件，控件或逻辑显示在哪一页，测试就放哪个目录；与页面无关的全局逻辑（tick、隐式声明等）归 `main/`。亮度滑条挂在主屏，SDIO 判定决定主屏是否显示 WIFI 键，二者都归 `main/`。

| 目录 | 页面 | 对应源码 | 创建时机 |
|---|---|---|---|
| `main/` | 主屏与时间 | `custom_main.c`、`custom_brightness.c`、`widgets_init.c`、`lv_conf.h` | 本 PR |
| `wifi/` | WiFi | `custom_wifi.c`、`setup_scr_WIFI.c` | 本 PR |
| `music/` | 音乐页 | `custom_musicplayer.c`、`setup_scr_Music_player.c` | 该页首个测试落地时 |
| `sketchpad/` | 画板 | `custom_sketchpad.c`、`setup_scr_Sketchpad.c` | 同上 |
| `exit/` | OFF 退出 | `src/main.c` 退出路径、`Main_OFF_btn_event_handler` | 同上 |
| `gif/` | GIF 页 | `setup_scr_Gif.c` | 同上 |

不预建空目录：git 不跟踪空目录，`.gitkeep` 只是噪音；`tests/README.md` 的类别表即登记处。

命名规则：

- 文件：`test_<被测对象>.c`，目录已表明类别，文件名不重复类别；需要访问 `static` 函数时配一个 `unit_<被测对象>.c`（D4）；不加序号或日期前缀（§2.7）。
- 可执行 target：`luckfox_add_test()` 按 `<类别>_<对象>` 生成（如 `main_brightness`），全局唯一。
- CTest 测试名：`<类别>.<对象>.<用例>`，如 `main.brightness.drag_writes_min_and_max`；用例名描述行为，PR #6 的 A、H9 等旧编号只在注释中对照。缺陷扫描编号不进文件名或测试名。
- 交叉编译的测试不经 `luckfox_add_test()`，由所在目录 `CMakeLists.txt` 的自定义命令编译并以 `add_test()` 注册，测试名仍按三级规则（如 `main.brightness_arm32.conversion`）。
- 标签：`luckfox_add_test()` 自动以所在目录名（类别）为标签，`ctest -L wifi` 只跑 WiFi 页；qemu 测试另加 `arm32`，`ctest -LE arm32` 可跳过。
- 改坏变体清单：放在被测测试旁，按被测对象一份，命名 `<对象>.mutants`（如 `brightness.mutants` 与 `brightness_arm32.mutants`），`tests` 正则匹配该对象的测试名前缀。
- 新增测试：把 `test_xxx.c` 放进对应页面目录，在该目录 `CMakeLists.txt` 加一行 `luckfox_add_test(xxx CASES …)`；新页面首个测试落地时建目录，并在 `tests/CMakeLists.txt` 加一行 `add_subdirectory(cases/<页面>)`。

### 5.3 构建

- 独立工程：`cmake -S tests -B build-tests && cmake --build build-tests -j && ctest --test-dir build-tests --output-on-failure -j`。顶层 `CMakeLists.txt` 不引用 `tests/`（它在未设交叉工具链时提前 `return()`，两者编译器也不同）。`build-tests/` 加入 `.gitignore`。
- 编译器为主机 `gcc`；公共选项 `-std=gnu99 -g -O1 -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all`，链接同样带 sanitizer。
- 被测源码静态库 `luckfox_app`：`lib/lvgl/src/**/*.c`、`generated/**/*.c`、`custom/*.c`，以 `-w` 编译（告警基线由交叉编译记录，§5.1），include 路径按 §2.1 配方；链接系统 `libdrm`、`libcjson`、`pthread`、`m`。
- `support/` 为 OBJECT 库，其目标文件直接并入每个测试可执行文件：被测源码（如 `custom_*.c`）引用 `support/` 定义的 5 个全局量与 `tst_backlight_root()`，`support/` 又引用 LVGL 与界面代码，两个静态库互相引用会受链接顺序影响。
- 测试代码与 `support/` 以 `-Wall -Wextra -Werror` 编译；被测源码的 include 目录以 `SYSTEM` 导出，头文件中的告警不波及测试代码。
- `luckfox_add_test(<对象> [UNIT <文件>] [CASES <用例>…] [LABELS …])`：由所在目录名得类别，建可执行 `<类别>_<对象>`（源文件 `test_<对象>.c`，`UNIT` 文件以 `-w` 编译），链接 `support` 与 `luckfox_app`；有 `CASES` 时每个用例注册一条 `add_test(NAME <类别>.<对象>.<用例> COMMAND <exe> <用例>)`，否则注册一条 `<类别>.<对象>`；每条测试超时 120 s。
- 32 位测试：选项 `LUCKFOX_TESTS_ARM32`，取值 `AUTO`（默认，找到 `arm-linux-gnueabihf-gcc` 与 `qemu-arm` 才注册）、`ON`（缺工具时 configure 报错，CI 用）、`OFF`。以 `add_custom_command` 调交叉 gcc（`-ffunction-sections -fdata-sections -Wl,--gc-sections` 丢弃未引用的界面代码及其 LVGL 依赖），以 `add_test(NAME main.brightness_arm32.conversion …)` 注册、标签 `main;arm32`，测试命令为 `qemu-arm -L /usr/arm-linux-gnueabihf <产物>`。两个工具都由两份 Dockerfile 提供（D8）。
- 手动目标 `screenshots`、`preview` 设 `EXCLUDE_FROM_ALL`；`preview` 仅在 `find_package(SDL2 CONFIG QUIET)` 或退回 `pkg_check_modules(sdl2)` 找到 SDL2 时定义。

### 5.4 `tests/support/`

- `app_env`：定义 5 个全局量；`tst_app_init(res, wifi_enable, music_enable)` 设 `SCALE = res / 480`、`QUIT_FLAG = 0`，调 `lv_init()`，注册内存帧缓冲显示（`full_refresh`）与脚本化指针输入，不打开任何设备节点；`tst_app_setup_ui()` 调 `setup_ui()`、`events_init()` 后运行约 1.5 s（主屏时钟 1 s 刷新提供一次重绘，性能监视器方显示数值）；`tst_run_ms()` 按 5 ms 步长跑 LVGL 定时器；`tst_pointer_set()`、`tst_tap()`、`tst_drag()` 驱动脚本化指针，`tst_tap()`、`tst_drag()` 的坐标按 480 设计坐标乘 `SCALE`；`tst_save_ppm()` 与 `tst_app_set_present_hook()` 供截图与预览目标使用。
- `capture`：`tst_capture_begin()` 把 fd 1、2 重定向到本进程临时目录下的 `capture.log`（sanitizer 报告仍写原 stderr）；`tst_capture_end()` 恢复 fd 并返回捕获文本，同时把它回显到 stdout；`tst_count_lines(text, needle)` 计文本中含 `needle` 的行数。
- `fake_fs`：`tst_tmp_dir()` 首次调用时以 `mkdtemp` 在 `$TMPDIR`（默认 `/tmp`）下建 `luckfox-tests-XXXXXX`，进程退出时删除；`tst_path()`、`tst_fmt()`、`tst_mkdirs()`、`tst_write_file()`、`tst_read_long()` 均以其为根；`tst_backlight_root()` 返回其下的 `sys/class/backlight`（D6）。
- `check.h`：只含头文件，32 位测试也可用；`CHECK(cond)`、`CHECK_EQ_INT(got, want)`、`CHECK_STR_CONTAINS(hay, needle)` 打印 `PASS`/`FAIL <文件>:<行> …` 并累计失败；`tst_run_case()` 按唯一的命令行参数选用例（参数个数不为 1 或用例名未知时返回 2），`tst_finish()` 按失败数返回退出码。捕获期间 stdout 被重定向，断言须在 `tst_capture_end()` 之后执行。

### 5.5 用例

| CTest 名 | 来源 | 断言 |
|---|---|---|
| `main.sdio_detect.{absent,empty,func1_only,ultra_w,hidden,no_newline,prefix}` | PR #5 | `luckfox_sdio_has_id(<假总线目录>, LUCKFOX_ULTRA_W_SDIO_ID)` 依次为 0/0/0/1/0/1/0 |
| `wifi.backend_release.without_timer` | PR #5 退出段错误 | `lv_init()` 后未创建 WIFI 定时器即调 `wifi_backend_release()`，进程正常返回 |
| `wifi.backend_release.twice` | 同上 | 连调两次仍正常返回（释放后置 `NULL`） |
| `main.brightness.drag_writes_min_and_max`（A） | PR #6 | `255/204`：拖到最左写 26、最右写 255；恰好一行 `brightness: using <目录> (max 255)` |
| `main.brightness.small_max_never_writes_zero_{9,1}`（H9、H1） | PR #6 | `max` 为 9、1 时拖到最左写 1 |
| `main.brightness.initial_zero_not_written`（B） | PR #6 | `255/0` 启动后仍为 0 |
| `main.brightness.popup_blocks_drag`（M） | PR #6 | `MUSIC_ENABLE=0` 点 MUSIC 弹窗后在弹窗上拖动，仍为 204 |
| `main.brightness.hidden_{no_device,non_numeric_max,zero_max,oversized_max}`（C、D、E、N） | PR #6 | 无目录、`abc`、`0`、`30000000` 时各恰好一行 `control hidden`，原因分别为 `no backlight device under` 与 `invalid max_brightness in` |
| `main.brightness.write_failure_reported_once`（F） | PR #6 | 控件创建后 `brightness` 换为指向 `/dev/full` 的符号链接，拖动后 stderr 恰好一行 `write backlight failed` |
| `main.brightness.writes_deduplicated`（G） | PR #6 | `10/10` 下拖动，inotify 计得的 `IN_CLOSE_WRITE` 为 1 到 15 次，结束为 10 |
| `main.brightness_arm32.conversion`（标签 `arm32`） | PR #6 Task 13 | 共 7 项：`sizeof(long) == 4`；`max=21474836` 初值 100，读回 100%、写 100% 得 21474836；`max=255` 初值 80，读回 80%、写 10% 得 26 |

亮度用例均走真实挂载路径：`setup_ui()` → `setup_scr_Main()` → `main_app_init()` → `brightness_ui_create()`，分辨率 480。

### 5.6 手动目标

- `screenshots`：以 480 与 720 分辨率各输出初始、拖到最左、音乐弹窗遮盖、无背光设备四类 PPM 到构建目录；找到 `ffmpeg` 时转 PNG。
- `preview`：SDL 开窗，鼠标即触摸，`PREVIEW_RES=480|720` 选分辨率，`PREVIEW_BACKLIGHT=<max>:<当前值>` 建假背光（不设则亮度控件隐藏）；SDL flush 与 PR #6 harness 相同，SDL2 查找与 README 写法参照 ESP-Pocket2 PR6。
- `tools/mutate.sh [id...]`（改坏检验，即 mutation testing）：本 PR 纳入的测试针对已修好的代码，第一次运行就是绿的，无法用「先失败再修好」证明它们能抓住回归，一条什么都不检查的测试同样是绿的；改坏检验故意把源码改坏一处，确认对应测试会失败。mutant / killed / survived 沿用变异测试的通用术语（PIT、mutmut、Stryker），本仓只用手写的少量针对性变体，不引入自动生成变体的工具。脚本启动时会 `rm -rf "$MUTATE_WORK/src"`，所以先把 `MUTATE_WORK` 规范化（解析 `..` 与符号链接），拒绝 `/`、仓库内的路径与仓库位于其 `src/` 之内的目录，只使用不存在、为空或带标记文件 `.luckfox-mutate-workdir`（须为普通文件，目录或符号链接不算）的目录，此后一律使用规范化后的路径；`custom/` 含符号链接时也拒绝（变体经副本写回会改到仓库），上述拒绝均以退出码 2 结束；它把 `custom/` 复制到工作目录 `$MUTATE_WORK/src`（默认 `/tmp/luckfox-mutate`，其余目录链接回仓库），以 `-DLUCKFOX_ROOT=<副本>` 配置一次测试工程；基线不全绿即退出。再按 `*.mutants` 的每条记录（`id`（只含字母、数字、`_`、`.`、`-`，且以字母或数字开头，因为它会用作日志文件名）、`file`、`old`、`new`、`tests` 五个 `键: 值` 行，空行分隔，`#` 开头为注释，`\n` 表示换行）改坏副本、增量重编，`tests` 正则下至少一条失败记为 `KILLED`，全部通过记为 `SURVIVED`，随后还原；`tests` 匹配不到已注册的测试（如缺 `qemu-arm` 时的 32 位用例）记为 `SKIPPED`，结尾汇总 `N run, M skipped`。清单中未知键、重复 id、记录内重复键、`file` 不在 `custom/` 下或含 `..`、`old` 不是恰好匹配一次均为清单错误。退出码：0 = 全部 `KILLED`，1 = 有 `SURVIVED`，2 = 工具或清单错误（含工作目录不合法、`custom/` 含符号链接、基线不绿、构建失败、id 不存在、一个变体都没跑）。并发运行须以不同的 `MUTATE_WORK` 区分。变体分布：`brightness.mutants` 9 个（PR #6 的 M1–M7、M10、M11），`brightness_arm32.mutants`（M12）、`sdio_detect.mutants`（S1）、`backend_release.mutants`（W1）各 1 个，共 12 个（§6 第 3 条）。它是手动工具，不进 CI（每个变体都要重编并重跑测试，全量约 50 s）；修复 PR 的测试可以先失败再修好，新增 `<对象>.mutants` 不是必需的。

| 清单 | id | 改坏方式 | 抓住它的测试 |
|---|---|---|---|
| `brightness.mutants` | M1 | 下限 10% 改成 0% | `main.brightness.drag_writes_min_and_max` |
| | M2 | 删掉同值去重 | `main.brightness.writes_deduplicated` |
| | M3 | 删掉控件下沉到底层 | `main.brightness.popup_blocks_drag` |
| | M4 | 创建控件时多写一次背光 | `main.brightness.initial_zero_not_written` |
| | M5 | 删掉 `max_brightness <= 0` 校验 | `main.brightness.hidden_zero_max` |
| | M6 | 写入失败每次都报错 | `main.brightness.write_failure_reported_once` |
| | M7 | 换算不再四舍五入 | `main.brightness.drag_writes_min_and_max` |
| | M10 | 删掉「最少写 1」的下限 | `main.brightness.small_max_never_writes_zero_1` |
| | M11 | 删掉 `max_brightness` 上界校验 | `main.brightness.hidden_oversized_max` |
| `brightness_arm32.mutants` | M12 | 换算中间值改回 32 位 `long` 运算 | `main.brightness_arm32.conversion` |
| `sdio_detect.mutants` | S1 | `SDIO_ID` 整行匹配改成前缀匹配 | `main.sdio_detect.prefix` |
| `backend_release.mutants` | W1 | `wifi_backend_release()` 换回 PR #5 之前的无判空实现 | `wifi.backend_release.*` |

M1–M7、M10、M11 沿用 PR #6 的编号，M8、M9 当时针对的代码不在本 PR 范围。

### 5.7 CI job `native-tests`

- `needs: build-image`，`if: github.event_name != 'schedule'`，`container` 与凭据同 `build-demo`，`permissions: contents: read, packages: read`。
- 步骤：检出本仓 → `cmake -S tests -B build-tests -DLUCKFOX_TESTS_ARM32=ON` → `cmake --build build-tests -j"$(nproc)"` → `ctest --test-dir build-tests --output-on-failure -j"$(nproc)"`。
- `qemu-user` 与 `arm-linux-gnueabihf-gcc` 都在 CI 镜像中（D8），job 不安装软件包；`ON` 使缺工具时 configure 直接失败，而不是静默跳过 32 位测试。
- 不上传产物。push 触发沿用 workflow 现有 `paths-ignore: '**.md'`。

### 5.8 `AGENTS.md`

- 「Lint」：主程序告警已生效，glibc 132 条、uClibc 131 条为既存基线，无 `-Werror`；`-O3` 行仍在 `add_executable()` 之后、未生效；LVGL 与 lv_drivers 子目录 `C_FLAGS` 仍为空。
- 「Cloud Agent 环境」：当前活动的 `.cursor/Dockerfile` 安装内容加 `qemu-user`。
- 「GitHub Actions」：镜像标签取 `.cursor/Dockerfile` 内容哈希，修改该文件的 PR 在合并前 `build-image` 必然失败，合并后 dev push 重建镜像才恢复（§2.4）。
- 「测试」：`tests/` 的构建运行命令；32 位测试需 `qemu-user` 与 `arm-linux-gnueabihf-gcc`、缺任一自动跳过，两者由两份 Dockerfile 提供，CI 以 `-DLUCKFOX_TESTS_ARM32=ON` 强制运行；分类与新增步骤指向 `tests/README.md`；测试不调用 `custom_init()` 及其原因。
- 「运行 / 验证 GUI」：native 渲染改为使用 `tests/` 的 `screenshots`、`preview` 目标，保留 §6.1 配方指针作背景；uClibc 产物冒烟所用的 `qemu-user` 已在 `.cursor/Dockerfile` 中，不再需要手动安装。
- `AGENTS.md` 每轮自动加载，需控制篇幅（32 KiB 以内），实际远低于该限额。

### 5.9 旧文档精简

- 范围：`docs/superpowers/plans/2026-09-25-lvgl-example-luckfox-pico-ultra-w-wifi-detect.md`、`docs/superpowers/plans/2026-09-27-lvgl-example-brightness-slider.md`，以及两份对应 spec 中描述「临时 harness（不入库）」的句子。
- 删：已入库测试的完整代码与脚本（PR #5 SDIO harness 与 `run.sh`、`_lv_ll_remove` 复现程序；PR #6 预览 harness 与 `build.sh`、`check.sh`、`mutate.sh` 与 9 条命令、`u32test.c` 与其 `build.sh`），各改为「以仓库 `tests/…` 为准」加一句要点；File Structure 表中对应行指向 `tests/`。被删内容与仓库对应物的逐项对照（含唯一未迁移的严格编译关卡）见 plan Task 9。
- 留：验证证据（命令与实测数值）、与计划的偏离、未入库内容全文（PR #5 双重 `fclose` 演示、Task 7 修复前对照程序构建命令、PR #6 截图与真机步骤）。

## 6. 验证（plan 执行时落地，此处只定判据）

1. 第一个提交：在最终分支上重做 §5.1 对比，glibc 与 uClibc 均 0 error、告警数与 §5.1 一致、产物差异仍只在 `.strtab`。
2. 原生测试：Cloud Agent 容器内按 §5.3 命令构建运行，§5.5 全部用例通过，无 ASan/UBSan/LSan 报告；以非 root 用户重跑一遍，结果相同。
3. 测试有效性：已修代码无法「先红」，改用改坏证明——`tools/mutate.sh` 的 12 个变体均 `KILLED`：PR #6 的 9 个亮度变体；亮度换算中间值改回 `long` 后 `main.brightness_arm32.conversion` 失败；`luckfox_sdio_has_id()` 的整行匹配改为前缀匹配后 `main.sdio_detect.prefix` 失败；`wifi_backend_release()` 换回 PR #5 之前的实现（`31e3506`）后 `wifi.backend_release.*` 失败。
4. 32 位：`LUCKFOX_TESTS_ARM32=ON` 时 `ctest -N -L arm32` 只列出 `main.brightness_arm32.conversion` 且通过；`AUTO` 且缺 `qemu-arm` 时不注册、其余用例照常。
5. 手动目标：`screenshots` 在 480/720 下产出 8 张图，与 PR #6 截图布局一致；`preview` 在 `DISPLAY=:1` 开窗并可用鼠标拖动滑条。
6. CI：改 Dockerfile 前最后一次 PR run 的 `build-image`、glibc、uClibc、`native-tests` 四个 job 均 success；本 PR 合并前 `build-image` 失败属预期（§2.4、D8）；合并后首次 dev push run 四个 job 均 success 为合并后验证。
7. 文档：`AGENTS.md` 字节数在 §5.8 限额（32 KiB）内；旧文档精简的 diff 只含删除的代码块与替换的指针句，证据表逐字不变。

## 7. 风险

| 风险 | 缓解 |
|---|---|
| CI 容器限制 ptrace，LeakSanitizer 无法工作 | 首次 CI 运行即暴露；若出现，只在 CI 设 `ASAN_OPTIONS=detect_leaks=0` 并在 plan 记录原因，本地仍开 |
| 界面用例依赖真实时间（`custom_tick_get()` 基于 `gettimeofday`），CI 负载高时拖动节奏变化 | 断言只看最终写入值与计数上界，不看中间帧；拖动前后留足等待；用例进程独立，`ctest -j` 不共享状态 |
| 合并前 PR CI 红：`build-image` 找不到 dev 签名镜像，两条交叉编译与原生测试都不运行，评审看不到 CI 结论 | 本地按 CI 步骤重放（`-DLUCKFOX_TESTS_ARM32=ON` 构建运行）；以改 Dockerfile 前最后一次绿色 PR run 作代码层证据；合并后观察首次 dev push run 四个 job 是否均 success |
| `#include` 被测 `.c` 时若静态库同名成员被拉入会重复定义 | 被测 `.c` 的全部外部符号已由测试文件定义，链接器不会再拉该成员；出现重复定义即编译期失败，不会静默 |
| ASan 下编译全部 LVGL 拖慢 CI | 静态库只编一次，所有测试共用；预计在交叉编译 job 同量级 |
| 测试构建与产品构建的 include 或宏不一致 | include 路径与 §2.1 配方一致，`lv_conf.h`、`lv_drv_conf.h` 用仓库原文件，不另写配置 |

## 8. 文档与提交

| 顺序 | 提交 | 内容 |
|---|---|---|
| 1 | `build(cmake): 🔧 恢复主程序编译告警` | FR1：告警段上移到 `add_executable()` 之前并改 `-std=gnu99` |
| 2 | `chore(cloud-env): 🐳 两份 Dockerfile 补装 qemu-user` | FR9：`.cursor/Dockerfile` 与 `.cursor/Dockerfile.luckfox_pico` 补装 `qemu-user` |
| 3 | `test(tests): ✅ 新增原生测试工程并纳入 SDIO 板型判定测试` | 测试骨架（`tests/CMakeLists.txt`、`support/`、README、`.gitignore`）与 `main.sdio_detect` |
| 4 | `test(tests): ✅ 纳入 wifi_backend_release() 判空回归测试` | `wifi.backend_release` |
| 5 | `test(tests): ✅ 纳入亮度滑条断言` | `main.brightness` 的 11 个用例 |
| 6 | `test(tests): ✅ 纳入 32 位亮度换算测试` | `main.brightness_arm32.conversion` 与 `LUCKFOX_TESTS_ARM32` 探测 |
| 7 | `test(tests): ✅ 新增截图、SDL 预览与改坏检验手动目标` | 手动目标 `screenshots`、`preview`、`tools/mutate.sh` 与 4 份 `*.mutants` |
| 8 | `ci(github-actions): 👷 新增原生测试 job` | `native-tests` job |
| 9 | `docs(agents): 📝 更新告警、测试与原生渲染说明` | FR7 |
| 10 | `docs(superpowers): 📝 PR #5、#6 plan 中已入库的测试改为指向 tests/` | FR8 旧文档精简 |
| 11 | `docs(superpowers): 📝 新增原生测试基础设施 spec 与 plan` | 本 spec 与关联 plan |

提交格式沿用仓库惯例 `type(scope): emoji 主题`（git cz），scope 参考 PR #4 的 `ci(github-actions)` 与 PR #1–#3 的 `chore(cloud-env)`；body 用 `-` 列原因、决策与被否决的替代，以及合并后须知（⚠️）。

spec 与 plan 先提交并推送到工作分支，草稿 PR 在实现前创建以便审阅文档；全部完成后以 `--force-with-lease` 重排，使 spec 与 plan 合为最后一个提交（仅为此目的的强推无需再询问作者）。

## 9. QA（grilling 问答与澄清）

Q1–Q8 针对「单个 PR 修完全部既存缺陷」的方案；范围已拆为本 PR（测试基础设施）与后续修复 PR，属于修复 PR 的结论并入 §10。

**Q1（第 1 轮）：修复范围——建议纳入 25 处、暂缓 6 项是否认可？**
A：**认可。** 拆 PR 后整体移入后续修复 PR，见 §10.1、§10.2；暂缓项为 7 项，含 Q3 的「启用 `-O3`」。

**Q2（第 1 轮）：失败测试放在哪里——入库的 `tests/` 原生工程、仓库外 harness，还是纯逻辑入库而 GUI 场景留仓库外？**
A：**入库（D2）。** 拆 PR 后测试基础设施成为本 PR 的全部内容。

**Q3（第 1 轮）：`-O3` 是否随告警提交一起生效？**
A：**不生效，只上移告警段；启用 `-O3` 暂缓另开 PR（D1、§10.2）。**

**Q4（第 1 轮）：生成代码直接改，还是挪进 `custom/` 封装？**
A：**需要新增逻辑的进 `custom/`，一两行的直接改 `generated/`，见 §10.3。**

**Q5（第 1 轮）：OFF 退出噪音用 `2>/dev/null` 还是改为 C 代码写零？**
A：**C 代码写零、设备路径作参数，以 `/dev/full` 测试，见 §10.3。**

**Q6（第 1 轮）：修复的提交粒度与顺序？**
A：**一个缺陷一个提交，跨模块的判空按模块拆，顺序与组内规则见 §10.3。**

**Q7（第 1 轮）：spec 如何提前推送到远端——备份分支、只存 Project store，还是直接提交工作分支？**
A：**直接提交到工作分支，全部完成后强推重排为最后一个提交（§8）；仅为把 superpowers 文档移到最后而强推时无需再询问，草稿 PR 在实现前创建。**

**Q8（第 1 轮）：合并前是否跑真机清单？tick 溢出是否做 72 分钟长跑？**
A：**跑清单；tick 溢出以 32 位模拟测试证明，不做长跑，见 §10.4。**

**Q9（第 2 轮）：精简旧文档单独提交，还是并入最后的 spec/plan 提交？**
A：**单独一个 `docs(superpowers)` 提交（D14）。**

**Q10（第 2 轮）：CI 里的 `qemu-user` 在 job 内安装、本 PR 补进 Dockerfile、先单独开 PR 改 Dockerfile，还是 32 位测试不进 CI？**
A：**`qemu-user` 进两份 Dockerfile，CI 直接使用镜像，接受本 PR 合并前 PR CI 失败（D8、§2.4）。**

**Q11（第 2 轮）：合并后 `AGENTS.md` 的「告警未生效」「无测试套件」会变成错误信息，是否一并更新？**
A：**更新（D13、§5.8）。**

**Q12（第 2 轮）：断言框架用纯 C 宏加 CTest，还是 vendored Unity/cmocka？**
A：**纯 C 宏加 CTest（D3）。**

**Q13（第 2 轮）：本 PR 的提交如何划分？**
A：**见 §8。**

**Q14（第 2 轮）：设计草案（独立工程、`#include` 被测 `.c` 访问 `static` 函数、`/dev/full` 与 inotify 替代 `su nobody` 与 `strace`、CI job）是否认可？**
A：**认可（D4–D7、D11、§5.3–§5.7）。**

**Q15（作者提问）：当前仓库的 LVGL 是怎么运行的？ESP-Pocket2 PR5、PR6、PR8 的 `pc_simulator` 与 `pc_simulator_lgfx` 能否参考？**
A：**运行链路见 §2.1；借鉴工程结构、SDL2 查找与 README 写法，代码不照搬，见 §2.6。**

**Q16（第 3 轮）：是否在本 PR 加交互式 SDL 预览目标？**
A：**加，作为只在找到 SDL2 时生成的手动目标 `preview`（D10、§5.6）。**

**Q17（作者提问与第 4 轮）：`tests/` 的结构与命名怎么设计？是否按页面分文件夹？文件名是否加 `001-` 或日期前缀？**
A：**按页面分、不加前缀（D9、§5.2），依据见 §2.7。** 逐项结论：用例放 `tests/cases/<页面>/` 而非 `tests/<页面>/`，与 `support/`、`tools/` 分层；类别目录名 `main`、`wifi`、`music`、`sketchpad`、`exit`、`gif`，亮度与 SDIO 判定归 `main/`；没有测试的类别不预建空目录，以 `tests/README.md` 类别表登记；文件 `test_<对象>.c`、CTest 名 `<类别>.<对象>.<用例>`、标签为类别名与 `arm32`；交叉编译的测试同样按三级命名（`main.brightness_arm32.conversion`）；改坏清单按被测对象拆分为 `<对象>.mutants`（M12 在 `brightness_arm32.mutants`，`brightness.mutants` 只含 PR #6 的 9 个变体）。

**Q18（作者提问与第 5 轮）：为什么叫「原生测试 / native-tests」？是专业名词吗？有没有更好的命名？**
A：**保留「原生测试 / native-tests」（方案 A）。** 是专业名词：编译器领域 native build（用本机编译器为本机构建）与 cross build（交叉编译）相对，产品构建交叉编译到 32 位 ARM，`tests/` 用开发主机自己的 gcc 编译并在主机上运行，与 Zephyr `native_sim`、PlatformIO `native` 平台同义；`qemu-arm` 运行的 32 位测试同样在开发主机上执行。中文「原生」易联想「原生应用」「原生 API」，嵌入式领域更常用 host-based testing、off-target testing，James Grenning《Test-Driven Development for Embedded C》称同时为主机与目标板构建为 dual-targeting。

| 方案 | 命名 | 取舍 |
|---|---|---|
| A（采纳） | 原生测试 / native-tests | 术语准确，与分支名一致，不必改名 |
| B | 主机测试 / host tests | 中文更直白；但要改 CI job 名、文档文件名、提交信息与 PR 标题，分支名 `cursor/native-tests-cbb9` 改名需关掉本 PR 另开，前后不一致 |
| C | 离板测试 / off-target tests | 最贴近嵌入式术语，但不如「主机测试」直白 |

**Q19（作者提问）：精简 PR #5、#6 旧文档时删掉的代码都已入库吗？**
A：**功能上全部有对应；唯一未迁移的是 PR #6 `build.sh` 末行以 `-Wall -Wextra -Werror` 单独编译 `custom_brightness.c` 的严格编译关卡，逐项对照表见 plan Task 9。**

**Q20（作者提问）：跑 ctest 时有没有显示 LVGL 界面的 GUI 窗口？**
A：**没有，ctest 全程 headless。** `tests/support/app_env.c` 把 LVGL 的显示驱动注册成内存帧缓冲，界面画在内存里、不开窗口；触摸由测试代码按脚本模拟（点击、拖动），所以 CI 与没有桌面的环境都能跑。要看界面用 §5.6 的两个手动目标：`cmake --build build-tests --target preview` 后运行 `PREVIEW_RES=480 PREVIEW_BACKLIGHT=255:204 DISPLAY=:1 ./build-tests/preview`（SDL 开窗，鼠标当触摸）；`screenshots` 把同样的场景存成截图。

**Q21（作者提问）：整个测试跑完有哪些输出文件（日志、截图等）？**
A：**ctest 只产出日志，不产出截图；截图来自手动的 `screenshots` 目标。**

| 来源 | 位置 | 内容 |
|---|---|---|
| `ctest` | `build-tests/Testing/Temporary/LastTest.log` | 每条测试的命令行、全部输出（`PASS`/`FAIL` 行、捕获到的程序输出）与耗时；有失败时另有 `LastTestsFailed.log` |
| 每条测试进程 | `$TMPDIR/luckfox-tests-XXXXXX/` | 假 sysfs、`capture.log` 等临时文件，进程正常退出时自动删除，跑完通常看不到；被 sanitizer 终止的进程会残留 |
| `screenshots` 目标 | `build-tests/screenshots/` | 480/720 各 4 张 PPM，装了 `ffmpeg` 时另存 PNG |
| `tools/mutate.sh` | `$MUTATE_WORK`（默认 `/tmp/luckfox-mutate/`） | `baseline.log`，每个变体的 `<id>.log` 与 `<id>.build.log` |
| CI | GitHub Actions job 日志 | `native-tests` job 不上传产物，结果只在 job 日志里 |

**Q22（作者提问）：需要在开发板上实机跑测试吗？**
A：**本 PR 不需要上板，`tests/` 本身也不能在板上跑；测试覆盖不了的硬件行为仍靠真机。**
- 本 PR 为什么不需要上板：对产品代码唯一的改动是 `CMakeLists.txt` 的告警选项，glibc 产物前后逐字节一致，uClibc 产物只在不加载的 `.strtab` 段差 23 字节，运行时内容不变（§5.1）。
- 测试为什么不能在板上跑：`tests/` 是主机工程，用主机 gcc 编译 x86 程序并开 ASan/UBSan，板上的 uClibc 工具链不支持这套编译选项；它的作用是不用开发板就能快速、可重复地回归逻辑。唯一例外是 32 位换算测试：用 ARM 编译器编出 32 位程序、在主机上由 `qemu-arm` 运行，覆盖「目标板 `long` 为 32 位」的差异。
- 只有真机能验证的部分：

| 只有真机能验证 | 原因 |
|---|---|
| 屏幕实际显示、触摸手感 | 测试用内存帧缓冲与模拟触摸，不经过 `/dev/fb0`、`/dev/input/event0` |
| 背光亮度、最低可见值 | 测试用假 sysfs，看不到真实 PWM 效果 |
| 音乐播放、WiFi 连接 | 依赖板上的 mpv、`wpa_supplicant` 与硬件 |
| uClibc ABI 与真实性能 | 测试跑在 x86 + glibc 上 |

- 什么时候必须上板：缺陷修复 PR 会改变产品行为，合并前按 §10.4 的清单在 Luckfox Pico Ultra W 上验证；上板运行 CI 编出的 uClibc 产物，不在板上跑 `ctest`。

## 10. 后续 PR：既存缺陷修复（已定结论）

本节只记录已确认的结论，供修复 PR 直接沿用；修复 PR 另写 spec 与 plan。测试基础设施由本 PR 提供，修复 PR 中每处修复先在 `tests/cases/<页面>/` 写失败测试再修。以下条目在 `dev` `cd9fd86` 上逐条核实仍存在。

### 10.1 范围：纳入 25 处

| 页面 | 条目 |
|---|---|
| 音乐页（`custom_musicplayer.c`、`setup_scr_Music_player.c`） | ① `vfork` 子进程 exec 失败后 `return` 改 `_exit(127)`；② `roller_str[256]` 由 `strcat` 拼接文件名栈溢出；③ `music_scan_list` 中 `sprintf(cmd[256])` 溢出，且文件名中的 `"` 可改写发给 mpv 的 JSON 命令；④ roller 选中 ≥32 字节文件名时 `strcmp` 死循环；⑤ `cJSON_Parse` 结果从不释放；⑥ `read` 读满 512 字节不补 NUL；⑦ mpv 退出后写套接字触发 SIGPIPE 杀进程；⑧ `opendir` 失败后仍 `readdir`；⑨ 3 个 `int` 函数缺 `return`；⑩ mpv 连接只试一次且 `sleep(0.1)` 实际为 0、启动时同步跑一次无参 mpv；⑪ `close(0)` 关闭父进程 stdin |
| 主屏与时间（`custom_main.c`、`widgets_init.c`、`lv_conf.h`、`custom.h`） | ⑫ 正午显示 AM、零点显示 0；⑬ `custom_tick_get()` 在 32 位 `time_t` 上溢出且基于墙钟；⑭ 日历 `highlighted_days` 为局部数组、`strtok` 改写日期标签文本；⑮ `custom_tick_get`、`open` 隐式声明；⑯ `luckfox_get_system_info` 的 `popen` 失败后仍使用 |
| 退出（`src/main.c`） | ⑰ `system("cat /dev/zero > /dev/fb0")` 打印 `cat: write error: No space left on device` |
| 画板（`setup_scr_Sketchpad.c`、`custom_sketchpad.c`） | ⑱ 画布缓冲区为已返回函数的栈 VLA，且按字节数当元素数分配（4 倍）；⑲ 屏幕坐标当画布坐标，笔迹偏移 |
| WiFi（`custom_wifi.c`、`setup_scr_WIFI.c`） | ⑳ SSID/密码不校验写坏 `wpa_supplicant.conf`；㉑ 相对路径临时文件加先 `remove` 后 `rename` 可能丢配置；㉒ 扫描结果按空格切分导致截断与粘连；㉓ `_wifi_status_update` 中 `pclose(NULL)`；㉔ 离开 WIFI 页后定时器仍每 5 s `popen`、每按 Load 多起一个 `udhcpc`；㉕ `sscanf` 读引号值不限宽度 |

### 10.2 暂缓（记录，不在修复 PR 修）

| 条目 | 暂缓原因 |
|---|---|
| mpv 监听线程跨线程调用 LVGL（数据竞争） | 需互斥量加 UI 定时器的小设计，约 35 行，TSan 验收 |
| WiFi 的 `popen` 移入工作线程、Scan 触发真实扫描 | 约 40 行且属行为变更 |
| `luckfox_get_input_device_info()` 返回局部数组地址 | 调用已注释，死代码 |
| 720 屏 18px 字体无映射与栈占用 | 本板为 480 屏，不可达 |
| 亮度只尝试第一个背光条目 | 本板只有一个背光设备，不可达（乘法溢出已在 PR #6 修复） |
| `custom_wifi.c` 的 `sprintf(buffer[128])` 与 `networks[1024]` 131 KB 栈 | 当前 UI 输入长度下不可达；后者非缺陷 |
| 启用 `-O3` 与 LVGL 子目录优化级别 | 性能变更，另开 PR 并真机回归（D1） |

不修：`strcpy(addr.sun_path, "/tmp/mpvsocket")`，常量 15 字节，无溢出。

### 10.3 修法与组织

- 生成代码：需要新增逻辑的（画布缓冲区分配、音乐列表字符串）放进 `custom/`，`generated/` 只改调用处；一两行即可修的（日历数组改 `static` 并先拷贝再 `strtok`、WIFI 密码框 `max_length`）直接改 `generated/`；spec 列出改过的生成文件。
- OFF 退出：改为 C 代码写零，设备路径作参数，写到 `ENOSPC` 静默停止（或先取 `FBIOGET_FSCREENINFO` 大小），测试以 `/dev/full` 断言无输出、正常返回。
- 编译层缺陷（缺 `return`、隐式声明）以对应 `-Werror=return-type`、`-Werror=implicit-function-declaration` 编译失败作为失败测试。
- 提交：一个缺陷一个 `fix` 提交；`popen`/`opendir` 判空按音乐、主屏、WiFi 拆 3 个；`roller_str` 溢出与 `sprintf` 溢出拆 2 个；顺序为音乐 → 主屏与时间 → 退出 → 画板 → WiFi，组内先高危；spec 与 plan 为 PR 最后一个提交。

### 10.4 真机验证

合并前在 Luckfox Pico Ultra W 上跑清单：音乐页放入长文件名与总长超过 255 字节的列表、杀掉 mpv 后按下一曲、日历翻月、画板笔迹、WIFI 输入含 `"` 或少于 8 位的密码并 Load、OFF 退出无噪音、正午显示 PM。tick 溢出以 32 位模拟测试证明，不做 72 分钟真机长跑。

## 11. 参考资料

- LVGL 测试目录：https://github.com/lvgl/lvgl/tree/release/v8.3/tests 、https://github.com/lvgl/lvgl/tree/master/tests/src/test_cases
- Zephyr：https://docs.zephyrproject.org/latest/develop/test/ztest.html 、https://docs.zephyrproject.org/latest/develop/twister/index.html
- CMake `add_test`：https://cmake.org/cmake/help/latest/command/add_test.html
- ESP-Pocket2：https://github.com/yuangezhizao/ESP-Pocket2/pull/5 、https://github.com/yuangezhizao/ESP-Pocket2/pull/6 、https://github.com/yuangezhizao/ESP-Pocket2/pull/8
- 本仓 `CMakeLists.txt`、`src/main.c`、`custom/custom_brightness.c`、`custom/custom_wifi.c`、`.github/workflows/build-luckfox-lvgl-demo.yml`、`.cursor/Dockerfile`、`.cursor/Dockerfile.luckfox_pico`
- 前序 spec/plan：[`2026-09-25-lvgl-example-luckfox-pico-ultra-w-wifi-detect-design.md`](2026-09-25-lvgl-example-luckfox-pico-ultra-w-wifi-detect-design.md)、[`2026-09-27-lvgl-example-brightness-slider-design.md`](2026-09-27-lvgl-example-brightness-slider-design.md) 及其 plan
