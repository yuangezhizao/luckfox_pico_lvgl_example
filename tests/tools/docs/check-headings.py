#!/usr/bin/env python3
"""检查 spec 与 plan 的标题编号（spec §2.1 的规则）。
规则见 AGENTS.md「文档（spec 与 plan）」一节；本文件中的「spec §n」「spec Qn」指 PR #11 spec
docs/superpowers/specs/2026-10-14-lvgl-example-heading-numbers-design.md。
用法（仓库根目录）：check-headings.py [文件…]，默认检查 docs/superpowers/specs/*.md 与 plans/*.md；有问题时逐行打印并退出 1。
依赖 markdown-it-py（Ubuntu 24.04：apt install python3-markdown-it）：标题、代码块、Setext 写法、表格与行内代码都由 CommonMark 解析器识别（GFM 表格扩展），
引用、附录编号与标题文字都取自解析后的渲染文字，不再按原始行用正则判断（spec Q28、Q30、Q31）。
原始 HTML 中的 <h1>–<h6> 按错误报出（标题须写成带编号的 Markdown 标题）；HTML 块中注释以外的文字也参与引用与附录编号检查（spec Q33）。
引用与附录编号按渲染后的连续文字检查：同一段落内软换行、HTML 标签两侧的文字连起来看，空白（含 &nbsp;）按 HTML 规则合并为一个空格（spec Q34）。
规则：代码块（围栏与缩进）内不算标题；标题不得位于引用块或列表内；## n. 从 1 连续编号，### n.m、#### n.m.k 与所在章、节一致且末级从 1 连续编号，### 只能出现在有效的 ## n. 或 ## 附录 之下，#### 只能出现在有效的 ### n.m（附录为 ### A.m）之下；空标题（单独的 ## 等）按缺序号报错，不用 Setext 标题（=== / --- 下划线），
不超过四级；QA 条目（段落开头加粗的 Qn）写成 **Qn. …** 并从 Q1 逐个加 1（QA_EXEMPT 中的历史 spec 除外）；有 ## 附录 的文件中，代码块外不再出现「附录 A1」或表格首列为 A1 这类未加点的附录编号（行内代码与「」内的引述除外）；plan 的 ### Task N：… 用全角冒号，N 只用数字、从 0 或 1 起逐个加 1，所在章标题含「Task」且只放 Task；章标题写明 Task 范围（如「（Task 1–5）」）时，该章的 Task 恰为这一范围；有 Task 的文件中，正文与 Task 定义以外的标题中的「Task N」、首列为 Task 的表格只引用本文件存在的 Task（指其他文件的 Task 时写在行内代码或「」内，表格首列可为「—」）；## 附录 X 之后的 ### 写成 X.n，附录之后不再出现正文章。
"""
import glob
import os
import re
import sys
from html.parser import HTMLParser

from markdown_it import MarkdownIt

MD = MarkdownIt("commonmark").enable("table")

# QA 编号检查豁免的历史 spec（按文件名匹配，PR #16 spec §3.3），都早于 PR #11 写入 AGENTS.md 的 **Q1. … 格式：
# 前 4 份用「**Q1：」「**Q1（第 1 轮）：」；2026-10-06 有子编号（Q2b、Q7b、Q13（作者提问，…）.）；2026-10-10 沿用会话中的编号
#（Q2、Q4–Q8、Q13…）。重新编号会改错大量引用，不改这些文件
QA_EXEMPT = {
    "2026-09-21-lvgl-example-github-actions-ci-design.md": "旧写法",
    "2026-09-25-lvgl-example-luckfox-pico-ultra-w-wifi-detect-design.md": "旧写法",
    "2026-09-27-lvgl-example-brightness-slider-design.md": "旧写法",
    "2026-09-28-lvgl-example-native-tests-design.md": "旧写法",
    "2026-10-06-lvgl-example-cloudagent-align-luckfox-pico-design.md": "子编号",
    "2026-10-10-lvgl-example-board-test-followups-design.md": "会话编号",
}

files = sys.argv[1:] or sorted(glob.glob("docs/superpowers/specs/*.md") + glob.glob("docs/superpowers/plans/*.md"))
bad = 0


def report(f, i, msg, line):
    global bad
    bad += 1
    print(f"{f}:{i}: {msg}: {line}")


def rendered(inline, base, code):
    """inline 的渲染文字，按软换行、硬换行拆成 [(行号, 文字)]。行内代码是否存在由解析器按 CommonMark 判断；
    code=True 时保留行内代码的文字（标题），False 时剔除（引用与附录编号检查：行内代码是对旧写法的描述）。"""
    out, line, buf = [], base, []
    for c in inline.children or []:
        if c.type in ("softbreak", "hardbreak"):
            out.append((line, "".join(buf)))
            buf, line = [], line + 1
        elif c.type in ("text", "text_special", "image"):
            buf.append(c.content)
        elif c.type == "code_inline":
            buf.append(c.content if code else " ")
    out.append((line, "".join(buf)))
    return out


def chars(segments, sep):
    """把 [(行号, 文字)] 连成逐字带行号的序列，段与段之间插入 sep；空白（含不换行空格）按 HTML 规则合并为一个空格。"""
    out = []
    for k, (ln, piece) in enumerate(segments):
        for ch in (sep if k else "") + piece:
            if ch.isspace():
                if out and out[-1][0] == " ":
                    continue
                ch = " "
            out.append((ch, ln))
    return out


class HtmlText(HTMLParser):
    """HTML 块中浏览器会显示的文字：标签不产生文字，注释与 <script>、<style> 的内容不显示，字符引用按 HTML 解码。"""

    def __init__(self, base):
        super().__init__(convert_charrefs=True)
        self.base, self.hidden, self.segs, self.heads = base, 0, [], []

    def handle_starttag(self, tag, attrs):
        if re.fullmatch(r"h[1-6]", tag):
            self.heads.append(self.base + self.getpos()[0] - 1)
        self.hidden += tag in ("script", "style")

    def handle_endtag(self, tag):
        self.hidden -= tag in ("script", "style") and self.hidden > 0

    def handle_data(self, data):
        if not self.hidden:
            self.segs.append((self.base + self.getpos()[0] - 1, data))


for f in files:
    text_all = open(f, encoding="utf-8").read()
    toks = MD.parse(text_all)
    src = text_all.split("\n")
    heads = []   # (行号, 级别, 渲染后的标题文字, 是否 Setext, 是否在容器内)
    texts = []   # 每个段落、单元格、标题或 HTML 块的渲染文字，逐字带行号（剔除行内代码）：用于引用与附录编号检查
    tables = []  # 每张表：[(行号, 首列渲染文字, 是否表头行), …]；有无前导竖线、是否在列表内都由解析器识别
    row = None   # 当前表格行：[行号, 是否表头行, 是否已取首列]
    for k, t in enumerate(toks):
        if t.type in ("fence", "code_block") and t.map:
            # 已闭合的围栏占「开始行 + 内容行 + 结束行」；未闭合时没有结束行。按解析结果计行数，不看原始行，引用块、列表内的围栏也适用
            if t.type == "fence" and t.map[1] - t.map[0] - t.content.count("\n") < 2:
                report(f, t.map[0] + 1, "代码块未闭合", t.markup)
        elif t.type == "html_block" and t.map:
            h = HtmlText(t.map[0] + 1)
            h.feed(t.content)
            h.close()
            for ln in h.heads:
                report(f, ln, "原始 HTML 标题，应写成带编号的 Markdown 标题", src[ln - 1][:60])
            texts.append(chars(h.segs, ""))  # 标签两侧的文字直接相连，如「Task <strong>99</strong>」显示为「Task 99」
        elif t.type == "table_open":
            tables.append([])
        elif t.type == "tr_open":
            row = [t.map[0] + 1, toks[k - 1].type == "thead_open", False]
        elif t.type == "heading_open":
            line = t.map[0] + 1
            title = "".join(x for _, x in rendered(toks[k + 1], line, True)).strip()
            heads.append((line, int(t.tag[1]), title, t.markup[0] in "=-", t.level > 0))
            if not (t.tag == "h3" and re.match(r"Task \w+[:：]", title)):  # 只排除 Task 定义本身，章标题中的「（Task 1–5）」照常核对
                texts.append(chars(rendered(toks[k + 1], line, False), " "))
        elif t.type == "inline" and toks[k - 1].type in ("th_open", "td_open") and row:
            cell = rendered(t, row[0], False)
            if not row[2]:
                tables[-1].append((row[0], "".join(x for _, x in cell).strip(), row[1]))
                row[2] = True
            texts.append(chars(cell, " "))
        elif t.type == "inline" and toks[k - 1].type != "heading_open" and t.map:
            texts.append(chars(rendered(t, t.map[0] + 1, False), " "))  # 软换行渲染为空白，跨行的「Task\n99」也连起来看
    qa_want = 1  # 下一条 QA 应有的号
    for k, t in enumerate(toks):
        # QA 条目：段落以加粗开头、加粗文字以「Q 加数字」开头；须写成「Qn. …」且从 Q1 逐个加 1。代码块、行内代码不算
        if t.type != "paragraph_open" or os.path.basename(f) in QA_EXEMPT:
            continue
        ch = toks[k + 1].children or []
        while ch and ch[0].type == "text" and not ch[0].content:  # 加粗开头时解析器在前面放一个空的 text
            ch = ch[1:]
        if not ch or ch[0].type != "strong_open":
            continue
        bold = ""
        for c in ch[1:]:
            if c.type == "strong_close":
                break
            bold += c.content if c.type in ("text", "text_special", "code_inline") else " "  # 标题如「Q13. `…` 是否同步」
        q = re.match(r"Q(\d+)", bold)
        if not q:
            continue
        i = t.map[0] + 1
        if not re.match(r"Q\d+\. \S", bold):
            report(f, i, "QA 条目应写成 **Qn. …**", src[i - 1][:60])
        if int(q.group(1)) != qa_want:
            report(f, i, f"QA 编号应为 Q{qa_want}", src[i - 1][:60])
        qa_want = int(q.group(1)) + 1
    h2 = h3 = h4 = 0
    parent2 = False  # 当前是否在有效的 ## n. / ## 附录 之下，### 只能出现在其下
    parent3 = False  # 当前是否在有效的 ### n.m / ### A.m 之下，#### 只能出现在其下
    appendix = None          # 当前附录字母
    chap_task = chap_other = False
    chap_line = 0
    undotted = []
    letters = set()
    tasks = []
    refs = []
    for tbl in tables:
        task_col = bool(tbl) and tbl[0][2] and tbl[0][1] == "Task"  # 表头取渲染文字，**Task**、[Task](…) 也算
        for i, cell, is_head in tbl:
            u = re.fullmatch(r"([A-Z])\d+", cell)
            if u and not is_head:
                undotted.append((i, u.group(1), src[i - 1]))
            if task_col and not is_head and cell != "—":
                refs.append((i, cell, src[i - 1]))
    for cs in texts:
        txt = "".join(c for c, _ in cs)
        bare = re.sub(r"「[^」]*」", lambda m: " " * len(m.group(0)), txt)  # 「」内是对旧写法的描述，不算；按长度替换以保留行号对应
        for u in re.finditer(r"附录 ([A-Z])\d+", bare):
            i = cs[u.start()][1]
            undotted.append((i, u.group(1), src[i - 1]))
        for r in re.finditer(r"Task (\d+[a-z]?)((?:[、–]\d+[a-z]?)*)", bare):
            i = cs[r.start()][1]
            refs += [(i, x, src[i - 1]) for x in re.findall(r"\d+[a-z]?", r.group(1) + r.group(2))]
    chap_title = ""
    chap_ranges = []  # (行号, 章标题, 写明的 Task 范围, 该章实际的 Task)
    for t in toks:  # 行内 HTML（段落、表格单元格、标题中的标签）
        for c in t.children or []:
            if c.type == "html_inline" and re.match(r"<h[1-6]\b", c.content, re.I):
                report(f, t.map[0] + 1 if t.map else 0, "原始 HTML 标题，应写成带编号的 Markdown 标题", c.content[:60])
    for i, depth, text, setext, nested in heads:
        line = src[i - 1]
        if setext:
            report(f, i, "Setext 标题，应写成带编号的 ATX 标题", line[:60])
        if nested:
            report(f, i, "标题位于引用块或列表内", line[:60])
        if depth == 1:
            continue
        if depth > 4:
            report(f, i, "超过四级", line)
            continue
        if depth == 2:
            parent2 = parent3 = False
            if chap_task and chap_other:
                report(f, chap_line, "Task 与其他小节同章", "")
            chap_task = chap_other = False
            chap_line, chap_title = i, text
            rng = re.search(r"（Task (\d+)(?:–(\d+))?）", text)
            chap_ranges.append((i, line, (int(rng.group(1)), int(rng.group(2) or rng.group(1))) if rng else None, []))
            a = re.match(r"附录 ([A-Z])(：|$| )", text)
            if a:
                appendix, h3, h4 = a.group(1), 0, 0
                letters.add(appendix)
                parent2 = True
                continue
            if appendix:
                report(f, i, "附录之后出现正文章", line)
            n = re.match(r"(\d+)\. \S", text)
            if not n:
                report(f, i, "## 缺序号", line)
                continue
            if re.match(r"\d+\. 附录", text):
                report(f, i, "附录写成 ## 附录 A", line)
            if int(n.group(1)) != h2 + 1:
                report(f, i, f"章号不连续（上一章 {h2}）", line)
            h2, h3, h4 = int(n.group(1)), 0, 0
            parent2 = True
        elif depth == 3:
            h4 = 0
            parent3 = False
            if re.match(r"Task \w+[:：]", text):
                if appendix:
                    report(f, i, "附录内出现 Task", line)
                if re.match(r"Task \w+:", text):
                    report(f, i, "Task 标题用半角冒号", line)
                tasks.append((i, re.match(r"Task (\w+)", text).group(1), line))
                if chap_ranges:
                    chap_ranges[-1][3].append(tasks[-1][1])
                if not chap_task and "Task" not in chap_title:
                    report(f, i, "Task 所在章不是 Task 章", "## " + chap_title)
                chap_task = True
                continue
            chap_other = True
            if not parent2:
                report(f, i, "### 前没有有效的 ## 父章", line)
                continue
            if appendix:
                n = re.match(rf"{appendix}\.(\d+) \S", text)
                if not n or int(n.group(1)) != h3 + 1:
                    report(f, i, f"附录小节应为 {appendix}.{h3 + 1}", line)
                    continue
                h3, parent3 = int(n.group(1)), True
                continue
            n = re.match(r"(\d+)\.(\d+) \S", text)
            if not n or int(n.group(1)) != h2 or int(n.group(2)) != h3 + 1:
                report(f, i, f"### 应为 {h2}.{h3 + 1}", line)
                continue
            h3, h4, parent3 = int(n.group(2)), 0, True
        else:
            if not parent3:
                report(f, i, "#### 前没有有效的 ### 父节", line)
                continue
            h4 += 1
            if appendix:
                n = re.match(rf"{appendix}\.(\d+)\.(\d+) \S", text)
                want, label = (h3, h4), f"{appendix}.{h3}.{h4}"
            else:
                n = re.match(r"(\d+)\.(\d+)\.(\d+) \S", text)
                want, label = (h2, h3, h4), f"{h2}.{h3}.{h4}"
            if not n or tuple(int(x) for x in n.groups()) != want:
                report(f, i, f"#### 应为 {label}", line)
    if chap_task and chap_other:
        report(f, chap_line, "Task 与其他小节同章", "")
    want = None  # 下一个 Task 应有的号；首个 Task 只能是 0 或 1
    for k, (i, tid, line) in enumerate(tasks):
        if not tid.isdigit():
            report(f, i, "Task 号不是数字", line)
            want = None if want is None else want + 1
            continue
        if k == 0 and int(tid) in (0, 1):
            want = int(tid)
        if want is None or int(tid) != want:
            report(f, i, "Task 号应为 " + ("0 或 1" if want is None else str(want)), line)
            want = int(tid)
        want += 1
    for i, line, rng, got in chap_ranges:
        if rng and got != [str(n) for n in range(rng[0], rng[1] + 1)]:
            report(f, i, "章标题的 Task 范围与该章实际的 Task（" + "、".join(got or ["无"]) + "）不符", line[:60])
    ids = {tid for _, tid, _ in tasks}
    for i, x, line in refs if ids else []:
        if x not in ids:
            report(f, i, f"引用了不存在的 Task {x}", line[:60])
    for i, letter, line in undotted:
        if letter in letters:
            report(f, i, "附录编号未加点", line[:60])

print(f"检查 {len(files)} 个文件，问题 {bad} 处")
sys.exit(1 if bad else 0)
