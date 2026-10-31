# GitHub Actions

改 `.github/workflows/build-luckfox-lvgl-demo.yml` 或排查 CI 失败前读本文件。CI 成功只证明交叉编译与 ABI 门禁通过，不能代替真机点亮；权威验证仍是物理 Luckfox Pico Ultra / Luckfox Pico Ultra W。

## 交叉编译

- CI 编两条：glibc（只设 `GLIBC_COMPILER=/usr/bin/arm-linux-gnueabihf-`，`make install` 整包 `install/luckfox_lvgl_demo/`）与 uClibc（只设 `LUCKFOX_SDK_PATH` 指向 CI 内 `yuangezhizao/luckfox-pico` 检出根，只出可执行文件）。
- 作者当前系统是 SDK 编的 Buildroot，板上使用 uClibc 产物；glibc 产物 ABI 不兼容（`libc.so.6` vs `libc.so.0`），不能在这块板上运行。
- uClibc gcc 不在 `.cursor/Dockerfile` 里；CI 从 `yuangezhizao/luckfox-pico@dev` sparse checkout `tools/linux/toolchain/`，不引入 submodule。

## CI 镜像

- CI 镜像只在 `dev` 上构建并 attest；`pull_request` 只复用 dest-lock（signer/source=`dev`）通过的 digest，失败即退出，不覆盖 GHCR。
- 镜像标签取 `.cursor/Dockerfile` 内容哈希，所以修改该文件的 PR 在合并前 `build-image` 必然失败（找不到 dev 签名镜像），依赖它的容器 job 被跳过，合并后 dev push 重建镜像才恢复。
- 不把 `container:` 换成 `luckfox-pico-ci`（其中无 `gcc-arm-linux-gnueabihf`，uClibc gcc 也不在镜像层）。
- 镜像末尾 `USER ubuntu`，`build-demo`、`native-tests`、`mutation-tests` 与 `doc-check` 以 `options: --user 0` 运行（Actions 要求 job 容器为超级用户）。

## 各 job

- `native-tests`（`🧪 原生测试`）以 `-DLUCKFOX_TESTS_ARM32=ON` 强制运行 32 位测试；在 `ctest` 之后构建 `preview`，用 CI 镜像中的 Xvfb、xdotool、ImageMagick 以 `--headless` 在 480、720 下运行 `tests/tools/preview/` 的两个核对脚本与 `shots.sh`，失败时上传截图与日志（artifact `preview-shots`，保留 7 天）。
- `mutation-tests`（`🧬 变体检验`）与 `native-tests` 用同一镜像、触发相同（`schedule` 跳过）、两者并行，全量运行 `tests/tools/mutate.sh`：退出码非 0（1 有变体存活，2 工具或清单错误）或出现 `SKIPPED` 即失败（镜像工具齐全，`SKIPPED` 只可能是 `tests:` 正则没匹配到用例），失败时按类型打印存活变体的清单记录、基线失败用例的重跑输出或构建日志末尾，不上传 artifact。它是 CI 中耗时最长的 job，同一 PR 的新推送会取消正在运行的 CI（`cancel-in-progress`）。逐提交测试仍只在本地执行。
- `🐚 ShellCheck` 在 runner 上直接运行（不用容器、不依赖 `build-image`），对 `git ls-files` 列出的 `*.sh` 与假 `wpa_cli` 执行 `shellcheck -x -P SCRIPTDIR`，要求 0 条（含 style 级）。
- `📝 文档检查`（`doc-check`）用同一镜像（`needs: build-image`，`options: --user 0`），触发同 `native-tests`（`schedule` 跳过），每次完整运行，不按改动文件跳过：先检查根 `AGENTS.md` 不超过 8,192 字节，再 `tests/tools/docs/check-headings.py` 检查全部 spec 与 plan，最后 `check-samples.sh` 运行检查器自身的样例（用法见 `docs/superpowers/AGENTS.md`）。体积超限时先按 spec [`2026-11-01-lvgl-example-agents-md-slim-design.md`](../docs/superpowers/specs/2026-11-01-lvgl-example-agents-md-slim-design.md) §3.1 把内容移到子目录说明或代码注释，再考虑调整预算。修改 `.cursor/Dockerfile` 的 PR 上它与其他容器 job 一同被跳过；只改 `.md` 的 dev push 因 `paths-ignore` 不触发 workflow，文档在 PR 上已检查。

## 编译告警

主程序以 `CMakeLists.txt` 告警段的 `-Wall -Wextra …` 编译，告警只记录、不作门禁；2026-10-27 时为 glibc 121 条、uClibc 120 条（新增的编译单元会重复报一次 `lib/lv_conf.h` 的 `-Wundef`）。LVGL 与 lv_drivers 子目录不受告警段影响，`C_FLAGS` 为空。现场统计：

```bash
GLIBC_COMPILER=/usr/bin/arm-linux-gnueabihf- cmake -S . -B /tmp/b-glibc && make -C /tmp/b-glibc -j 2>&1 | grep -c 'warning:'
```
