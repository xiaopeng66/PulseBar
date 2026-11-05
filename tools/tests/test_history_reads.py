"""Bounded history reads must not destroy a previously valid snapshot or disk file."""
from pathlib import Path
import importlib.util

spec = importlib.util.spec_from_file_location("history_regressions", Path(__file__).with_name("test_history_regressions.py"))
history = importlib.util.module_from_spec(spec)
spec.loader.exec_module(history)
native = history.native


def make_code():
    code = history.make_code().split("int wmain(", 1)[0]
    code += r'''
#include <type_traits>
#include <iterator>
string DiskBytes(const wstring& path) {ifstream in(path,ios::binary);return string(istreambuf_iterator<char>(in),{});}
template<class File> bool LoadResult(File& file) {
    if constexpr(is_void_v<decltype(file.Load())>) {file.Load();return true;}
    else return file.Load();
}
template<class File> bool PersistResult(File& file) {
    if constexpr(is_void_v<decltype(file.Save())>) {file.Save();return true;}
    else return file.Save();
}
int wmain(int argc,wchar_t** argv) {
    if(argc!=3) return 2;
    const wstring mode=argv[1],root=argv[2];
    if(root.find(L"E:\\AI\\Zcode\\tmp\\pulsebar-fix-20261105\\")!=0) return 3;
    const wstring path=root+L"\\read-"+mode+L".dat";
    const string original="lines: \"1\"\n2026/11/04 100/200\n";
    CHistoryTrafficFile file(path);
    bool ok=false;
    if(mode==L"missing") {
        ok=LoadResult(file)&&PersistResult(file)&&CCommon::FileExist(path.c_str());
    } else if(mode==L"empty" || mode==L"invalid") {
        Seed(path,mode==L"empty"?"":"not a history file\n");
        string before=DiskBytes(path);
        bool loaded=LoadResult(file);bool saved=PersistResult(file);
        ok=!loaded&&!saved&&DiskBytes(path)==before;
    } else if(mode==L"locked") {
        Seed(path,original);if(!LoadResult(file)) return 4;
        HANDLE lock=CreateFileW(path.c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(lock==INVALID_HANDLE_VALUE) return 5;
        bool loaded=LoadResult(file);CloseHandle(lock);
        bool saved=PersistResult(file);
        ok=!loaded&&!saved&&Has(file,4,100)&&DiskBytes(path)==original;
    } else if(mode==L"truncated" || mode==L"legacy-empty") {
        Seed(path,mode==L"truncated"?"lines: \"2\"\n2026/11/04 100/200\n":"lines: \"0\"\n");
        string before=DiskBytes(path);
        bool loaded=LoadResult(file);
        if(mode==L"legacy-empty") ok=loaded&&PersistResult(file);
        else ok=!loaded&&!PersistResult(file)&&DiskBytes(path)==before;
    } else if(mode==L"recover-backup" || mode==L"no-valid-backup") {
        Seed(path,"not a history file\n");
        if(mode==L"recover-backup") Seed(path+L".bak",original);
        theApp.m_history_traffic_path=path;CPulseBarDlg host(path);host.LoadHistoryTraffic();
        if(mode==L"recover-backup")
            ok=Has(host.m_history_traffic,4,100)&&PersistResult(host.m_history_traffic)
                &&DiskBytes(path).find("100/200")!=string::npos;
        else
            ok=!PersistResult(host.m_history_traffic)&&DiskBytes(path)=="not a history file\n";
    } else if(mode==L"readable") {
        Seed(path,original);ok=LoadResult(file)&&Has(file,4,100);
    }
    printf("%s history-read %ls\n",ok?"PASS":"FAIL",mode.c_str());return ok?0:1;
}
'''
    return code


def main():
    run, env = native.build(make_code(), "history-read")
    results = [native.execute(run, env, mode, timeout=2) for mode in ("readable", "missing", "empty", "invalid", "locked", "recover-backup", "no-valid-backup", "truncated", "legacy-empty")]
    print(f"ARTIFACTS {run}")
    return int(not all(results))


if __name__ == "__main__":
    raise SystemExit(main())
