"""Compile the actual disk acquisition branch and check runtime values across HI_HDD."""
from pathlib import Path
import importlib.util

spec = importlib.util.spec_from_file_location("native_support", Path(__file__).with_name("native_test_support.py"))
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)


def main():
    text = native.source("PulseBarDlg.cpp")
    start = text.index("    if (lite_version /*|| is_arm64ec*/ || !settings->IsHardwareEnable(HI_HDD))")
    end = text.index("    //获取内存利用率", start)
    block = text[start:end]
    code = r'''#include <string>
#include <vector>
#include <cstdio>
using namespace std;
constexpr int HI_HDD=4;
struct MonitorSettings {bool hdd=false;wstring hard_disk_name=L"_Total";bool IsHardwareEnable(int)const{return hdd;}};
struct App {int m_hdd_usage=0;bool m_disk_speed_available=false;unsigned long long m_disk_read_speed=0,m_disk_write_speed=0;} theApp;
struct Usage {int FindDiskIndex(const wstring&){return 0;}const vector<wstring>& GetDiskNames(){static vector<wstring> names{L"_Total"};return names;}
    bool GetDiskUsage(int,int& value){value=50;return true;}};
struct Speed {unsigned long long read=10,write=20;bool valid=true;
    bool GetDiskSpeed(unsigned long long& r,unsigned long long& w){r=read;w=write;return valid;}};
class Probe {public:bool lite_version=false,m_get_disk_usage_by_pdh=false;Usage m_disk_usage_helper;Speed m_disk_speed_helper;
void Sample(MonitorSettings* settings){wstring hard_disk_name=settings->hard_disk_name;
'''+block+r'''
}};
int wmain(int argc,wchar_t** argv) {
    if(argc!=3)return 2;wstring mode=argv[1];Probe p;MonitorSettings settings;
    if(mode==L"cold-on")settings.hdd=true;
    p.Sample(&settings);
    bool ok=theApp.m_disk_speed_available&&theApp.m_disk_read_speed==10&&theApp.m_disk_write_speed==20;
    if(mode==L"hot-on") {
        settings.hdd=true;p.m_disk_speed_helper.read=30;p.m_disk_speed_helper.write=40;p.Sample(&settings);
        ok=ok&&theApp.m_disk_read_speed==30&&theApp.m_disk_write_speed==40;
    }
    if(mode==L"failure") {
        settings.hdd=true;p.m_disk_speed_helper.valid=false;p.Sample(&settings);ok=ok&&!theApp.m_disk_speed_available;
    }
    printf("%s disk-speed %ls\n",ok?"PASS":"FAIL",mode.c_str());return ok?0:1;
}
'''
    run, env = native.build(code, "disk-speed")
    results = [native.execute(run, env, mode) for mode in ("off", "cold-on", "hot-on", "failure")]
    print(f"ARTIFACTS {run}")
    return int(not all(results))


if __name__ == "__main__":
    raise SystemExit(main())
