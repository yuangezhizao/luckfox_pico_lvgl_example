#!/usr/bin/env bash
# 切换假 wpa_cli 的状态：wpa_state 取值（如 SCANNING、COMPLETED、DISCONNECTED）或 unreachable（连不上 wpa_supplicant）。
# WiFi 页每 5 s 查询一次，最多 5 s 后生效。不带参数时打印当前状态与最近 5 次调用。
# 用法：wifi.sh [<wpa_state>|unreachable]
set -euo pipefail
# shellcheck source=lib.sh
. "$(dirname "$0")/lib.sh"

pv_load
d="$PREVIEW_WORK/wifi"
case "${1:-}" in
    '')
        if [ -f "$d/state" ]; then echo "当前状态：$(cat "$d/state")"; else echo "当前状态：unreachable"; fi
        tail -n 5 "$d/cmd.log"
        ;;
    unreachable) rm -f "$d/state" ;;
    *[!A-Z0-9_]*) pv_die "状态只含大写字母、数字与下划线，或为 unreachable：$1" ;;
    *) echo "$1" >"$d/state" ;;
esac
