"""The production disk sampler keeps physical numbers across gaps and finds later devices."""
from pathlib import Path
import importlib.util

spec = importlib.util.spec_from_file_location("native_support", Path(__file__).with_name("native_test_support.py"))
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)


def main():
    text = native.no_includes(native.source("PdhHardwareQuery/DiskSpeed.cpp"))
    methods = "\n".join(native.method(text, signature) for signature in (
        "CDiskSpeedMonitor::~CDiskSpeedMonitor()",
        "void CDiskSpeedMonitor::CloseHandles()",
        "void CDiskSpeedMonitor::EnsureHandles()",
        "bool CDiskSpeedMonitor::GetDiskSpeed(unsigned long long& read_bps, unsigned long long& write_bps)"))
    code = r'''#include <algorithm>
#include <cstdint>
#include <cwchar>
#include <set>
#include <string>
#include <vector>
using HANDLE=void*; using DWORD=unsigned long; using BOOL=int; using ULONGLONG=unsigned long long;
using SIZE_T=std::size_t; using LARGE_INTEGER=struct Counter;
struct Counter {long long QuadPart{};}; struct DISK_PERFORMANCE {Counter BytesRead,BytesWritten;};
const HANDLE INVALID_HANDLE_VALUE=reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1));
constexpr DWORD FILE_SHARE_READ=1,FILE_SHARE_WRITE=2,OPEN_EXISTING=3,IOCTL_DISK_PERFORMANCE=4;
std::set<int> present{0,2};std::set<int> opened;long long bytes[16]{};ULONGLONG tick{};
HANDLE CreateFileW(const wchar_t* path,DWORD,DWORD,void*,DWORD,DWORD,void*) {
    const int drive=path[std::wcslen(path)-1]-L'0'; if(!present.count(drive))return INVALID_HANDLE_VALUE;
    opened.insert(drive);return reinterpret_cast<HANDLE>(static_cast<intptr_t>(drive+1));
}
BOOL CloseHandle(HANDLE){return 1;}
BOOL DeviceIoControl(HANDLE handle,DWORD,void*,DWORD,DISK_PERFORMANCE* output,DWORD,DWORD*,void*) {
    const int drive=static_cast<int>(reinterpret_cast<intptr_t>(handle)-1);bytes[drive]+=100;
    output->BytesRead.QuadPart=bytes[drive];output->BytesWritten.QuadPart=bytes[drive]*2;return 1;
}
ULONGLONG GetTickCount64(){tick+=1000;return tick;}
int swprintf_s(wchar_t* out,const wchar_t* format,int value){return std::swprintf(out,64,format,value);}
class CDiskSpeedMonitor {public:CDiskSpeedMonitor()=default;~CDiskSpeedMonitor();bool GetDiskSpeed(unsigned long long&,unsigned long long&);
private:void EnsureHandles();void CloseHandles();std::vector<HANDLE> m_handles;std::vector<int> m_disk_numbers;
    std::vector<long long> m_prev_read,m_prev_write;std::vector<bool> m_prev_valid;
    ULONGLONG m_last_tick{},m_last_scan_tick{};bool m_has_prev{},m_rescan_requested{true};};
'''+methods+r'''
int wmain(int argc,wchar_t** argv){if(argc!=3||std::wstring(argv[2]).find(PULSEBAR_TEST_ROOT)!=0)return 2;
    CDiskSpeedMonitor monitor;unsigned long long read{},write{};monitor.GetDiskSpeed(read,write);
    if(opened!=std::set<int>{0,2})return 10;
    if(!monitor.GetDiskSpeed(read,write)||read==0||write==0)return 11;
    present.insert(3);monitor.GetDiskSpeed(read,write);monitor.GetDiskSpeed(read,write);
    if(!opened.count(3))return 12;
    const auto before=read;if(!monitor.GetDiskSpeed(read,write)||read<=before)return 13;
    return 0;}
'''
    run, env = native.build(code, "disk-enumeration")
    result = native.execute(run, env, "enumerate")
    print(f"ARTIFACTS {run}")
    return int(not result)


if __name__ == "__main__":
    raise SystemExit(main())
