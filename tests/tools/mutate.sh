#!/usr/bin/env bash
# 改坏检验：逐条应用 tests/cases/*/*.mutants 的变体，确认对应测试失败（KILLED）后还原。
# 用法：tests/tools/mutate.sh [变体 id...]；工作目录默认 /tmp/luckfox-mutate，可用 MUTATE_WORK 覆盖。
# 退出码：0 = 全部 KILLED；1 = 有变体 SURVIVED；2 = 工具或清单错误（工作目录不合法、custom/ 含符号链接、基线不绿、构建失败、清单不合法、id 不存在、一个变体都没跑）。
set -Eeuo pipefail
trap 'exit 2' ERR
REPO=$(cd "$(dirname "$0")/../.." && pwd)
JOBS=$(nproc)

mutants() {
    python3 - "$REPO" "$@" <<'PY'
import glob
import os
import re
import sys

repo, action = sys.argv[1], sys.argv[2]


def fail(message):
    print("mutate.sh: " + message, file=sys.stderr)
    sys.exit(2)


required = ("id", "file", "old", "new", "tests")
records = []
for path in sorted(glob.glob(os.path.join(repo, "tests/cases/*/*.mutants"))):
    record = {}
    for number, raw in enumerate(open(path, encoding="utf-8").read().split("\n") + [""], 1):
        if raw.startswith("#"):
            continue
        if not raw.strip():
            if record:
                records.append(record)
                record = {}
            continue
        key, sep, value = raw.partition(":")
        if not sep or key not in required:
            fail("%s:%d: bad line: %r" % (path, number, raw))
        if key in record:
            fail("%s:%d: duplicate key %s in record (blank line between records missing?)" % (path, number, key))
        value = value[1:] if value.startswith(" ") else value
        record[key] = value.replace("\\n", "\n")
        record.setdefault("_where", "%s:%d" % (path, number))
    if record:
        records.append(record)
seen = set()
for r in records:
    missing = [k for k in required if k not in r]
    if missing:
        fail("%s: record missing %s" % (r["_where"], ", ".join(missing)))
    if not r["id"] or not r["tests"] or not r["old"]:
        fail("%s: id, tests and old must not be empty" % r["_where"])
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.-]*", r["id"]):
        fail("%s: id must match [A-Za-z0-9][A-Za-z0-9_.-]*: %s" % (r["_where"], r["id"]))
    if r["id"] in seen:
        fail("%s: duplicate id %s" % (r["_where"], r["id"]))
    seen.add(r["id"])
if action == "list":
    for r in records:
        print("\t".join([r["id"], r["file"], r["tests"]]))
    sys.exit(0)
root, wanted = sys.argv[3], sys.argv[4]
r = next((r for r in records if r["id"] == wanted), None)
if r is None:
    fail("unknown id " + wanted)
target = os.path.join(root, r["file"])
text = open(target, encoding="utf-8").read()
count = text.count(r["old"])
if count != 1:
    fail("%s: old must match exactly once in %s, found %d" % (wanted, r["file"], count))
open(target, "w", encoding="utf-8").write(text.replace(r["old"], r["new"]))
PY
}

# 启动时会 rm -rf "$SRC"：拒绝 / 与仓库内路径、以及会让仓库落在 $SRC 之内的路径，只使用不存在、为空或带本脚本标记文件（普通文件）的目录；此后一律使用规范化后的路径。
WORK=$(realpath -m -- "${MUTATE_WORK:-/tmp/luckfox-mutate}")
REPO_REAL=$(realpath -- "$REPO")
if [[ $WORK == / || $WORK == "$REPO_REAL" || $WORK == "$REPO_REAL"/* ]]; then
    echo "MUTATE_WORK must not be / or inside the repository: $WORK"
    exit 2
fi
SRC=$WORK/src
BUILD=$WORK/build
if [[ $REPO_REAL == "$SRC" || $REPO_REAL == "$SRC"/* ]]; then
    echo "MUTATE_WORK/src must not contain the repository: $SRC"
    exit 2
fi
MARKER=$WORK/.luckfox-mutate-workdir
if [[ -d $WORK && ! ( -f $MARKER && ! -L $MARKER ) ]]; then
    entries=$(find "$WORK" -mindepth 1 -maxdepth 1 -print -quit)
    if [[ -n $entries ]]; then
        echo "MUTATE_WORK is not empty and was not created by this script: $WORK (remove it or choose another MUTATE_WORK)"
        exit 2
    fi
fi
mkdir -p "$WORK"
touch "$MARKER"
LIST=$WORK/mutants.list
if ! mutants list >"$LIST"; then
    echo "mutants list failed: cannot parse tests/cases/*/*.mutants"
    exit 2
fi
for want in "$@"; do
    if ! grep -qxF -- "$want" < <(cut -f1 "$LIST"); then
        echo "unknown mutant id: $want"
        exit 2
    fi
done

rm -rf "$SRC"
mkdir -p "$SRC"
for d in lib generated include src; do
    ln -s "$REPO/$d" "$SRC/$d"
done
# 变体会写回副本里的文件，副本中的符号链接会把写入带回仓库。
if [[ -n $(find "$REPO/custom" -type l -print -quit) ]]; then
    echo "custom/ must not contain symlinks: mutants are written through the copy"
    exit 2
fi
cp -R "$REPO/custom" "$SRC/custom"

cmake -S "$REPO/tests" -B "$BUILD" -DLUCKFOX_ROOT="$SRC" >/dev/null </dev/null
cmake --build "$BUILD" -j"$JOBS" >/dev/null </dev/null
if ! ctest --test-dir "$BUILD" -j"$JOBS" >"$WORK/baseline.log" 2>&1 </dev/null; then
    echo "baseline not green, see $WORK/baseline.log"
    exit 2
fi

survived=0
ran=0
skipped=0
while IFS=$'\t' read -r id file tests; do
    if [[ $# -gt 0 && " $* " != *" $id "* ]]; then
        continue
    fi
    if [[ $file != custom/* || $file == *..* ]]; then
        echo "[$id] file must be under custom/ and must not contain '..': $file"
        exit 2
    fi
    listing=$(ctest --test-dir "$BUILD" -N -R "$tests" </dev/null)
    if ! grep -q 'Total Tests: [1-9]' <<<"$listing"; then
        echo "[$id] SKIPPED: no test matches $tests"
        skipped=$((skipped + 1))
        continue
    fi
    if ! mutants apply "$SRC" "$id"; then
        echo "[$id] cannot apply mutant"
        exit 2
    fi
    if ! cmake --build "$BUILD" -j"$JOBS" >"$WORK/$id.build.log" 2>&1 </dev/null; then
        echo "[$id] BUILD FAILED, see $WORK/$id.build.log"
        exit 2
    fi
    ran=$((ran + 1))
    if ctest --test-dir "$BUILD" -R "$tests" -j"$JOBS" >"$WORK/$id.log" 2>&1 </dev/null; then
        echo "[$id] SURVIVED"
        survived=1
    else
        echo "[$id] KILLED by: $(grep -E '^[[:space:]]+[0-9]+ - ' "$WORK/$id.log" | sed -E 's/^[[:space:]]+[0-9]+ - //' | tr '\n' ' ')"
    fi
    cp "$REPO/$file" "$SRC/$file"
done <"$LIST"
cmake --build "$BUILD" -j"$JOBS" >/dev/null </dev/null
echo "$ran run, $skipped skipped"
if [[ $ran -eq 0 ]]; then
    echo "no mutant was run"
    exit 2
fi
exit $survived
