#!/usr/bin/env bash
# 结束本会话的 preview 与 start.sh 自起的 X 服务器；会话目录中的日志与截图保留。没有会话时什么也不做。
# 用法：stop.sh
set -euo pipefail
# shellcheck source=lib.sh
. "$(dirname "$0")/lib.sh"

[ -f "$PREVIEW_WORK/session" ] || exit 0
# shellcheck source=/dev/null
. "$PREVIEW_WORK/session"
if pv_alive "$PV_PID"; then
    kill "$PV_PID"
    for _ in $(seq 25); do
        pv_alive "$PV_PID" || break
        sleep 0.2
    done
    if pv_alive "$PV_PID"; then kill -9 "$PV_PID"; fi
fi
# 只结束 start.sh 自起的 X 服务器（PV_XPID 非空），不碰 Cloud Agent 桌面
if [ -n "$PV_XPID" ] && kill -0 "$PV_XPID" 2>/dev/null; then
    case "$(ps -o comm= -p "$PV_XPID")" in
        Xvfb | Xtigervnc) kill "$PV_XPID" ;;
    esac
fi
rm -f "$PREVIEW_WORK/session"
