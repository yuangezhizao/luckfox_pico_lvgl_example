# Luckfox Pico LVGL Example — 既存缺陷统一修复设计规格

- **日期**：2026-10-01
- **状态**：spec 与 plan 已随 PR 提交，实现与本地自动化验证已完成；CI [run 37125871849](https://github.com/yuangezhizao/luckfox_pico_lvgl_example/actions/runs/37125871849) 四个 job 全部 success，原生测试 78/78，除 `docs/superpowers` 外代码树与最终提交相同；Task 32 真机验证已执行：准备、主屏、mpv 耗时、OFF 退出通过，WiFi 多数子项通过，画板与音乐页不通过，遗留问题转后续 PR（见 plan 验证证据与真机遗留问题）
- **分支**：`cursor/fix-existing-defects-spec-6be3`
- **关联计划**：[`../plans/2026-10-01-lvgl-example-fix-existing-defects.md`](../plans/2026-10-01-lvgl-example-fix-existing-defects.md)。代码以仓库为准；plan 留 Task、证据与偏离。
- **范围来源**：[`2026-09-28-lvgl-example-native-tests-design.md`](2026-09-28-lvgl-example-native-tests-design.md) §10（纳入 25 处、暂缓 7 项），本 spec 在其上增补 5 条同函数内的关联缺陷（§3.2）。

## 1. 概述与目标

例程在主屏时间、WiFi 页、画板、音乐页、OFF 退出存在一批既存缺陷，其中栈溢出、死循环、配置丢失、命令注入类在真机正常使用路径上可达。测试基础设施与编译告警已由 PR #7 就绪。本 PR 在单个 PR 内逐处修复，每处先在 `tests/cases/<页面>/` 写出会失败的测试再修，修复与其测试同一个提交。

目标：

- §3.1、§3.2 列出的 30 处缺陷全部修复，每处都有在修复前失败、修复后通过的自动化测试（编译层缺陷以对应 `-Werror=` 编译失败为失败测试）。
- 修复后 `ctest` 全绿，glibc 与 uClibc 两条交叉编译与 `native-tests` job 全绿。
- 行为变更只限于消除缺陷本身；新增的输入校验与提示是 §5.2 明确列出的行为变更。

非目标：§3.3 的 7 项暂缓；`strcpy(addr.sun_path, "/tmp/mpvsocket")` 本身不是缺陷（常量 15 字节，无溢出），仅随 D7 把路径改成可被测试覆盖的宏，板上取值不变；`custom_wifi.c` 写配置时对所有 `network={}` 块一并改写、`ssid=<hex>` 与裸 64 位十六进制 `psk=` 读不出（现状行为，不是回归）、同名多 BSSID 在扫描下拉框里不去重（现状）；编译告警的逐条清理（本 PR 期间新出现的告警只在 plan 的偏离记录中登记，不逐条修）；`-O3` 与 LVGL 优化级别；重构 `custom/` 与系统调用的耦合（仅为可测性做函数内最小拆分）。

## 2. 背景与依据

### 2.1 25 处在最新 `dev` 上的复核

基线 `origin/dev` `45732b6`（PR #7 已合入）。逐文件读源码核对，行号为该提交的行号。

| 条目 | 位置 | 结论 |
|---|---|---|
| ① `vfork` 子进程 exec 失败后 `return 0` | `custom_musicplayer.c:376` | 仍在；真机固件自带 `mpv`，板上不触发，主机无 `mpv` 时触发（`tests/README.md` 因此规定测试不调 `custom_init()`） |
| ② `roller_str[256]` 由 `strcat` 拼接 | `setup_scr_Music_player.c:413`、`custom_musicplayer.c:238-243` | 仍在 |
| ③ `sprintf(cmd[256], "…/music/%s…")`，`"` 可改写 JSON | `custom_musicplayer.c:224-227` | 仍在 |
| ④ roller 选中 ≥32 字节文件名时 `strcmp` 死循环 | `:258-262`（`buf[32]` 截断后永不相等） | 仍在 |
| ⑤ `cJSON_Parse` 结果不释放 | `:163` | 仍在 |
| ⑥ `read` 读满 512 字节不补 NUL | `:155-160` | 仍在 |
| ⑦ mpv 退出后写套接字触发 SIGPIPE | 全部 `write(fd_mpv, …)` | 仍在 |
| ⑧ `opendir` 失败后仍 `readdir` | `:212-215` | 仍在 |
| ⑨ 3 个 `int` 函数缺 `return` | `music_player_thread_init`、`music_app_init`、`music_app_quit` | 仍在 |
| ⑩ 只 `connect` 一次、`sleep(0.1)` 实为 `sleep(0)`、启动时同步跑无参 `mpv` | `:381`、`:397`、`custom_main.c:230` | 仍在 |
| ⑪ `close(0)` | `:382` | 仍在 |
| ⑫ `tm_hour > 12` 判下午 | `custom_main.c:65` | 仍在；12:00–12:59 显示 AM，0:00–0:59 显示 0 |
| ⑬ `custom_tick_get()` 用 `gettimeofday` 与 `tv_sec * 1000000` | `custom_main.c:238-252` | 仍在 |
| ⑭ `highlighted_days` 为局部数组、`strtok` 改写日期标签 | `widgets_init.c:115-123` | 仍在 |
| ⑮ `custom_tick_get`、`open` 隐式声明 | `lib/lv_conf.h:89`、`custom_main.c:117` | 仍在；`-Werror=implicit-function-declaration` 下两处均报错；`custom/custom.h:18` 包含 `<linux/fcntl.h>`，直接补 `<fcntl.h>` 会让 `struct flock` 重复定义而编译失败（D8） |
| ⑯ `popen` 失败后仍 `fgets` | `custom_main.c:347-352` | 仍在 |
| ⑰ `system("cat /dev/zero > /dev/fb0")` 打印写满错误 | `src/main.c:99` | 仍在 |
| ⑱ 画布缓冲区为已返回函数的栈 VLA，且按字节数当元素数 | `setup_scr_Sketchpad.c:34-38` | 仍在 |
| ⑲ 屏幕坐标当画布坐标 | `custom_sketchpad.c` 的 `lv_indev_get_point` 原值 | 仍在 |
| ⑳ SSID/密码不校验 | `custom_wifi.c:53-66,100-111`、`setup_scr_WIFI.c:64,104` | 仍在 |
| ㉑ 相对路径临时文件、先 `remove` 后 `rename` | `custom_wifi.c:79,124-125` | 仍在 |
| ㉒ 扫描结果按空格切分 | `custom_wifi.c:237-264` | 仍在 |
| ㉓ `_wifi_status_update` 中 `pclose(NULL)` | `custom_wifi.c:177-181` | 仍在 |
| ㉔ 离开 WIFI 页后定时器仍每 5 s `popen`、每按 Load 多起一个 `udhcpc` | `custom_wifi.c:129-130,291-293,368-371` | 仍在 |
| ㉕ `sscanf` 读引号值不限宽度 | `custom_wifi.c:158,161` | 仍在 |

真机可达性：除 ①、⑯ 外全部在 Luckfox Pico Ultra W 的正常使用路径上（⑯ 的 `popen` 只在 `fork` 失败时失败，属防御性修复）。改动量：多数 1–15 行；较大的是音乐列表与 mpv 命令构造（②③④，新增 `custom/` 接口）、画布缓冲区（⑱⑲）与 WiFi 校验、扫描解析、配置写入（⑳㉑㉒）。

### 2.2 `wpa_supplicant` 对 `"`、`\`、`#` 的解析（实测）

本地装 `wpasupplicant` 2.10（Ubuntu 24.04），让 wpa_supplicant 以 `-D wired` 加载配置并打印解析结果，命令为 `wpa_supplicant -i lo -D wired -c t.conf -ddK -f out.log`（`-K` 显示 PSK 明文），配置形如 `network={ ssid="…" psk="…" }`；另起守护进程，用 `wpa_cli set_network 0 ssid "…"`、`set_network 0 psk "…"`、`save_config` 走应用同款流程。

| 值（引号内的内容） | 结果 |
|---|---|
| `a"b`、`pa"ss1234`（含 `"`） | 解析为原值，正常 |
| `a\b`、`pa\ss1234`、`abc\`（含 `\`） | `\` 按字面保留，不是转义符，正常 |
| `a#b`、`pass#word1`（只含 `#`） | 正常 |
| `a"b#c`、`a"#b`、`"#`（`#` 出现在某个 `"` 之后，且其前 `"` 个数为奇数） | **丢失**：`a"#b` 截成 `a`，`a"b#c` 与 `"#` 整个字段缺失 |
| `a#b"c`、`a#"b`、`#"`（`#` 在 `"` 之前） | 正常 |
| `a"b"c#d`、`a#b#c"d` | 正常 |
| `中文网络`、`中文密码abc`（UTF-8） | 正常，PSK 长度按字节计 |
| `wpa_cli` 流程提交含 `"` 或 `\` 的值 | 返回 `OK`；wpa_supplicant 自己写回 `ssid="a"b"`，不转义，与本应用 `sprintf` 写出的格式一致 |

结论：`\` 无任何问题；`"` 单独出现合法；只有「`#` 出现在 `"` 之后」会被当注释丢失（解析器逐字符切换引号状态，`#` 落在引号外即视为注释）。本规格取保守判据：值里任何 `#` 之前出现过 `"` 就拒绝，不去复刻解析器的奇偶规则，不依赖 wpa_supplicant 的具体版本。上表是 2.10 的行为；板上固件由 SDK 的 `project/app/wifi_app/Makefile`（`PKG_NAME_WPA_TOOLS := wpa_supplicant-2.6`）构建，2.6 的注释剥离取「第一个 `"` 之后的最后一个 `"`，只在其后找 `#`」，表中「丢失」的各类值在 2.6 上都能正常解析，所以该判据在两个版本上都安全，在 2.6 上只是多拒绝了一些本可使用的值。真正容不下 `"` 的是本工程自己的读取：`_wifi_conf_get` 的 `sscanf(" ssid=\"%[^\"]\"")` 遇到第一个 `"` 就停，回读丢尾，且不限宽度（㉕）。

### 2.3 LVGL 8.3.10 的相关行为

- `lv_roller_set_options(obj, NULL, …)` 触发 `LV_ASSERT_NULL`；本仓 `lib/lv_conf.h` 中 `LV_USE_ASSERT_NULL 1`、`LV_ASSERT_HANDLER while(1);`，空列表时传 NULL 会让 UI 线程死循环。选项数按 `\n` 计，含 `\n` 的文件名会让一个文件占两项、下标整体错位。
- `LV_ROLLER_MODE_INFINITE` 下 `lv_roller_get_selected()` 返回对真实选项数取模后的下标，可直接对应链表 `id`。
- `lv_textarea_set_text()` 在设了 `max_length` 时逐字符 `add_char`，但密码模式末尾把完整原文 `strcpy` 进 `pwd_tmp`，`lv_textarea_get_text()` 在密码模式返回 `pwd_tmp`：从配置回填的长口令不受 `max_length` 限制。`max_length` 按字符数计，32 个字符的 SSID 框可达 128 字节。故校验必须按字节在提交时做。
- `lv_scr_load_anim` 之后离开的屏幕不会被删除，本仓生成代码用 `Main_del`、`WIFI_del` 等标志决定是否重新 `setup_scr_*`：`ui_load_scr_animation(…, auto_del=false)` 会把离开页的标志置 false，再次进入时不再调用 `setup_scr_WIFI`，故 `wifi_backend_init()`（在 `setup_scr_WIFI.c:401`）全程只执行一次，定时器若在离开页面时删除则再也不会重建。
- `LV_CANVAS_BUF_SIZE_TRUE_COLOR(w, h)` 已是字节数（`(LV_COLOR_SIZE/8)*w*h`），把它当 `lv_color_t` 元素数声明数组会多分配 4 倍；宏体里的 `w`、`h` 没有加括号，调用处只传变量或自行加括号的表达式。
- `lv_canvas_set_buffer()`（`lv_canvas.c:66-80`）只设置 `cf`、`w`、`h`、`data`，不设置 `dsc.data_size`（构造函数把它置 0）。

### 2.4 32 位 `time_t` 与现有 arm32 测试

- Ubuntu 的 `arm-linux-gnueabihf-gcc` 13.3 默认 `_TIME_BITS=64`、`_FILE_OFFSET_BITS=64`，`sizeof(time_t)` 为 8（`_Static_assert(sizeof(time_t)==8)` 通过）；加 `-U_TIME_BITS -U_FILE_OFFSET_BITS` 后 `time_t` 变为 4 字节。板上 Buildroot uClibc 的 `time_t` 为 32 位。沿用现有 arm32 目标（`tests/cases/main/CMakeLists.txt`）而不加这两个选项，旧实现不会溢出，测试在修复前就会通过。
- 32 位 `time_t` 下 `tv_sec * 1000000` 在 32 位 `long` 算术中溢出：`tv_sec` 超过 2147（`2^31 / 10^6`）即溢出。板上 `tv_sec` 约 1.7×10^9，溢出是常态，tick 每约 71.6 分钟（`2^32` μs）出现一次约 4.29×10^6 ms 的回跳；模拟时钟若从接近 0 起算，则在绝对秒 2148 处首次出错。另外基于墙钟，WiFi 联网后 NTP 校时会让 tick 突变。

### 2.5 mpv 启动与套接字出现耗时

x86 云端 VM 上 `mpv --idle=yes --input-ipc-server=/tmp/mpvsocket` 启动到套接字出现耗时 42–137 ms（5 次）；Luckfox 的 ARM 核明显更慢，板上清缓存与预热耗时已测（见 plan 验证证据），整板重启未测（§8）。`mpv` 不存在时 exec 失败的子进程退出码为 127。现状 `sleep(1)` 是真睡 1 秒，连接前那句 `sleep(0.1)` 等于 `sleep(0)`。

### 2.6 测试基础设施用法

见 `tests/README.md`。本 PR 用到的机制：`luckfox_add_test()` 注册用例；`unit_<被测对象>.c` `#include` 被测 `.c` 以调用 `static` 函数；`BACKLIGHT_SYSFS_DIR` 同款的 `#ifndef` 宏默认值加测试覆盖，让被测路径指向 `tst_tmp_dir()`（覆盖值是运行期表达式：`tests/CMakeLists.txt` 给 `custom_brightness.c` 加 `COMPILE_DEFINITIONS "BACKLIGHT_SYSFS_DIR=tst_backlight_root()"`，原型经 `-include support/fake_fs.h` 取得；`tst_tmp_dir()` 每个进程各自 `mkdtemp`，`ctest -j` 下天然隔离。因此被测代码只能把这类路径宏当 `const char *` 用、经 `snprintf("%s/%s")` 拼接，不能做字符串字面量拼接，也不能对它 `sizeof`）；在测试可执行文件里定义同名函数（`popen`、`time`、`clock_gettime`、`gettimeofday`、`rename`）可覆盖来自静态库 `luckfox_app` 的调用，用于注入失败与时间；`PATH` 指向临时目录里的假 `mpv`、`wpa_cli`、`pkill`、`udhcpc` 脚本；`tests/tools/mutate.sh` 的 `.mutants` 清单检验测试能抓住对应改坏。两条限制：`.mutants` 的 `file` 只能是 `custom/` 下的文件，`generated/` 与 `lib/lv_conf.h` 的改动只由对应 ctest 验收，不配 `.mutants`；`luckfox_add_test()` 的 `TIMEOUT` 固定 120 s，roller 选中与画板分配失败用例使用 `alarm(5)`，死循环在约 5 秒以 SIGALRM 失败，其余死循环类用例最长等 120 s 判失败。`ctest -j` 并行运行各用例，凡用到固定路径（`/tmp/mpvsocket`、配置文件）的用例都要把路径换到各自的 `tst_tmp_dir()`。

## 3. 需求

### 3.1 纳入：25 处（来自 PR #7 spec §10.1）

| 页面 | 条目 |
|---|---|
| 主屏与时间 | ⑫⑬⑭⑮⑯ |
| WiFi | ⑳㉑㉒㉓㉔㉕ |
| 画板 | ⑱⑲ |
| 音乐页 | ①②③④⑤⑥⑦⑧⑨⑩⑪ |
| 退出 | ⑰ |

### 3.2 纳入：5 条关联缺陷（本 spec 增补，在 §3.1 对应函数内）

| 编号 | 缺陷 | 位置 | 并入 |
|---|---|---|---|
| C1 | `/music` 缺失或为空时 `music_app_init`、Next/Prev、roller 回调解引用 `playing_music_node==NULL`（`opendir` 判空后仍崩） | `custom_musicplayer.c:107-110,271-293,419-424` | ⑧ 之后单独一个提交 |
| C2 | 文件名含 `\n` 会让 roller 一个文件占两项、下标错位 | 选项串构造 | ② 之后单独一个提交 |
| C3 | 写配置时 `strstr(line,"ssid=")` 同时命中 `scan_ssid=1`、`bssid=`，`strstr(line,"psk=")` 命中 `wpa_psk=` 等，误改用户配置 | `custom_wifi.c:102-110` | ㉑ 之后单独一个提交 |
| C4 | `_wifi_conf_load` 里 `popen("wpa_cli")` 失败直接 `exit(EXIT_FAILURE)` | `custom_wifi.c:53-57` | 与 ㉓ 同一个 `popen` 失败处理提交 |
| C5 | 用 `strstr(line,"}")` 判块结束，SSID/口令含 `}` 时该行在匹配 `ssid=` 之前就被当成块结束，写回时原样保留、读取时读不出；块开始的 `strstr(line,"network={")` 同理会命中值里的 `network={` | `custom_wifi.c:91,97,147,152` | C3 之后单独一个提交 |

### 3.3 暂缓（记录，不在本 PR 修；原样沿用 PR #7 spec §10.2）

| 条目 | 暂缓原因 |
|---|---|
| mpv 监听线程跨线程调用 LVGL（数据竞争） | 需互斥量加 UI 定时器的小设计，约 35 行，TSan 验收 |
| WiFi 的 `popen` 移入工作线程、Scan 触发真实扫描 | 约 40 行且属行为变更 |
| `luckfox_get_input_device_info()` 返回局部数组地址 | 调用已注释，死代码 |
| 720 屏 18px 字体无映射与栈占用 | 本板为 480 屏，不可达 |
| 亮度只尝试第一个背光条目 | 本板只有一个背光设备，不可达（乘法溢出已在 PR #6 修复） |
| `custom_wifi.c` 的 `sprintf(buffer[128])` 与 `networks[1024]` 131 KB 栈 | 当前 UI 输入长度下不可达（⑳ 加入长度校验后更不可达）；后者非缺陷 |
| 启用 `-O3` 与 LVGL 子目录优化级别 | 性能变更，另开 PR 并真机回归 |

## 4. 设计决策

| # | 决策 | 依据 / 被否决的替代 |
|---|---|---|
| D1 | 每处缺陷一个 `fix` 提交，测试与修复同一个提交；修复前失败的证据（运行输出）记入 plan，不提交会失败的测试 | PR #7 非目标「不提交会失败的测试」；一个缺陷一个提交便于回滚与 bisect |
| D2 | 需要新增逻辑的放进 `custom/`，`generated/` 只改调用处；一两行可修的（日历数组改 `static` 并先拷贝再 `strtok`、WIFI 密码框 `max_length`）直接改 `generated/`，spec 列出改过的生成文件（§5.6） | 生成代码无 `.guiguider` 工程文件，改动越小越好 |
| D3 | 音乐选项串与 mpv 命令用动态分配加 `snprintf` 与 cJSON，不靠截断或跳过长文件名 | `d_name` 最长 255 字节，刚好放进 `filename[256]`，「超过 255 字节跳过」是死代码；溢出根因是拼接缓冲区固定大小。否决：加大固定缓冲区（仍有上限）、截断文件名（改写用户数据） |
| D4 | 选项串所有权在 `custom/` 内闭环：`music_roller_options()` 永不返回 NULL，空列表返回 `""`；先构造新串，成功后释放旧串并保存新指针；分配失败时同时清空新链表与旧选项，置空 `head`、`playing_music_node` 和选项指针并返回错误，`music_roller_options()` 返回空串；每次扫描前释放旧节点链表并把 `playing_music_node` 置空，重复扫描不泄漏 | `lv_roller_set_options(NULL)` 在本仓配置下死循环（§2.3）。否决：由 `generated/` 调用方分配缓冲区（需把大小算法暴露给生成代码） |
| D5 | roller 回调用 `lv_roller_get_selected()` 取下标，按 `id` 在链表上找节点，并判 NULL、越界 | 否决：加大 `buf[32]`（仍可能被更长文件名截断后找不到） |
| D6 | SIGPIPE：用 `send(fd, buf, len, MSG_NOSIGNAL)`，在 `custom/` 里收口为一个 `mpv_send()` 替换全部 `write(fd_mpv, …)`；`mpv_send()` 在 ③ 首次引入（先用 `write`，供 `loadfile` 命令使用），⑦ 改为 `send(…, MSG_NOSIGNAL)` 并替换其余全部 `write(fd_mpv, …)` | 否决：`signal(SIGPIPE, SIG_IGN)`，是进程级设置，会影响 WiFi 的 `popen` 等其他路径 |
| D7 | mpv 连接：去掉 `sleep(1)`、无效的 `sleep(0.1)` 与 `custom_init()` 里同步的 `system("mpv 2>&1 >/dev/null")`；`vfork` 之前先 `unlink(sock_path)` 移除旧监听路径，避免误连仍存活的旧实例；`vfork` 返回后每 50 ms 轮询一次，每轮先 `waitpid(pid, WNOHANG)`、再 `connect`，总上限 5 s（生产预算 5000 ms）：子进程已退出（`mpv` 缺失，退出码 127）或 `mpv` 中途退出即放弃并返回 -1；`vfork` 之后的失败路径（5 s 超时而子进程仍存活、`socket()` 失败、`pthread_create` 失败）一律先 `kill(pid, SIGKILL)` 再 `waitpid(pid, …, 0)` 回收，然后返回 -1，已经被 `waitpid(WNOHANG)` 回收的路径不再处理，所以失败路径不留 `mpv` 进程和僵尸进程（否则它会一直占着音频设备，直到应用退出时才由 `PDEATHSIG` 带走）；每次 `connect` 失败后关闭该套接字并重新 `socket()`；轮询期间用局部 fd，连上后才赋给全局 `fd_mpv`。套接字路径改为 `#ifndef MPV_SOCKET_PATH`，宏随 ① 引入，按 §2.6 的先例当 `const char *` 表达式使用：`vfork` 之前用 `snprintf` 拼出 `--input-ipc-server=<路径>` 参数，路径长度的运行期检查（`strlen(MPV_SOCKET_PATH) >= sizeof(addr.sun_path)`，或 `snprintf` 拼参数被截断）放在 `vfork` 之前，不满足时打印一行并返回 -1，此时还没有启动 `mpv`，通过后才在连接处拷贝到 `sun_path`；只在 `#ifndef` 的默认分支里定义 `"/tmp/mpvsocket"` 并紧跟 `_Static_assert(sizeof(MPV_SOCKET_PATH) <= sizeof(((struct sockaddr_un *)0)->sun_path), …)`，即默认值编译期校验、覆盖值运行期校验。`socket()` 与 `pthread_create` 失败的两条清理路径难以在测试里稳定触发，不单独加用例，靠代码审查确认。`fd_mpv` 初值改为 -1，`mpv_send()` 对 `fd_mpv < 0` 直接返回 -1，失败路径 `close` 并置回 -1 | 重试针对的是 exec 成功后套接字尚未出现的启动竞态（§2.5），不是 `mpv` 缺失；mpv 存在时通常 100 ms 内连上，比固定 `sleep(1)` 省近 1 s。`mpv` 缺失靠轮询发现：`vfork` 返回时子进程可能还没变成僵尸（单核上每 5000 次只有个位数次能在返回后立即 `waitpid` 到退出），所以不依赖 `vfork` 返回后的那一次 `waitpid`，单核上约 50 ms 内发现 127。宏的写法与测试先例一致：连接测试用 2.5 倍速单调时钟检验 5000 ms 的生产预算，测试自身读取真实时钟，超时断言为 1900–2900 ms（5000 / 2.5 = 2000 ms；下限 1900 ms 容纳 100 ms 计时误差，上限 2900 ms 留出 900 ms 调度与回收余量，仍低于 MUSIC-10c 的 8000 / 2.5 = 3200 ms）；测试把路径覆盖成 `tst_mpv_socket_path()` 这类运行期表达式，同一可执行文件的多个 CASES 各是一个进程、各有自己的 `tst_tmp_dir()`；默认分支的 `_Static_assert` 与运行期覆盖在主机 gcc、`arm-linux-gnueabihf-gcc`、uClibc gcc 8.3 上均能编译，默认值改成 109 字节时报 `static assertion failed`。否决：保持固定 `sleep(1)` 加单次 `connect`（每次启动白等 1 s，慢板子仍可能连不上）；套接字路径写死（`ctest -j` 下假 `mpv` 与连不上的用例互相串扰）；宏限定为字符串字面量、做 `"--input-ipc-server=" MPV_SOCKET_PATH` 拼接并无条件 `_Static_assert`（测试只能用 CMake 给每个可执行文件传一个字面量，同一可执行文件的多个 CASES 共用一条路径，`ctest -j` 下仍串扰；改成运行期表达式则拼接直接编译失败，`sizeof` 退化为指针大小，断言恒真）；新增一个可由测试赋值的全局路径变量或读环境变量（前者改全局接口，后者改变板上运行期行为） |
| D8 | ⑮ 先于 ⑬：新增头文件 `custom/custom_tick.h` 声明 `uint32_t custom_tick_get(void)`，`lib/lv_conf.h` 的 `LV_TICK_CUSTOM_INCLUDE` 指向它（解决 `custom_tick_get` 的隐式声明），`custom/custom.h:58` 原有的同名声明改为 `#include "custom_tick.h"`，声明只留一处；`open` 的隐式声明靠把 `custom/custom.h:18` 的 `#include <linux/fcntl.h>` 改成 `#include <fcntl.h>` 解决（`custom.h` 被 `custom_main.c` 包含，`<fcntl.h>` 声明 `open` 与 `O_*`）。⑬ 再把 tick 改用 `clock_gettime(CLOCK_MONOTONIC)`，毫秒数在 `uint64_t` 中计算，返回 `uint32_t` 回绕值，用独立的 `bool` 标志记录是否已取起点（不用 `start_ms == 0` 当哨兵） | LVGL 的 `lv_tick_elaps` 按 `uint32_t` 回绕处理，回绕本身无害。单调时钟解决 NTP 校时让 tick 突变。不能在保留 `<linux/fcntl.h>` 的同时给 `custom_main.c` 另加 `<fcntl.h>`：两者同时包含会报 `redefinition of 'struct flock'`（glibc 与主机 gcc 均复现）；全仓没有代码依赖 `linux/fcntl.h` 独有的符号，改成 `<fcntl.h>` 后 `custom/*.c`、tests 工程与顶层 glibc 交叉编译均通过，`custom_brightness.c:10` 解释该冲突的注释随 ⑮ 改写：删去「与 `<fcntl.h>` 同时包含会重复定义」这一已失效的理由，只保留 sysfs 用 stdio 读写的事实。否决：给 `open` 写局部 `extern` 声明（绕开而不解决头文件冲突）；保留 `gettimeofday` 仅补 `uint64_t` 转换（仍基于墙钟）。风险：uClibc 是否需额外链接 `-lrt`，由 uClibc 交叉编译 job 把关（§7） |
| D9 | OFF 退出：新增 `custom/` 函数 `int custom_fb_clear(const char *path)`，每次写固定的小块（4096 字节），不带 `O_CREAT`；`ENOSPC`（含最后一块跨越帧缓冲末尾）或总量达到上限（默认 64 MiB，`#ifndef` 可被测试覆盖，是总量而非块大小）静默返回 0；`EINTR` 重试；短写（`0 < n < 4096`）计入总量后继续循环；`write` 返回 0 视为写到末尾，静默返回 0（否则总量不增长，上限永远到不了）；其他错误（含 `open` 失败）返回 -1 并向 stderr 打一行；`src/main.c` 改为调用它 | 写到 `ENOSPC` 即帧缓冲写满，是预期结束条件；块必须小于帧缓冲：内核 `fb_write` 在 `count` 大于帧缓冲总长时先写满再返回 `EFBIG`，大块会把正常结束误判为错误（480 屏帧缓冲 480×480×4 = 921 600 字节，4096 字节的块远小于它）。否决：先取 `FBIOGET_FSCREENINFO` 的 `smem_len`（多一个 ioctl 依赖，`/dev/full` 与普通文件测不了） |
| D10 | 画布缓冲区：`custom/custom_sketchpad.c` 新增 `lv_sketchpad_set_size(obj, w, h)`，用 `lv_mem_alloc(LV_CANVAS_BUF_SIZE_TRUE_COLOR(w, h))` 一次性分配并 `lv_canvas_set_buffer`，指针存进 `lv_sketchpad_t` 新增字段，分配失败返回错误且不调用 `lv_canvas_set_buffer`；不改 `dsc.data_size`（`lv_canvas_set_buffer` 不设置，保持与现状一致，§2.3）；调用处对宏参数自行加括号；析构先 `lv_img_cache_invalidate_src` 再 `lv_mem_free`；`generated/setup_scr_Sketchpad.c` 只改调用处。笔迹坐标用点击点减去 `lv_obj_get_coords()` 的左上角换算为画布坐标；分配失败后画笔（`LV_EVENT_PRESSING`）与清除按钮回调遇空缓冲直接返回 | 否决：把 VLA 改 `static` 数组（480 屏 691 KB、720 屏 1.5 MB 常驻 BSS，且大小随分辨率变化）；坐标不改会让笔迹与手指偏移 |
| D11 | WiFi 校验规则（提交时、`popen("wpa_cli")` 之前）：SSID 1–32 字节，不含控制字符（`<0x20`、`0x7f`）；密码 8–63 字节，不含控制字符；空密码拒绝；UTF-8 合法，非法 UTF-8 字节序列拒绝（扫描下拉框可显示非 UTF-8 的 SSID；选择该条目时，在回填输入框前即拒绝非法 UTF-8、显示提示并保留原输入）；`"`、`\` 合法；SSID 或密码里任何 `#` 之前出现过 `"` 则拒绝（§2.2）。密码框 `max_length` 由 32 改为 63（字符数），但校验依据是字节数而非 `max_length` | `save_config` 会让 wpa_supplicant 自己落盘，校验必须先于任何 `wpa_cli` 调用。否决：拒绝全部 `"` 与 `\`（误伤合法口令，也拒掉扫描出来的特殊字符网络）；只拒绝 `"`（`\`、`"` 单独出现都合法） |
| D12 | WiFi 校验失败的提示：新增一个独立的红色小标签（不复用 `WIFI_loaded_wifi_label`），由 `custom/` 在 `wifi_app_init()` 末尾创建，父对象 `guider_ui.WIFI`，初始 `LV_OBJ_FLAG_HIDDEN`，文字色 `0xff0000`，约 (150, 245)、宽 310，`LV_LABEL_LONG_WRAP`，字体只用工程已有的 12/16/20 号中的 16（不用 18），文案用纯 ASCII（字体无 CJK）；位置、尺寸、字体与生成代码一样经 `luckfox_lv_obj_set_pos`、`luckfox_lv_obj_set_size`、`luckfox_lv_obj_set_style_text_font` 设置，随 `SCALE` 缩放；`WIFI_ssid_ta`、`WIFI_psw_ta` 各再注册一个 `LV_EVENT_VALUE_CHANGED` 回调来隐藏它，回调在标签创建之后才注册，且先判标签指针非 NULL（`lv_textarea_set_text` 也发该事件，`wifi_app_init()` 回填时就会触发；清除、合法下拉选中、回填都会自动隐藏提示）；校验失败在 `_wifi_conf_load` 之前返回，不改变连接指示；C4 的 `popen` 失败提示复用该标签 | 5 秒定时器的 `COMPLETED` 分支只取消隐藏、不重设文本，复用该标签会让错误提示残留成 SSID（`_wifi_conf_load` 还在第 62 行就把该标签设为 SSID）。设计取舍：需新增一个控件（由 `custom/` 创建，`generated/` 不改） |
| D13 | 配置读取改为取该行第一个到最后一个 `"` 之间的内容，带宽度限制，并只匹配去掉行首空白后以 `ssid=` 或 `psk=` 开头的行；块开始同样只认去掉行首空白后以 `network={` 开头的行（现为 `strstr(line, "network={")`，`custom_wifi.c:91,147`，值里含 `network={` 也会命中）；块结束只在去掉行首空白与行尾 `\r`、`\n`、空白后整行为 `}` 时判定；`_wifi_conf_get` 增加缓冲区大小参数（调用方只有 `wifi_app_init()`，缓冲区各 128 字节） | `sscanf(%[^"])` 遇 `"` 即停且无宽度（㉕）；`strstr` 子串匹配会误命中 `scan_ssid=`、`bssid=`、`wpa_psk=`（C3）与值里的 `}`、`network={`（C5）。读与写共用同一组解析函数 |
| D14 | 配置写入：临时文件创建在 `WPA_FILE_PATH` 同一目录，文件名用专用后缀 `.luckfox.new`（不用 `.tmp`，那是 wpa_supplicant 自己 `save_config` 的临时文件名），由 `snprintf("%s.luckfox.new", WPA_FILE_PATH)` 拼出，缓冲区不够则返回错误、不写任何文件（不做字面量拼接，§2.6）；创建后 `fchmod` 成原文件的模式（`fopen("w")` 受 umask 影响，PSK 不能变成全局可读），`fflush` + `fsync` 后直接 `rename` 覆盖，不再 `remove`；`WPA_FILE_PATH` 已是 `custom_wifi.c:24` 的无条件 `#define`，⑳ 把它改成 `#ifndef` 默认值，自 ⑳ 起可被测试覆盖（⑳ 的测试需要它指向临时目录） | 同目录保证 `rename` 原子且不跨文件系统；否决：保留 `remove` 再 `rename`（失败时配置丢失） |
| D15 | 扫描解析：按 TAB 位置切分（`wpa_cli scan_results` 是 `bssid\tfreq\tsignal\tflags\tssid`），不用 `strtok`；以 `\n` 显式连接各 SSID；去掉 `strlen < 16` 过滤（长 SSID 会出现在下拉框里，是 §5.2 列出的行为变更）；从 `fgets` 读到的整行里取 SSID 字段，先去掉行尾的 `\n`（及 `\r`）再直接解码，不经 128 字节的截断拷贝；解码 `\"`、`\\`、`\e`、`\n`、`\r`、`\t`、`\xNN`（十六进制大小写均接受）；下列条目整条丢弃，不进下拉框：解码后为空；解码后含控制字符（`<0x20`、`0x7f`，含 `\x00`）；带残缺或未知的转义（如行尾的 `\x4`、`\q`、行尾单个 `\`）；解码后超过 127 字节（`networks[].ssid` 的容量 `MAX_CONF_LEN` 减 1，不放大该数组）。保留下来的条目最多 10 个（`MAX_NETWORKS`），上限按保留项计数，被丢弃的条目不占名额。有保留项时仍在末尾追加 `\n...`，一项都不保留时下拉框为 `scanning` | `strtok` 把连续 TAB 合并，`flags` 为空的条目错位；SSID 之间以显式 `\n` 分隔；hostap 的 `printf_encode` 对 `"`、`\`、ESC、换行、回车、TAB 输出转义形式，其余小于 32 或大于 126 的字节输出 `\xNN`，不解码会让用户选到与 AP 真实名称不同的 SSID（如 `a\"b`、`\xe4\xb8\xad` 编码的中文）。空项与控制字符的丢弃依据：`lv_dropdown` 以 `\n` 分隔选项（`lv_dropdown.c:121-125`），解码出的换行会把一个 SSID 拆成多项，空项成为选不中的空选项，其余控制字符的 SSID 即使选中也会被 D11 拒绝；行尾换行必须先去掉，否则每个 SSID 都带控制字符而被整条丢弃。非法 UTF-8 与超过 32 字节的 SSID 仍显示在下拉框里，选中时在回填输入框前拒绝并提示，保留原输入，避免 LVGL 过滤或截断后提交另一个名称；D11 拒绝的其他 SSID（如 `"` 之后含 `#`）仍显示在下拉框里，选中后在提交时被拒绝；残缺和未知转义无法还原出 AP 的真实名称，解码后长度超过 127 字节的条目放不进 `networks[].ssid`，同样丢弃；隐藏网络的 SSID 字段为空，旧实现靠 `strtok` 合并分隔符间接跳过。否决：`\e \n \r \t` 保持字面不解码（下拉框显示的名字与 AP 不符，选中后连不上） |
| D16 | WIFI 定时器：不在离开页面时删除，回调开头加 `lv_scr_act() != guider_ui.WIFI` 守卫，不满足则直接返回；生成代码不动 | 定时器只创建一次（§2.3），删除后不会重建，连接状态从此不再刷新；否决：`lv_timer_pause/resume`、在 `LV_EVENT_SCREEN_LOADED` 立即刷新（都需要在生成代码或 `custom/` 里接屏幕事件，多一处接线）。代价：回到 WIFI 页后最多 5 s 内连接状态仍是旧的 |
| D17 | `udhcpc`：重新启动前先 `pkill -f '[u]dhcpc -i wlan0'`，与启动命令分两次 `system()`；`pclose(NULL)` 判空；不加 `-n` | 裸 `pkill udhcpc` 按进程名匹配，会误杀其他接口（如 eth0）的实例；`pkill -f 'udhcpc -i wlan0'` 与启动命令合在同一条 `sh -c` 里时，`sh` 自己的命令行也匹配，会连 shell 一起杀（退出码 143）；`[u]dhcpc` 形式不匹配 `pkill` 自身与父 shell。板上已确认 `/bin/pkill` 来自 procps-ng 3.3.17，dhcpcd 与 udhcpc 均运行，测试前 wlan0 有两个 udhcpc（见 plan 验证证据）；若板上没有 `pkill`，改用 `-p` 指定 pidfile，下次 Load 前在 C 里读 pid 并校验 `/proc/<pid>/cmdline` 后 `kill`。`-n` 会改变重试行为，不放进本 PR |
| D18 | `popen`/`opendir` 判空：主屏、WiFi、音乐各一个提交；`custom_wifi.c` 的 `popen("wpa_cli")` 失败改为提示并返回（C4），`_wifi_status_update` 失败时不再 `pclose(NULL)`（㉓） | 与 PR #7 spec §10.3 一致 |
| D19 | 编译层缺陷（缺 `return`、隐式声明）以 `-Werror=return-type`、`-Werror=implicit-function-declaration` 对对应源文件 `-c -o /dev/null` 编译失败作为失败测试（须用 `-c`：gcc 在 `-fsyntax-only` 下不产生缺 `return` 的诊断，`-c` 下 `custom_musicplayer.c` 报 3 处），经新增的 `luckfox_add_compile_check()` 注册为 ctest；该函数直接读 `LUCKFOX_INCLUDE_DIRS` 生成普通 `-I`，不从 `luckfox_app` 继承包含目录 | PR #7 spec §10.3。该函数随首个使用方的提交之前先加一个 `test(tests)` 提交。`luckfox_app` 以 `SYSTEM` 导出包含目录（`tests/CMakeLists.txt:59`），继承后 `lib/` 成为 `-isystem`，GCC 对系统头里展开的宏不报诊断：修复前的 `lv_hal_tick.c`（`custom_tick_get` 来自 `lib/lv_conf.h` 的宏）会编译通过，检查形同虚设 |
| D20 | 为可测性只做函数内最小拆分：不改全局行为；除下述 `music_scan_list` 接口变更外，现有公开函数的调用方式不变，`wifi_app_init`、`wifi_backend_init`、`wifi_backend_release` 在 `custom.h` 中的声明由 `()` 补为 `(void)` 完整原型，`custom_wifi.c` 中的定义保持 `()` 不变，行为不变；接口变更为 `music_scan_list`：`int music_scan_list(char *)` → `int music_scan_list(void)`，并新增 `const char *music_roller_options(void)`，调用方只有 `setup_scr_Music_player.c`；新增声明见 §5.6 | 避免本 PR 变成重构 |

## 5. 设计

### 5.1 主屏与时间（`custom_main.c`、`widgets_init.c`、`lib/lv_conf.h`、`custom.h`、`custom_tick.h`）

| 提交 | 修法 | 失败测试 |
|---|---|---|
| ⑮ 隐式声明 | 按 D8：新增 `custom/custom_tick.h`，`LV_TICK_CUSTOM_INCLUDE` 指向它；`custom/custom.h:18` 改为 `<fcntl.h>`，`custom.h:58` 的声明改为包含 `custom_tick.h`；`custom_brightness.c:10` 的注释删去失效的冲突理由 | `main.compile.no_implicit_decl`：`-Werror=implicit-function-declaration` 对 `custom_main.c`（`open`，`custom_main.c:117`）与 `lib/lvgl/src/hal/lv_hal_tick.c`（`custom_tick_get`，`lib/lv_conf.h:89`）做 `-c -o /dev/null` 编译（普通 `-I`，D19），修复前两者均报错；另对只含 `#include <fcntl.h>` 与 `#include "custom.h"` 的单文件做同样编译，修复后须通过（防止 `<linux/fcntl.h>` 冲突回归；修复前报 `redefinition of 'struct flock'`） |
| ⑬ tick | 按 D8：`custom_tick_get()` 改用单调时钟与 `uint64_t` 毫秒，独立 `bool` 记录起点 | `main.tick.monotonic_wrap`（32 位）：arm32 目标加 `-U_TIME_BITS -U_FILE_OFFSET_BITS`，在测试可执行文件里定义同名 `clock_gettime`、`gettimeofday` 由同一个模拟时钟驱动。起点取非 0 且小于 2148 s 的值（如 10 s）：`Δ = 1 s` 断言旧实现在溢出前正确；`Δ` 跨过绝对秒 2148、3000 s、50 天的断言在旧实现上失败；另取板上量级起点 1 700 000 000 s 验证新实现；所有比较都按 `uint32_t` 取模：`tick(t+Δ)-tick(t) == (uint32_t)(Δ*1000)`。`main.tick.step_back_host`（主机）：只桩墙钟 `gettimeofday`，`CLOCK_MONOTONIC` 保持单调递增，回拨发生在首次采样之后，断言 tick 不倒退（旧实现基于墙钟，会倒退） |
| ⑫ 正午显示 AM | 按 `tm_hour >= 12` 判 PM，小时取 `(tm_hour + 11) % 12 + 1`；`meridiem` 用 `snprintf` 写入；每秒走字的 `clock_count_12()` 本身正确且每 5 s 被重新校准，不改 vendored LVGL | `main.clock.meridiem`：在测试可执行文件里定义 `time()`，断言 0/11/12/13/23 点的 `小时` 与 `AM/PM` |
| ⑭ 日历 | `generated/widgets_init.c`：`highlighted_days` 改 `static`，先把日期标签文本拷进局部缓冲区，把三次 `strtok`（年、月、日）前移到 `lv_obj_add_flag(lv_layer_top(), LV_OBJ_FLAG_CLICKABLE)` 与 `lv_calendar_create` 之前，任一结果为 NULL（标签文本不含 `/` 或只有一个 `/`）就直接返回，之后才置顶层可点击并创建日历，因此畸形文本既不调用 `atoi`，也不留下半成品日历和一直可点击的顶层 | `main.calendar.highlight_survives`：断言日期标签文本保持 `yyyy/mm/dd` 不被截断，再读 `lv_calendar_get_highlighted_dates()` 的第一项等于该日期（用例以 `ASAN_OPTIONS=detect_stack_use_after_return=1` 运行，修复前读的是已返回函数的局部数组，ASan 报 `stack-use-after-return`；比较指针是否在线程栈范围内区分不了修复前后，局部数组此时在 ASan 假栈上）；`main.calendar.no_slash` 与 `main.calendar.one_slash`：各是独立 CASE（独立进程），都不先创建日历（保证日历指针为 NULL，否则 `Main_datetext_1_init_calendar` 在 `widgets_init.c:108` 直接返回），分别把日期标签设为 `abc`（第二次 `strtok` 返回 NULL）和 `2023/07`（第三次 `strtok` 返回 NULL）后调用；断言进程不崩溃，且未创建日历：日历指针是 `static` 读不到，改为断言 `lv_obj_get_child_cnt(lv_layer_top()) == 0`（测试里顶层本无其他子对象）且 `lv_obj_has_flag(lv_layer_top(), LV_OBJ_FLAG_CLICKABLE)` 为假（后者同时验证解析在置可点击之前）。修复前 `abc` 在 `atoi(NULL)` 处被 UBSan 报错或段错误，`2023/07` 同样在第三个 `atoi` 处，两个用例均为红 |
| ⑯ `popen` 判空 | `luckfox_get_system_info` 在 `popen` 失败时返回（按非 Ubuntu 处理） | `main.sysinfo.popen_failure`：在测试可执行文件里定义返回 NULL 的 `popen`，须不崩溃 |

### 5.2 WiFi（`custom_wifi.c`、`setup_scr_WIFI.c`）

| 提交 | 修法 | 失败测试 |
|---|---|---|
| ⑳ 输入校验 | D11 + D12：校验函数放进 `custom/`，`WIFI_load_btn_event_handler` 在调 `_wifi_conf_load` 前校验，失败则显示提示并返回；`generated/setup_scr_WIFI.c` 的密码框 `max_length` 32→63；同一提交把 `WPA_FILE_PATH` 改为 `#ifndef` 默认值（D14） | `wifi.validate.rules`（表驱动：空 SSID、33 字节 SSID、中文 SSID、7/8/63/64 字节密码、空密码、含控制字符、非法 UTF-8、`a"b`、`a\b`、`a#b`、`a"#b`、`a#b"c`；`wifi_input_check` 是新增接口，修复前在链接阶段失败，按 §5.5 的先例）；`wifi.load.invalid_no_wpa_call`：`PATH` 里的假 `wpa_cli` 记录 stdin，`WPA_FILE_PATH` 指向临时配置文件，非法输入时假 `wpa_cli` 日志为空、配置文件不变、提示标签可见；`wifi.load.dropdown_overlong`：通过下拉框事件选择 33 字节 ASCII 或中文名称，回填前拒绝并提示，空输入不被改写，点击 Load 后无命令与配置写入；`wifi.validate.utf8`：10 种非法与 8 种合法序列分别用于 SSID、密码，共 36 次判定；`wifi.load.dropdown_utf8`：非法 UTF-8 下拉项拒绝回填并提示；`wifi.load.edit_hides_hint`：编辑输入后隐藏提示；`wifi.compile.prototypes`：三个 WiFi 初始化与释放函数具有完整原型 |
| ㉑ 配置写入 | D14 | `wifi.conf_write.same_dir_atomic`：在测试可执行文件里定义 `rename()` 记录（源、目标）并可注入失败，断言源与目标同目录且文件名不是 `<conf>.tmp`，新文件模式与原文件一致，注入失败时原配置文件仍在且内容不变（旧实现已先 `remove`）；`wifi.conf_write.io_failures`：8 种 I/O 失败注入均保留原文件且不 rename |
| C3 行匹配 | D13：只匹配去掉行首空白后以 `ssid=`、`psk=` 开头的行 | `wifi.conf_write.only_ssid_psk_lines`：配置含 `scan_ssid=1`、`bssid=…`、`key_mgmt=`、`wpa_psk=`，提交后这些行保持不变 |
| C5 块边界判定 | D13：整行为 `}` 才判块结束，行首为 `network={` 才判块开始 | `wifi.conf_write.brace_in_value`：SSID 与密码分别含 `}` 与 `network={`，写入后读回相等；`wifi.conf_write.crlf_block_end`：CRLF 块末行终止 network 块，块外 SSID/PSK 保持不变 |
| ㉕ 读取 | D13：首个到最后一个 `"`，带宽度限制 | `wifi.conf_get.long_and_quoted`：200 字符值不溢出（ASan），含 `"` 的值读回相等 |
| ㉒ 扫描解析 | D15 | `wifi.scan.parse`：假 `wpa_cli` 输出含带空格 SSID、`flags` 为空、长 SSID、含 `\"` 与 `\\` 的 SSID、大小写两种 `\xNN` 编码的 UTF-8 SSID，以及应丢弃的条目（空 SSID、含 `\e`、`\n`、`\r`、`\t`、`\x00`、`\x01` 的 SSID、行尾残缺的 `\x4`、未知转义 `\q`、行尾单个 `\`、解码后 128 字节的 SSID）；下拉框末尾还有追加的 `...`，所以断言 `lv_dropdown_get_option_cnt()` 等于应保留的条目数加 1，前面各选项逐项等于解码后的 SSID，最后一项等于 `...`；`wifi.scan.parse_all_discarded`：假 `wpa_cli` 输出的条目全部应丢弃，且至少含一条旧实现会显示、新实现丢弃的项（如 `\x01`、`\q`、含 `\e` 的 SSID），不能只有空 SSID 行（旧实现靠 `strtok` 合并分隔符跳过空 SSID，也会走 `scanning`，用例修复前就是绿的），断言选项数为 1 且唯一选项等于 `scanning`；`wifi.scan.parse_cap`：输出 12 条有效 SSID 另加 2 条应丢弃的条目（共 14 行），2 条丢弃项排在第 10 个保留项之前（放在末尾则「按保留项计」与「按原始行计」结果相同，区分不出），断言保留项正好 10 个（丢弃项不占名额）、选项数为 11；`wifi.scan.append_truncated`、`wifi.scan.append_error`：格式化截断或负返回值时撤回整条，剩余长度不下溢 |
| ㉓、C4 `popen` 失败处理 | D18 | `wifi.popen.failure`：在测试可执行文件里定义返回 NULL 的 `popen`，`_wifi_status_update` 不崩溃，`_wifi_conf_load` 提示并返回而不退出进程 |
| ㉔ 定时器 | D16 | `wifi.timer.inactive_screen`：定义计数 `popen`，WIFI 屏不是当前屏时推进 6 s 调用数为 0；是当前屏时断言 `>= 1`（真实时间 6 s 落在两次触发之间，不卡 5 s 边界） |
| ㉔ `udhcpc` | D17 | `wifi.dhcp.single_instance`：`WPA_FILE_PATH` 指向一份可读的临时配置（`fopen` 失败会在 `udhcpc` 之前返回），`PATH` 里的假 `pkill`、`udhcpc` 记录命令，连按两次 Load，每次启动前都有范围限定为 `wlan0` 的 `pkill`，且二者是两次独立调用 |

`WPA_FILE_PATH` 用 `#ifndef` 默认值，测试按 §2.6 覆盖成 `tst_tmp_dir()` 下的路径。

行为变更清单（均为消除缺陷所必需）：提交时的 SSID/密码校验与红色提示标签（D11、D12），含拒绝非法 UTF-8；非法 UTF-8 或超过 32 字节的 SSID 从下拉框回填输入框前即被拒绝并显示提示，保留原输入；密码框 `max_length` 32→63；扫描下拉框不再仅保留长度小于 16 字节的条目，较长 SSID 也能显示（D15）；`\"`、`\\`、`\xNN` 转义在下拉框里解码显示，解码后为空、含控制字符、带残缺或未知转义、长度超过 127 字节的 SSID 不再出现在下拉框（D15）。

### 5.3 画板（`custom_sketchpad.c`、`setup_scr_Sketchpad.c`）

- ⑱：`lv_sketchpad_set_size()`（D10）；`generated/setup_scr_Sketchpad.c` 只改调用处。失败测试 `sketchpad.canvas.buffer_not_on_stack`（用 `pthread_getattr_np` 取栈范围，取完调用 `pthread_attr_destroy`，否则 glibc 为它分配的 cpuset 会被 LSan 报泄漏；断言 `lv_canvas_get_img(obj)->data` 不在范围内；这是区分旧实现的主断言）与 `sketchpad.canvas.buffer_full_write`（按 `w*h*sizeof(lv_color_t)` 写满整块缓冲区，ASan 不报越界，用来约束新分配的大小；写满正确字节数本身分不出旧的 4 倍 VLA，故不作为修复前失败的依据，也不断言 `data_size`）。
- ⑱：分配失败判空由 `sketchpad.canvas.alloc_failure`（`#ifndef` 注入分配失败）覆盖；该用例在失败后再触发一次画笔与清除，二者遇空缓冲直接返回（D10）。
- ⑲：笔迹坐标换算，在 ⑱ 的堆缓冲之上画。失败测试 `sketchpad.stroke.canvas_coordinates`：脚本化触摸在已知屏幕点画一笔，断言画布上对应 `(x - x1, y - y1)` 附近 3×3 像素内有笔色像素（线宽 2 且启用 `LV_DRAW_COMPLEX`，单个像素可能因抗锯齿不是笔色），偏移 20 px 处仍为底色。

### 5.4 音乐页（`custom_musicplayer.c`、`setup_scr_Music_player.c`）

| 提交 | 修法 | 失败测试（ctest 名） |
|---|---|---|
| ② 选项串溢出 | `music_scan_list(void)` 在 `custom/` 内用 `malloc` 按实际长度拼选项串；新增 `const char *music_roller_options(void)`；`setup_scr_Music_player.c` 改为先 `music_scan_list()` 再 `lv_roller_set_options(…, music_roller_options(), …)`（LVGL 会拷贝）；分配失败返回错误，同时清空新链表与旧选项，避免选项与链表 id 错位；`custom_musicplayer.c:23` 的 `MUSIC_DIR_PATH` 由无条件 `#define` 改为 `#ifndef` 默认值，测试按 §2.6 把它覆盖到 `tst_tmp_dir()` 下的子目录（② 与 C2 的用例都要在临时目录里造文件） | `music.list.options_over_255`：12 个 200 字节文件名（不超过 200 字节，避开 ③ 的 `cmd[256]` 溢出），断言选项串长度与内容；缺失 music_roller_options 接口时先在链接阶段失败；`music.list_alloc.options_alloc_failure` 注入选项串分配失败，断言选项和节点同时清空且后续扫描可恢复 |
| C2 含 `\n` 的文件名 | 扫描时跳过含 `\n` 的文件名并向 stdout 打一行 | `music.list.newline_name_skipped` |
| ③ mpv 命令构造溢出与注入 | 首次引入 `mpv_send()`（先用 `write`）；`loadfile` 命令用 cJSON 构造（`cJSON_CreateObject`/`cJSON_PrintUnformatted`）并经 `mpv_send()` 发出，文件名中的 `"`、`\` 自动转义，不再 `sprintf(cmd[256])`；命令里的路径前缀保持字面量 `/music/`，不改用 `MUSIC_DIR_PATH`（板上两者相同） | `music.mpv_cmd.escaping_and_length`：临时目录放 `a"b.mp3` 与 255 字节文件名，`fd_mpv` 用 `socketpair`，读对端每行用 cJSON 解析，断言 `command[1]` 等于 `/music/<原文件名>`；修复前 ASan 报栈溢出（② 已修，所以失败只来自 `cmd[256]`） |
| ④ roller 选中长文件名死循环 | 回调用 `lv_roller_get_selected()` 取下标，按 `id` 找节点，判 NULL 与越界 | `music.roller.select_long_name`：先摆上音乐屏（至少建出 `Music_player_music_name` 标签与 roller），100 字节文件名，派发 `LV_EVENT_VALUE_CHANGED`，断言标签文本命中；用例开头设置 `alarm(5)`，旧回调死循环在约 5 秒以 SIGALRM 失败 |
| ⑧ `opendir` 判空 | 失败时打印并返回，选项串为 `""` | `music.list.missing_dir` |
| C1 空列表空指针 | `music_app_init`、Next/Prev、roller 回调在 `playing_music_node==NULL`（空列表）时直接返回 | `music.empty_list.no_crash`；`music.empty_list.alloc_failure_no_crash` 注入选项串分配失败，验证 `music_app_init()` 返回 -1，上一曲、下一曲与 roller 回调不崩溃 |
| ⑦ SIGPIPE | `mpv_send()` 改用 `send(…, MSG_NOSIGNAL)`，替换其余全部 `write(fd_mpv, …)`，写失败只打印不退出 | `music.mpv_send.peer_closed`：关闭 `socketpair` 对端后触发暂停按钮，进程须存活（修复前被信号终止） |
| ⑤ cJSON 泄漏 | 每次 `cJSON_Parse` 后 `cJSON_Delete` | `music.monitor.no_leak`：对端一次 `write` 写入 8 行完整 JSON（总长小于 512 字节，否则落进 ⑥ 才修的无 NUL 读），用不带 `id`、`data` 的事件行（如 `{"event":"idle"}`），因为带它们的 `property-change` 行会操作音乐屏控件，屏未建出时指针为 NULL；让线程第一次 `read` 就解析完，测试侧轮询 `fd_mpv` 这一端（线程 `read` 的一端，测试握着的对端读数不会归零）的 `FIONREAD` 到 0 后再多等 100 ms 才退出，进程退出时 LSan 不报泄漏（线程 `pthread_detach` 且不返回，需保证解析发生在退出之前；最后一次解析的对象仍被线程栈引用，不算泄漏，所以至少要 2 行） |
| ⑥ `read` 不补 NUL | 读 `sizeof(buf)-1` 字节并补 `'\0'` | `music.monitor.full_buffer_line`：灌 ≥512 字节无换行数据，修复前 ASan 报越界读 |
| ⑨ 缺 `return` | 三个函数补 `return`（成功 0、失败 -1），`music_player_thread_init` 成功路径末尾补 `return 0` | `music.compile.return_type`：`-Werror=return-type` 以 `-c -o /dev/null` 编译 `custom_musicplayer.c`，修复前报 3 处 |
| ① vfork 子进程 `return` | `execlp` 失败后 `_exit(127)`；同一提交引入 `MPV_SOCKET_PATH` 宏（D7：`snprintf` 拼参数、运行期长度检查、默认分支 `_Static_assert`）与 `fd_mpv` 初值 -1，测试按 §2.6 把它覆盖成 `tst_tmp_dir()` 下的路径 | `music.player.exec_failure`：`PATH` 不含 `mpv`，调用 `music_player_thread_init()` 须返回 -1 且进程存活；修复前子进程 `return` 破坏父进程栈；`music.mpv_send.negative_fd_no_send`：未连接时返回 -1，包装的 send 调用数为 0 |
| ⑩ 连接与探测 | 按 D7；删除 `custom_init()` 里的 `system("mpv …")`；重写连接段时保留第一次 `socket()` 之前的 `close(0)`，留给 ⑪ 删除（否则 ⑪ 的 `stdin_kept` 在修复前就是绿的） | `music.player.slow_socket`（假 `mpv` 脚本延迟约 1300 ms 才创建套接字，断言连接成功；测试侧将生产单调时钟加速 2.5 倍，1300 ms 实际延迟对应 3250 ms 生产时钟，低于 5000 ms 预算；测试自身使用真实时钟计时）、`music.player.timeout_reaps_child`（假 `mpv` 永不创建套接字，断言返回 -1，且实时时间在约 1.9 s 到 2.9 s 之间，对应生产时钟 5000 ms 的预算；假 `mpv` 进程已被回收，由 `music_player_thread_init` 返回后、测试进程退出之前立刻检查 `kill(pid, 0)` 返回 `ESRCH`——测试进程退出后 `PDEATHSIG` 也会让 `ESRCH` 成立，所以必须在退出前检查；修复前无超时回收，进程存活）、`music.player.mpv_missing_fast`（`mpv` 缺失，断言 500 ms 内返回 -1；旧实现至少 1 s）；`music.player.stale_socket`（预置仍在监听的旧 socket，让假 mpv 延迟建立新端点，断言连接成功且旧监听器未收到连接） |
| ⑪ `close(0)` | 删除该调用 | `music.player.stdin_kept`：用例开头若 fd 0 未打开，先 `open("/dev/null", O_RDONLY)` 占住 0（ctest 下 fd 0 本就是打开的字符设备；若 fd 0 已关闭，修复后 `socket()` 也会拿到 0）；调用前后对 fd 0 做 `fstat`，断言 fd 0 的类型未变（不是套接字）且 `fd_mpv != 0`（修复前 `close(0)` 后 `socket()` 恰好返回 0，`fcntl(0, F_GETFD)` 仍成功，不能用它判断） |

所有会触发 `music_scan_list` 或 `_music_*` 的用例（②、C2、③、④、⑧、C1）都先把 `fd_mpv` 设成 `socketpair` 的一端：扫描时会向 `fd_mpv` 写 `loadfile` 命令，④ 摆上音乐屏时 `music_app_init()` 与选中回调还会写播放控制命令，`fd_mpv` 初值 -1 在 ① 才引入，此前默认的 0 是 stdin，不能让测试写到它。

假 `mpv` 由 `tests/cases/music/` 下的 Python 脚本实现（测试工程本就依赖 `python3`，`mutate.sh` 用它解析清单），shebang 写绝对解释器路径 `#!/usr/bin/python3`；`PATH` 把临时目录放最前，且 `PATH` 里不能含真 `mpv`。脚本按参数创建 `--input-ipc-server` 指定的 Unix 套接字（路径落在各用例自己的 `tst_tmp_dir()`），启动前先 `unlink` 该路径，支持延迟创建与永不创建，把自己的 pid 写进用例目录下的文件（供 `timeout_reaps_child` 核对回收），并回显收到的行。`music_player_thread_init()` 以 `prctl(PR_SET_PDEATHSIG, SIGKILL)` 保证测试退出时假 `mpv` 一并退出。`tests/README.md` 与 `AGENTS.md` 规定：常规 UI 测试不调用 `custom_init()`，因为它会拉起 `mpv` 并执行硬件初始化；mpv 启动与连接由 `cases/music/` 的假 mpv 用例单独验证。

### 5.5 退出（`src/main.c`）

⑰：`custom_fb_clear(const char *path)`（D9）放进 `custom/`，`src/main.c` 的 `system("cat /dev/zero > /dev/fb0")` 改为 `custom_fb_clear("/dev/fb0")`。失败测试 `exit.fb_clear.enospc_silent`：路径 `/dev/full`，断言返回 0 且 stdout/stderr 无输出；`exit.fb_clear.cap`：测试先创建普通文件（函数本身不带 `O_CREAT`），写入上限覆盖为 1 MiB，断言写满即返回；`exit.fb_clear.open_failure`：路径不存在，断言返回 -1 并打印一行。`exit.fb_clear.zero_write_silent` 注入 write 返回 0，断言正常返回且静默；`exit.fb_clear.eintr_retries` 注入首次 EINTR，断言重试后文件内容与长度正确。修复前该函数不存在，五个测试在链接阶段即失败，行为对照另行归档：plan 记录 `/dev/full` 上旧 `cat` 命令的输出（`cat: write error: No space left on device`）。

### 5.6 改动的生成文件、头文件与 `tests/` 变更

- `lib/lv_conf.h`（`LV_TICK_CUSTOM_INCLUDE`）、`generated/widgets_init.c`（日历数组 `static`）、`generated/setup_scr_WIFI.c`（密码框 `max_length`）、`generated/setup_scr_Sketchpad.c`（调用 `lv_sketchpad_set_size()`）、`generated/setup_scr_Music_player.c`（调用 `music_roller_options()`）。
- `custom/` 头文件：新增 `custom/custom_tick.h`（`uint32_t custom_tick_get(void)`）与 `custom/custom_fb.h`（`int custom_fb_clear(const char *path)`），均由 `custom/custom.h` 包含；`custom/custom.h` 将 `<linux/fcntl.h>` 换成 `<fcntl.h>`，新增 `lv_res_t lv_sketchpad_set_size(lv_obj_t *obj, lv_coord_t w, lv_coord_t h)`、`const char *music_roller_options(void)`、`const char *wifi_input_check(const char *ssid, const char *password)`，将 `int music_scan_list(char *)` 改为 `int music_scan_list(void)`，并将 `wifi_app_init`、`wifi_backend_init`、`wifi_backend_release` 在 `custom.h` 中的声明由 `()` 补为 `(void)` 完整原型，`custom_wifi.c` 中的定义保持 `()` 不变，行为不变；`custom/custom_brightness.c:10` 的注释删去失效理由。
- 测试头文件：新增 `tests/cases/wifi/wifi_env.h`、`tests/cases/wifi/wifi_ui.h`、`tests/cases/music/music_env.h`，分别共享 WiFi 环境、提示控件查找与音乐环境辅助函数。
- `tests/`：新增 `luckfox_add_compile_check()`（单独提交，普通 `-I`，D19）；`tests/support/fake_fs.c` 按 `tst_backlight_root()` 的写法补 `wpa_supplicant.conf`、音乐目录、mpv 套接字的路径函数（各随首个使用它的提交加入），另加 `tst_fake_exec()`（在临时目录写假命令并把该目录放到 `PATH` 最前，⑳ 引入）与 `tst_fb_clear_max_bytes`（⑰ 的写入上限覆盖值），`tests/CMakeLists.txt` 给对应源文件加运行期表达式的 `COMPILE_DEFINITIONS`，`unit_` 文件在 `#include` 被测 `.c` 之前自行 `#define` 同一表达式（源文件属性不作用于 `unit_` 文件）；`cases/sketchpad/`、`cases/music/`、`cases/exit/` 由各自页面的第一个提交在 `tests/CMakeLists.txt` 补 `add_subdirectory`（`tests/README.md` 目录表已登记这三个目录，不再新增行）；`cases/main/`、`cases/wifi/` 新增用例；arm32 目标新增 tick 用例；`custom/` 里新增逻辑的有价值测试配套 `.mutants`（`generated/` 与 `lv_conf.h` 的改动不配，见 §2.6）；`tests/README.md` 与 `AGENTS.md` 中「测试不调用 `custom_init()`」一条在修复 ① 的提交里改写。

### 5.7 用例与变体总数

最终树有 78 条用例（21 条基线 + 57 条新增）与 62 个变体（12 个原有 + 50 个新增）。变体按页面计：主屏 16、WiFi 22、画板 2、音乐 18、退出 4。下表逐个登记 `tests/cases/*/*.mutants` 的 id；对应测试正则以各清单的 `tests:` 为准。

| 清单 | 变体 id |
|---|---|
| `tests/cases/exit/fb_clear.mutants` | `EXIT-17a`、`EXIT-17b`、`EXIT-17c`、`EXIT-17d` |
| `tests/cases/main/brightness.mutants` | `M1`、`M2`、`M3`、`M4`、`M5`、`M6`、`M7`、`M10`、`M11` |
| `tests/cases/main/brightness_arm32.mutants` | `M12` |
| `tests/cases/main/clock.mutants` | `MAIN-12a`、`MAIN-12b` |
| `tests/cases/main/sdio_detect.mutants` | `S1` |
| `tests/cases/main/sysinfo.mutants` | `MAIN-16` |
| `tests/cases/main/tick.mutants` | `MAIN-13a`、`MAIN-13b` |
| `tests/cases/music/connect.mutants` | `MUSIC-10a`、`MUSIC-10b`、`MUSIC-12`、`MUSIC-10c` |
| `tests/cases/music/empty_list.mutants` | `MUSIC-C1` |
| `tests/cases/music/list.mutants` | `MUSIC-2`、`MUSIC-13` |
| `tests/cases/music/monitor.mutants` | `MUSIC-5`、`MUSIC-6` |
| `tests/cases/music/mpv_cmd.mutants` | `MUSIC-3` |
| `tests/cases/music/mpv_send.mutants` | `MUSIC-7`、`MUSIC-14` |
| `tests/cases/music/newline.mutants` | `MUSIC-C2` |
| `tests/cases/music/opendir.mutants` | `MUSIC-8` |
| `tests/cases/music/player.mutants` | `MUSIC-1` |
| `tests/cases/music/return.mutants` | `MUSIC-9` |
| `tests/cases/music/roller.mutants` | `MUSIC-4` |
| `tests/cases/music/stdin.mutants` | `MUSIC-11` |
| `tests/cases/sketchpad/canvas.mutants` | `SKETCH-18` |
| `tests/cases/sketchpad/stroke.mutants` | `SKETCH-19` |
| `tests/cases/wifi/backend_release.mutants` | `W1` |
| `tests/cases/wifi/conf_get.mutants` | `WIFI-25` |
| `tests/cases/wifi/conf_match.mutants` | `WIFI-C3`、`WIFI-C5a`、`WIFI-C5b`、`WIFI-C5c` |
| `tests/cases/wifi/conf_write.mutants` | `WIFI-21a`、`WIFI-21b` |
| `tests/cases/wifi/dhcp.mutants` | `WIFI-24d` |
| `tests/cases/wifi/popen.mutants` | `WIFI-23`、`WIFI-C4` |
| `tests/cases/wifi/scan.mutants` | `WIFI-22a`、`WIFI-22b`、`WIFI-22c` |
| `tests/cases/wifi/timer.mutants` | `WIFI-24t` |
| `tests/cases/wifi/validate.mutants` | `WIFI-20a`、`WIFI-20b`、`WIFI-20c`、`WIFI-20utf8`、`WIFI-20cont`、`WIFI-20hide`、`WIFI-20length` |

## 6. 提交组织

- 一个缺陷一个 `fix` 提交（测试 + 修复），`popen`/`opendir` 判空按主屏、WiFi、音乐拆 3 个，`roller_str` 溢出与 `sprintf` 溢出拆 2 个。
- 顺序规则：页面按 LVGL 界面顺序排列（主屏 → WiFi → 画板 → 音乐 → 退出），组内先高危，有依赖时依赖优先。否决：按风险排页面（音乐的栈溢出、死循环最多，放最前），各页面的提交互不依赖，风险排序换不来正确性，按界面顺序更便于对照界面逐页审阅。
- 依赖只在页面内：⑮ 先于 ⑬（⑮ 新增 `custom_tick.h`，⑬ 在它之上改实现）；`WPA_FILE_PATH` 自 ⑳ 起可被覆盖，早于使用它的 ㉑ 与 ㉔；② 先于 ③（③ 的 255 字节文件名用例要求 ② 已消除选项串溢出，且 ② 的用例用不超过 200 字节的文件名，不触发 ③）；`MUSIC_DIR_PATH` 的 `#ifndef` 随 ② 引入（② 是第一个需要临时音乐目录的提交）；`mpv_send()` 在 ③ 首次引入、⑦ 再改 `send`；`MPV_SOCKET_PATH` 与 `fd_mpv` 初值随 ① 引入。跨页面共用的只有前置提交的 `luckfox_add_compile_check()`（⑮、⑨ 使用）与 ⑮ 对 `custom.h` 的改动：⑮ 是第一个 `fix` 提交，之后各页的 `unit_` 文件可以与 `<fcntl.h>` 同时包含而不撞 `struct flock`。`tests/support/` 里新增的路径函数与 `tests/CMakeLists.txt` 的 `add_subdirectory` 都由首个使用它的提交加入，属于各自页面。
- 提交序列（共 30 个 `fix`，另加 1 个前置 `test` 与最后 1 个 `docs`）：
  - 前置：`test(tests)`：`luckfox_add_compile_check()`。
  - 主屏 5：⑮ ⑬ ⑫ ⑭ ⑯。
  - WiFi 9：⑳ ㉑ C3 C5 ㉕ ㉒ ㉓+C4 ㉔（定时器）㉔（`udhcpc`）。
  - 画板 2：⑱ ⑲。
  - 音乐 13：② C2 ③ ④ ⑧ C1 ⑦ ⑤ ⑥ ⑨ ① ⑩ ⑪。
  - 退出 1：⑰。
- spec 与 plan 仅出现在 PR 最后一个 `docs(superpowers)` 提交中，前 31 个提交分别交付测试基础设施与 30 项修复。
- commit message 遵循 `git cz`（Conventional + gitmoji）：subject 单一主题，body 用 `-` 并列 why（根因、关键决策与被否决的替代、权衡），不写实现罗列。

## 7. 验证（plan 执行时落地，此处只定判据）

1. 每个 `fix` 提交：先单独运行新测试，记录修复前失败的输出（ASan/LSan 报告、超时、信号、编译错误），应用修复后同一测试通过；`tests/tools/mutate.sh` 对新增 `.mutants` 全部 `KILLED`。
2. 全量 `ctest --test-dir build-tests --output-on-failure -j"$(nproc)"` 全绿，含 arm32 用例（`qemu-arm`）；并行下用到固定路径的用例互不串扰（§2.6）。
3. glibc 与 uClibc 两条交叉编译成功；uClibc 需确认 `clock_gettime` 可链接（否则在顶层 `CMakeLists.txt` 补 `rt`），以及 `custom.h` 改包含 `<fcntl.h>` 后各 `custom/*.c` 仍可编译。
4. CI 四个 job（`build-image`、glibc、uClibc、`native-tests`）全绿。
5. 本 PR 期间新出现的编译告警只在 plan 的偏离记录中登记，不逐条修。
6. 真机验证（§8）已在物理 Luckfox Pico Ultra W 上执行，结果与未测项见 plan 验证证据，遗留问题转后续 PR；CI 成功不能代替真机点亮。

## 8. 真机验证清单

以下为验收判据；已执行结果、未测项与遗留问题见 plan 的 Task 32 验证证据。

- 主屏：日历翻月；正午（12:xx）显示 PM、零点显示 12 AM；tick 溢出以 32 位模拟测试证明，不做 72 分钟真机长跑。
- WiFi：输入含 `"`、`\`、`}` 的 SSID 与口令并 Load，确认写出的配置与回读一致；输入少于 8 位的密码、`"` 之后含 `#` 的值，确认被拒绝且提示可见；SSID 含空格的网络出现在扫描下拉框且不被截断；离开 WIFI 页后确认不再周期性 `wpa_cli status`；连按两次 Load 后 `ps` 只有一个 `udhcpc -i wlan0`。
- 画板：笔迹落点与手指一致。
- 音乐页：放入长文件名（≥100 字节）与总长超过 255 字节的列表、文件名含 `"` 的文件、`/music` 为空与缺失；杀掉 `mpv` 后按下一曲，进程不退出；记录板上 `mpv` 启动到 `/tmp/mpvsocket` 出现的耗时，核对 D7 的 5 s 上限是否足够；耗时在冷启动后测，并对比先跑一次无参 `mpv` 预热页缓存的情形（D7 删掉的 `system("mpv …")` 可能起过预热作用）。
- OFF 退出：终端无 `cat: write error` 噪音。
- 板上确认 `which pkill`（预期来自 procps-ng）、`ps | grep -i dhcp`（既看 `udhcpc` 也看 `dhcpcd`，决定 D17 用 `pkill` 还是 pidfile）、`wpa_supplicant -v`（预期 2.6，核对 §2.2 的 `"`、`#` 解析行为）。

## 9. 风险

| 风险 | 缓解 |
|---|---|
| uClibc 上 `clock_gettime` 需要 `-lrt`，链接失败 | uClibc 交叉编译 job 把关；必要时顶层 `CMakeLists.txt` 补 `rt` |
| 板上没有 `pkill` | 本次目标板已确认 procps-ng 3.3.17 提供 `/bin/pkill`；其他固件使用 D17 的 pidfile 备选方案 |
| 板上 wpa_supplicant（SDK 构建 2.6）与测得的 2.10 对 `"`、`#` 的解析不同 | D11 取保守判据（`#` 之前出现过 `"` 即拒绝），两个版本上都安全，2.6 上只是多拒绝一些本可使用的值；本次目标板已确认 v2.6，特殊字符写入与非法输入拒绝结果见 plan 验证证据 |
| `custom.h` 改包含 `<fcntl.h>` 影响其他翻译单元 | 全仓除 `custom_brightness.c:10` 的注释外没有对 `linux/fcntl.h` 的依赖；`custom/*.c`、tests 工程与顶层 glibc 交叉编译已验证通过，uClibc 由交叉编译 job 把关 |
| 覆盖 `popen`、`time`、`rename` 等符号的测试可能与 ASan 运行时或 LVGL 的使用冲突 | 只在对应用例的可执行文件里定义，每用例一个进程；冲突时改用 `-Wl,--wrap` |
| 测试可执行文件覆盖 `time` 影响 LVGL 内部 | 覆盖函数在未启用模拟时转调真实实现（`dlsym(RTLD_NEXT, …)`） |
| D12 新增一个提示标签是行为变更 | 只在输入非法时可见；放在 `custom/` 内创建，不改 `generated/` 布局 |
| 单个 PR 含 30 个修复提交，评审量大 | 每个提交单一主题、测试与修复同提交，便于逐个评审与回滚；按页面分组便于分批评审 |

## 10. QA（设计决策与约束澄清）

grilling 问答记录：问句与推荐取自问答原文，同一主题的多轮问答并入同一条；正文已吸收的结论只留节号指向。「答」只记录作者原话或明确的采纳，没有回答记录的条目照实写明。

**1. mpv 连接要不要重试（⑩，D7、§2.5）**

- 问：⑩ mpv 连接。现状是只连一次，`sleep(0.1)` 实际是 `sleep(0)`，还有 `custom_init()` 里一句同步的 `system("mpv 2>&1 >/dev/null")`。后者会在前台跑一次无参 mpv，结果也没被使用。方案是：删掉那句 `system`，连接改为最多重试 10 次、每次间隔 100 ms（`usleep`），全部失败则返回 -1，和现在连接失败的处理一致。
- 推荐：如上。
- 答（原话）：「有必要重试连接吗？mpv 如果未安装第一次就知道了，重试的过程中应该也不会再安装 mpv 吧，这里重试的意义是什么？」
- 回复（原文要点）：重试的目的不是应对 mpv 没装。
  - mpv 未安装：重试没有用。exec 失败后子进程直接退出，套接字永远不会出现，重试只会白白拖慢启动。
  - 启动竞态：mpv 的 exec 成功后，`/tmp/mpvsocket` 要等 mpv 初始化完才出现，`connect` 抢在前面会失败。这才是重试的意义。
  - 耗时：x86 云端 VM 上套接字出现耗时 42–137 ms（5 次，§2.5）；Luckfox 的 ARM 核比这慢得多，实际耗时没法在板上测。
  - 现状：代码里是 `sleep(1)` 固定等 1 秒再 `connect` 一次，是真的在睡；真正没用的是 `connect` 前面那句 `sleep(0.1)`。当前行为是「每次启动白等 1 秒，板子慢时仍可能连不上」。
  - 备选：保持现状（固定 `sleep(1)` 加单次 `connect`，只删无效的 `sleep(0.1)` 和那句 `system`），代价是启动仍白等 1 秒、慢板子仍有概率连不上；推荐轮询方案。
- 结论：按推荐的轮询方案写入 D7：去掉固定等待，轮询 `connect`（间隔 50 ms，总上限 5 s（生产预算 5000 ms）），`mpv` 缺失靠轮询中的 `waitpid(WNOHANG)` 看到退出码 127 放弃，不靠耗尽重试上限。
- 问（测试延迟与超时收尾）：生产连接总预算为 5000 ms，测试单调时钟加速 2.5 倍，`slow_socket` 的假 mpv 实际延迟 1300 ms，对应 3250 ms 生产时钟；测试自身保留真实计时断言。轮询达到 5 s 预算仍未连接且子进程存活时，是否 kill 加 waitpid 回收？A 回收，不留占用音频设备的进程或僵尸；B 不处理。
- 推荐：采用上述时钟与延迟设置，超时回收选 A。
- 答（原话）：「同意使用推荐」。
- 结论：`slow_socket` 延迟取约 1300 ms，测试单调时钟加速 2.5 倍；启动前先 unlink 旧监听路径，`stale_socket` 验证不会误连旧实例；`timeout_reaps_child` 验证超时回收；D7 超时分支补 `kill(pid, SIGKILL)` 加 `waitpid`（D7、§5.4 ⑩）。

**2. 25 处缺陷是否仍在、范围（§2.1、§3）**

- 问：25 处里 24 处确认存在，⑮ 待编译验证。暂缓 7 项不变，范围以 PR #7 spec §10 为准，不再从零讨论。请确认。
- 推荐：确认。⑮ 在写测试时用 `-Werror` 实测，实测不存在则从 spec 删去并说明。
- 答：没有单独的回答记录。结论见 §2.1：⑮ 已用 `-Werror=implicit-function-declaration` 编译，两处均报错，25 处全部保留；范围沿用 PR #7 spec §10：25 处纳入、7 项暂缓（§3.3）。

**3. 音乐列表字符串与命令构造（②③④⑧，D3、D4、D5、D20）**

- 问（`custom/` 接口）：现状是 `generated` 里 `char roller_str[256]`，调用 `music_scan_list(roller_str)`。方案 A：`custom/` 里用 `malloc` 按实际长度拼出选项串，新增 `const char *music_roller_options(void)`；`generated` 只改成先调用 `music_scan_list(NULL)`，再把 `music_roller_options()` 的返回值交给 `lv_roller_set_options`（LVGL 会自行拷贝）。方案 B：保持调用方传缓冲区，增加 `size` 参数，超出就截断。另：mpv 的 JSON 命令改用已有的 cJSON 构造，`"` 和 `\` 自动转义；roller 选中项改用 `lv_roller_get_selected()` 取下标，按 `id` 找节点，不再 `strcmp`（解决 ④）。
- 推荐：方案 A。
- 确认问：`music_scan_list(void)` 加 `const char *music_roller_options(void)`，永不返回 NULL，空列表返回 `""`，所有权在 `custom/` 内；缓冲区用动态分配加 `snprintf`；mpv 命令用 cJSON 构造并释放；含 `\n` 的文件名跳过；roller 回调改用 `lv_roller_get_selected()` 取下标，并判空、判越界；测试注入点为 `MUSIC_DIR_PATH` 宏覆盖，`fd_mpv` 用 `socketpair`。
- 答：遵从推荐。
- 否决的做法：方案 B（截断会改写用户数据，缓冲区仍有上限）；「超过 255 字节的文件名直接跳过」（`d_name` 最长 255 字节，刚好放进 `filename[256]`，那是死代码，溢出的根因是固定大小的拼接缓冲区，D3）。空列表时 `lv_roller_set_options(NULL)` 会触发断言，`lib/lv_conf.h` 里断言处理是 `while(1);`，会死循环（§2.3，D4）。含 `\n` 与空列表空指针的纳入见第 9 条。

**4. 屏蔽 SIGPIPE 的方式（⑦，D6）**

- 问：⑦ SIGPIPE。是用 `send(fd, buf, len, MSG_NOSIGNAL)` 包装全部 `write(fd_mpv, ...)`，还是在 `music_player_thread_init` 里 `signal(SIGPIPE, SIG_IGN)`？后者是进程级设置，会影响整个进程，包括 WiFi 的 `popen`。
- 推荐：`MSG_NOSIGNAL`，在 `custom/` 里收口成一个 `mpv_send()`；顺带处理 ⑤（`cJSON_Delete`）和 ⑥（读 511 字节并补 NUL）。
- 答：遵从推荐。

**5. 画板缓冲区与坐标（⑱⑲，D10）**

- 问：在 `custom/custom_sketchpad.c` 新增 `lv_sketchpad_set_size(obj, w, h)`，用 `lv_mem_alloc` 一次性分配 `w*h*sizeof(lv_color_t)` 并设为画布缓冲；析构时释放；`generated/setup_scr_Sketchpad.c` 只改成调用它；坐标用 `point - obj 的屏幕左上角` 换算为画布坐标；失败测试用脚本化触摸画一笔，断言画布对应像素被改写。
- 推荐：如上。
- 确认问：分配判空；⑱ 和 ⑲ 各一个失败测试；析构时先 invalidate 再 free。
- 答：遵从推荐。

**6. WiFi 输入校验与提示（⑳㉕，D11、D12、§2.2）**

- 问（规则）：SSID 1 到 32 字节；密码 8 到 63 个字符；密码框 `max_length` 由 32 改为 63（`generated` 的一行改动）；不接受空密码（开放网络），因为现有 UI 只写 `psk`；不合法时拒绝写 `wpa_supplicant.conf`，在页面上提示；读取配置时 `sscanf` 加宽度限制。提示复用现有控件，还是新增一个红色小标签？
- 推荐：按上面规则；提示复用现有状态标签，不新增控件。
- 确认问：校验放在 `popen("wpa_cli")` 之前，因为 `save_config` 会让 wpa_supplicant 自己落盘；长度按字节：SSID 1–32，密码 8–63，空密码拒绝；UTF-8 SSID 合法；提示不复用 `WIFI_loaded_wifi_label`，因为 5 秒定时器会让它残留成 SSID。
- 问（`"`、`\`、`#`）：方案 1：全部放行，同时把 `_wifi_conf_get` 改成取「第一个 `"` 到最后一个 `"`」，只拒绝「`"` 之后又出现 `#`」的组合并提示。方案 2：拒绝 `"`，放行 `\`。方案 3：`"`、`\`、`#` 都拒绝。方案 1 不误伤合法口令，也不拒掉扫描出来的特殊字符网络，但读写两侧都要改；方案 2、3 改动小，会误伤合法口令。依据是 wpa_supplicant 的实测（§2.2）：`\` 与单独出现的 `"` 正常，只有 `"` 之后又出现 `#` 才会被截断或整行解析失败；真正对 `"` 不友好的是本工程自己的 `sscanf` 读取（回读时遇到第一个 `"` 就截断，且没有宽度限制）。
- 推荐：方案 1。
- 答：遵从推荐。

**7. WiFi 配置写入与扫描解析（㉑㉒，D13、D14、D15）**

- 问：临时文件放在 `WPA_FILE_PATH` 同目录，写完先 `fflush` 再 `fsync`，然后直接 `rename` 覆盖，不再先 `remove`；扫描结果按 `\t` 切分（`wpa_cli scan_results` 本来就是 `bssid\tfreq\tsignal\tflags\tssid`），不会被 SSID 里的空格截断；失败测试把 `wpa_cli` 换成测试用的假脚本。
- 推荐：如上。
- 确认问：扫描按 TAB 位置切分，用 `\n` 连接，去掉 `strlen<16` 过滤，解码 `\"`、`\\`、`\xNN`；`WPA_FILE_PATH` 可被测试覆盖；临时文件放同目录，`fsync` 后直接 `rename`。
- 答：遵从推荐。

**8. WiFi 定时器与 `udhcpc`（㉓㉔，D16、D17）**

- 问：页面卸载（`LV_EVENT_SCREEN_UNLOADED`）时删除 5 秒定时器；按 Load 前先用 `pkill udhcpc`，避免重复拉起，或者改用 `udhcpc -n -i wlan0` 一次性执行；`_wifi_status_update` 里 `pclose(NULL)` 加判空。
- 推荐：`-n` 会改变重试行为，不放进本 PR。
- 确认问（定时器与命令）：不删定时器，回调开头加 `lv_scr_act() != guider_ui.WIFI` 守卫，生成代码不动；`udhcpc` 分两次 `system()`：先 `pkill -f '[u]dhcpc -i wlan0'`，再启动；`pclose(NULL)` 判空，不加 `-n`。依据：WIFI 页的 `*_del` 标志在离开后置为 false，再次进入不会重新调用 `setup_scr_WIFI`，所以 `wifi_backend_init()` 只跑一次，卸载时删掉定时器后状态再也不刷新（§2.3）；把 `pkill -f 'udhcpc -i wlan0'` 和启动命令合在一条 `sh -c` 里，shell 自己会被杀（退出码 143），`'[u]dhcpc -i wlan0'` 写法下 shell 存活，eth0 的 `udhcpc` 不受影响。
- 问（清理方式）：A 用 `pkill -f '[u]dhcpc -i wlan0'`，它还会清掉开机脚本里已有的 wlan0 实例，但依赖板上有 `pkill`，无法确认。B 用 `-p` 写 pidfile，下次 Load 前在 C 里读 pid，校验 `/proc/<pid>/cmdline` 后 `kill`，不依赖 `pkill`，也不会误杀其他接口，但清不掉开机脚本那个实例。请在板上跑 `which pkill; ps | grep udhcpc` 确认。
- 推荐：A；板上没有 `pkill` 就改 B。
- 答：遵从推荐。板上确认项见 §8。

**9. 5 条关联缺陷是否纳入（§3.2）**

- 问：下面 5 条是对 ⑧、㉑ 范围的扩展，按 User Rule 2，确认前不写进 spec，请逐条回答「纳入」或「不纳入」。
  - C1：`/music` 目录缺失或为空时，`music_app_init` 和 Next/Prev 会解引用 NULL，这是 ⑧ 的延伸。
  - C2：文件名含 `\n` 时跳过（已并入音乐列表方案）。
  - C3：写配置时的 `strstr("ssid=")` 会误改 `scan_ssid=1` 和 `bssid=` 行，`strstr("psk=")` 同理；改成只匹配去掉行首空白后以 `ssid=` 或 `psk=` 开头的行。
  - C4：`_wifi_conf_load` 里 `popen("wpa_cli")` 失败直接 `exit(EXIT_FAILURE)`，改为提示并返回。
  - C5：`custom_wifi.c` 用 `strstr(line, "}")` 判断 `network={}` 块结束，SSID 或口令里含 `}` 时，该行在匹配 `ssid=` 之前就被当成块结束，写回时那一行原样保留，读取时读不出；改成「去掉行首空白后整行就是 `}`」才判结束。
- 推荐：全部纳入。C1：⑧ 修了 `opendir` 判空后，后面仍会崩，不一起修等于没修。C3：这会真的写坏用户的配置。C4：一行改动。C5：和 C3 是同一类问题，放进同一个 fix 提交。
- 答：遵从推荐。各自单独提交（C5 在 C3 之后单独一个提交；C4 与 ㉓ 合并为一个 `popen` 失败处理提交，与 PR #7 spec §10.3 按页面拆 3 个一致）。

**10. tick、隐式声明与 OFF 退出（⑬⑮⑰，D8、D9、§2.4）**

- 问：⑬：`custom_tick_get()` 改用 `clock_gettime(CLOCK_MONOTONIC)` 计算毫秒，全程 64 位运算，最后返回 `uint32_t` 回绕值；32 位模拟测试沿用现有 `arm32` 机制。⑰：`custom/` 新增 `custom_fb_clear(const char *path)`，循环写零，到 `ENOSPC` 或总量上限时静默返回，`EINTR` 重试，短写计入总量后继续循环；`src/main.c` 调它，不再 `system("cat…")`；测试把路径指向 `/dev/full`，断言没有输出且正常返回。
- 推荐：如上。
- 确认问：arm32 测试加 `-U_TIME_BITS -U_FILE_OFFSET_BITS` 并用桩注入时间（Ubuntu armhf 编译器默认 `time_t` 为 8 字节，加这两个选项后才变 4 字节，沿用现有 arm32 目标时旧实现不会溢出）；`custom_fb_clear` 在 `EINTR` 时重试，短写计入总量后继续循环；`ENOSPC`、`write` 返回 0 或达到总量上限时静默返回 0，其他错误返回 -1 并打印一行；写入块为 4096 字节，远小于帧缓冲，不会触发块大小超过帧缓冲总长的 `EFBIG`，不单独处理 `EFBIG`。
- 答：遵从推荐。
- 隐式声明（⑮）：`open` 的声明靠补 `<fcntl.h>`，但 `custom/custom.h:18` 已包含 `<linux/fcntl.h>`，两者同时包含会让 `struct flock` 重复定义。选项：A 把 `custom.h:18` 改成 `<fcntl.h>`；B 给 `open` 写局部 `extern` 声明。答：允许改 `custom/custom.h:18`（选 A，D8）。

## 11. 参考资料

- [`2026-09-28-lvgl-example-native-tests-design.md`](2026-09-28-lvgl-example-native-tests-design.md) §10（范围）与 `tests/README.md`（用法）。
- hostap `wpa_supplicant/config.c` 配置解析、`wpa_supplicant/ctrl_iface.c` 的 `scan_results` 输出：https://w1.fi/cgit/hostap/ 。
- SDK 构建的 `wpa_supplicant` 版本：`yuangezhizao/luckfox-pico@dev` 的 `project/app/wifi_app/Makefile`（`PKG_NAME_WPA_TOOLS`）。
- LVGL 8.3 `lv_roller`、`lv_textarea`、`lv_canvas`：https://docs.lvgl.io/8.3/ 。
- 本仓 `custom/custom_musicplayer.c`、`custom/custom_main.c`、`custom/custom_wifi.c`、`custom/custom_sketchpad.c`、`generated/setup_scr_Music_player.c`、`generated/setup_scr_Sketchpad.c`、`generated/setup_scr_WIFI.c`、`generated/widgets_init.c`、`src/main.c`、`lib/lv_conf.h`。
