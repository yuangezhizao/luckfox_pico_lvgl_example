#!/usr/bin/env bash
# 启动 preview（鼠标当触摸），音乐目录放 N 首，假 wpa_cli 初始为连不上。
# 用法：start.sh [--headless] [480|720] [歌曲数]，默认 480 屏、4 首。
# 有 DISPLAY 且未加 --headless 时用当前显示（Cloud Agent 桌面为 :1）；否则自起 X 服务器：有 Xvfb 用 Xvfb，否则用 Xtigervnc，
# 也可用 PREVIEW_XSERVER=Xvfb|Xtigervnc 指定。
# 先执行 cmake --build build-tests --target preview；构建目录可用 PREVIEW_BUILD、会话目录可用 PREVIEW_WORK 覆盖，PREVIEW_BACKLIGHT 原样传给 preview。
set -euo pipefail
# shellcheck source=lib.sh
. "$(dirname "$0")/lib.sh"

headless=0
if [ "${1:-}" = --headless ]; then
    headless=1
    shift
fi
[ -n "${DISPLAY:-}" ] || headless=1
PV_RES=${1:-480}
songs=${2:-4}
case "$PV_RES" in 480 | 720) ;; *) pv_die "分辨率只能是 480 或 720：$PV_RES" ;; esac
case "$songs" in '' | *[!0-9]*) pv_die "歌曲数须为非负整数：$songs" ;; esac
pv_need xdotool import convert
PREVIEW_TITLE='Luckfox LVGL preview'
[ -x "$PREVIEW_BUILD/preview" ] || pv_die "没有 $PREVIEW_BUILD/preview，先执行 cmake --build $PREVIEW_BUILD --target preview"

pv_prepare_work
"$PREVIEW_DIR/stop.sh"
rm -rf "$PREVIEW_WORK/tmp" "$PREVIEW_WORK/wifi"
mkdir -p "$PREVIEW_WORK/tmp" "$PREVIEW_WORK/wifi" "$PREVIEW_WORK/shots"
: >"$PREVIEW_WORK/wifi/cmd.log"

# 成功启动前任何一处退出（连不上显示、等不到窗口、认不出主屏等）都结束本次启动的进程：
# 已写会话文件时交给 stop.sh，否则直接结束 preview 与自起的 X 服务器
PV_XPID=
PV_PID=
started=0
cleanup() {
    [ "$started" = 1 ] && return
    if [ -f "$PREVIEW_WORK/session" ]; then
        "$PREVIEW_DIR/stop.sh" || true
    else
        if [ -n "$PV_PID" ]; then kill "$PV_PID" 2>/dev/null || true; fi
        if [ -n "$PV_XPID" ]; then kill "$PV_XPID" 2>/dev/null || true; fi
    fi
}
trap cleanup EXIT
if [ "$headless" = 1 ]; then
    n=99
    while [ -e "/tmp/.X11-unix/X$n" ] || [ -e "/tmp/.X$n-lock" ]; do
        n=$((n + 1))
        [ "$n" -le 199 ] || pv_die "找不到空闲的显示号（:99–:199）"
    done
    xs=${PREVIEW_XSERVER:-}
    if [ -z "$xs" ]; then
        if command -v Xvfb >/dev/null 2>&1; then xs=Xvfb; elif command -v Xtigervnc >/dev/null 2>&1; then xs=Xtigervnc; fi
    fi
    case "$xs" in
        Xvfb)
            pv_need Xvfb
            setsid nohup Xvfb ":$n" -screen 0 800x800x24 -nolisten tcp >"$PREVIEW_WORK/xserver.log" 2>&1 </dev/null &
            ;;
        Xtigervnc)
            pv_need Xtigervnc
            setsid nohup Xtigervnc ":$n" -geometry 800x800 -depth 24 -SecurityTypes None -rfbport -1 -localhost=1 \
                >"$PREVIEW_WORK/xserver.log" 2>&1 </dev/null &
            ;;
        '') pv_die "无桌面运行需要 Xvfb 或 Xtigervnc，请先安装：$PREVIEW_APT" ;;
        *) pv_die "PREVIEW_XSERVER 只能是 Xvfb 或 Xtigervnc：$xs" ;;
    esac
    PV_XPID=$!
    export DISPLAY=":$n"
    for _ in $(seq 50); do
        xdotool getdisplaygeometry >/dev/null 2>&1 && break
        sleep 0.2
    done
fi
PV_DISPLAY=$DISPLAY
if ! xdotool getdisplaygeometry >/dev/null 2>&1; then
    pv_die "连不上显示 $PV_DISPLAY（无桌面时加 --headless），X 服务器日志见 $PREVIEW_WORK/xserver.log"
fi

# 启动前已有的同名窗口（手动启动或上次遗留的 preview），退回找窗口时排除
# 没有同名窗口时 xdotool search 以 1 退出，在 set -e 与 pipefail 下要吞掉
before=" $({ xdotool search --name "$PREVIEW_TITLE" 2>/dev/null || true; } | tr '\n' ' ') "
PREVIEW_RES=$PV_RES TMPDIR="$PREVIEW_WORK/tmp" PREVIEW_WIFI_DIR="$PREVIEW_WORK/wifi" PATH="$PREVIEW_DIR/bin:$PATH" \
    setsid nohup "$PREVIEW_BUILD/preview" >"$PREVIEW_WORK/preview.log" 2>&1 </dev/null &
PV_PID=$!
PV_WIN=
PV_TMP=
# 窗口先于界面创建；临时目录 luckfox-tests-* 在界面初始化（亮度控件找背光目录）时才创建，两者都等到为止。
# 最多 30 s：CI runner 上曾有一次 15 s 内窗口还没出现。窗口按 _NET_WM_PID 与名字同时匹配（--all，xdotool 默认是「或」）；
# 找不到时只考虑启动后新出现的同名窗口，恰好一个时改用它，并打印两个 PID，便于查明原因
t0=$SECONDS
end=$((SECONDS + 30))
while [ "$SECONDS" -lt "$end" ]; do
    if [ -z "$PV_WIN" ]; then
        PV_WIN=$(xdotool search --all --pid "$PV_PID" --name "$PREVIEW_TITLE" 2>/dev/null | head -1) || true
        if [ -z "$PV_WIN" ]; then
            wins=$(for w in $(xdotool search --name "$PREVIEW_TITLE" 2>/dev/null); do
                case "$before" in *" $w "*) ;; *) echo "$w" ;; esac
            done)
            if [ -n "$wins" ] && [ "$(echo "$wins" | wc -l)" -eq 1 ]; then
                PV_WIN=$wins
                echo "start.sh: 按 _NET_WM_PID 找不到窗口，改用启动后新出现的唯一同名窗口 $PV_WIN（窗口 PID $(xdotool getwindowpid "$PV_WIN" 2>/dev/null || echo 无)，preview PID $PV_PID）" >&2
            fi
        fi
    fi
    PV_TMP=$(find "$PREVIEW_WORK/tmp" -mindepth 1 -maxdepth 1 -type d -name 'luckfox-tests-*' | head -1)
    [ -n "$PV_WIN" ] && [ -n "$PV_TMP" ] && break
    # 刚 fork 时进程名还不是 preview，这里只判断进程是否存在
    kill -0 "$PV_PID" 2>/dev/null || break
    sleep 0.2
done
{
    printf 'PV_RES=%q\nPV_DISPLAY=%q\nPV_PID=%q\nPV_XPID=%q\nPV_WIN=%q\nPV_TMP=%q\n' \
        "$PV_RES" "$PV_DISPLAY" "$PV_PID" "$PV_XPID" "$PV_WIN" "$PV_TMP"
} >"$PREVIEW_WORK/session"
if [ -z "$PV_WIN" ] || [ -z "$PV_TMP" ] || ! pv_alive "$PV_PID"; then
    {
        echo "== 诊断：窗口 [$PV_WIN]，临时目录 [$PV_TMP]，preview PID $PV_PID"
        ps -o pid,ppid,pgid,stat,etime,comm -p "$PV_PID" || echo "preview 进程已不存在"
        for w in $(xdotool search --name "$PREVIEW_TITLE" 2>/dev/null); do
            echo "同名窗口 $w：$(xdotool getwindowname "$w" 2>/dev/null)，PID $(xdotool getwindowpid "$w" 2>/dev/null || echo 无)"
        done
        tail -n 5 "$PREVIEW_WORK/preview.log"
    } >&2 || true
    pv_die "30 s 内没有等到 preview 窗口与临时目录，日志见 $PREVIEW_WORK/preview.log"
fi
# tst_app_setup_ui()（tests/support/app_env.c）先运行 1.5 s 才进入 SDL 事件循环，主屏在此期间已画出；
# 这时的点击，按下与抬起在同一轮事件循环中处理，LVGL 看不到按下。看到主屏后再等 2 s
pv_wait_page main
sleep 2
mkdir -p "$PV_TMP/music"
"$PREVIEW_DIR/songs.sh" "$songs" >/dev/null
started=1
echo "preview 已启动：DISPLAY=$PV_DISPLAY，$PV_RES 屏，窗口 $PV_WIN（等了 $((SECONDS - t0)) s），$songs 首歌，假 WiFi 为连不上；会话目录 $PREVIEW_WORK"
