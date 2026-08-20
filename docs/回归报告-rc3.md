# dshinqt v0.1.0-rc3 回归报告

> 生成：2026-08-20 | 基线：`v0.1.0-rc2`（54dbc3a）→ 本版 4 个提交，工作区干净
> 回归范围：rc3 四项新功能（下载承接 / 剪贴板权限 / 按钮对齐 / 启动前构建自检）、构建、单元测试、运行冒烟

## 一、工程状态

| 指标 | 值 |
| --- | --- |
| 提交总数 | 29（rc2 之后 +4，见下） |
| rc3 新增提交 | `eef7ae5` 下载承接 · `32a3062` 剪贴板权限 · `04549e5` 浏览按钮对齐 · `2425719` 启动前构建产物自检 |
| 源文件数 | 49（src/ 42 + tests/ 2 + 资源类） |
| src 代码行 | cpp 3518 + h 890 = **4408 行**（rc2 4175 → +233） |
| tests 代码行 | 136 行（27 用例，rc2 21 → +6） |
| 图标资源 | 10 枚 SVG（未变） |
| 命名空间 | 全部类型位于 `dshinqt` |

## 二、构建验证 ✅

| 配置 | 结果 |
| --- | --- |
| Debug（deepseek-harness-in-qt） | ✅ 编译零错误 |
| Release（deepseek-harness-in-qt） | ✅ 编译零错误 |
| Debug / Release（tst_core 测试目标） | ✅ 编译零错误 |
| 部署 | ✅ `cmake --install` 成功，windeployqt 全量运行时 |

## 三、单元测试 ✅

`tst_core`：**27 passed, 0 failed**（Debug 与 Release 双跑通过）

- rc2 原有 21 用例全保留
- 新增 `downloadDirectory`（配置/回退系统下载目录，1 用例）
- 新增 `isDistStale`（产物缺失/早于提交/同时/晚于提交/隔天新鲜，5 用例）

## 四、运行冒烟 ✅

| 场景 | 结果 |
| --- | --- |
| 测试壳启动（独立 WebEngine userData，3081 并存） | ✅ 正常起壳 |
| 启动自检「新鲜」路径 | ✅ 无弹框阻塞，直接进入服务流程（当前仓库 dist 晚于 HEAD 提交，判据实测正确） |
| 3080 服务 | ✅ 由壳拉起（LISTENING），attach 路径正常 |
| 主页加载 | ✅ webview-latest.html 278KB，锚点 `data-composer-card` / `data-conversation-scroll` / `__DSH_BOOT__` 全部命中 |
| 壳退出后服务常驻 | ✅ 3080 服务在壳退出后继续运行（冒烟结束后已手动清理） |
| 3081 对话实例 | ✅ 全程未受影响（壳 24600 + 服务 67224 于 10:31 由用户重启，早于冒烟） |

> 偶发观察：首次冒烟中测试壳进程随父命令结束被回收（沙箱 job 语义，非产品缺陷）；复跑 40s 存活、页面就绪、行为正常。

## 五、代码回归审查

### 5.1 rc3 新功能自查

| 检查点 | 结论 |
| --- | --- |
| 下载承接 | ✅ `downloadRequested` → mkpath + `accept()`；目录不可创建回退系统下载目录；文件名空回退 URL 末段；`stateChanged` 完成/失败转发日志页；每次下载实时读设置（改路径即时生效） |
| 剪贴板权限 | ✅ `permissionRequested` → `ClipboardReadWrite` 授予（本地可信页面），其余类型 deny 留日志；依据 dsh `writeClipboard` 在异步 API 存在时不走 execCommand 兜底，权限拒绝即静默失败 |
| 浏览按钮对齐 | ✅ 与 QLineEdit 同 padding/border/圆角 QSS，高度同规则自然相等 |
| 启动自检 | ✅ QtConcurrent 后台 `git log -1 --format=%h%x09%ct` + dist 时间戳；`watcher->isRunning()` 防重入；非 git/路径无效/产物新鲜 → 原流程；重建确认 → 停旧服务 → 日志页构建 → `Origin::Startup` 成功后 `startService`、失败回错误页可重试；BuildFlowManager busy 守卫在 |
| 判据验证 | ✅ 真实仓库实测：HEAD 提交 1787152310（08-19 23:11）vs dist 1787190806782（08-20 09:53）→ 判定新鲜 |

### 5.2 rc2 遗留观察项状态

| # | 项 | rc3 状态 |
| --- | --- | --- |
| O1 | 克隆/一键构建无取消入口 | 未动（可选） |
| O2 | checkout 失败文案可能误导 | 未动（可选） |
| O3 | 测试覆盖缺口 | 部分补上：`downloadDirectory`、`isDistStale` 已测；`checkoutBranch` 前缀剥离、`parseServiceInfo`、`readLogTail` 半行缓冲仍未测 |
| O4 | CommandRunner busy 忽略无调用方通知 | 未动（低） |

### 5.3 rc3 遗留观察项（低，不阻塞定版）

| # | 项 | 说明 |
| --- | --- | --- |
| N1 | 自检判据用提交时间 | 远端提交带未来时间戳（时钟偏差）会多触发一次重建提示；重建后自愈 |
| N2 | 极端漏判场景 | 先手工构建、再 pull 时间戳早于该构建的提交 → dist 看似新鲜不漏判提醒（正常 pull 的 merge 提交时间均为新，罕见） |
| N3 | 自检弹框可被跳过 | 选「否」直接启动，产物过期风险由用户自担（与 rc1/rc2 手动行为一致） |
| N4 | 沙箱回收冒烟壳 | 测试环境行为，非产品缺陷 |

## 六、结论

**v0.1.0-rc3 回归通过**：Debug/Release/测试双配置零错误、27 用例全绿（Debug+Release 双跑）、运行冒烟全链路正常（启动自检新鲜路径 → 服务拉起 → 主页锚点命中 → 常驻，3081 对话实例零影响）。rc3 四项新功能实现与判据均经代码自查与实测。遗留观察项均为低风险（O1/O2/N1/N2/N3 可选优化，O3 建议后续补测）。
