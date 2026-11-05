"""F03/F04 RED: inaccessible existing backup must not be overwritten after recovery failed.

Extract current production LoadHistoryTraffic, SaveHistoryTrafficFull,
BackupHistoryTrafficFile, complete CHistoryTrafficFile and CAtomicFile.
Use real Win32 exclusive handles only on new synthetic files in this review dir.
Clock/log/MFC lock are fixture boundaries; no real application/config is opened.
The three public test APIs retain their actual production control flow.
"""
from pathlib import Path
import importlib.util

HERE = Path(__file__).resolve().parent
TESTS = HERE if (HERE / "native_test_support.py").is_file() else Path("E:/AI/Zcode/projects/traffic-monitor/src/tools/tests")
spec = importlib.util.spec_from_file_location("history_base", TESTS / "test_history_regressions.py")
history = importlib.util.module_from_spec(spec)
spec.loader.exec_module(history)
native = history.native
if "review-" in HERE.name:
    native.RUN_ROOT = HERE


def make_code():
    code = history.make_code().split("int wmain(", 1)[0]
    dlg = native.source("PulseBarDlg.cpp")
    code = code.replace("class CPulseBarDlg {public:", "class CSingleLock {public: CSingleLock(void*,BOOL) {} };\nclass CPulseBarDlg {public:\n    int m_history_lock{};")
    for signature in ("bool CPulseBarDlg::SaveHistoryTrafficFull()", "bool CPulseBarDlg::BackupHistoryTrafficFile()"):
        body = native.method(dlg, signature)
        declaration = body[:body.index("{")].strip().replace("CPulseBarDlg::", "") + ";"
        code = code.replace("    void LoadHistoryTraffic();", "    void LoadHistoryTraffic();\n    " + declaration, 1)
        code += body + "\n"
    code += r'''
#include <iterator>
string ReadRaw(const wstring& path) { ifstream f(path,ios::binary); return string(istreambuf_iterator<char>(f),{}); }
int wmain(int argc,wchar_t** argv) {
    if(argc!=3) return 2;
    const wstring mode=argv[1],root=argv[2];
    if(root.find(L"E:\\AI\\Zcode\\tmp\\pulsebar-fix-20261105\\")!=0) return 3;
    test_clock.wDay=5;
    const wstring path=root+L"\\review-"+mode+L".dat";
    const string backup="lines: \"2\"\r\n2026/11/04 100/200\r\n2026/11/03 300/400\r\n";
    const string main="lines: \"1\"\r\n2026/11/05 10/20\r\n";
    const bool missing=mode==L"missing-both" || mode==L"valid-main-missing-backup";
    const bool invalid=mode==L"valid-main-invalid-backup";
    if(!missing) Seed(path+L".bak",invalid?"unreadable history record":backup);
    const bool have_main=mode.rfind(L"valid-main",0)==0;
    if(have_main) Seed(path,main);
    const bool locked=mode==L"missing-main-locked-backup" || mode==L"valid-main-locked-backup";
    HANDLE handle=INVALID_HANDLE_VALUE;
    if(locked) {
        handle=CreateFileW((path+L".bak").c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(handle==INVALID_HANDLE_VALUE) return 4;
    }
    theApp.m_history_traffic_path=path;
    CPulseBarDlg host(path); host.LoadHistoryTraffic();
    const bool recovered_before=Has(host.m_history_traffic,4,100) && Has(host.m_history_traffic,3,300);
    if(handle!=INVALID_HANDLE_VALUE) CloseHandle(handle);
    host.m_history_traffic.SetTodayTrafficKBytes(21,31);
    const bool saved=host.SaveHistoryTrafficFull();
    const bool backed=saved && host.BackupHistoryTrafficFile();
    CHistoryTrafficFile reread(path+L".bak"); const bool loaded=reread.Load();
    const bool old_days=loaded && Has(reread,4,100) && Has(reread,3,300);
    // Either fail closed or successfully recover before overwrite. Mere success is not enough.
    const bool ok=missing ? (saved && backed && loaded && Has(reread,5,21))
        : invalid ? (saved && !backed && ReadRaw(path+L".bak")=="unreadable history record")
        : locked ? (old_days && ReadRaw(path+L".bak")==backup)
        : (recovered_before && saved && backed && old_days);
    printf("%s mode=%ls load_recovered_days=%d save=%d backup=%d backup_preserved_days=%d\n",
        ok?"PASS":"FAIL",mode.c_str(),recovered_before,saved,backed,old_days);
    return ok?0:1;
}
'''
    return code


def main():
    run, env = native.build(make_code(), "history-backup-recovery")
    modes = ("missing-main-readable-backup", "valid-main-readable-backup", "missing-main-locked-backup", "valid-main-locked-backup",
             "missing-both", "valid-main-missing-backup", "valid-main-invalid-backup")
    results = [native.execute(run, env, mode) for mode in modes]
    print(f"RESULT {sum(results)}/{len(results)}")
    print(f"ARTIFACTS {run}")
    return int(not all(results))


if __name__ == "__main__":
    raise SystemExit(main())
