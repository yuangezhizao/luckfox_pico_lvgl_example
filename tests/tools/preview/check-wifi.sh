#!/usr/bin/env bash
# 核对 WiFi 页圆环与连接图标（PR #13 plan 附录 A.4 的 pv-wifi-check.sh）：连不上时进入 WiFi 页，
# 依次切到 SCANNING、COMPLETED、连不上，每步轮询截图，直到圆环区与图标区的颜色数符合预期；结束后回到主屏。
# 颜色数为 1 表示区域内没有内容；图标区只取图标下半部分，上半部分在 480 屏与右上角的性能监视器重叠。
# 截图在 $PREVIEW_WORK/shots/<RES>-wifi-*.png。
# 用法：check-wifi.sh（preview 已由 start.sh 启动并停在主屏）；退出码 0 符合，1 不符，2 环境错误。
set -euo pipefail
# shellcheck source=lib.sh
. "$(dirname "$0")/lib.sh"

pv_need xdotool import convert
pv_load
log="$PREVIEW_WORK/wifi/cmd.log"

measure() {
    local f
    f=$(pv_shot "wifi-$1")
    spinner=$(pv_colors "$f" 90 90 25 20)
    # 连接图标 64×64 不缩放，左上角为缩放后的 (375, 15)；取下半部分 64×34
    icon=$(convert "$f" -crop "64x34+$(pv_sc 375)+$(($(pv_sc 15) + 30))" +repage -format %k info:)
}

# match 实际值 规则：规则为 1 或 '>1'
match() {
    case "$2" in
        1) [ "$1" -eq 1 ] ;;
        '>1') [ "$1" -gt 1 ] ;;
    esac
}

expect() {
    measure "$1"
    match "$spinner" "$2" && match "$icon" "$3"
}

step() {
    "$PREVIEW_DIR/wifi.sh" "$1"
    if pv_wait 15 expect "$@"; then
        echo "$PV_RES $1 圆环区=$spinner 图标区=$icon"
    else
        echo "$PV_RES $1 不符：圆环区=$spinner 图标区=$icon；预期 $2、$3；截图 $PREVIEW_WORK/shots/$PV_RES-wifi-$1.png" >&2
        exit 1
    fi
}

called() {
    tail -n +"$((n0 + 1))" "$log" | grep -q 'wpa_cli status$'
}

pv_wait_page main
"$PREVIEW_DIR/wifi.sh" unreachable
n0=$(wc -l <"$log")
pv_tap 50 410 # 主屏 WIFI
# 进入 WiFi 页时立即查询状态（PR #13 WiFi [2/3]），不等 5 s 轮询
end=$((SECONDS + 2))
until called; do
    if [ "$SECONDS" -ge "$end" ]; then
        echo "$PV_RES 进入 WiFi 页 2 s 内没有调用 wpa_cli status" >&2
        exit 1
    fi
    sleep 0.1
done
pv_wait_page wifi
step unreachable 1 1
step SCANNING '>1' 1
step COMPLETED 1 '>1'
step unreachable 1 1
pv_tap 410 400 # Back
pv_wait_page main
echo "$PV_RES check-wifi 通过"
