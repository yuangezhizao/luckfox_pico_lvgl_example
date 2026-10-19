# shellcheck shell=bash
# preview 脚本的公共函数，由同目录的脚本 source，不单独运行。用法见 tests/README.md「preview 脚本」。
# 退出码约定：0 成功；1 核对不符；2 环境或用法错误（缺工具、preview 未运行等）。

PREVIEW_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
PREVIEW_REPO=$(cd "$PREVIEW_DIR/../../.." && pwd)
PREVIEW_WORK=${PREVIEW_WORK:-/tmp/luckfox-preview}
PREVIEW_BUILD=${PREVIEW_BUILD:-$PREVIEW_REPO/build-tests}
PREVIEW_APT='sudo apt-get install -y imagemagick xdotool xvfb'

pv_die() {
    echo "$(basename "$0"): $*" >&2
    exit 2
}

# pv_need 命令...：缺任何一个即退出，并提示安装命令。
pv_need() {
    local c missing=()
    for c in "$@"; do
        command -v "$c" >/dev/null 2>&1 || missing+=("$c")
    done
    [ ${#missing[@]} -eq 0 ] || pv_die "缺少 ${missing[*]}，请先安装：$PREVIEW_APT"
}

# 读入 start.sh 写的会话文件：PV_RES、PV_DISPLAY、PV_PID、PV_XPID、PV_WIN、PV_TMP。
pv_load() {
    [ -f "$PREVIEW_WORK/session" ] || pv_die "preview 未运行，先执行 $PREVIEW_DIR/start.sh"
    # shellcheck source=/dev/null
    . "$PREVIEW_WORK/session"
    export DISPLAY=$PV_DISPLAY
    pv_alive "$PV_PID" || pv_die "preview 已退出，日志见 $PREVIEW_WORK/preview.log"
}

# pv_alive pid：进程存在且名字是 preview（防止 pid 被复用）。
pv_alive() {
    [ -n "$1" ] && [ "$(ps -o comm= -p "$1" 2>/dev/null)" = preview ]
}

# start.sh 会删除会话目录中的 tmp/、wifi/，先确认目录归本脚本所有（做法同 tests/tools/mutate.sh）：
# 拒绝 / 与仓库内的路径，只使用不存在、为空或带标记文件 .luckfox-preview-workdir（普通文件）的目录，首次使用时创建标记；
# 此后 PREVIEW_WORK 为规范化后的路径。
pv_prepare_work() {
    local work repo marker
    work=$(realpath -m -- "$PREVIEW_WORK")
    repo=$(realpath -- "$PREVIEW_REPO")
    if [ "$work" = / ] || [ "$work" = "$repo" ] || [ "${work#"$repo"/}" != "$work" ]; then
        pv_die "PREVIEW_WORK 不能是 / 或仓库内的路径：$work"
    fi
    if [ -e "$work" ] && [ ! -d "$work" ]; then
        pv_die "PREVIEW_WORK 不是目录：$work"
    fi
    marker="$work/.luckfox-preview-workdir"
    if [ -d "$work" ] && ! { [ -f "$marker" ] && [ ! -L "$marker" ]; } &&
        [ -n "$(find "$work" -mindepth 1 -maxdepth 1 -print -quit)" ]; then
        pv_die "PREVIEW_WORK 不为空，也不是本脚本创建的：$work（删除它或换一个目录）"
    fi
    mkdir -p "$work"
    touch "$marker"
    PREVIEW_WORK=$work
}

# 480 设计坐标按 RES/480 换算，截断取整，与 custom/custom_main.c 的 luckfox_lv_obj_set_pos() 相同。
pv_sc() {
    echo $(($1 * PV_RES / 480))
}

# pv_click_px x y：按窗口内像素坐标点击；按住约 150 ms 再抬起，否则点击可能被丢掉（tests/README.md「手动目标」）。
pv_click_px() {
    xdotool mousemove --window "$PV_WIN" "$1" "$2" mousedown 1
    sleep 0.15
    xdotool mouseup 1
}

# pv_tap x y：按 480 设计坐标点击。
pv_tap() {
    pv_click_px "$(pv_sc "$1")" "$(pv_sc "$2")"
}

# pv_drag x0 y0 x1 y1：按 480 设计坐标拖动，分 20 步移动。
pv_drag() {
    local i x0 y0 x1 y1
    x0=$(pv_sc "$1") y0=$(pv_sc "$2") x1=$(pv_sc "$3") y1=$(pv_sc "$4")
    xdotool mousemove --window "$PV_WIN" "$x0" "$y0" mousedown 1
    sleep 0.15
    for i in $(seq 1 20); do
        xdotool mousemove --window "$PV_WIN" $((x0 + (x1 - x0) * i / 20)) $((y0 + (y1 - y0) * i / 20))
        sleep 0.03
    done
    xdotool mouseup 1
}

# pv_shot 名字：截取窗口到 shots/<RES>-<名字>.png，打印文件路径。
pv_shot() {
    local f="$PREVIEW_WORK/shots/$PV_RES-$1.png"
    mkdir -p "$PREVIEW_WORK/shots"
    import -window "$PV_WIN" "$f"
    echo "$f"
}

# pv_pixel 文件 x y：窗口内像素坐标处的颜色，如 srgb(255,224,168)。
pv_pixel() {
    convert "$1" -format "%[pixel:p{$2,$3}]" info:
}

# pv_colors 文件 宽 高 x y：区域内的颜色数（480 设计坐标）；1 表示区域内没有内容。
pv_colors() {
    convert "$1" -crop "$(pv_sc "$2")x$(pv_sc "$3")+$(pv_sc "$4")+$(pv_sc "$5")" +repage -format %k info:
}

# pv_wait 超时秒 命令...：反复执行命令（命令自行截图判断），连续两次成功才返回 0，超时返回 1。
# 要求连续两次，避免在动画中途碰巧满足条件。
pv_wait() {
    local end=$((SECONDS + $1)) ok=0
    shift
    while :; do
        if "$@"; then
            ok=$((ok + 1))
            [ "$ok" -ge 2 ] && return 0
        else
            ok=0
        fi
        [ "$SECONDS" -lt "$end" ] || return 1
        sleep 0.2
    done
}

# 当前页面：按 (230, 400) 与 (384, 400) 两点的颜色区分主屏、WiFi 页与画板，其余为 other。
pv_page() {
    local f a b
    f=$(pv_shot page)
    a=$(pv_pixel "$f" "$(pv_sc 230)" "$(pv_sc 400)")
    b=$(pv_pixel "$f" "$(pv_sc 384)" "$(pv_sc 400)")
    case "$a/$b" in
        'srgb(255,150,0)/srgb(255,255,255)') echo main ;;
        'srgb(255,255,255)/srgb(151,151,152)') echo wifi ;;
        'srgb(255,150,0)/srgb(151,151,152)') echo pad ;;
        *) echo other ;;
    esac
}

pv_on_page() {
    [ "$(pv_page)" = "$1" ]
}

# pv_wait_page 页面：最多等 10 s，不在该页面时退出码 2。
pv_wait_page() {
    pv_wait 10 pv_on_page "$1" || pv_die "10 s 内没有进入 $1 页面（当前 $(pv_page)），截图见 $PREVIEW_WORK/shots/$PV_RES-page.png"
}
