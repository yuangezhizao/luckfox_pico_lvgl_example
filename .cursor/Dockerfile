# Luckfox Pico LVGL Example — Cloud Agent 环境镜像（自建 Ubuntu 24.04 方案 / environment.json 引用本文件）
#
# 目标：把开发环境「代码化」，不依赖任何人的个人快照（Personal snapshot）；任何人从带本文件的分支起 Cloud Agent，都能得到可复现的「交叉编译 + native 渲染验证」环境，让 `export GLIBC_COMPILER=/usr/bin/arm-linux-gnueabihf- && mkdir -p build && cd build && cmake .. && make && make install`（不先导出该变量的话 CMakeLists 会提前 return、不产出任何 target，详见 AGENTS.md）与 GUI-Guider 屏幕的本机渲染开箱即用。
#
# 说明：
# - Cloud Agent 本身就运行在「一个容器」里；本 Dockerfile 只是「声明这个容器长什么样」，并非 docker-in-docker。
# - 本仓是单个 LVGL 应用（非 SDK）：LVGL/lv_drivers/libdrm/libcjson 均 vendored 于 lib/；
#     · 交叉编译（目标 ARM）：本仓 ARM 交叉工具链【由本 Dockerfile apt 安装】（gcc/g++-arm-linux-gnueabihf），CMakeLists 走 $GLIBC_COMPILER 前缀。⚠️ 与参考 SDK 仓库「工具链随仓库内置、无需在镜像内安装」相反：本仓工具链必须在镜像内安装。
#     · native 渲染验证（x86-64）：额外需系统 libdrm-dev/libcjson-dev —— custom/*.c 源码调用这些 API（链接期需符号），但 native harness 运行时不触达该路径（故不需真实 DRM 设备）。
# - 本仓【无 build.sh】，构建直接用 cmake；不存在参考里的「build.sh 选板 / 加入 PATH」流程。
# - 基于 ubuntu:24.04（贴合默认 Cloud Agent 与本机 24.04 LTS）：官方仅就 computer use 声明镜像须为 Debian/Ubuntu 系；本仓不做 computer use，但 SDL 桌面渲染/录屏复用的正是同一套平台 X/VNC 桌面（其初始化脚本走 apt），故按同一约束选 Ubuntu 基底，同时也贴合默认环境与 apt 生态。
# - 不要 COPY 项目源码：Cursor 会自行 checkout 正确的 commit 到工作区。
# - FROM 同时锁定 tag 与 digest：tag（24.04）便于人读，digest 保证镜像内容字节级不可变。
#     获取/更新 digest：docker buildx imagetools inspect ubuntu:24.04（取 index digest）。当前对应 24.04.4 LTS。
FROM ubuntu:24.04@sha256:4fbb8e6a8395de5a7550b33509421a2bafbc0aab6c06ba2cef9ebffbc7092d90

# DEBIAN_FRONTEND=noninteractive：无人值守安装，避免 tzdata/keyboard 交互卡住构建。仅构建期需要，用 ARG（不持久化到运行期）。
ARG DEBIAN_FRONTEND=noninteractive
# TZ：设运行期时区环境变量，glibc 据此让 date/localtime() 走 CST，影响容器内时间与渲染产物时间戳（注：它并不改镜像的 /etc/localtime，该文件仍是 tzdata 默认的 Etc/UTC；让 tzdata 免交互安装的是上面的 DEBIAN_FRONTEND）。
ENV TZ=Asia/Shanghai

# ┌─ 平台会「自动安装」的包（无需在本文件重复安装）──────────────────────────────
# │ Cursor 每次启动都会跑桌面初始化脚本；该脚本比对 Aptfile 的 sha256，首次（或清单变化）才按容器内 /usr/local/share/vnc-desktop.Aptfile 用 `apt-get install --no-install-recommends` 安装下列，已装过则跳过（实测 sha256=819987b7…，本环境 2026-07-26 核验，与参考项目一致）：
# │   VNC     : tigervnc-standalone-server tigervnc-common tigervnc-tools
# │   XFCE    : xfce4 xfce4-terminal xfce4-settings thunar
# │   X11     : x11-utils x11-xserver-utils xdg-utils xdotool xclip procps
# │   D-Bus   : dbus-x11 at-spi2-core
# │   应用    : mousepad seahorse sudo ffmpeg
# │   主题    : adwaita-icon-theme gnome-themes-extra gnome-keyring plank sassc libglib2.0-dev-bin libxml2-utils dconf-cli xz-utils
# │   locale  : locales
# │   字体    : xfonts-base xfonts-terminus fonts-noto fonts-wqy-microhei fonts-droid-fallback fonts-noto-color-emoji fonts-liberation fonts-croscore fonts-cantarell fonts-jetbrains-mono
# │   桌面库  : libx11-dev libxkbfile-dev libsecret-1-dev libgbm-dev libnotify4 libnss3 libxss1
# │   Mesa    : libgl1-mesa-dri libglx-mesa0 libgl1
# │   Python  : python3-minimal python3-numpy
# │   资产依赖: bash ca-certificates coreutils curl findutils gzip tar
# │ 另外平台还会：把 gh 软链到 /usr/local/bin/gh、配置 git 身份与 SSH 提交签名、准备 /opt/cursor/artifacts（以上本环境实测属实；参考项目另称每次启动会 git clean -fd，本环境未找到对应脚本、故不采信）。
# │ ⚠️ 本仓用于交叉编译 + native 渲染验证、不用桌面 computer use，但平台仍会装上述包；其中 ffmpeg 与 X/VNC 桌面恰是 spec §6.1 所述 SDL 桌面渲染/录屏路径的依赖，并非纯冗余、勿当无用。
# │   维护提示（清单实测 2026-07-26）：若平台不再自动装致编译/工具异常，把缺的项补进下面 apt。
# └──────────────────────────────────────────────────────────────────────────────

# 从裸 Ubuntu 24.04 自装本项目「构建 + 验证」依赖（base 镜像 digest 锁定 + 依赖清单固化；apt 版本随上游漂移）：
# - 基础工具    : git（构建期即需——本文件末尾的 git config 依赖它，且它并不在上方平台清单里）、sudo（容器内提权）、ca-certificates（运行期 https：git clone / curl / gh；注意 Ubuntu 官方 apt 源走 http，本行 apt 自身并不需要它）。sudo 与 ca-certificates 虽也在上方平台清单里，仍于镜像层自装：目的是不依赖平台运行期行为、把能力固化进镜像以保证可复现，apt 幂等无副作用。
# - ARM 交叉工具链: gcc-arm-linux-gnueabihf、g++-arm-linux-gnueabihf（编 32-bit ARM 目标；本仓工具链在此安装）。
# - 构建系统    : cmake；build-essential 提供 native x86 gcc/g++/make（native 渲染 harness 需要）。
# - native 验证 : libdrm-dev、libcjson-dev（x86 版，供 custom/*.c 链接期符号）；libsdl2-dev（SDL 驱动桌面开窗渲染 + ffmpeg 录制演示）；pkg-config 便利工具（可选）。
RUN apt-get update && apt-get install -y --no-install-recommends \
      git sudo ca-certificates \
      gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf \
      cmake build-essential \
      libdrm-dev libcjson-dev libsdl2-dev pkg-config \
    && rm -rf /var/lib/apt/lists/*

# 允许对检出的工作区执行 git（避免 "detected dubious ownership" 报错）。
RUN git config --system --add safe.directory '*'
