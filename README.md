**简体中文 | [English](./README_en-us.md)**

[![LICENSE](https://img.shields.io/badge/license-Anti%20996-blue.svg?style=flat-square)](https://github.com/996icu/996.ICU/blob/master/LICENSE)

# PulseBar

PulseBar 是一款用于 Windows 平台的网速监控任务栏工具，可以显示当前网速、CPU 及内存利用率，嵌入任务栏显示，并提供历史流量统计与插件扩展能力。本项目基于 [TrafficMonitor](https://github.com/zhongyang219/TrafficMonitor) 二次开发，聚焦任务栏形态并做了大量重构。

## 主要特性

* 任务栏内嵌显示：网速、CPU / 内存利用率、（可选）硬件温度
* 托盘图标四种样式：蓝色 / 浅色 / 深色 / 自动（跟随任务栏深浅色）
* 托盘网速实时渲染模式
* 多网卡自动与手动选择、网络详细信息
* 历史流量统计
* 插件系统
* 深浅色主题自适应、DPI 缩放

## 构建

* 使用 Visual Studio 2022 打开 `PulseBar.sln`，或在已安装 VS 2022 Build Tools 和 Python 3.10+ 的环境运行 `build-sln.bat`（Debug：`build-debug.bat`）。
* Release 产物位于 `Bin/x64/Release/PulseBar.exe`。脚本在编译成功后调用 `tools/stage-languages.py`，将权威目录 `PulseBar/language` 的 18 个 INI 原样复制到产物旁的 `language`；Debug 还复制到仓库根 `language`，供脚本的工作目录加载。语言缺失、编码异常或目标不安全会使构建入口返回失败。
* 直接用 Visual Studio / MSBuild 构建后，可运行 `python -X utf8 -I -B tools/stage-languages.py --build-output "Bin/x64/Release"` 补齐语言；其它架构/配置使用对应的 `Bin` 输出目录。此步骤只准备构建语言，不读取 EXE/DLL，不同步 portable/installer。
* `tools/sync-distribution.py` 是原维护工作区的显式本地分发工具，不是公共构建依赖；默认仅检查，写入必须另行显式指定 `--write`，原有工作区/路径、备份及隔离限制不变。普通 clone 不需要运行它。
* 依赖 Microsoft Visual C++ 运行环境：[最新 VC++ 可再发行程序包](https://docs.microsoft.com/zh-CN/cpp/windows/latest-supported-vc-redist?view=msvc-170)

## 相关链接

* 使用帮助：[Help.md](./Help.md)
* 问题反馈：[Issues](https://github.com/xiaopeng66/PulseBar/issues)

## 许可证

本项目基于 [TrafficMonitor](https://github.com/zhongyang219/TrafficMonitor)（作者 zhongyang219）二次开发，遵循 [Anti-996 License](./LICENSE) 发布；原始版权声明见 [LICENSE](./LICENSE) 与 [LICENSE_CN](./LICENSE_CN)。
