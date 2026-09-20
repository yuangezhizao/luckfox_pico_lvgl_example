# Luckfox Pico LVGL Example — Cloud Agent grilling 技能安装设计规格

- **日期**：2026-09-20
- **分支**：`cursor/install-grilling-skill-23fb`
- **参考安装机制**：[`yuangezhizao/luckfox-pico#4`](https://github.com/yuangezhizao/luckfox-pico/pull/4)（Environment Build `install` 写入全局 grilling）
- **参考技能版本**：[`yuangezhizao/luckfox-pico#5`](https://github.com/yuangezhizao/luckfox-pico/pull/5)（pin 至 `85f83d3`）
- **既有环境规格**：[`2026-07-24-lvgl-example-cloudagent-env-design.md`](2026-07-24-lvgl-example-cloudagent-env-design.md)（Dockerfile 配置即代码；本 spec 只补 grilling 的 `install`，不改镜像依赖）
- **关联计划**：[`../plans/2026-09-20-lvgl-example-grilling-skill.md`](../plans/2026-09-20-lvgl-example-grilling-skill.md)

## 1. 概述与目标

为本仓 Cloud Agent 环境补上 **grilling 全局技能**：在 `.cursor/environment.json` 增加 `install`，于创建 Environment Build 时把固定 Git commit 的 `SKILL.md` 写入仓库外 `$HOME/.cursor/skills/grilling/SKILL.md`，使后续从该 Build 启动的新 Agent 开箱具备该技能。机制对齐 luckfox-pico PR #4，技能文件版本直接使用 PR #5 的 pin（跳过 `2ab9580`），单个 PR 一次落地。

不把 `SKILL.md` 纳入 git。不引入 IMA。不搬运 luckfox-pico PR #8/#9/#10 的 `install.sh` / `start.sh` / ubuntu 运行用户 / sshd / Tailscale。

## 2. 背景与现状

本仓已有 Dockerfile 模式环境（PR #1）且镜像已自带 `curl`（PR #2）。当前 `.cursor/environment.json` 只有 `build`，没有 `install`。既有规格曾约定 grilling 落在仓库内 `.cursor/skills/grilling/SKILL.md` 并以 `.git/info/exclude` 排除；该路径从未进远程仓库，Cloud Agent checkout 得不到该文件。本会话实测 `$HOME/.cursor/skills/grilling/` 不存在，插件清单亦无 grilling。

luckfox-pico 的安装路径：

| 阶段 | 做法 |
|---|---|
| PR #4 | `environment.json` 的 `install` 为一条 `curl`，写入 `$HOME/.cursor/skills/grilling/SKILL.md`，pin `2ab9580`（逐条一问） |
| PR #5 | 仅把 pin 改为 `85f83d3`，并写明 install 发生在 **创建 Environment Build** 时；成功的非草稿 Build 捕获磁盘并自动成为 active；草稿 Build 只用于验证、须显式激活 |
| PR #8 之后 | `install` 改为 `sudo -n -E bash .cursor/install.sh`；脚本仍用同一条 `curl`（仍 pin `85f83d3`），另加 `chown ubuntu`、sshd 等。那些改动依赖默认用户 ubuntu 与远程接入，**不是 grilling 本身** |

Cursor 官方：`install` 在创建 Build 时于项目根执行，须幂等；成功 Build 只保留磁盘状态。技能发现路径含 `~/.cursor/skills/`（用户级）与 `.cursor/skills/`（项目级）。

## 3. 需求

功能性需求：
- FR1：`.cursor/environment.json` 增加 `install`，创建 Environment Build 时安装 grilling 到 `$HOME/.cursor/skills/grilling/SKILL.md`。
- FR2：下载 URL 固定为 [`mattpocock/skills` commit `85f83d3fde1d3a90d5c9a657f6998c79a6c37308` 的 `skills/productivity/grilling/SKILL.md`](https://raw.githubusercontent.com/mattpocock/skills/85f83d3fde1d3a90d5c9a657f6998c79a6c37308/skills/productivity/grilling/SKILL.md)，与 luckfox-pico PR #5 同一文件版本。
- FR3：`curl` 参数与 luckfox-pico PR #4/#5 逐字相同（见 §5.2）。
- FR4：`AGENTS.md` 的 Cloud Agent 环境小节补一条：grilling 由 `install` 写入 `$HOME/.cursor/skills/`、不进 git、须从捕获了该 install 的 Build 启动。
- FR5：同步既有 cloud-env spec 中已过时的「省略 install / 仓库内 `.cursor/skills/` 排除」结论，避免两份规格互相打架。
- FR6：单个 PR、两次提交：实现（`.cursor/environment.json` 的 `install` 与 `AGENTS.md` 一条 gotcha）在前，本 spec 与 plan 在最后。

非功能性需求：
- NFR1：grilling 文件不进 git；目标路径在仓库外，不需要 gitignore。
- NFR2：不配置 IMA（凭据、远端更新、额外运行时依赖都不符合最小环境）。
- NFR3：不在 `install` 里做 SHA-256 校验、临时文件或重命名恢复；成功路径直接覆盖目标文件，幂等。
- NFR4：创建 Build 时须能访问 `raw.githubusercontent.com`；受限 egress 必须显式放行该域名。
- NFR5：不改 Dockerfile、不改交叉编译 / native 渲染依赖、不加 `start` / `terminals`。
- NFR6：验收哈希为 `sha256:10ff989e7498b23b5acb49d5048f11dcd906757d2f79c5cdf8a00001381296f2`（对本 pin 的 `SKILL.md` 实测；与 luckfox-pico PR #5 验证值相同）。

## 4. 设计决策

| 决策 | 结论 | 理由 |
|---|---|---|
| D1 安装位置 | `$HOME/.cursor/skills/grilling/SKILL.md`（用户级，仓库外） | 与 luckfox-pico PR #4 相同；Cloud Agent 能发现 `~/.cursor/skills/`；不进 git、无需 gitignore |
| D2 触发点 | `environment.json` 的 `install`，在创建 Environment Build 时执行 | 官方把可预先落到磁盘的准备放进 `install`；成功 Build 捕获磁盘后新 Agent 开箱即有 |
| D3 载体 | 内联一条 `curl`，不引入 `.cursor/install.sh` | 本仓只需 grilling；luckfox-pico 现网 `install.sh` 还装 sshd、`chown ubuntu`，依赖后续 PR 的 ubuntu 用户，本仓既有规格 D5 不设 `USER` |
| D4 技能版本 | 直接 pin `85f83d3`，不经过 `2ab9580` | 用户指定「直接更新为 luckfox-pico PR5 的文件版本」；本仓从未装过旧版，不必分两次 PR |
| D5 校验 | install 不做内容摘要；plan 用 NFR6 哈希做验收 | 对齐 luckfox-pico PR #4 取舍；`--fail` 避免把 HTTP 错误页写成技能文件 |
| D6 覆盖策略 | `--create-dirs --output` 直接写最终路径 | 成功路径幂等；异常中断可能留下不完整同名文件（与参考相同的已知风险） |
| D7 运行用户 | 不 `chown ubuntu`，不硬编码 `/root` 或 `/home/ubuntu` | 本仓 Agent 与 install 均按平台默认用户、`$HOME` 展开；ubuntu 用户在本镜像中不保证存在，`chown ubuntu` 会失败 |
| D8 文档 | 新 spec 为 grilling 权威来源；既有 cloud-env spec 只改过时结论并留指针；`AGENTS.md` 只加一条 gotcha | 既有 spec 仍管 Dockerfile；Agent 面向说明留在 `AGENTS.md`，避免下一轮再误报「未安装」 |
| D9 否决入库 | 不把 `SKILL.md` 放进 `.cursor/skills/` 提交 | 与「不进 git」一致；版本钉在 URL 的 commit，而不是仓库副本 |
| D10 否决镜像烘焙 | 不在 Dockerfile `RUN curl` | 升 pin 不应重建交叉编译镜像；官方 Build 生命周期就是 `install` 写磁盘 |

`85f83d3` 相对 `2ab9580` 的行为（来自 luckfox-pico PR #5，本仓直接采用新行为）：

| 维度 | `2ab9580` | `85f83d3`（本仓） |
|---|---|---|
| 提问调度 | 设计树上逐题串行 | 每轮询问 frontier 中全部已解除依赖的决策 |
| 同轮区隔 | 无专门分隔线 | 相邻问题以 Markdown `---` 分隔 |
| 问题格式 | 自由文本 | 固定模板：❓ Qn + ➡️ 推荐答案 |

## 5. `environment.json` 设计

### 5.1 完整文件

`build` 保持现状；只追加 `install`。仍省略 `start` / `terminals`。

```json
{
  "build": {
    "dockerfile": "Dockerfile",
    "context": ".."
  },
  "install": "curl --fail --location --max-redirs 3 --proto \"=https\" --tlsv1.2 --retry 3 --retry-max-time 120 --connect-timeout 15 --max-time 90 --create-dirs --output \"$HOME/.cursor/skills/grilling/SKILL.md\" \"https://raw.githubusercontent.com/mattpocock/skills/85f83d3fde1d3a90d5c9a657f6998c79a6c37308/skills/productivity/grilling/SKILL.md\""
}
```

`install` 在项目根执行；输出路径用 `$HOME`，与 cwd 无关。`curl` 已由 `.cursor/Dockerfile` apt 安装，install 阶段可直接调用。

### 5.2 curl 参数（与 luckfox-pico PR #4/#5 相同）

| 参数 | 含义 |
|---|---|
| `--fail` | 常规 HTTP 4xx/5xx 使 curl 失败，而非把错误页写成技能文件；认证相关响应仍可能是例外 |
| `--location` / `--max-redirs 3` | 跟随 HTTP 3xx，最多 3 次 |
| `--proto "=https"` / `--tlsv1.2` | 只允许 HTTPS，TLS 1.2 或更高 |
| `--retry 3` / `--retry-max-time 120` | 对指定暂态失败最多重试 3 次，在 120 秒窗口内发起重试 |
| `--connect-timeout 15` / `--max-time 90` | 连接阶段最多 15 秒；每次传输最多 90 秒 |
| `--create-dirs` / `--output <path>` | 创建目标目录，并直接写入最终 `SKILL.md` |

### 5.3 Environment Build 生效语义

- `install` 在**创建 Environment Build** 时运行，不在每个 Agent 启动时重跑。
- 成功的**非草稿** Build 捕获磁盘状态并自动成为 active；之后的新 Agent 从该 Build 启动即带有 grilling。
- **草稿** Build 只用于验证，须显式激活后才成为 active。
- 失败 Build 不替换当前 active；已运行会话不会自动获得该文件。
- ⚠️ 合并后须等待 dev（或本分支）上成功的非草稿 Build 成为 active，或显式激活验证通过的草稿 Build；本会话以及合并前已启动的 Agent 都不会因此出现 `$HOME/.cursor/skills/grilling/SKILL.md`。

## 6. 文档与提交

### 6.1 改动文件

| 文件 | 动作 |
|---|---|
| `.cursor/environment.json` | 追加 §5.1 的 `install` |
| `AGENTS.md` | 在「Cloud Agent 环境」下增加 FR4 那一条 |
| 本 spec | 新增，grilling 权威来源 |
| [`2026-07-24-lvgl-example-cloudagent-env-design.md`](2026-07-24-lvgl-example-cloudagent-env-design.md) | 把 D6 / NFR3 / §5.2 / §9 / §10 改为当前结论并指向本 spec |
| `docs/superpowers/plans/2026-09-20-lvgl-example-grilling-skill.md` | 新增实施计划 |
| `docs/superpowers/plans/2026-07-24-lvgl-example-cloudagent-env.md` | **不改**。那是已完成的环境落地记录，排除规则只描述当时做法 |

`AGENTS.md` 拟增条目（实现时按此语义，可微调措辞、不改变事实）：

- grilling：`environment.json` 的 `install` 在创建 Environment Build 时把固定 commit `85f83d3` 的技能写入 `$HOME/.cursor/skills/grilling/SKILL.md`（不进 git）。新 Agent 须从捕获了该 install 的 Build 启动。

### 6.2 提交策略

单个 PR，标题口径：`chore(cloud-env): 🔧 配置 Cursor Cloud Agent 环境以安装 grilling 技能`。

提交（两次，subject 单一主题）：
- `chore(cloud-env)`：`.cursor/environment.json` 追加 `install`，`AGENTS.md` 一条 gotcha
- `docs(superpowers)`：本 spec、既有 cloud-env spec 指针、plan（最后一次）

## 7. 验证（plan 执行时落地，此处只定判据）

1. `python3 -m json.tool .cursor/environment.json` 成功；`install` 字符串与 §5.1 一致。
2. 隔离 `HOME` 下连续执行两次 `install` 命令：第二次仍成功；目标文件存在且 `sha256sum` 为 NFR6。
3. 文件内容含 frontier 调度与同轮分隔（`❓`、`➡️`、Markdown `---`），且不是 `2ab9580` 那种「一次只问一题、无 round 模板」文本。
4. 草稿 Environment Build 成功，install 阶段退出 0 且无 curl 失败。此条只证明创建 Build 时在环境内执行了 `install`，不证明新 Agent 已读到技能文件。
5. 从该 Build 起的新 Cloud Agent 上 `$HOME/.cursor/skills/grilling/SKILL.md` 哈希为 NFR6，且不存在 IMA 技能目录。此条为端到端；未另起则不记作通过，不得用隔离 `HOME`、本会话缺文件或第 4 条代替。
6. `git status` 无 `SKILL.md` 待提交。

未改 `install` 的当前会话得不到该文件，不得用本会话缺文件来否定 §5 设计。

## 8. 风险

| 风险 | 缓解 |
|---|---|
| 创建 Build 需访问 `raw.githubusercontent.com` | 文档写明；受限 egress 放行该域名；`curl` 限制 HTTPS / 重定向 / 重试 |
| 不做 install 内 SHA-256 | 验收用 NFR6；`--fail` 降低错误页落盘 |
| 中断可能留下不完整 `SKILL.md` | 与参考相同的已知取舍；重跑幂等 install 覆盖 |
| `--create-dirs` 以 root 建目录时，若日后改以非 root 用户跑 Agent，可能无法 traverse | 本仓不引入 ubuntu 用户；若日后改运行用户，须另开 spec 处理 `chown`（luckfox-pico PR #9 的课） |
| 合并后旧会话仍无 grilling | §5.3：须新 Build + 新 Agent |

## 9. 安全 / 诚实边界

- 只安装 grilling，不装 IMA，不把技能文件或凭据写入仓库。
- 不编造 Environment Build 结果；未跑的端到端验证不写成已通过。
- 不把 luckfox-pico 后续 sshd / Tailscale / 默认用户改动说成本仓 grilling 的必要部分。
- `install` 只走 HTTPS，TLS 1.2+。

## 10. 参考资料

- Cursor 官方《Cloud 环境设置 · Install script / environment.json》：https://cursor.com/cn/docs/cloud-agent/setup#docker
- grilling PR #4：https://github.com/yuangezhizao/luckfox-pico/pull/4
- grilling PR #5：https://github.com/yuangezhizao/luckfox-pico/pull/5
- 技能文件（PR #5 pin）：https://github.com/mattpocock/skills/blob/85f83d3fde1d3a90d5c9a657f6998c79a6c37308/skills/productivity/grilling/SKILL.md
- 本仓既有环境规格：[`2026-07-24-lvgl-example-cloudagent-env-design.md`](2026-07-24-lvgl-example-cloudagent-env-design.md)
- 本仓 `.cursor/environment.json`、`AGENTS.md`、`.cursor/Dockerfile`（已含 `curl`）
