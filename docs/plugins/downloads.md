# PulseBar 插件商店

本页是 PulseBar **官方插件**的下载入口。软件内点「插件管理 → 更多插件下载」会打开**内置插件商店**（直接列出本清单并可一键安装），不需要先来本页。
开发接口与示例见 [插件开发指南](development.md)。

## 官方插件

| 插件 | 版本 | 架构 | 下载 | SHA-256 |
| --- | --- | --- | --- | --- |
| Magpie Taskbar | 1.4.8 | x64 | [MagpieTaskbar.dll](https://github.com/xiaopeng66/PulseBar/releases/download/plugins-v1.4.8/MagpieTaskbar.dll) | `99f436e039aaf8cdcd905d7152569babc06ad71ed812f640fdd45beb5a6372ca` |

插件二进制放在 [插件 Release](https://github.com/xiaopeng66/PulseBar/releases/tag/plugins-v1.4.8) 里，不进仓库，避免仓库随每次插件更新膨胀。`plugins_version.xml` 的 `download` 字段直接指向 Release 资产；旧版本 Release 和发布资产均保留。

「官方」指由本项目维护并随本仓库发布的插件。在软件插件管理列表里新增的「来源」列会显示「官方」字样；该标记来自清单的 `official="true"`，需要先成功拉取过清单（启动时或手动刷新后）。第三方插件不显示该标记。

## 检查更新

打开「插件管理」时会自动读取 GitHub 的 [插件清单](plugins_version.xml)。也可点击左上角的「检查更新」重新检查。

## 安装与更新

- 已安装：在「插件管理」中选中「可用更新」列有版本号的插件，点击右上角「更新」。安装完成后点击「重启生效」。
- 第一次安装：打开「插件商店」选择插件，也可用「安装本地插件…」选择下载好的 DLL。文件会安装到 PulseBar.exe 同级的 `plugins` 目录。

手动安装：

1. 从通知区菜单**正常退出** PulseBar，等待程序关闭；不要强制结束进程，否则未保存的流量记录或设置可能丢失。
2. 确认要更新的那份程序目录（日常使用通常是 `portable`，**不要把开发构建目录与日常运行目录混用**）。
3. 把插件 DLL 及其依赖放到**正在使用的 `PulseBar.exe` 同级的 `plugins` 文件夹**。
4. 启动同一目录的 `PulseBar.exe`，打开「插件管理」确认状态为「加载成功」。
5. 需要在任务栏显示插件指标时，在「监控项目与顺序…」里勾选插件提供的项目。

```text
日常使用目录/
  PulseBar.exe
  language/
  plugins/
    MagpieTaskbar.dll
```

## 安全说明

- 插件 DLL 在 PulseBar **进程内执行**，不是沙箱程序。只安装可信来源的插件，不要为运行陌生插件关闭系统防护。
- 下载后建议核对上面的 SHA-256；与表中不一致时不要安装。
- 插件版本与 PulseBar 版本是两回事：安装前确认插件声明的宿主版本与 **x64 / x86 / ARM64EC** 架构。

## 发布新版本（维护者）

1. 建一个 `plugins-v<版本>` 的 Release，上传新的 DLL（插件二进制不进仓库）。
2. 修改 `plugins_version.xml` 中该插件的 `version`、`download`（指向新 Release 资产）和 `sha256`。
3. 同步更新本页表格的版本、SHA-256 与下载链接。
4. 提交推送后，已安装用户下次启动或点「检查更新」即可看到提示；插件列表里会显示「官方」角标。
