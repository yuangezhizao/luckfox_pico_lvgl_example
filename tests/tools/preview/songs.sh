#!/usr/bin/env bash
# 把 preview 的音乐目录改成 01..N 首（N=0 为清空）；之后在窗口里返回主屏再点 MUSIC 即可刷新。
# 用法：songs.sh <歌曲数>
set -euo pipefail
# shellcheck source=lib.sh
. "$(dirname "$0")/lib.sh"

n=${1:-}
case "$n" in '' | *[!0-9]*) pv_die "用法：songs.sh <歌曲数>" ;; esac
pv_load
m="$PV_TMP/music"
mkdir -p "$m"
find "$m" -maxdepth 1 -name '*.mp3' -delete
for i in $(seq 1 "$n"); do
    # 长文件名、共同前缀，用于核对列表的省略显示（PR #10）
    : >"$m/$(printf 'luckfox_roller_clip_test_long_common_prefix_abcdefghijklmnopq_%02d.mp3' "$i")"
done
echo "当前歌曲数：$(find "$m" -maxdepth 1 -name '*.mp3' | wc -l)"
