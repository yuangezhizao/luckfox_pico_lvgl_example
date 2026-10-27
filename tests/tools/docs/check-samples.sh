#!/bin/bash
# shellcheck disable=SC2016
# 上一行对整个文件生效：样例中的反引号是 Markdown 行内代码，有意写在单引号中、按字面传给 printf，不需要展开。
# check-headings.py 的改坏与正常样例（PR #11 spec Q16–Q34 中 Codex 指出过的写法）：坏样例应退出 1，正常样例应退出 0。
# PR #11 spec：docs/superpowers/specs/2026-10-14-lvgl-example-heading-numbers-design.md；规则见 AGENTS.md「文档（spec 与 plan）」一节。
# 用法：bash check-samples.sh [check-headings.py 路径，默认与本脚本同目录]；全部符合时最后一行为「样例 N 个，不符合 0 个」。
# mk 样例改写下面 S0、P724、P1001、P1010 四份历史文档并断言被替换的原文存在；改动这些原文时要同步调整样例。
set -u
# 下面要 cd 到临时目录：检查器路径先转成绝对路径；历史文档取自本脚本所在仓库（tests/tools/docs/ 上三级），与当前目录无关
D=$(cd "$(dirname "$0")" && pwd) || exit 2; W=$(cd "$D/../../.." && pwd) || exit 2
CHK=$(realpath "${1:-$D/check-headings.py}") || exit 2; T=$(mktemp -d) || exit 2; trap 'rm -rf "$T"' EXIT; cd "$T" || exit 2
S0=$W/docs/superpowers/specs/2026-10-10-lvgl-example-board-test-followups-design.md
P724=$W/docs/superpowers/plans/2026-07-24-lvgl-example-cloudagent-env.md
P1001=$W/docs/superpowers/plans/2026-10-01-lvgl-example-fix-existing-defects.md
P1010=$W/docs/superpowers/plans/2026-10-10-lvgl-example-board-test-followups.md
# 生成失败（原文不存在、断言不成立）时以 2 退出，避免少生成样例却报全部符合
mk() { cp "$1" "bad-$2.md" || exit 2; python3 - "bad-$2.md" "$3" "$4" <<'EOF' || exit 2
import sys
p, a, b = sys.argv[1:]
s = open(p, encoding="utf-8").read()
assert a in s, (p, a)
open(p, "w", encoding="utf-8").write(s.replace(a, b, 1))
EOF
}
bad() { printf '%b' "$2" > "bad-$1.md"; }
ok() { printf '%b' "$2" > "ok-$1.md"; }
# Q16 ####末级；Q17 Task 号；Q19 引用不存在的 Task；Q24 章标题 Task 范围
mk "$S0" q16a '#### 2.2.1 ' '#### 2.2.99 '; mk "$S0" q16b '#### 2.2.2 ' '#### 2.2.1 '
mk "$S0" q16c '#### 2.2.1 ' '#### 2.3.1 '; mk "$S0" q16d '#### 2.2.1 F1' '#### F1'
mk "$P724" q17a '### Task 2：' '### Task 99：'; mk "$P724" q17b '### Task 2：' '### Task foo：'
mk "$P724" q17c '### Task 2：' '### Task 1：'; mk "$P724" q17d '### Task 1：' '### Task 2：'
mk "$P724" q17e '### Task 2：' '### Task 2b：'; mk "$P724" q17f '### Task 7：' '### Task 8：'
mk "$P1010" q19a '（Task 2 起）' '（Task 99 起）'; mk "$P1010" q19b '| 13 | 音乐 [7/10]' '| 21 | 音乐 [7/10]'
mk "$P1001" q24a '（Task 1–5）' '（Task 1–99）'; mk "$P1001" q24b '（Task 1–5）' '（Task 1–4）'
mk "$P1001" q24c '（Task 31–32）' '（Task 31–33）'; mk "$P1001" q24d '## 11. 退出（Task 30）' '## 11. 退出（Task 29）'
# Q25 #### 父节；Q26 ### 父章、缩进与 Setext；Q27 空标题；Q28 容器内标题与未闭合代码块；Q29 引用块、列表内的围栏；Q30 无前导竖线与列表内的表格、多个反引号的行内代码；Q31 不成对的反引号、转义反引号与带标记的表头；Q32 多位数的附录编号；Q33 原始 HTML 中的标题与文字；Q34 跨标签、跨行与不换行空格的引用
bad q25a '# T\n\n## 1. a\n\n#### 1.0.1 y\n'; bad q25b '# T\n\n## 附录 A\n\n#### A.0.1 b\n'
bad q25c '# T\n\n## 1. a\n\n### 1.1 x\n\n#### 1.1.1 y\n\n## 2. b\n\n#### 2.1.1 z\n'; bad q25d '## 1. Tasks\n\n### Task 1：x\n\n#### 1.0.1 y\n'
bad q26a '# T\n\n### 0.1 x\n'; bad q26b '# T\n\n## 缺号\n\n### 0.1 x\n'; bad q26c '# T\n\n  ## 缩进的章\n'
bad q26d '# T\n\n正文段落\n---\n\n## 1. a\n'; bad q26e '# T\n\n正文\n===\n'
bad q27a '# doc\n\n##\n'; bad q27b '# doc\n\n## #\n'; bad q27c '# doc\n\n## 1. a\n\n###\n'; bad q27d '# doc\n\n## 1. a\n\n####\n'
bad q28a '# doc\n\n> ## 未编号\n'; bad q28b '# doc\n\n- ## 未编号\n'; bad q28c '# doc\n\n## 1. a\n\n> ##### 太深\n'; bad q28d '# doc\n\n## 1. a\n\n```\ncode\n'
TK='# T\n\n## 1. Tasks\n\n### Task 1：a\n\n### Task 2：b\n\n## 2. 表\n\n'
bad q30a "${TK}Task | 说明\n--- | ---\n99 | x\n"; bad q30b "${TK}- 列表\n\n  | Task | 说明 |\n  |---|---|\n  | 99 | x |\n"
bad q30c '# T\n\n## 1. a\n\n## 附录 A\n\n### A.1 x\n\n编号 | 说明\n--- | ---\nA1 | y\n'
bad q31a "$TK"'正文 ``Task 99``` 结尾\n'; bad q31b "$TK"'正文 \\`Task 99\\` 结尾\n'
bad q31c "$TK"'**Task** | 说明\n--- | ---\n99 | x\n'; bad q31d "$TK"'[Task](x) | 说明\n--- | ---\n99 | x\n'
AP='# T\n\n## 1. a\n\n## 附录 A\n\n### A.1 x\n\n'
bad q32a "$AP"'| 编号 | 说明 |\n|---|---|\n| A10 | y |\n'; bad q32b "$AP"'见附录 A10。\n'
bad q33a '# T\n\n## 1. a\n\n<h2>未编号</h2>\n'; bad q33b '# T\n\n## 1. a\n\n<H5>太深</H5>\n'; bad q33c '# T\n\n## 1. a\n\n正文 <h2>行内</h2> 结尾\n'
bad q33d "$TK"'<details><summary>见 Task 99</summary>\n\n内容\n\n</details>\n'
bad q34a "$TK"'<details><summary>见 Task <strong>99</strong></summary>\n\n内容\n\n</details>\n'; bad q34b "$TK"'正文 Task\n99 跨行\n'
bad q34c "$TK"'正文 Task&nbsp;99\n'; bad q34d "$TK"'<p>Task&#32;99</p>\n'
bad q29a '# doc\n\n## 1. a\n\n> ```\n> 未闭合\n\n## 2. b\n'; bad q29b '# doc\n\n## 1. a\n\n- 列表\n\n  ~~~\n  未闭合\n'
ok 1 '# T\n\n## 1. a\n\n### 1.1 x\n\n#### 1.1.1 y\n\n## 附录 A\n\n### A.1 z\n\n#### A.1.1 w\n'
ok 2 '# doc\n\n## 1. a ##\n\n### 1.1 C#\n\n#### 1.1.1 x #\n'; ok 3 '# doc\n\n#5 不是标题\n\n## 1. a\n'
ok 4 '# doc\n\n    ## 缩进 4 格是代码\n\n## 1. a\n'; ok 5 '# T\n\n## 1. a\n\n- 列表\n\n---\n\n| a |\n|---|\n| b |\n'
ok 6 '# T\n\n## 1. a\n\n> 引用里的 Task 2 说明\n\n````md\n## 代码里\n```\n````\n'
ok 10 "${TK}Task | 说明\n--- | ---\n2 | x\n—  | 已删去\n"; ok 11 '# T\n\n## 1. a\n\n## 附录 A\n\n### A.1 x\n\n``旧写法 附录 A1 与 ` 号``\n'
ok 12 "$TK"'正文 `Task 99` 是行内代码里的旧写法\n'; ok 13 '# T\n\n## 1. `foo` 说明\n\n### 1.1 `bar`\n'
ok 14 "$AP"'| 编号 | 说明 |\n|---|---|\n| A.10 | y |\n\n见附录 A.10。\n'
ok 15 "$TK"'<!-- <h2>注释里的标题</h2> 与 Task 99 不渲染 -->\n\n第一行<br>第二行，见 Task 2\n'
ok 16 "$TK"'<p>见 Task <strong>2</strong></p>\n\n<!-- Task 99 -->\n\n<script>Task 99</script>\n'
ok 7 '# T\n\n## 1. a\n\n> ```python\n> x = 1\n> ```\n'; ok 8 '# T\n\n## 1. a\n\n- 列表\n\n  ```bash\n  echo 1\n  ```\n'; ok 9 '# T\n\n## 1. a\n\n~~~~\n```\n~~~\n~~~~~\n'
# QA 编号（PR #16 spec §3.3）：段首加粗的 Qn 须写成「Qn. 」且从 Q1 连续；代码块、行内代码与 **QA …** 不算
QP='# T\n\n## 1. QA\n\n'
bad qa1 "${QP}**Q1. a**\n\n**Q3. b**\n"; bad qa2 "${QP}**Q2. a**\n"; bad qa3 "${QP}**Q1. a**\n\n**Q1. b**\n"
bad qa4 "${QP}**Q1：a**\n"; bad qa5 "${QP}**Q1.a**\n"
ok 17 "${QP}**Q1. a**\n\n- 问：x\n\n**Q2. b**\n"; ok 18 "${QP}"'```\n**Q9. x**\n```\n'
ok 19 "${QP}"'`**Q9. x**` 是旧写法\n'; ok 20 "${QP}**QA 记录**\n\n**Quick** 说明\n"
n=0; miss=0
for f in bad-*.md ok-*.md; do
  python3 "$CHK" "$f" > /dev/null; rc=$?; n=$((n + 1))
  case $f in bad-*) want=1;; *) want=0;; esac
  [ "$rc" = "$want" ] || { miss=$((miss + 1)); echo "不符合：$f rc=$rc，应为 $want"; }
done
echo "样例 $n 个，不符合 $miss 个"
rm -rf "$T"
[ "$miss" = 0 ]
