# PulseBar 插件商店

本页是 PulseBar **官方插件**的下载入口，对应软件「插件管理 → 更多插件下载」。
开发接口与示例见 [插件开发指南](development.md)。

## 官方插件

| 插件 | 版本 | 架构 | 下载 | SHA-256 |
| --- | --- | --- | --- | --- |
| Magpie Taskbar | 3.12.0 | x64 | [MagpieTaskbar.dll](https://github.com/xiaopeng66/PulseBar/releases/download/plugins-v3.12.0/MagpieTaskbar.dll) | `05ccf9434da4dbad8ab9365e8f555afdf88c248a2aa5b007bbc2e5a44e92f228` |

插件二进制放在 [插件 Release](https://github.com/xiaopeng66/PulseBar/releases/tag/plugins-v3.12.0) 里，不进仓库，避免仓库随每次插件更新膨胀。`plugins_version.xml` 的 `download` 字段直接指向 Release 资产。

「官方」指由本项目维护并随本仓库发布的插件，软件插件列表会显示「官方」角标；第三方插件不在本表内，请以插件自身「详细信息」中的作者链接为准。

## 检查更新

软件启动时会拉取本目录的 [`plugins_version.xml`](plugins_version.xml)，把本地插件版本与表中版本比对：

- 有新版：插件管理列表中该行显示「有更新，最新版本：x.y.z」
- 已是最新：不显示提示
- 拉取失败（断网、代理拦截）：静默保留上一次成功结果，不影响已安装插件

当前更新源默认走 GitHub；在「通用设置 → 更新源」切到 Gitee 时使用 Gitee 镜像路径。

## 安装与更新

推荐用软件内置的「插件管理 → 安装插件…」：选择 DLL 后自动复制到配置目录的 `plugins\`，架构不符会提前跳过，并可一键重启加载。

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
