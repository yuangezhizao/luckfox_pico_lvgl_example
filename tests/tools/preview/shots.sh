#!/usr/bin/env bash
# 截主屏、WiFi 页、点 Scan 后与画板（轻点 3 下、拖一笔）各一张，结束后回到主屏（PR #10 plan 附录 C.5 的 preview_run.sh）。
# 先把假 WiFi 设为连不上，点 Scan 后出现红色提示 wpa_supplicant not reachable。截图在 $PREVIEW_WORK/shots/<RES>-shot-*.png。
# 用法：shots.sh（preview 已由 start.sh 启动并停在主屏）。
set -euo pipefail
# shellcheck source=lib.sh
. "$(dirname "$0")/lib.sh"

pv_need xdotool import convert
pv_load

# has_content 宽 高 x y：区域（480 设计坐标）内出现内容
has_content() {
    [ "$(pv_colors "$(pv_shot shot-wait)" "$@")" -gt 1 ]
}

pv_wait_page main
"$PREVIEW_DIR/wifi.sh" unreachable
pv_shot shot-main >/dev/null
pv_tap 50 410 # WIFI
pv_wait_page wifi
pv_shot shot-wifi >/dev/null
pv_tap 65 400 # Scan
# 红色提示位于 (150, 247)–(394, 261)
pv_wait 10 has_content 250 20 148 244 || pv_die "10 s 内没有出现 Scan 的提示"
pv_shot shot-wifi-scan >/dev/null
pv_tap 410 400 # Back
pv_wait_page main
pv_tap 145 410 # PAD
pv_wait_page pad
for x in 120 240 360; do
    pv_tap "$x" 100
    sleep 0.3
done
pv_drag 100 220 380 300
pv_wait 5 has_content 20 20 230 250 || pv_die "5 s 内画板上没有出现笔迹"
pv_shot shot-pad >/dev/null
pv_tap 384 400 # Back
pv_wait_page main
rm -f "$PREVIEW_WORK/shots/$PV_RES-shot-wait.png"
ls "$PREVIEW_WORK/shots/$PV_RES"-shot-*.png
