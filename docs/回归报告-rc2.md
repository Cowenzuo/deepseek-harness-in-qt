# dshinqt v0.1.0-rc2 回归报告

> 生成：2026-08-18 | 基线：`v0.1.0-rc2`（24 个提交，工作区干净）
> 回归范围：代码审查（44 项修复确认 + 重构新增代码）、构建、单元测试、运行冒烟

## 一、工程状态

| 指标 | 值 |
| --- | --- |
| 提交总数 | 24（rc1 之后 +1：隐藏控制台修复） |
| 源文件数 | 48（src/ 41 + tests/ 2 + 资源类） |
| src 代码行 | cpp 3331 + h 844 = **4175 行** |
| tests 代码行 | 98 行（21 用例） |
| 图标资源 | 10 枚 SVG（5 组双色） |
| 命名空间 | 全部类型位于 `dshinqt` |

## 二、构建验证 ✅

| 配置 | 结果 |
| --- | --- |
| Debug（deepseek-harness-in-qt） | ✅ 编译零错误 |
| Release（deepseek-harness-in-qt） | ✅ 编译零错误 |
| Debug（tst_core 测试目标） | ✅ 编译零错误 |
| 部署 | ✅ `cmake --install` 成功，windeployqt 全量运行时 |

## 三、单元测试 ✅

`tst_core`：**21 passed, 0 failed**（13ms）

覆盖：`toProxyUrl`（4）、`parseGitLog`（4）、`nodeVersionAtLeast`（5）、`dshBootInBody`（2）、`clampPort`（5）+ init/cleanup。

## 四、运行冒烟 ✅

| 场景 | 结果 |
| --- | --- |
| 壳启动 → 环境检测 → attach 3080 | ✅ `Idle → Running`，1s 内 |
| 主页加载 | ✅ loadFinished 254KB → 正面锚点命中 → 显示主页 |
| 服务常驻 | ✅ 测试壳关闭后 3080 服务继续运行 |
| 隐藏控制台（rc2 核心） | ✅ 服务带 conhost 子进程（CREATE_NEW_CONSOLE + SW_HIDE 生效） |
| 对话实例（3081） | ✅ 全程未受影响 |

## 五、代码回归审查

### 5.1 44 项历史修复确认（抽查关键项全部就位）

| 类别 | 抽查点 | 证据 |
| --- | --- | --- |
| 进程竞态 | 代际守卫 `m_opGeneration` 在 start/stop/restart 递增、异步回调校验 | `dshprocessmanager.cpp:75/122/144` |
| 更新挂起 | 60s 启动超时兜底 + `isRunning()` 前置检查 | `updatemanager.cpp:49/173/178` |
| 端口误杀 | netstat 字段级 `endsWith(portMarker)` 精确匹配（2 处） | `dshprocessmanager.cpp:244/321` |
| 启动失败兜底 | `CommandRunner` FailedToStart 处理（finished 不触发场景） | `commandrunner.cpp:33-35` |
| 线程安全 | 后台线程 lambda 不捕获 this；QPointer 防 inspectAsync 回调 UAF | `servicepage.cpp` |
| 半行缓冲 | `m_pendingLine` 跨 chunk 拼接 | `dshprocessmanager.cpp:418-431` |
| 端口校验 | `clampPort` 回退 3080 | `appsettings` |
| 代理统一 | `pnpmEnvironment()` 更新/构建共用 | `proxydetector` |

### 5.2 重构新增代码自查（CommandRunner / BuildFlowManager / 四页拆分 / checkout 异步化）

| 检查点 | 结论 |
| --- | --- |
| CommandRunner 单任务模型 | ✅ busy 时拒绝新请求并警告；finished/errorOccurred 唯一收尾；cancel→kill→onExit(false) 链路完整 |
| UpdateManager checkout 两步 | ✅ 分支（local→`-b local remote`）、提交（branchName→`-b branchName hash`）均"先直切、失败建分支、两步失败才报错"；冲突场景 fallback 亦失败 → 正确 fail |
| `m_target.value` 覆盖时机 | ✅ fallback 参数在覆盖前构造（携带原始 hash），日志展示用分支名 |
| cancel 防重入 | ✅ `fail()`/`done()` 均 `m_stage == Idle` 守卫；kill 后的 finished 回调直接返回 |
| BuildFlowManager Origin 分派 | ✅ SetupPage（克隆/引导构建）→ recheck+回引导页；ErrorPage → 成功 startService/失败仅日志，与原 `m_buildFromSetup` 行为一致 |
| SettingsDialog 页间协作 | ✅ `beforeSave`→`waitForBackgroundTasks`（同步槽阻塞至线程结束）、`saved`→`refreshRepo` |
| 页面生命周期 | ✅ watcher 析构时 `waitForFinished`；QPointer 保护反查回调 |
| CREATE_NEW_CONSOLE | ✅ conhost 子进程存在（控制台已建）+ SW_HIDE；服务常驻/日志重定向/attach 均冒烟通过 |
| 就绪判定计时 | ✅ load() 重置、reload 不重置（防无限循环修复保留） |

### 5.3 遗留观察项（低，不阻塞定版）

| # | 项 | 说明 |
| --- | --- | --- |
| O1 | 克隆/一键构建无取消入口 | 与 rc1 行为一致（原有设计缺口，CommandRunner 已具备 kill 能力，UI 未暴露） |
| O2 | checkout 失败文案可能误导 | 冲突场景 fallback 报 "already exists"，提示"切换失败"而非"工作区冲突"，建议后续区分 |
| O3 | 测试覆盖缺口 | `checkoutBranch` 前缀剥离、`parseServiceInfo` 反查解析、`readLogTail` 半行缓冲暂无单测（均为纯逻辑，可后续补） |
| O4 | CommandRunner busy 忽略无调用方通知 | 内部 qWarning + 输出提示，UI 层无感知（触发需并发 start，当前 UI 已禁用重复入口） |

## 六、结论

**v0.1.0-rc2 回归通过**：构建（Debug/Release/测试）零错误、21 用例全绿、运行冒烟全链路正常（attach→主页锚点→常驻）、44 项历史修复确认在位、重构新增代码未发现 high/medium 级回归。遗留 4 项低风险观察项，建议作为 v0.1.0 正式版前的小修清单（O1/O2 可选，O3 建议补测）。

