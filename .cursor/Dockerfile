# Luckfox Pico LVGL Example — Cloud Agent 环境镜像（自建 Ubuntu 24.04 方案 / environment.json 引用本文件）
#
# 目标：把开发环境「代码化」，不依赖任何人的个人快照（Personal snapshot）；任何人从带本文件的分支起 Cloud Agent，都能得到可复现的「交叉编译 + native 渲染验证」环境，让 `export GLIBC_COMPILER=/usr/bin/arm-linux-gnueabihf- && mkdir -p build && cd build && cmake .. && make && make install`（不先导出该变量的话 CMakeLists 会提前 return、不产出任何 target，详见 AGENTS.md）与 GUI-Guider 屏幕的本机渲染开箱即用。
#
# 说明：
# - Cloud Agent 本身就运行在「一个容器」里；本 Dockerfile 只是「声明这个容器长什么样」，并非 docker-in-docker。本仓安装 Tailscale，Agent Run 使用 kernel 模式（docs/superpowers/specs/2026-10-06-lvgl-example-cloudagent-align-luckfox-pico-design.md §3.2；理由见 luckfox-pico 08-30 spec §2.1）；Cursor 官方 userspace 路径只作回退。
# - 本仓是单个 LVGL 应用（非 SDK）：LVGL/lv_drivers/libdrm/libcjson 均 vendored 于 lib/；
#     · 交叉编译（目标 ARM）：本仓 ARM 交叉工具链【由本 Dockerfile apt 安装】（gcc/g++-arm-linux-gnueabihf），CMakeLists 走 $GLIBC_COMPILER 前缀。⚠️ 与参考的 luckfox-pico 仓库「工具链随仓库内置、无需在镜像内安装」相反：本仓工具链必须在镜像内安装。
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

# ┌─ 平台会「自动安装」的包 ───────────────────────────────────────────────────
# │ Cursor 在 Environment Build 阶段按容器内 /usr/local/share/vnc-desktop.Aptfile，
# │ 以 `apt-get install --no-install-recommends` 安装下列包；Agent Run 时 desktop-init.sh 只负责启动桌面。
# │ 2026-10-06 在 luckfox-pico 仓的 Agent（同一 FROM 的自建 Ubuntu 24.04 镜像）复核：sha256=819987b7…，58 个包名与下列清单一致。
# │   VNC     : tigervnc-standalone-server tigervnc-common tigervnc-tools
# │   XFCE    : xfce4 xfce4-terminal xfce4-settings thunar
# │   X11     : x11-utils x11-xserver-utils xdg-utils xdotool xclip procps
# │   D-Bus   : dbus-x11 at-spi2-core
# │   应用    : mousepad seahorse sudo ffmpeg
# │   主题    : adwaita-icon-theme gnome-themes-extra gnome-keyring plank sassc libglib2.0-dev-bin libxml2-utils dconf-cli xz-utils
# │   locale  : locales
# │   字体    : xfonts-base xfonts-terminus fonts-noto fonts-wqy-microhei fonts-droid-fallback fonts-noto-color-emoji fonts-liberation fonts-croscore fonts-cantarell fonts-jetbrains-mono
# │   桌面库  : libx11-dev libxkbfile-dev libsecret-1-dev libgbm-dev libnotify4 libnss3 libxss1
# │   Mesa    : libgl1-mesa-dri libglx-mesa0 libgl1（Aptfile 原文单列「Mesa software rendering for WebGL」，分组同原文）
# │ 本镜像在平台安装前已有 Mesa 三包与 libgbm-dev：由下方 libsdl2-dev 的依赖链带入（Mesa 25.2，2026-10-07 实测）；无 GPU 时提供 llvmpipe 软件 OpenGL，SDL 预览默认用它上屏，见 docs/superpowers/specs/2026-10-06-lvgl-example-cloudagent-align-luckfox-pico-design.md §2.5。
# │   Python  : python3-minimal python3-numpy
# │   资产依赖: bash ca-certificates coreutils curl findutils gzip tar
# │   浏览器  : google-chrome-stable（单独脚本 install-google-chrome 安装）
# │ 平台 Build 步骤还会配置 git 身份与 SSH 提交签名（含 git-cleanup 子步骤）、把 gh 软链到 /usr/local/bin/gh、创建 /opt/cursor/artifacts；
# │ Agent Run 启动时 artifacts 被替换为指向 /cursor/stores/self/artifacts 的链接。
# │ ⚠️ 本仓用于交叉编译 + native 渲染验证、不用桌面 computer use，但平台仍会装上述包；其中 ffmpeg 与 X/VNC 桌面恰是 docs/superpowers/specs/2026-07-24-lvgl-example-cloudagent-env-design.md §6.1 所述 SDL 桌面渲染/录屏路径的依赖，并非纯冗余。该清单只作 Cloud Agent 环境记录，不作为本文件 apt 取舍依据。
# └──────────────────────────────────────────────────────────────────────────────

# apt 取舍只看当前 FROM digest 的 dpkg：不在 FROM 则显式安装，已在 FROM 则不写。2026-10-06 对该 digest 执行 docker run --rm --entrypoint dpkg-query ubuntu:24.04@sha256:4fbb8e6a8395de5a7550b33509421a2bafbc0aab6c06ba2cef9ebffbc7092d90 -W（amd64，92 个包），下列包均不在 FROM。
# 下方 apt 包分组与 docs/superpowers/specs/2026-10-06-lvgl-example-cloudagent-align-luckfox-pico-design.md §2.2 一致：先本仓构建依赖（基础工具、平台引导、ARM 交叉工具链、构建系统、native 验证、32 位运行），再 A / B / C 类；每组前一行组名注释，组内按字母序。
# - 基础工具    : git（构建期即需——本文件末尾的 git config 依赖它）、sudo（容器内提权）、ca-certificates（运行期 https：git clone / curl / gh；注意 Ubuntu 官方 apt 源走 http，本行 apt 自身并不需要它）。
# - 平台引导    : curl（Cursor 安装 exec-daemon 时用它从 S3 拉取 tar.gz；该步早于平台按 Aptfile 补装 curl，故必须镜像内自带——ubuntu:24.04 基础镜像默认不含 curl）；curl 还被下一层拉 Tailscale 密钥使用。
# - ARM 交叉工具链: gcc-arm-linux-gnueabihf、g++-arm-linux-gnueabihf（编 32-bit ARM 目标；本仓工具链在此安装）。
# - 构建系统    : cmake；build-essential 提供 native x86 gcc/g++/make（native 渲染 harness 需要）。
# - native 验证 : libdrm-dev、libcjson-dev（x86 版，供 custom/*.c 链接期符号）；libsdl2-dev（SDL 驱动桌面开窗渲染 + ffmpeg 录制演示）；pkg-config 便利工具（可选）。
# - 32 位运行 : qemu-user（qemu-arm 用户态运行 32 位 ARM 程序：tests/ 的 32 位换算测试 main.brightness_arm32.conversion 与 uClibc 产物冒烟；CI 的 native-tests job 以 LUCKFOX_TESTS_ARM32=ON 依赖它）。⚠️ 修改本文件会改变 CI 镜像标签（取本文件哈希），该 PR 在合并前 build-image 找不到 dev 签名镜像而失败，合并后 dev push 重建镜像。
# - A 类 iproute2 / jq：`ss` 与 JSON 摘要供 .cursor/start.sh 使用；FROM 没有。procps 已在 FROM，不重复。
# - B 类 tcpdump / htop / neofetch：抓包与排查 CLI。neofetch 在 universe；本 RUN 带 --no-install-recommends，避免拉上 imagemagick 等推荐包。
# - B 类 tree / iftop / iotop / screen / ncdu / traceroute / nmap / ngrep / conntrack / psmisc / fping / dmidecode：诊断 CLI。apt 写 conntrack（AlmaLinux 包名 conntrack-tools）；不写 conntrack-tools / conntrackd / iotop-c。nmap/ngrep/ncdu 等在 universe，与 neofetch 相同前提。
# - C 类 通用 CLI：vim / file / rsync / unzip / wget / xz-utils / openssh-client / openssl / locales：与 luckfox-pico 仓库对齐的通用 CLI。
# - 绝不写 which：which 无独立实体包（命令由 essential 包 debianutils 内置、FROM 已有）；ubuntu:24.04（universe 默认开启）上写它会解析到虚包 gnu-which 并安装成功（退出 0）、徒增冗余包，官方镜像 Ubuntu 22.04 上则报 "E: Unable to locate package which" 致退出码 100。
# - 公开 Dockerfile 层不安装 openssh-server：FROM 也没有；server 由 .cursor/install.sh 写入私有 Build 快照，避免 host key 进入公开层。
RUN apt-get update && apt-get install -y --no-install-recommends \
      # 基础工具
      ca-certificates git sudo \
      # 平台引导
      curl \
      # ARM 交叉工具链
      g++-arm-linux-gnueabihf gcc-arm-linux-gnueabihf \
      # 构建系统
      build-essential cmake \
      # native 验证
      libcjson-dev libdrm-dev libsdl2-dev pkg-config \
      # 32 位运行
      qemu-user \
      # A 类：start.sh 依赖
      iproute2 jq \
      # B 类：诊断 CLI
      conntrack dmidecode fping htop iftop iotop ncdu neofetch ngrep \
      nmap psmisc screen tcpdump traceroute tree \
      # C 类：通用 CLI
      file locales openssh-client openssl rsync unzip vim wget xz-utils \
    && rm -rf /var/lib/apt/lists/*

# Tailscale 使用官方 Ubuntu 24.04 Noble stable apt 仓库，不锁定版本；节点认证只在 Agent Run 的 start 阶段进行。本层与上方 apt RUN 一样带 --no-install-recommends。
RUN install -d -m 0755 /usr/share/keyrings \
    && curl -fsSL https://pkgs.tailscale.com/stable/ubuntu/noble.noarmor.gpg \
       -o /usr/share/keyrings/tailscale-archive-keyring.gpg \
    && curl -fsSL https://pkgs.tailscale.com/stable/ubuntu/noble.tailscale-keyring.list \
       -o /etc/apt/sources.list.d/tailscale.list \
    && apt-get update \
    && apt-get install -y --no-install-recommends tailscale \
    && rm -rf /var/lib/apt/lists/*

# Cursor 平台在运行时提供 ubuntu 用户；为 OpenSSH 登录后的提权固化免密 sudo，不在此镜像层安装 openssh-server。
RUN printf '%s\n' 'ubuntu ALL=(ALL) NOPASSWD:ALL' > /etc/sudoers.d/ubuntu \
    && chmod 0440 /etc/sudoers.d/ubuntu \
    && visudo -cf /etc/sudoers.d/ubuntu

# 允许对检出的工作区执行 git（避免 "detected dubious ownership" 报错）。
RUN git config --system --add safe.directory '*'

# Cloud Agent 默认运行用户。须写在 docker build 的全部 RUN 之后：USER 会改变后续 RUN 的身份，构建期没有 sudo -n -E；JSON 的 sudo -n -E 只用于镜像建成后的 install/start。
USER ubuntu
