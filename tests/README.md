# 原生测试

`tests/` 是在 x86 主机上运行的独立 CMake 工程：用主机 gcc 以 ASan/UBSan 编译产品实际使用的 `lib/lvgl`、`generated/`、`custom/`，headless 运行，不需要开发板、`/dev/fb0` 或触摸屏。它与顶层交叉编译工程互不引用。

## 运行

```bash
cmake -S tests -B build-tests
cmake --build build-tests -j"$(nproc)"
ctest --test-dir build-tests --output-on-failure -j"$(nproc)"
```

- 依赖：`build-essential`、`cmake`、`libdrm-dev`、`libcjson-dev`（均在 `.cursor/Dockerfile` 中）。
- 只跑一个页面：`ctest --test-dir build-tests -L wifi`；按名字筛选：`ctest --test-dir build-tests -R '^main\.brightness\.'`；只列出不运行：加 `-N`。

## 目录与分类

用例按页面放在 `cases/<页面>/`：控件或逻辑显示在哪一页，测试就放哪个目录；与页面无关的全局逻辑（tick、隐式声明等）归 `main/`。

| 目录 | 页面 | 对应源码 |
|---|---|---|
| `main/` | 主屏与时间（含亮度滑条、SDIO 板型判定） | `custom_main.c`、`custom_brightness.c`、`widgets_init.c`、`lv_conf.h` |
| `wifi/` | WiFi | `custom_wifi.c`、`setup_scr_WIFI.c` |
| `music/` | 音乐页 | `custom_musicplayer.c`、`setup_scr_Music_player.c` |
| `sketchpad/` | 画板 | `custom_sketchpad.c`、`setup_scr_Sketchpad.c` |
| `exit/` | OFF 退出 | `src/main.c` 退出路径、`Main_OFF_btn_event_handler` |
| `gif/` | GIF 页 | `setup_scr_Gif.c` |

还没有测试的页面不建目录；本表即登记处。

`support/` 是公共代码（不含用例，函数统一 `tst_` 前缀）：`check.h` 断言与用例分派，`app_env` 提供 `src/main.c` 的 5 个全局量与 headless 显示、脚本化触摸，`capture` 捕获输出，`fake_fs` 管理本进程临时目录。

## 命名

- 文件 `test_<被测对象>.c`，不加序号或日期前缀；需要调用被测文件里的 `static` 函数时，配一个 `unit_<被测对象>.c`：它 `#include` 被测 `.c`、导出 `tst_` 前缀的包装函数，随被测源码以 `-w` 编译，测试文件本身以 `-Wall -Wextra -Werror` 编译。
- CTest 测试名 `<类别>.<对象>.<用例>`，用例名描述行为；标签为类别名。
- 依赖界面或模块静态状态的用例，每个用例一个进程：同一可执行文件按命令行参数选用例（`tst_run_case()`）。

## 新增测试

1. 在对应页面目录新建 `test_xxx.c`，用 `tst_case_t` 表列出用例，`main()` 调 `tst_run_case()`。
2. 在该目录 `CMakeLists.txt` 加一行 `luckfox_add_test(xxx [UNIT unit_xxx.c] CASES <用例>...)`。
3. 新页面第一个测试：建目录与 `CMakeLists.txt`，在 `tests/CMakeLists.txt` 加 `add_subdirectory(cases/<页面>)`，并更新上表。

## 注意

- 测试不调用 `custom_init()`：它用 `vfork()` 启动 `mpv`，主机无 `mpv` 时子进程 `return` 会破坏父进程栈。
- 捕获输出期间 `CHECK` 的打印也会被捕获，断言放在 `tst_capture_end()` 之后。
- `custom_brightness.c` 在测试构建中以 `BACKLIGHT_SYSFS_DIR=tst_backlight_root()` 编译，背光目录位于本进程临时目录，`ctest -j` 并行互不干扰。
