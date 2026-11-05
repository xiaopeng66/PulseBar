"""Regression tests for current production history methods and host recovery."""
from pathlib import Path
import importlib.util
import sys

spec = importlib.util.spec_from_file_location("native_support", Path(__file__).with_name("native_test_support.py"))
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)


def make_code():
    data = native.source("CommonData.h")
    code = r'''#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <deque>
#include <list>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <initializer_list>
using namespace std;
using CString = wstring;
SYSTEMTIME test_clock = {2026,11,0,4,12,0,0,0};
void TestLocalTime(SYSTEMTIME* value) { *value = test_clock; }
#define GetLocalTime TestLocalTime
constexpr int IDS_HISTORY_TRAFFIC_LOST_ERROR_LOG = 1;
class CCommon {public:
    static bool FileExist(const wchar_t* path) {return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;}
    static CString LoadTextFormat(int, initializer_list<size_t>) {return L"recovered";}
    static void WriteLog(const CString&, const wchar_t*) {}
};
'''
    code += data[data.index("struct Date"):data.index("//历史流量统计中")]
    common = native.source("CommonData.cpp")
    for sig in ("bool Date::DateGreater(", "bool Date::DateEqual(", "unsigned __int64 HistoryTraffic::kBytes() const"):
        code += native.method(common, sig) + "\n"
    atomic = '#include "AtomicFile.h"\n' if (native.SOURCE / "AtomicFile.h").exists() else ""
    code += atomic + native.no_includes(native.source("HistoryTrafficFile.h")) + "\n"
    code += native.no_includes(native.source("HistoryTrafficFile.cpp")) + "\n"
    code += r'''
struct App {
    wstring m_history_traffic_path, m_log_path;
    unsigned long long m_today_up_traffic=0, m_today_down_traffic=0;
} theApp;
class CPulseBarDlg {public:
    CHistoryTrafficFile m_history_traffic;
    explicit CPulseBarDlg(const wstring& path):m_history_traffic(path){}
    void LoadHistoryTraffic();
};
'''
    code += native.method(native.source("PulseBarDlg.cpp"), "void CPulseBarDlg::LoadHistoryTraffic()")
    code += r'''
void Seed(const wstring& path, const string& data) {ofstream out(path, ios::binary); out << data;}
bool Has(const CHistoryTrafficFile& file, int day, unsigned long long up) {
    for (const auto& row:file.GetTraffics()) if(row.day==day && row.up_kBytes==up) return true;
    return false;
}
int wmain(int argc,wchar_t** argv) {
    if(argc!=3) return 2;
    const wstring mode=argv[1], root=argv[2];
    if(root.find(L"E:\\AI\\Zcode\\tmp\\pulsebar-fix-20261105\\")!=0) return 3;
    const wstring path=root+L"\\"+mode+L".dat";
    bool ok=false;
    if(mode==L"rollover") {
        Seed(path,"lines: \"2\"\n2026/11/04 100/200\n2026/11/03 300/400\n");
        CHistoryTrafficFile file(path); file.Load(); test_clock.wDay=5; file.OnDateChanged();
        file.SetTodayTrafficKBytes(10,0); file.SaveTodayOnly();
        CHistoryTrafficFile restarted(path); restarted.Load();
        ok=Has(restarted,4,100)&&Has(restarted,3,300)&&Has(restarted,5,10);
    } else if(mode==L"equal-backup") {
        test_clock.wDay=5;
        Seed(path,"lines: \"2\"\n2026/11/05 500/600\n2026/11/03 700/800\n");
        Seed(path+L".bak","lines: \"2\"\n2026/11/04 100/200\n2026/11/03 999/999\n");
        theApp.m_history_traffic_path=path; CPulseBarDlg host(path); host.LoadHistoryTraffic();
        ok=Has(host.m_history_traffic,4,100)&&Has(host.m_history_traffic,3,700)
            &&Has(host.m_history_traffic,5,500)&&host.m_history_traffic.GetTraffics().size()==3;
    } else if(mode==L"same-day") {
        Seed(path,"lines: \"2\"\n2026/11/04 100/200\n2026/11/03 300/400\n");
        CHistoryTrafficFile file(path); file.Load(); file.SaveTodayOnly(); file.SaveTodayOnly();
        CHistoryTrafficFile restarted(path); restarted.Load();
        ok=Has(restarted,4,100)&&Has(restarted,3,300)&&restarted.GetTraffics().size()==2;
    }
    printf("%s history %ls\n",ok?"PASS":"FAIL",mode.c_str());
    return ok?0:1;
}
'''
    return code


def main():
    run, env = native.build(make_code(), "history")
    results = [native.execute(run, env, mode) for mode in ("rollover", "equal-backup", "same-day")]
    print(f"ARTIFACTS {run}")
    return int(not all(results))


if __name__ == "__main__":
    raise SystemExit(main())
