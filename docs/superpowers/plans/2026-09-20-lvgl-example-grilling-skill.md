# Luckfox Pico LVGL Example — Cloud Agent grilling 技能安装 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让本仓 Cloud Agent 在创建 Environment Build 时把固定版本的 grilling 写入 `$HOME/.cursor/skills/grilling/SKILL.md`，后续从该 Build 启动的新 Agent 开箱具备该技能。

**Architecture:** `.cursor/environment.json` 保留现有 `build`，追加一条内联 `curl` 作为 `install`。目标路径在仓库外；技能 pin 为 `mattpocock/skills@85f83d3`。不改 Dockerfile，不引入 `install.sh` / `start` / IMA。

**Tech Stack:** Cursor Cloud Agent `environment.json` `install`、`curl`（镜像已有）、HTTPS `raw.githubusercontent.com`。

**Spec:** [`docs/superpowers/specs/2026-09-20-lvgl-example-grilling-skill-design.md`](../specs/2026-09-20-lvgl-example-grilling-skill-design.md)

## Global Constraints

设计一律以 spec 为准。每个 Task 默认包含：

- 目标路径：`$HOME/.cursor/skills/grilling/SKILL.md`；URL pin `85f83d3fde1d3a90d5c9a657f6998c79a6c37308`；`curl` 参数与 spec §5.2 相同；验收哈希 `sha256:10ff989e7498b23b5acb49d5048f11dcd906757d2f79c5cdf8a00001381296f2`
- 不把 `SKILL.md` 纳入 git；不引入 IMA；install 内不做 SHA-256 / 临时文件 / 重命名恢复
- 不改 Dockerfile、不加 `start` / `terminals`、不复制 luckfox-pico `install.sh`、不 `chown ubuntu`、不硬编码 `/root` 或 `/home/ubuntu`
- 单个草稿 PR #3（`cursor/install-grilling-skill-23fb`）；不另开 PR；不改 `docs/superpowers/plans/2026-07-24-lvgl-example-cloudagent-env.md`
- 相对 `origin/dev`：实现（`.cursor/environment.json` + `AGENTS.md`）在前，spec/plan 单个提交在最后
- 本会话即使改了 `install` 也不会出现 grilling；端到端须新 Build + 新 Agent。草稿 Build 只验证、不自动成为 active

## File Structure

| 文件 | 责任 |
|---|---|
| `.cursor/environment.json` | `build` 不变；追加 spec §5.1 的 `install` |
| `AGENTS.md` | 「Cloud Agent 环境」下增加 spec §6.1 那一条 gotcha |
| `docs/superpowers/specs/2026-09-20-lvgl-example-grilling-skill-design.md` | 设计规格 |
| `docs/superpowers/specs/2026-07-24-lvgl-example-cloudagent-env-design.md` | 过时的「省略 install」结论改为指向本 spec |
| `docs/superpowers/plans/2026-09-20-lvgl-example-grilling-skill.md` | 本文件：Task、完成情况、最终验证证据 |

---

### Task 1: `.cursor/environment.json` 追加 `install`（已完成）

**落地文件（以仓库为准，不在此重复 JSON 正文）：**

- Modify: `.cursor/environment.json`（`build` 不变；追加 spec §5.1 的 `install`）

**Interfaces:** 消费 spec §5.1；产出 `install` 字符串供 Task 3 原样执行、供 Environment Build 使用。

- [x] **Step 1:** 失败断言（当时无 `install`）
- [x] **Step 2:** 追加 `install`（正文见仓库）
- [x] **Step 3:** `python3 -m json.tool` 成功；`install` 与 spec §5.1 逐字相同；无 `start` / `terminals`
- [x] **Step 4:** 实现提交（与 Task 2 同一 `chore(cloud-env)`）

### Task 2: `AGENTS.md` 增加 grilling gotcha（已完成）

**落地文件（以仓库为准，不在此重复条目正文）：**

- Modify: `AGENTS.md`（「Cloud Agent 环境」列表末尾）

**Interfaces:** 消费 spec §6.1；不改变 Task 1 的 `install`。

- [x] **Step 1:** 当时 `AGENTS.md` 无 `grilling`
- [x] **Step 2:** 追加 spec §6.1 那一条（正文见仓库）
- [x] **Step 3:** 全文件仅一处 `grilling`；未改构建 / lint / 运行小节
- [x] **Step 4:** 与 Task 1 同一实现提交

### Task 3: 隔离 `HOME` 验收 install 命令（已完成）

**Files:** 无仓库文件。

**Interfaces:** 消费仓库中的 `install` 字符串；产出哈希与内容特征，见「最终验证证据」。

- [x] **Step 1:** 隔离 `HOME` 连续执行两次 `install`；核对哈希与 `frontier` / `❓` / `➡️`；无 IMA；工作区无 `.cursor/skills/grilling/SKILL.md`

```bash
set -euo pipefail
cmd=$(python3 -c 'import json; print(json.load(open(".cursor/environment.json"))["install"])')
tmp=$(mktemp -d)
HOME="$tmp" bash -c "$cmd"
HOME="$tmp" bash -c "$cmd"
sha256sum "$tmp/.cursor/skills/grilling/SKILL.md"
grep -F 'frontier' "$tmp/.cursor/skills/grilling/SKILL.md"
grep -F '❓' "$tmp/.cursor/skills/grilling/SKILL.md"
grep -F '➡️' "$tmp/.cursor/skills/grilling/SKILL.md"
test ! -e "$tmp/.cursor/skills/ima" && test ! -e "$tmp/.cursor/skills/ima/SKILL.md"
test ! -e /workspace/.cursor/skills/grilling/SKILL.md
```

Expected: 两次退出 0；SHA-256 第一列为 `10ff989e7498b23b5acb49d5048f11dcd906757d2f79c5cdf8a00001381296f2`；三处 grep 均命中。实测见「最终验证证据」。

### Task 4: 草稿 Environment Build（已完成）

**Files:** 无仓库改动。Build 必须包含已推送的 `environment.json`。

**Interfaces:** 消费远端 `cursor/install-grilling-skill-23fb`；产出 `SUCCEEDED` 的草稿 `buildId`。

- [x] **Step 1:** `git push` 本分支（不 force-push `dev`、不删远端分支、不另开 PR）
- [x] **Step 2:** `trigger-environment-build`（`refs.repoUrl=github.com/yuangezhizao/luckfox_pico_lvgl_example`，`refs.ref=cursor/install-grilling-skill-23fb`，不传 `environmentJson`）
- [x] **Step 3:** `list-environment-builds` 至终态后取 `environment-build-logs`；`SUCCEEDED` 才算通过；未另起 Agent 读哈希

### Task 5: 回填验证证据（已完成）

**落地文件（以仓库为准，不在此重复表格正文）：**

- Modify: 本 plan「最终验证证据」

**Interfaces:** 消费 Task 3–4 实测；不改 spec 设计结论；不改 2026-07-24 plan。

- [x] **Step 1:** 回填「最终验证证据」
- [x] **Step 2:** 与 spec 同一 `docs(superpowers)` 提交（最后一次）
- [x] **Step 3:** 更新草稿 PR #3 正文（`update_pr`，不另开 PR）

---

## Execution Handoff

仓库施工无剩余。§7.5 新 Agent 读哈希未做，不记作通过。新 Agent 须从捕获了该 `install` 的 Build 启动。

## 完成情况总结

| 交付物 | 状态 |
|---|---|
| Task 1 `environment.json` `install` | ✅ |
| Task 2 `AGENTS.md` grilling gotcha | ✅ |
| Task 3 隔离 HOME 验收 | ✅ |
| Task 4 草稿 Environment Build | ✅ |
| Task 5 验证证据与 PR 正文 | ✅ |
| spec + plan | ✅ |
| §7.5 新 Agent 读哈希 | 未做 |

**整体结论：** `install` pin `85f83d3` 已写入 `.cursor/environment.json`；草稿 Build `bld-20260920-ada5d9ab-1db9-418c-a2fc-c4b2d76bfbd1` `SUCCEEDED`。§7.1–4 与 §7.6 已通过；§7.5 新 Agent 读哈希未做，不记作通过。

---

## 最终验证证据

通过标准见 spec §7。

| 项 | 期望 | 实测 |
|---|---|---|
| `python3 -m json.tool` | 退出 0 | 退出 0 |
| `install` 字符串 | 与 spec §5.1 逐字相同 | 与 spec §5.1 逐字相同 |
| 隔离 HOME 两次 `curl` | 两次退出 0 | 两次退出 0（tmp `/tmp/tmp.rTdAipRxUL`） |
| `SKILL.md` SHA-256 | `10ff989e7498b23b5acb49d5048f11dcd906757d2f79c5cdf8a00001381296f2` | `10ff989e7498b23b5acb49d5048f11dcd906757d2f79c5cdf8a00001381296f2` |
| 内容特征 | 含 `frontier`、`❓`、`➡️` | 含 `frontier`、`❓`、`➡️` |
| IMA 路径 | 不存在 | 不存在 |
| 工作区 / git | 无 `.cursor/skills/grilling/SKILL.md`，porcelain 无该文件 | 无 `.cursor/skills/grilling/SKILL.md`；Task 3 时 porcelain 为空 |
| 草稿 Environment Build（§7.4） | `SUCCEEDED`；install 退出 0；无 curl 失败 | `bld-20260920-ada5d9ab-1db9-418c-a2fc-c4b2d76bfbd1` 状态 `SUCCEEDED`；日志 curl `100 1987` 后 `[INSTALL] Exit code: 0`，无 `curl:` 失败；`Install finished. Preparing snapshot…`；`Warming skipped (draft).` Dashboard：https://cursor.com/dashboard/cloud-agents/builds/bld-20260920-ada5d9ab-1db9-418c-a2fc-c4b2d76bfbd1 |
| 新 Agent 读哈希（§7.5） | 新 Agent 上哈希为 NFR6、无 IMA | 未另起，不记作通过 |
