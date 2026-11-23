"""Keep both plugin update entry points off the window thread."""
from pathlib import Path
import importlib.util
import sys

spec = importlib.util.spec_from_file_location("support", Path(__file__).with_name("native_test_support.py"))
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)


def main():
    manager = native.source("PluginManagerDlg.cpp")
    store = native.source("PluginStoreDlg.cpp")
    helper = native.source("PluginUpdateHelper.cpp")
    init = native.method(manager, "BOOL CPluginManagerDlg::OnInitDialog()")
    check = native.method(manager, "void CPluginManagerDlg::OnBnClickedCheckPluginUpdates()")
    store_init = native.method(store, "BOOL CPluginStoreDlg::OnInitDialog()")
    store_fetch = native.method(store, "void CPluginStoreDlg::StartFetch()")
    refresh = native.method(helper, "void CPluginUpdateHelper::RefreshAsync()")
    manager_install = native.method(manager, "void CPluginManagerDlg::OnBnClickedManagePluginUpdate()")
    store_install = native.method(store, "void CPluginStoreDlg::OnBnClickedStoreInstall()")
    install = native.method(helper, "const wchar_t* CPluginPackageInstall::Start(")
    finish = native.method(helper, "CPluginPackageInstall::Completion CPluginPackageInstall::Finish(")
    problems = []
    if "OnBnClickedCheckPluginUpdates()" not in init or "RefreshAsync()" not in check:
        problems.append("Opening Plugin Manager must check independently of the startup preference")
    if "StartFetch()" not in store_init or "RefreshAsync()" not in store_fetch:
        problems.append("Plugin Store must use the shared catalog request on open and refresh")
    if not refresh.find("std::thread") < refresh.find("FetchPluginCatalog()") or "std::thread" not in refresh:
        problems.append("Catalog fetching must run within the shared worker")
    if "m_install.Start(" not in manager_install or "m_install.Start(" not in store_install:
        problems.append("Both dialogs must launch the shared verified download")
    if not 0 <= install.find("std::packaged_task") < install.find("DownloadBinaryFile(") or "std::thread" not in install:
        problems.append("Downloading and validation must happen before returning to the UI thread")
    if "CommitPluginUpdate(" not in finish or "MarkPendingRestart(" not in finish:
        problems.append("A verified download must be committed before offering a restart")
    for problem in problems:
        print("FAIL", problem)
    print(f"RESULT {len(problems)} problems")
    return int(bool(problems))


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    raise SystemExit(main())
