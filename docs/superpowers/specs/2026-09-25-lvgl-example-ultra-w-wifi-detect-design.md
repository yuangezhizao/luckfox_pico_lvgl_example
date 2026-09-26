# Luckfox Pico LVGL Example — Ultra W WiFi 判定修复设计规格

- **日期**：2026-09-25
- **状态**：已批准，按关联 plan 实施
- **分支**：`cursor/ultra-w-wifi-detect-7dfe`
- **关联计划**：[`../plans/2026-09-25-lvgl-example-ultra-w-wifi-detect.md`](../plans/2026-09-25-lvgl-example-ultra-w-wifi-detect.md)。代码以仓库为准；plan 留 Task、证据与偏离。
- **关联固件**：[`yuangezhizao/luckfox-pico` run 35115207257](https://github.com/yuangezhizao/luckfox-pico/actions/runs/35115207257)，job `Luckfox Pico Ultra W · EMMC`，artifact [`luckfox-pico-firmware_ultra-emmc_on-ubuntu-24.04`](https://github.com/yuangezhizao/luckfox-pico/actions/runs/35115207257/artifacts/10458251515)，源码 `d4cd3f7`

## 1. 概述与目标

在 Luckfox Pico Ultra W 上刷入当前 SDK 编出的 Buildroot 固件后，例程主界面不显示 WIFI 按键。根因是例程靠 `/proc/device-tree/model` 精确等于 `"Luckfox Pico Ultra W"` 判定 WiFi，而 SDK 自 `824b817f` 起 Ultra 与 Ultra W 共用一份设备树，`model` 恒为 `"Luckfox Pico Ultra"`。本 spec 把例程的 WiFi 判据从「型号字符串」改为「SDIO 总线上是否枚举到 Ultra W 板载 AIC8800DC 的 SDIO ID `C8A1:C18D`」，与 SDK 加载 aic8800 驱动的 `insmod_wifi.sh` 及 `wifi_start.sh` 所用 ID 相同。

目标：

- 统一设备树固件（`model = "Luckfox Pico Ultra"`）上，有 WiFi 模块的 Ultra W 显示 WIFI 按键；无模块的 Ultra 不显示。
- 旧固件（`model = "Luckfox Pico Ultra W"`）行为不变。
- 恢复 WIFI 键后，无论是否进入过 WIFI 页，点 OFF 均正常退出并清屏。
- 不改 SDK、不改设备树、不改 GUI-Guider 生成代码（`generated/`）。

非目标：SDK 侧同类残留的修复（见 §9）；Ubuntu 路径的 WiFi（`luckfox_get_system_info()` 在 Ubuntu 上本就置 `WIFI_ENABLE=0` 并跳过型号检测，本次不动）；`custom_wifi.c` 里既存的 `sprintf`/`strcpy` 等不安全调用（本次只改其 `wifi_backend_release()`，见 FR7）；新增板型支持。

## 2. 背景与根因

### 2.1 例程现状

`src/main.c` 在非 Ubuntu 系统上调用 `luckfox_get_wifi_enable_info()`（`custom/custom_main.c`），其逻辑为：读 `/proc/device-tree/model`，等于 `"Luckfox Pico Ultra W"` 置 `WIFI_ENABLE=1`，等于 `"Luckfox Pico Ultra"` 置 0，其他型号 `exit(EXIT_FAILURE)`。`WIFI_ENABLE` 决定三件事：`generated/setup_scr_Main.c` 是否隐藏 `Main_Wifi_btn` 并重排按键；进入 WIFI 页时 `wifi_backend_init()`；退出时 `src/main.c` 是否 `wifi_backend_release()`。该函数另有一处既存缺陷：`fgets` 失败分支先 `fclose(file)`，随后函数末尾再次 `fclose(file)`，重复关闭同一 `FILE*` 属未定义行为。

创建与释放不对称：`custom/custom_wifi.c` 的全局 `wifi_update_timer`（零初始化）只在首次进入 WIFI 页时由 `wifi_backend_init()` 创建（`generated/setup_scr_WIFI.c` 调用；`gui_guider.c` 初始 `WIFI_del = true`，离开 WIFI 页时置 `false`，故最多创建一次），而 `src/main.c` 只要 `WIFI_ENABLE == 1` 就在主循环结束后调用 `wifi_backend_release()` 并无条件 `lv_timer_del(wifi_update_timer)`。启动后不进 WIFI 页直接点 OFF 即 `lv_timer_del(NULL)`：LVGL 8.3 `_lv_ll_remove()` 对非头非尾的 `NULL` 走 else 分支，`_lv_ll_get_prev(ll, NULL)` 解引用空指针段错误（`lib/lvgl/src/misc/lv_timer.c`、`lv_ll.c`；host 编译 vendored `lv_ll.c` 在 2 节点链表上调用 `_lv_ll_remove(&ll, NULL)` 实测触发 SIGSEGV，由信号处理函数捕获，复现程序见 plan Task 2），`cat /dev/zero > /dev/fb0` 清屏随之被跳过。统一设备树固件上 `WIFI_ENABLE` 恒为 0，此路径原本不可达；按 SDIO ID 恢复 WIFI 键后在 Ultra W 上可达。

上游 `LuckfoxTECH/luckfox_pico_lvgl_example` 最后一次提交是 2024-11-28，早于 SDK 的设备树合并，判据从未随之更新。

### 2.2 SDK 设备树历史

`yuangezhizao/luckfox-pico` 在 `d4cd3f7` 比 `LuckfoxTECH/luckfox-pico` `main`（`824b817f`）领先 32 个提交、落后 0 个，这 32 个提交未触及 DTS、BoardConfig、`luckfox-config` 或 WiFi 脚本，故 fork 如实复现官方行为。`rv1106g-luckfox-pico-ultra-w.dts` 的生命周期：

| 提交 | 日期 | 变更 |
|---|---|---|
| `d3153ac9` | 2024-03-16 | 新增 Ultra 支持：`ultra.dts`、`ultra-w.dts`（`model = "Luckfox Pico Ultra W"`）、`rv1106-luckfox-pico-ultra-ipc.dtsi`、Ultra_W 的 Buildroot/Ubuntu BoardConfig |
| `8f34c276` | 2024-08-21 | 内核升 5.10.160，两份 DTS 移至 `sysdrv/tools/board/kernel/` |
| `485f09ec` | 2025-03-05 | 移回 `sysdrv/source/kernel/arch/arm/boot/dts/` |
| `d2da6f0f`（#274，标题 `Add Luckfox Pico 86Panel Support`） | 2025-05-09 | 删除 Ubuntu 版 Ultra_W BoardConfig；squash 正文含 `project/cfg/BoardConfig_IPC : Discontinue support for Ubuntu`，当前 SDK 已无任何 Ubuntu BoardConfig |
| `824b817f`（[#355](https://github.com/LuckfoxTECH/luckfox-pico/pull/355)） | 2026-03-22 | 删除 `ultra-w`/`pi-w`/`86panel-w` 三份 DTS 与对应 Buildroot BoardConfig；lunch 菜单去掉 W 项 |

`8f34c276` 的 author date 为 2024-08-21、committer date 为 2024-10-14，表中日期均为 author date。

`824b817f` 的差异（PR 正文只有标题 `Pullrequest 260319`、无评论；它由 10 个子提交 squash 而成，相关的是 `da1238a1` "Unify device tree for Luckfox Pico series"、`b8ebcda6` "Unify BoardConfig for Luckfox Pico series"、`c263bac1` "Update lunch menu list"、`a1dc5a32` "wifi_start.sh : Add support for aic8800dc"）：

- 删除前 `ultra-w.dts` 相比 `ultra.dts` 只多 `model` 的 " W" 与 WiFi/BT 节点：`restart-poweroff`、`sdio_pwrseq`、`wireless-bluetooth`、`&sdmmc` 的 SDIO 配置、`sdmmc0_det` pinctrl、`&uart1` 的 BT 配置与 `uart1_gpios`。
- 合并后 `ultra.dts` 相比旧 `ultra-w.dts` 只有两处不同：`model` 去掉 " W"；删掉 `BT,wake_host_irq = <&gpio1 RK_PA2 ...>`，该脚与 `sdio_pwrseq` 的 `reset-gpios` 是同一引脚。
- 合并后 Ultra BoardConfig 与旧 Ultra_W BoardConfig 只有 `LF_ORIGIN_BOARD_CONFIG`、`RK_KERNEL_DTS` 两处文件名不同（均含 `RK_ENABLE_WIFI=y`、`RK_ENABLE_WIFI_CHIP=AIC8800DC`、`rv1106-bt.config`、`luckfox_pico_w_defconfig`）。Pi 与 86Panel 按同一模式合并。
- lunch 菜单：合并前 `LF_HARDWARE` 依次为 `…Pro_Max`、`Ultra`、`Ultra_W`、`Pi`、`Pi_W`、`86Panel`、`86Panel_W`、`Zero`（即 `[5] Ultra`、`[6] Ultra_W`），合并后去掉三个 W 项；fork CI 的 `printf '5\n0\n0\n'` 前后都落在 Ultra。
- Buildroot 的 aic8800 驱动加载不依赖 `model` 是更早就有的：`sysdrv/drv_ko/wifi/insmod_wifi.sh` 的 aic8800 分支条件为 `model` 含 `W` **或** `/sys/bus/sdio/devices/*/uevent` 含 `C8A1:C18D`，后一条件由 `99424375`（#310，2025-08-14，新增 Pico Zero）加入。#355 的子提交 `a1dc5a32` 再在 `project/app/wifi_app/bin/wifi_start.sh` 补了按 `C8A1:C18D` 启动 aic8800 的 `wpa_supplicant`。两处都是 `grep` 子串匹配。

即「保留 W 版配置、去掉非 W 版」：一份镜像同时服务两种硬件，`model` 不再区分是否带 WiFi。

### 2.3 CI 与真机证据

- run 35115207257 的 Ultra W job 以 `printf '5\n0\n0\n' | ./build.sh lunch` 选板；lunch 菜单只有 `[5] RV1106_Luckfox_Pico_Ultra`，日志确认选中 `BoardConfig-EMMC-Buildroot-RV1106_Luckfox_Pico_Ultra-IPC.mk`、`RK_KERNEL_DTS=rv1106g-luckfox-pico-ultra.dts`、`RK_ENABLE_WIFI=y`、`RK_ENABLE_WIFI_CHIP=AIC8800DC`、`RK_BUILDROOT_DEFCONFIG=luckfox_pico_w_defconfig`。
- 作者 Ultra W 刷该固件后实测：`/proc/device-tree/model` 为 `Luckfox Pico Ultra`；`/dev/dri/card0`、`/dev/fb0`、`/dev/input/event0` 存在；`/usr/lib/libdrm.so.2`、`/usr/lib/libcjson.so.1` 存在；`ls /sys/bus/sdio/devices` 为 `mmc1:e9ea:1  mmc1:e9ea:2`；`lsmod` 有 `aic8800_bsp`/`aic8800_fdrv`/`aic8800_btlpm`；`iw dev` 有 `wlan0`；`/oem/usr/ko/aic8800*.ko` 存在。硬件与固件都是 W，WiFi 栈已工作，只有例程判据失效。
- 作者 Ultra W（同一固件、另一次开机）实测 SDIO 与 WiFi 运行状态（`uevent` 两行为 `grep -H SDIO_ /sys/bus/sdio/devices/*/uevent` 的输出，未列 `MODALIAS`）：

  | 项 | 结果 |
  |---|---|
  | `cat /proc/device-tree/model` | `Luckfox Pico Ultra` |
  | `ls /sys/bus/sdio/devices` | `mmc1:7a8a:1  mmc1:7a8a:2` |
  | `mmc1:7a8a:1/uevent` | `SDIO_CLASS=07`、`SDIO_ID=C8A1:C08D`、`SDIO_REVISION=0.0` |
  | `mmc1:7a8a:2/uevent` | `SDIO_CLASS=07`、`SDIO_ID=C8A1:C18D`、`SDIO_REVISION=0.0` |
  | `which wpa_cli wpa_supplicant udhcpc` | `/usr/bin/wpa_cli`、`/usr/bin/wpa_supplicant`、`/sbin/udhcpc` |
  | `ls -l /etc/wpa_supplicant.conf` | `-rw-rw-r-- 1 1007 1007 145 Jun 4 2026` |
  | `pidof wpa_supplicant` | `590` |
  | `ls /var/run/wpa_supplicant` | `wlan0`（控制套接字存在） |

- `mmc1:RRRR:N` 中 `RRRR` 是 SDIO 卡的 RCA（相对卡地址，每次枚举分配，两次开机分别为 `e9ea`、`7a8a`），`N` 是 function 编号，二者都不是厂商/设备 ID；判据只能读 `uevent` 内容，不能依赖设备目录名。
- 镜像核实（artifact 10458251515，`SHA256SUMS` 校验 `boot.img`/`rootfs.img`/`oem.img` 均 OK）：
  - `boot.img` 含两份内核 FDT（偏移 2048 与 3714560，各 41740 字节），根节点 `model` 长 19 字节、值为 `Luckfox Pico Ultra\0`，不含 `Luckfox Pico Ultra W`，含 `sdio-pwrseq` 节点。
  - `rootfs.img`（`debugfs`）：有 `/usr/bin/wpa_cli`、`/usr/bin/wpa_supplicant`、`/usr/bin/wifi_start.sh`、`/sbin/udhcpc`（符号链接）、`/etc/wpa_supplicant.conf`（含 `ctrl_interface=/var/run/wpa_supplicant` 与一个 `network={}` 块）、`/etc/init.d/S99hciinit`、`/usr/bin/luckfox-config`；无 `/usr/bin/wifi_bt_init.sh`、无 `/etc/rc.local`。
  - `oem.img`：`/usr/ko` 有 `insmod_ko.sh`、`insmod_wifi.sh`（aic8800 分支条件与 §2.2 所述一致）、`aic8800_bsp/fdrv/btlpm.ko`、`aic8800dc_fw`。
  - 开机加载链：`/etc/init.d/S21appinit` → `/oem/usr/bin/RkLunch.sh` → `/oem/usr/ko/insmod_ko.sh` → 后台 `insmod_wifi.sh` → aic8800 分支 `insmod`。
  - `S99hciinit` 在 `lsmod` 见 `aic8800_fdrv` 时执行 `hciattach -s 1500000 /dev/ttyS1 any 1500000 flow nosleep` 并 `ifconfig wlan0 up && udhcpc -i wlan0`，即 BT 固定占用 UART1（`ttyS1`）。

### 2.4 SDIO ID 依据

- 内核 `sysdrv/source/kernel/drivers/mmc/core/sdio_bus.c` 为每个 SDIO function 的 `uevent` 依次输出 `SDIO_CLASS=%02X`、`SDIO_ID=%04X:%04X`（厂商:设备，大写十六进制）、`SDIO_REVISION=%u.%u`、每条 CIS 信息串一行 `SDIO_INFO%u=%s`、`MODALIAS=sdio:c%02Xv%04Xd%04X`。
- SDK 驱动 `sysdrv/drv_ko/wifi/aic8800dc/aic8800_bsp/aicsdio.c`：`SDIO_VENDOR_ID_AIC8800DC 0xc8a1`、`SDIO_DEVICE_ID_AIC8800DC 0xc08d`；同文件其他芯片的 function 2 设备 ID 均为 function 1 加 `0x100`（AIC8801 `0x0145`/`0x0146`，AIC8800D80 `0x0082`/`0x0182`）。AIC8800DC 的 function 1 为 `C8A1:C08D`、function 2 为 `C8A1:C18D`，后者即 `insmod_wifi.sh`/`wifi_start.sh` 所匹配的值。真机 `uevent`（§2.3）与此逐项一致，两个 function 的 `SDIO_CLASS` 均为 `07`（WLAN）。
- 驱动本身按 `SDIO_DEVICE_CLASS(SDIO_CLASS_WLAN)` 匹配并在 probe 中只处理 function 2，与上面的 function 划分一致。
- 与固件行为互证：镜像中加载 `aic8800_bsp.ko` 的只有 `insmod_wifi.sh`（SDK 其余加载它的 `emmc_wifi_bt_init.sh`、`overlay-luckfox-glibc-ultra/usr/bin/wifi_bt_init.sh` 均要求 `model` 等于 `"... Ultra W"`，且未装入该镜像，见 §9）；作者板子 `model` 不含 `W` 而 aic8800 驱动已加载，正是 `uevent` 命中 `C8A1:C18D` 的结果。

### 2.5 Ultra 引脚与非 W 板

`luckfox-config` 的 Ultra 引脚图未列出 `GPIO1_A2`、`GPIO0_A5`、`GPIO3_A*`（sdmmc0）与 `UART1_M0`，统一设备树新增的 WiFi/BT 节点占用的引脚都不在排针上。非 W 板上 `sdmmc` 作为 non-removable SDIO 口启用但无设备，推断 `/sys/bus/sdio/devices` 为空、内核有探测失败日志；eMMC 走独立控制器，挂在 mmc 总线而非 sdio 总线。此推断未在非 W 硬件上实测。

## 3. 需求

- FR1：`luckfox_get_wifi_enable_info()` 按 §5 判定表设置 `WIFI_ENABLE`。
- FR2：SDIO 检测用 `opendir`/`readdir` 遍历 `/sys/bus/sdio/devices`，跳过以 `.` 开头的项，逐个读 `<项>/uevent`，任一行（去掉末尾换行后）精确等于 `SDIO_ID=C8A1:C18D` 即判定有 Ultra W 板载 WiFi；目录打不开、`uevent` 打不开或无匹配行视为无。不调用 `popen`/`system`/shell glob。SDK 脚本用 `grep` 子串匹配，本仓用整行精确匹配，避免 `C8A1:C18DX` 之类误中。
- FR3：旧固件 `model = "Luckfox Pico Ultra W"` 直接置 1，不做 SDIO 检测。
- FR4：未知型号仍 `exit(EXIT_FAILURE)`，报错文案不变。
- FR5：修正同函数内的重复 `fclose`，每条路径恰好关闭一次。
- FR6：`AGENTS.md` 增加一条 gotcha：SDK `824b817f` 起 `/proc/device-tree/model` 不区分 Ultra 与 Ultra W，例程以 SDIO ID `C8A1:C18D` 判定 WiFi。
- FR7：`custom/custom_wifi.c` 的 `wifi_backend_release()` 仅在 `wifi_update_timer != NULL` 时 `lv_timer_del()`，删除后置 `NULL`，消除 §2.1 所述退出段错误。
- NFR1：只改 `custom/custom_main.c`、`custom/custom_wifi.c`（仅 FR7）及 FR6 的 `AGENTS.md`；不动 `generated/`、`src/main.c`、`CMakeLists.txt`、README。
- NFR2：新代码不引入 `strcpy`/`strcat`/`sprintf`/`gets`；路径拼接用 `snprintf` 并检查截断，读行用 `fgets`，缓冲区长度由 `sizeof` 约束。C11 Annex K 的 `*_s` 函数在 glibc/uClibc 均不可用，字符串比较沿用既有代码的 `strcmp`（两侧均为 `fgets` 截断后以 NUL 结尾的缓冲区或字面量）。
- NFR3：glibc 与 uClibc 两条 CI 行均须编译通过。

## 4. 设计决策

| 决策 | 结论 | 理由 |
|---|---|---|
| D1 修复位置 | 改例程判据，不改 SDK/DTS | 统一设备树对 W 硬件功能等价且去掉一处引脚冲突；`model` 已不承载 WiFi 信息，应由消费方改判据（§10 Q3、Q4） |
| D2 WiFi 判据 | 任一 SDIO function 的 `uevent` 含 `SDIO_ID=C8A1:C18D` | Ultra W 的硬件配置是确定的（板载 AIC8800DC），与 SDK 加载驱动的 `insmod_wifi.sh` 及 `wifi_start.sh` 所用 ID 相同，例程显示 WIFI 键与固件加载驱动同条件；只认该芯片，不把其他 SDIO 设备误判为 WiFi；SDIO 在内核阶段枚举，早于 `S21appinit` 等用户态脚本，不依赖后台 `insmod_wifi.sh` 是否已完成（§2.4、§10 Q5） |
| D3 型号前置条件 | 仍先读 `model`，只在 Ultra 系列两种字符串上做判定 | 保持「非 Ultra 板直接退出」的既有语义，不把判据扩大到其他板型 |
| D4 旧固件兼容 | `"Luckfox Pico Ultra W"` 直接置 1 | 旧设备树明确声明 W；保留原行为，零风险；「`model` 为 W 或 SDIO ID 命中」与 `insmod_wifi.sh` 的 aic8800 分支同构 |
| D5 顺带修复 | 修重复 `fclose` | 同函数重写时顺手消除未定义行为；不扩大到其他函数 |
| D6 文档 | 只加 `AGENTS.md` 一条 gotcha，不改 README | README「Ultra 上不显示 WIFI 键」的描述仍成立，否决改成「无无线网卡则不显示」；gotcha 防止后续仅凭 `model` 判定是否带 WiFi（旧固件 `model` 为 `Luckfox Pico Ultra W` 时直接视为带 WiFi） |
| D7 退出段错误 | 本 PR 内修 `wifi_backend_release()`（FR7） | 恢复 WIFI 键使该段错误在 Ultra W 上可达，属本修复引入的用户可见行为变化；改动 4 行且与定时器最多创建一次、主循环结束后才释放的生命周期相容；否决「不修只记录风险」（合并即带入可稳定复现的崩溃） |

## 5. 设计

判定表（`model` 已去掉末尾换行）：

| `model` | `/sys/bus/sdio/devices/*/uevent` | `WIFI_ENABLE` |
|---|---|---|
| `Luckfox Pico Ultra W` | 不检测 | 1 |
| `Luckfox Pico Ultra` | 有一行为 `SDIO_ID=C8A1:C18D` | 1 |
| `Luckfox Pico Ultra` | 无匹配行、目录为空或不存在 | 0 |
| 其他 | 不检测 | `exit(EXIT_FAILURE)` |
| 读取失败 | 不检测 | 保持 `custom_init()` 置的 0，不退出 |

结构：

- `custom/custom_main.c` 的 `DEFINES` 区新增 `LUCKFOX_SDIO_BUS_DIR`（`"/sys/bus/sdio/devices"`）与 `LUCKFOX_ULTRA_W_SDIO_ID`（`"SDIO_ID=C8A1:C18D"`）。
- `STATIC FUNCTIONS` 区新增 `static int luckfox_sdio_has_id(const char *bus_dir, const char *uevent_line)`，返回 1/0，只依赖 libc；目录作参数传入，便于本机用临时目录验证（§6 第 1 步）。
- `luckfox_get_wifi_enable_info()` 的 `"Luckfox Pico Ultra"` 分支改为 `WIFI_ENABLE = luckfox_sdio_has_id(LUCKFOX_SDIO_BUS_DIR, LUCKFOX_ULTRA_W_SDIO_ID);`。
- `dirent.h`、`string.h`、`stdio.h` 已由 `custom/custom.h` 引入，无需新头文件。
- `custom/custom_wifi.c` 的 `wifi_backend_release()` 按 FR7 判空并置 `NULL`。

错误处理：`/proc/device-tree/model` 打不开维持现状（`perror` + `exit`）；读取失败只 `perror`，`WIFI_ENABLE` 保持 0；`opendir`/`fopen` 失败与路径截断都按「无匹配」处理且不打印错误（非 W 板或内核无 SDIO 总线属正常情况）。`uevent` 按 `fgets` 256 字节分段读行：本板两个 function 均无 `SDIO_INFO` 行（§2.3），最长行为 27 字节的 `MODALIAS=sdio:c07vC8A1dC18D`；长度由卡 CIS 决定的 `SDIO_INFO` 行超过 255 字节时会分段，只有某一段恰好等于完整判据行才会误中，无现实可能。

## 6. 验证（plan 执行时落地，此处只定判据）

1. 本机逻辑验证：从 `custom/custom_main.c` 抽出 `luckfox_sdio_has_id()`，用临时 harness（不入库）以 host gcc `-Wall -Wextra -Werror` 编译，在临时目录模拟 sysfs，覆盖：目录不存在→0；目录为空→0；仅有 `SDIO_ID=C8A1:C08D`→0；某 function 含 `SDIO_ID=C8A1:C18D`→1；匹配行位于 `.` 开头的项内→0；匹配行无末尾换行→1；`SDIO_ID=C8A1:C18DX` 这类前缀相同的行→0。
2. 本机交叉编译：Cloud Agent 容器内走 glibc 路径 `cmake && make && make install` 成功，产物为 ELF32 ARM。
3. CI：PR 上 `构建 Luckfox Pico LVGL 例程` 的 glibc 与 uClibc 两行均绿，ABI 门禁通过。
4. 真机前置采集（Ultra W，刷 run 35115207257 固件）：`cat /proc/device-tree/model`、`ls /sys/bus/sdio/devices`、`grep -H SDIO_ /sys/bus/sdio/devices/*/uevent`、`which wpa_cli wpa_supplicant udhcpc`、`ls -l /etc/wpa_supplicant.conf`、`pidof wpa_supplicant`、`ls /var/run/wpa_supplicant`，结果回填 plan。**判据门槛**：必须有一个 function 显示 `SDIO_ID=C8A1:C18D`；若没有，停止实施并按实测值修订 D2，不得带着未证实的 ID 合并。实测结果见 §2.3。
5. 真机功能（uClibc artifact）：主界面出现 WIFI 键且五键布局正常；进入 WIFI 页不崩溃；退出分两条路径各跑一次并分列记录：进入过 WIFI 页后点 OFF、启动后不进 WIFI 页直接点 OFF，均须正常退出（无段错误、屏幕被清）且 `echo $?` 为 0（`wifi_backend_release()` 在两条路径上均被执行）。WIFI 页扫描/连接经 `wpa_cli` 与运行中的 `wpa_supplicant` 通信，结果如实记录；若届时 `wpa_supplicant` 未运行，记为「按键已恢复、WiFi 页功能受固件限制」，不算本修复失败。
6. 非 W 板与旧固件：作者无对应硬件/镜像，只按判定表做代码走查，plan 中标注「未实测」，不得写成通过。

## 7. 风险

| 风险 | 缓解 |
|---|---|
| 后续 Ultra W 批次换用其他 AIC 芯片（如 D80） | 不显示 WIFI 键但不崩溃；届时随 SDK `insmod_wifi.sh` 的判据同步增加 ID |
| 其他开机时序下 `wpa_supplicant` 未运行，WIFI 页 `wpa_cli` 无法通信 | 作者板子已实测在运行且控制套接字存在（§2.3）；如遇未运行属固件范畴，不在本仓绕过 |
| 未来官方给 W 恢复独立 `model` | 判定表第一行已兼容 |
| 其他 Ultra 衍生板（B/BW）`model` 不同 | 不在本仓支持范围，维持既有 `exit` 语义 |

## 8. 文档与提交

| 文件 | 动作 | 何时 |
|---|---|---|
| `custom/custom_main.c`、`custom/custom_wifi.c` | FR1–FR5、FR7 | `fix(custom)` 提交 |
| `AGENTS.md` | FR6 一条 gotcha | 单独 `docs(agents)` 提交 |
| 本 spec 与关联 plan | 设计、验证证据、偏离 | PR 最后单个 `docs(superpowers)` 提交 |

## 9. luckfox-pico 待修问题（仅记录，本 PR 不修复）

以下同属 `824b817f` 的残留，位于 `yuangezhizao/luckfox-pico`，需另开 spec 修复；修复原则同样是改成检测硬件，适合回馈 LuckfoxTECH 上游：

- `project/cfg/BoardConfig_IPC/overlay/overlay-luckfox-config/usr/bin/luckfox-config` 的 `luckfox_check_uart`：条件为 `"$LUCKFOX_CHIP_MODEL" == "Luckfox Pico Ultra W"`，统一设备树后永不成立，「BlueTooth is enable,Can't enable UART1」的拦截失效。而 `S99hciinit` 在 aic8800 驱动加载后固定把 BT 挂在 `/dev/ttyS1`（UART1，§2.3），用户可经 `luckfox-config` 把 UART1 切到排针 M1（`GPIO2_A4/A5`）与 BT 冲突，属功能回退。该 overlay 在 Ultra BoardConfig 的 `RK_POST_OVERLAY` 内，镜像中有 `/usr/bin/luckfox-config`，问题实际生效。
- `sysdrv/tools/board/emmc/emmc_wifi_bt_init.sh`（同目录 `emmc_rc.local` 第 4 行调用 `/usr/bin/wifi_bt_init.sh`，推断二者配套安装）与 `overlay-luckfox-glibc-ultra/usr/bin/wifi_bt_init.sh`：仍比对 `"... Ultra W"`。但当前 Buildroot 镜像不含它们：rootfs 无 `/usr/bin/wifi_bt_init.sh` 与 `/etc/rc.local`；`sysdrv/Makefile`、`project/build.sh` 未引用 `tools/board/emmc`；所有 BoardConfig 的 `RK_POST_OVERLAY` 都不含 glibc overlay；Ubuntu BoardConfig 已在 `d2da6f0f` 停止。属死代码，修复或删除另议，优先级低于上一条。
- SDK `README.md`/`README_CN.md` 仍列出 `RV1106_Luckfox_Pico_Ultra_W` lunch 项与已删除的 BoardConfig。
- 本仓 [PR #4](https://github.com/yuangezhizao/luckfox_pico_lvgl_example/pull/4) 评论中的上板步骤写有「model 应为 Luckfox Pico Ultra W，否则 exit」，与代码不符，需另行更正。

## 10. QA（设计决策/约束澄清）

**Q1：烧的是 luckfox-pico CI 编出的 Ultra W 固件，为什么 `model` 是 Luckfox Pico Ultra 而不是 Ultra W？**
A：**CI 选板正确，是 SDK 设备树本来就这样写。** 当前 SDK lunch 菜单没有 W 项，Ultra BoardConfig 即原 W 配置，所用 `rv1106g-luckfox-pico-ultra.dts` 的 `model` 为 `"Luckfox Pico Ultra"`，fork 与官方一致；镜像 `boot.img` 的 DTB 中 `model` 同为 `Luckfox Pico Ultra`。Buildroot 的 aic8800 驱动由 `insmod_wifi.sh` 按「`model` 含 W 或 SDIO ID `C8A1:C18D`」加载，不受影响；受影响的是仍比对 `"... W"` 字符串的代码。证据见 §2.2、§2.3。

**Q2：SDK 历史中是否存在 Ultra W 的设备树？因为什么移除？**
A：**存在，`d3153ac9` 加入，`824b817f`（#355）删除。** 官方未公开理由，由差异推断：W 与非 W 的 DTS/BoardConfig 只差 WiFi/BT 节点与几项开关，三块 W 板（Ultra、Pi、86Panel）成对维护；合并方式是保留 W 配置，让一份镜像覆盖两种硬件；顺带去掉与 `sdio_pwrseq` 复位脚重复占用 `GPIO1_A2` 的 `BT,wake_host_irq`；Buildroot 驱动加载早在 `99424375`（#310）就有 SDIO ID 兜底，合并不会让 W 板丢驱动，同 PR 另在 `wifi_start.sh` 补了 aic8800 的 SDIO ID 匹配。Ubuntu 版 Ultra_W BoardConfig 已先于合并在 `d2da6f0f`（#274）删除。详见 §2.2。

**Q3：官方移除是否合理？是否有必要在自己分支 revert，加回 W 的 dts？**
A：**合理但属无声的破坏性变更；不 revert。** W 硬件拿到的配置与删除前等价，还少一处引脚冲突；新增节点所占引脚不在 Ultra 排针上，非 W 板代价推断仅为探测失败日志与轻微启动开销（未实测）。不合理处在于 `model` 原是软件区分 W 的唯一依据，删除后没有替代、未写入 `UPDATE_LOG`、依赖脚本未同步。revert 只换回一个字符串，却要恢复 `build.sh` lunch 菜单中的三个 W 项（合并前为 `[5] Ultra`、`[6] Ultra_W`，其后 Pi/86Panel/Zero 编号随之后移）并核对 fork CI 的 `printf '5\n0\n0\n'`、与后续同步官方冲突、带回 `GPIO1_A2` 冲突，W 镜像烧到非 W 板还会谎报型号。确有程序硬依赖 W 字符串时，退路是在 fork 加一个 `#include "rv1106g-luckfox-pico-ultra.dts"` 后只覆盖 `model` 的薄 DTS 加一份 BoardConfig，不改官方文件；本 spec 不采用。

**Q4：官方修改是否不彻底？例程仍判断 W 该怎么解决？**
A：**不彻底；各消费方改为检测硬件。** #355 改了 28 个文件，未改本仓例程、`luckfox-config` 的 `luckfox_check_uart`、两份 `wifi_bt_init.sh`、SDK README。其中 `luckfox_check_uart` 在当前镜像中生效，两份 `wifi_bt_init.sh` 未装入当前镜像（死代码）。本仓按本 spec 修；SDK 侧见 §9。

**Q5：WiFi 判据用 SDIO 设备存在、精确 SDIO ID，还是 `wlan0`？**
A：**精确 SDIO ID `C8A1:C18D`（D2）。** Ultra W 的硬件配置是确定的（板载 AIC8800DC），与 SDK `insmod_wifi.sh`、`wifi_start.sh` 所用 ID 相同，例程与固件口径一致；ID 与驱动源码一致，并经作者 Ultra W 的 `uevent` 实测确认（§2.3、§2.4）。否决「SDIO 下存在任一设备」（不区分设备种类，任何 SDIO 设备都会被当作 WiFi）；否决 `/sys/class/net/wlan0` 存在或 `lsmod` 含 `aic8800_fdrv`（依赖驱动已加载，`insmod_wifi.sh` 在后台执行，例程早于它完成自启会误判，驱动加载失败时也会隐藏按键）；否决「改 DTS `model`」（迁就错误判据、会谎报非 W 板）；否决「检查 `/oem/usr/ko/aic8800*.ko`」（反映固件打包而非硬件，统一固件在非 W 板上同样存在）。

**Q6：修复范围只在本仓，还是连同 SDK 一起？**
A：**本 PR 只修本仓例程；SDK 侧问题记录在 §9，不在本 PR 修复。** SDK 残留在另一个仓库、验证方式不同（需重编固件），另开 spec。

**Q7：真机验证与未实测项如何记录？**
A：**真机验证步骤与判据写在 §6，实测结果回填 plan「验证证据」。** 作者只有 Ultra W 与当前统一固件，非 W 板与旧固件（`model = "Luckfox Pico Ultra W"`）无法实测，按判定表代码走查并如实标为「未实测」。

**Q8：PR 与文档提交如何安排？**
A：**提交序为 `fix(custom)` → `docs(agents)` → `docs(superpowers)`（spec 与 plan 为 PR 最后单个提交），以草稿 PR 承载 Review。** 评审发现的问题直接合并进对应提交，不追加修复提交。

**Q9：恢复 WIFI 键后暴露的退出段错误是否在本 PR 修？**
A：**修（D7、FR7）。** 缺陷在旧固件上即存在，但统一设备树固件上原本不可达；本 PR 让它在 Ultra W 上可稳定复现，故随判据修复一并处理，范围仅 `wifi_backend_release()` 判空，`custom_wifi.c` 其余既存问题仍不在范围内。

## 11. 参考资料

- SDK 设备树合并提交：https://github.com/LuckfoxTECH/luckfox-pico/commit/824b817f
- SDK PR #355：https://github.com/LuckfoxTECH/luckfox-pico/pull/355
- SDK `insmod_wifi.sh` 加入 SDIO ID 兜底：https://github.com/LuckfoxTECH/luckfox-pico/commit/99424375
- SDK 停止 Ubuntu BoardConfig：https://github.com/LuckfoxTECH/luckfox-pico/commit/d2da6f0f
- SDK 源码：`sysdrv/drv_ko/wifi/insmod_wifi.sh`、`sysdrv/drv_ko/insmod_ko.sh`、`project/app/wifi_app/bin/wifi_start.sh`、`project/cfg/BoardConfig_IPC/overlay/overlay-luckfox-buildroot-init/etc/init.d/S99hciinit`、`sysdrv/drv_ko/wifi/aic8800dc/aic8800_bsp/aicsdio.c`、`sysdrv/source/kernel/drivers/mmc/core/sdio_bus.c`
- fork 固件 CI：https://github.com/yuangezhizao/luckfox-pico/actions/runs/35115207257
- fork 固件 CI workflow：https://github.com/yuangezhizao/luckfox-pico/blob/dev/.github/workflows/build-luckfox-pico-firmware.yml
- luckfox-config 官方文档：https://wiki.luckfox.com/Luckfox-Pico-RV1106/Peripherals/Luckfox-config/
- 本仓 `custom/custom_main.c`、`custom/custom_wifi.c`、`generated/setup_scr_Main.c`、`src/main.c`
