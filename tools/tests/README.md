# PulseBar 原生回归

这些测试针对当前工作树中的生产实现，而不是审计时冻结的源码。它们不会启动正式 PulseBar、Explorer 或真实网络请求，也不读取用户配置、修改注册表或自启动。

## 环境与运行

- Windows、Python 3.10+、Visual Studio 2022 Build Tools（MSVC、Windows SDK、MFC）。
- `native_test_support.py` 使用当前源码路径，默认工具链为 VS2022 BuildTools。
- 本工作区的安全约束要求所有测试产物写入 `E:/AI/Zcode/tmp/pulsebar-fix-20261105/` 的独占子目录。运行后保留 `test.cpp`、编译及模式日志；归档后再清理这些子目录。
- 用 `-I -B -X utf8` 避免从当前目录导入不相关模块、写入字节码及 Windows 默认编码差异。

示例：

```text
python -X utf8 -I -B E:/AI/Zcode/projects/traffic-monitor/src/tools/tests/test_history_atomic.py
python -X utf8 -I -B E:/AI/Zcode/projects/traffic-monitor/src/tools/tests/test_network_refresh_chain.py
python -X utf8 -I -B E:/AI/Zcode/projects/traffic-monitor/src/tools/tests/test_ui_regressions.py
python -X utf8 -I -B E:/AI/Zcode/projects/traffic-monitor/src/tools/tests/test_options_apply.py --verify-mutations
```

## 覆盖范围

| 测试 | 主要行为 |
| --- | --- |
| `test_history_regressions.py` | 跨日保存、同条数备份恢复、同日重复保存 |
| `test_history_atomic.py` | 原子写入、短写/零写/flush/close/替换失败、备份提交快照及 CRLF |
| `test_history_reads.py` | 缺失/损坏/占用/截断历史的有界加载与写保护 |
| `test_history_backup_recovery.py` | 不可读备份禁止覆盖，可读备份恢复，缺失备份正常创建 |
| `test_history_cache.py` | 今日流量修改后历史视图缓存失效 |
| `test_monitor_settings.py` | 单轮不可变设置快照、并发发布、硬件初始化入口 |
| `test_network_regressions.py` | 64 位计数器、方向重置、网卡重排/删除/复用、失败后重新建基线 |
| `test_network_refresh_chain.py` | 自动刷新与健康网卡连续统计、恢复网卡、详情稳定身份 |
| `test_disk_speed.py` | HDD 开关不阻断磁盘吞吐采样，失败刷新 available |
| `test_ini_sections.py` | 缺右括号、第一行段名、跨行边界、prefix 过滤 |
| `test_json_regressions.py` | 合法与截断 JSON、转义及错误值类型 |
| `test_ip_worker_errors.py` | 异常转失败结果，不逃逸工作线程 |
| `test_ip_response_limit.py` | 有限响应读取，超大响应提前停止 |
| `test_ui_regressions.py` | 稳定 owner、重建命中表、逐项颜色、禁用固定面板、百分比几何 |
| `test_options_apply.py` | 真实宿主 Apply/重建/Cancel/更新源链，9 场景；可选 owner/baseline/restore 三个内存负向变体 |
| `test_release_languages.py` | 只执行三架构 CI 的语言暂存步骤，核对 18 个 INI 原字节及 artifact 路径覆盖 |
| `test_public_build.py` | 公共构建入口的受控镜像；编译器替换为桩，验证退出码、12 配置语言暂存及路径限制 |

`test_ui_regressions.py` 默认运行全部组，也可传组名（例如 `f12_layout_hit`）；每次仍运行共同对照。选择子组不等于全套验收。

## 证据边界

- 历史 I/O 使用真实 Win32 文件 API 和专属构造数据；故障测试在明确 API 边界注入故障并断言注入确实命中。未模拟真实断电。
- 网卡、HTTP、硬件和窗口依赖使用内存夹具。实际采样、差分、刷新路由、配置提交和命中方法尽可能保留生产控制流。
- UI 测试是原生方法级验证，不是 MFC 桌面、DPI、多屏、休眠或可访问性验收。网络测试不证明真实设备驱动行为。
- Apply 链保留真实宿主提交/重建和任务栏 `OnCancel`，窗口/模态驱动、预验证控件草稿、持久化/插件投递是内存边界；初始化快照由夹具输入，硬件分支按 `WITHOUT_TEMPERATURE` 排除。它不证明真实 MFC/OS 生命周期或完整 UAF 安全。`--verify-mutations` 只有预定业务断言正常失败（exit 1）才认可负向变体被检出，不接受崩溃/超时/编译失败。
- 成功编译不是行为通过；脚本必须 exit 0。编译/提取失败与产品断言失败要分开记录。
- 原生组数、源码合同条数、unittest 方法数、PE 资源检查数不相加成一个“总通过率”。

已有旧回归仍位于容器目录 `../tools/fuzz/`（相对 Git 根），不要把这些会执行外部操作的脚本无差别批量运行。公共构建和语言包测试应在各自受控镜像中运行，不指向正式输出。
