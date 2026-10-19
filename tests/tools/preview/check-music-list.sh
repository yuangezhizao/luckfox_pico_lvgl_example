#!/usr/bin/env bash
# 核对音乐页列表面板的位置与列表按钮的高亮（PR #10 plan 附录 C.5 的 pv-check.sh）：放 4 首，进音乐页，
# 依次展开、点背景、再展开、按按钮收起，每步轮询截图，直到按钮中心颜色与面板底边符合预期；结束后回到主屏。
# 面板底边：面板左侧一列由上往下最后一次颜色变化的行，收起时为空。截图在 $PREVIEW_WORK/shots/<RES>-music-*.png。
# 用法：check-music-list.sh（preview 已由 start.sh 启动并停在主屏）；退出码 0 符合，1 不符，2 环境错误。
set -euo pipefail
# shellcheck source=lib.sh
. "$(dirname "$0")/lib.sh"

pv_need xdotool import convert
pv_load
OPEN='srgb(255,224,168)'   # 展开：橙色线条与白底的过渡色
CLOSED='srgb(182,176,187)' # 收起
case "$PV_RES" in
    480) EDGE=305 ;;
    720) EDGE=458 ;;
esac
# 列表按钮 32×32 不缩放：中心为缩放后的左上角 (139, 310) + 16（spec §2.4）
LX=$(($(pv_sc 139) + 16))
LY=$(($(pv_sc 310) + 16))
COL=$(pv_sc 60)
Y0=$(pv_sc 295)
Y1=$(pv_sc 312)

measure() {
    local f
    f=$(pv_shot "music-$1")
    btn=$(pv_pixel "$f" "$LX" "$LY")
    # 取 COL 列 Y0..Y1 的颜色，记下最后一次变化的行
    edge=$(convert "$f" -crop "1x$((Y1 - Y0 + 1))+$COL+$Y0" +repage -depth 8 txt:- |
        awk -v y0="$Y0" 'NR > 1 { split($1, p, /[,:]/); if (NR > 2 && $3 != prev) e = y0 + p[2]; prev = $3 } END { print e }')
}

expect() {
    measure "$1"
    [ "$btn" = "$2" ] && [ "$edge" = "$3" ]
}

step() {
    if pv_wait 10 expect "$@"; then
        echo "$PV_RES $1 按钮中心=$btn 面板底边=${edge:-无}"
    else
        echo "$PV_RES $1 不符：按钮中心=$btn 面板底边=${edge:-无}；预期 $2、${3:-无}；截图 $PREVIEW_WORK/shots/$PV_RES-music-$1.png" >&2
        exit 1
    fi
}

pv_wait_page main
"$PREVIEW_DIR/songs.sh" 4 >/dev/null
pv_tap 240 410 # 主屏 MUSIC
step enter "$CLOSED" ""
pv_click_px "$LX" "$LY"
step open "$OPEN" "$EDGE"
pv_tap 420 160 # 背景空白处
step bg "$CLOSED" ""
pv_click_px "$LX" "$LY"
step reopen "$OPEN" "$EDGE"
pv_click_px "$LX" "$LY"
step btnclose "$CLOSED" ""
pv_click_px 32 32 # 返回：64×64 不缩放，位于 (0, 0)
pv_wait_page main
echo "$PV_RES check-music-list 通过"
