"""插件商店的清单拉取必须离开 UI 线程。

打开商店、点「刷新」都不允许在窗口线程上同步等待 DNS/TLS/重定向：
先用已有快照填充，快照为空时后台线程拉取，完成后再 PostMessage 回来刷新。

静态契约；不联网、不启动 GUI。
"""
from pathlib import Path
import importlib.util
import sys

SUPPORT = Path(__file__).resolve().with_name("native_test_support.py")
spec = importlib.util.spec_from_file_location("store_async_support", SUPPORT)
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)


def main() -> int:
    problems = []
    source = native.source("PluginStoreDlg.cpp")

    init = native.method(source, "BOOL CPluginStoreDlg::OnInitDialog()")
    if "CheckForUpdate" in init:
        problems.append("OnInitDialog 不能在 UI 线程上直接拉取清单")
    if "RefreshList()" not in init:
        problems.append("OnInitDialog 必须先用已有快照填充列表")

    refresh = native.method(source, "void CPluginStoreDlg::RefreshList()")
    if "CheckForUpdate" in refresh:
        problems.append("RefreshList 只能读快照，网络请求属于 StartFetch")

    start = native.method(source, "void CPluginStoreDlg::StartFetch()")
    for token, message in (
        ("std::thread", "StartFetch 必须在工作线程里执行网络请求"),
        ("PostMessage", "StartFetch 完成后要 PostMessage 回 UI 线程"),
        ("WM_STORE_FETCH_DONE", "StartFetch 必须使用 WM_STORE_FETCH_DONE 通知"),
        ("IsWindow", "后台线程回投前必须确认对话框窗口仍有效"),
        ("m_fetching", "StartFetch 必须防止重复起线程"),
    ):
        if token not in start:
            problems.append(message)

    done = native.method(source, "LRESULT CPluginStoreDlg::OnStoreFetchDone(")
    if "RefreshList()" not in done:
        problems.append("拉取成功后必须在 UI 线程刷新列表")
    if "IDS_STORE_FETCH_FAILED" not in done:
        problems.append("拉取失败必须显示失败提示")

    button = native.method(source, "void CPluginStoreDlg::OnBnClickedStoreRefresh()")
    if "StartFetch()" not in button:
        problems.append("刷新按钮必须走 StartFetch")
    if "CheckForUpdate" in button:
        problems.append("刷新按钮不能在 UI 线程上直接拉取清单")

    if "ON_MESSAGE(WM_STORE_FETCH_DONE" not in source:
        problems.append("缺少 WM_STORE_FETCH_DONE 的消息映射")

    # 覆盖已加载的插件 DLL：删除会失败，但改名可以（Windows 加载器语义）。
    # 商店必须走「改名让位 -> 新文件顶替」，而不是让用户先退出程序。
    install = native.method(source, "void CPluginStoreDlg::OnBnClickedStoreInstall()")
    if "MoveLoadedPluginAside(" not in install:
        problems.append("覆盖已加载插件时必须先用 MoveLoadedPluginAside 改名让位")
    if "DeleteFileW(target_path) == FALSE" not in install:
        problems.append("必须先尝试删除，删不掉（被加载）才改名")
    aside = native.method(source, "bool MoveLoadedPluginAside(")
    if "MoveFileW(path, backup)" not in aside:
        problems.append("MoveLoadedPluginAside 必须用 MoveFileW 改名")

    manager = native.source("PluginManager.cpp")
    if "*.dll.pb-old*" not in manager:
        problems.append("LoadPlugins 必须清理上次改名留下的 .dll.pb-old 残留")

    for problem in problems:
        print("FAIL", problem)
    print(f"RESULT {len(problems)} problems")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    raise SystemExit(main())
