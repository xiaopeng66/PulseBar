"""F06/F07 RED: current network acquisition -> delta -> in-memory history.

Compile the current production methods, not an audit/frozen copy. Extract one
contiguous prefix of DoMonitorAcquisition through the history counter update;
exclude subsequent persistence, sensor, plugin and UI work. All network APIs
and the clock are fixture boundaries: no real adapters, application or UI run.
"""
from pathlib import Path
import importlib.util

SUPPORT = Path(__file__).resolve().with_name("native_test_support.py")
spec = importlib.util.spec_from_file_location("network_native_support", SUPPORT)
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)


def make_code():
    dlg = native.source("PulseBarDlg.cpp")
    header = native.source("PulseBarDlg.h")
    common = native.source("CommonData.h")
    code = r'''#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include "MonitorSettings.h"
using namespace std;
struct VirtualInterface {
    DWORD identity;
    const char* description;
    ULONGLONG received;
    ULONGLONG sent;
    ULONGLONG luid{};
};
vector<VirtualInterface> fixture_interfaces;
DWORD fixture_failed_identity{};
bool fixture_entry_api_available=true;
ULONGLONG fixture_tick{};
unsigned fixture_table_calls{};
DWORD fixture_table_error=NO_ERROR;

// The legacy API really exposes only DWORD counters. Deliberately truncate here
// so a production caller cannot obtain 64-bit bytes by widening MIB_IFROW later.
DWORD FixtureGetIfTable(MIB_IFTABLE* table, ULONG* size, BOOL) {
    ++fixture_table_calls;
    if(fixture_table_error!=NO_ERROR) return fixture_table_error;
    const auto required=static_cast<ULONG>(max<size_t>(sizeof(MIB_IFTABLE),
        offsetof(MIB_IFTABLE,table)+fixture_interfaces.size()*sizeof(MIB_IFROW)));
    const auto capacity=*size; *size=required;
    if(table==nullptr || capacity<required) return ERROR_INSUFFICIENT_BUFFER;
    memset(table,0,required);
    table->dwNumEntries=static_cast<DWORD>(fixture_interfaces.size());
    for(size_t i=0;i<fixture_interfaces.size();++i) {
        const auto& source=fixture_interfaces[i];
        auto& row=table->table[i];
        row.dwIndex=source.identity; row.dwOperStatus=IF_OPER_STATUS_OPERATIONAL;
        row.dwType=IF_TYPE_ETHERNET_CSMACD;
        row.dwInOctets=static_cast<DWORD>(source.received);
        row.dwOutOctets=static_cast<DWORD>(source.sent);
        row.dwDescrLen=static_cast<DWORD>(strlen(source.description));
        memcpy(row.bDescr,source.description,row.dwDescrLen+1);
    }
    return NO_ERROR;
}

// Reserved OS seam for the approved 64-bit, identity-based production reader.
// It returns only the requested interface; row order is never an identity.
NETIO_STATUS NETIOAPI_API_ FixtureGetIfEntry2(MIB_IF_ROW2* row) {
    if(!fixture_entry_api_available) return ERROR_NOT_SUPPORTED;
    for(const auto& source:fixture_interfaces) {
        const auto luid=source.luid!=0?source.luid:source.identity;
        if(row->InterfaceLuid.Value!=0 ? row->InterfaceLuid.Value!=luid : row->InterfaceIndex!=source.identity)
            continue;
        if(source.identity==fixture_failed_identity) return ERROR_GEN_FAILURE;
        *row={}; row->InterfaceIndex=source.identity; row->InterfaceLuid.Value=luid;
        row->OperStatus=IfOperStatusUp; row->InOctets=source.received; row->OutOctets=source.sent;
        return NO_ERROR;
    }
    return ERROR_FILE_NOT_FOUND;
}
HMODULE WINAPI FixtureGetModuleHandleW(LPCWSTR name) {
    return _wcsicmp(name,L"iphlpapi.dll")==0?reinterpret_cast<HMODULE>(1):nullptr;
}
FARPROC WINAPI FixtureGetProcAddress(HMODULE,LPCSTR name) {
    return fixture_entry_api_available && strcmp(name,"GetIfEntry2")==0
        ?reinterpret_cast<FARPROC>(&FixtureGetIfEntry2):nullptr;
}
ULONG FixtureGetAdaptersInfo(IP_ADAPTER_INFO* info, ULONG* size) {
    const auto count=fixture_interfaces.size();
    const auto required=static_cast<ULONG>(max<size_t>(1,count)*sizeof(IP_ADAPTER_INFO));
    const auto capacity=*size; *size=required;
    if(info==nullptr || capacity<required) return ERROR_BUFFER_OVERFLOW;
    if(count==0) return ERROR_NO_DATA;
    memset(info,0,required);
    for(size_t i=0;i<count;++i) {
        auto& adapter=info[i]; const auto& source=fixture_interfaces[i];
        adapter.Index=source.identity; adapter.Type=MIB_IF_TYPE_ETHERNET;
        strcpy_s(adapter.Description,source.description);
        strcpy_s(adapter.AdapterName,source.description);
        strcpy_s(adapter.IpAddressList.IpAddress.String,"192.0.2.1");
        strcpy_s(adapter.IpAddressList.IpMask.String,"255.255.255.0");
        strcpy_s(adapter.GatewayList.IpAddress.String,"192.0.2.254");
        adapter.Next=(i+1<count)?&info[i+1]:nullptr;
    }
    return NO_ERROR;
}
ULONGLONG FixtureGetTickCount64() { return fixture_tick; }
void FixtureGetLocalTime(SYSTEMTIME* value) { *value={2026,11,0,5,12,0,0,0}; }
#define GetIfTable FixtureGetIfTable
#define GetIfEntry2 FixtureGetIfEntry2
#define GetModuleHandleW FixtureGetModuleHandleW
#define GetProcAddress FixtureGetProcAddress
#define GetAdaptersInfo FixtureGetAdaptersInfo
#define GetTickCount64 FixtureGetTickCount64
#define GetLocalTime FixtureGetLocalTime
#define _T(value) L##value
constexpr int IDS_NO_CONNECTION=1;
constexpr UINT ID_SELETE_CONNECTION=10;
struct CCriticalSection { recursive_mutex mutex; };
struct CSingleLock {
    unique_lock<recursive_mutex> lock;
    CSingleLock(CCriticalSection* section,BOOL):lock(section->mutex){}
    void Unlock() { lock.unlock(); }
};
class CCommon { public:
    static wstring StrToUnicode(const char* value) { return wstring(value,value+strlen(value)); }
    static string UnicodeToStr(const wstring& value) { return string(value.begin(),value.end()); }
    static wstring LoadText(const wchar_t*,int,const wchar_t*) { return L"<fixture unavailable>"; }
    static double StringSimilarDegree_LD(const string& a,const string& b) { return a==b?1.:0.; }
};
'''
    adapter_header = (native.SOURCE / "AdapterCommon.h").read_bytes()
    try:
        adapter_header_text = adapter_header.decode("utf-8-sig")
    except UnicodeDecodeError:
        adapter_header_text = adapter_header.decode("gbk")
    code += native.no_includes(adapter_header_text) + "\n"
    code += native.no_includes(native.source("AdapterCommon.cpp")) + "\n"
    code += native.method(common, "struct Date") + ";\n"
    code += native.method(common, "struct HistoryTraffic") + ";\n"
    code += native.method(native.source("CommonData.cpp"), "unsigned __int64 HistoryTraffic::kBytes() const") + "\n"
    code += r'''
struct History {
    HistoryTraffic today;
    History() { today.year=2026; today.month=11; today.day=5; }
    const HistoryTraffic& GetTodayTraffic() const { return today; }
    void SetTodayTrafficKBytes(unsigned long long up,unsigned long long down) { today.up_kBytes=up; today.down_kBytes=down; }
    void OnDateChanged() { today={}; today.year=2026; today.month=11; today.day=5; }
};
struct App {
    unsigned __int64 m_in_speed{},m_out_speed{},m_today_up_traffic{},m_today_down_traffic{};
    struct Config { bool m_select_all{},m_auto_select{}; string m_connection_name; } m_cfg_data;
    shared_ptr<const MonitorSettings> settings=make_shared<MonitorSettings>();
    shared_ptr<const MonitorSettings> GetMonitorSettingsSnapshot() const { return settings; }
} theApp;
class CPulseBarDlg { public:
'''
    # Keep actual production counter/list field types (widening only a test
    # surrogate would conceal the narrowing bug).
    code += header[header.index("    CCriticalSection m_connection_lock;"):header.index("    CCPUUsage m_cpu_usage_helper;")]
    code += r'''
    CCriticalSection m_history_lock;
    History m_history_traffic;
    atomic_bool m_connection_change_flag{false};
    int m_zero_speed_cnt{}, refresh_requests{};
    bool UpdateIfTable(bool* resized=nullptr);
    MIB_IFROW GetConnectIfTable(int connection_index);
    NetWorkConection GetConnection(int connection_index);
    void RequestConnectionRefresh() { ++refresh_requests; }
    void PostMessage(UINT,WPARAM=0) {}
    void DoMonitorAcquisition();
    ~CPulseBarDlg() { free(m_pIfTable); }
};
'''
    for signature in ("bool CPulseBarDlg::UpdateIfTable(",
                      "MIB_IFROW CPulseBarDlg::GetConnectIfTable(",
                      "NetWorkConection CPulseBarDlg::GetConnection(",
                      "static int GetMonitorTimerCount("):
        code += native.method(dlg, signature) + "\n"
    sample = native.method(dlg, "void CPulseBarDlg::DoMonitorAcquisition()")
    end = sample.index("m_history_traffic.SetTodayTrafficKBytes(")
    end = sample.index(";", end) + 1
    code += sample[:end] + "\n}\n"
    code += r'''
VirtualInterface Row(DWORD id,const char* name,ULONGLONG received,ULONGLONG sent) {
    return {id,name,received,sent};
}
bool Check(bool condition,const char* message) {
    if(!condition) printf("  %s\n",message);
    return condition;
}
void Sample(CPulseBarDlg& host) {
    fixture_tick+=1000;
    host.DoMonitorAcquisition();
}
bool Start(CPulseBarDlg& host,const char* selected,bool filtered=false,bool all=false) {
    if(!host.UpdateIfTable()) return Check(false,"fixture cannot initialize the production table buffer");
    if(filtered) {
        NetWorkConection connection{}; connection.description=selected;
        host.m_connections.push_back(connection);
        CAdapterCommon::GetIfTableInfo(host.m_connections,host.m_pIfTable);
    } else CAdapterCommon::GetAllIfTableInfo(host.m_connections,host.m_pIfTable);
    bool found=false;
    for(size_t i=0;i<host.m_connections.size();++i) {
        if(host.m_connections[i].description_2==selected) {
            host.m_connection_selected=static_cast<int>(i); found=true; break;
        }
    }
    if(!found) return Check(false,"fixture selected interface is not in production connection mapping");
    theApp.m_cfg_data.m_select_all=all;
    theApp.m_cfg_data.m_connection_name=selected;
    host.m_connection_change_flag=true;
    Sample(host);
    return Check(theApp.m_in_speed==0 && theApp.m_out_speed==0
        && theApp.m_today_down_traffic==0 && theApp.m_today_up_traffic==0,
        "the initial sample must only establish a baseline, not count lifetime traffic");
}
bool Expect(const CPulseBarDlg& host,ULONGLONG raw_in,ULONGLONG raw_out,
    ULONGLONG down_speed,ULONGLONG up_speed,ULONGLONG today_down,ULONGLONG today_up,
    ULONGLONG history_down_kb,ULONGLONG history_up_kb) {
    bool ok=Check(host.m_in_bytes==raw_in && host.m_out_bytes==raw_out,
                  "sampled counters must be full-width bytes of the selected interface(s)");
    ok=Check(theApp.m_in_speed==down_speed && theApp.m_out_speed==up_speed,
             "per-direction delta at a 1000 ms interval must reach the matching speed") && ok;
    ok=Check(theApp.m_today_down_traffic==today_down && theApp.m_today_up_traffic==today_up,
             "only valid direction deltas must reach the exact in-memory daily byte totals") && ok;
    ok=Check(host.m_history_traffic.today.down_kBytes==history_down_kb
             && host.m_history_traffic.today.up_kBytes==history_up_kb,
             "history KB fields must reflect the same deltas, not lifetime/other-interface bytes") && ok;
    if(!ok) {
        printf("  observed raw=%llu/%llu speed=%llu/%llu today=%llu/%llu historyKB=%llu/%llu\n",
            host.m_in_bytes,host.m_out_bytes,theApp.m_in_speed,theApp.m_out_speed,
            theApp.m_today_down_traffic,theApp.m_today_up_traffic,
            host.m_history_traffic.today.down_kBytes,host.m_history_traffic.today.up_kBytes);
        printf("  expected raw=%llu/%llu speed=%llu/%llu today=%llu/%llu historyKB=%llu/%llu\n",
            raw_in,raw_out,down_speed,up_speed,today_down,today_up,history_down_kb,history_up_kb);
    }
    return ok;
}
bool Normal() {
    fixture_interfaces={Row(11,"A",1024,2048)};
    CPulseBarDlg host; if(!Start(host,"A")) return false;
    fixture_interfaces={Row(11,"A",4096,6144)}; Sample(host);
    return Expect(host,4096,6144,3072,4096,3072,4096,3,4);
}
bool CrossDword() {
    fixture_interfaces={Row(11,"A",0xffffff00ULL,65536)};
    CPulseBarDlg host; if(!Start(host,"A")) return false;
    fixture_interfaces={Row(11,"A",0x100000500ULL,69632)}; Sample(host);
    bool ok=Expect(host,0x100000500ULL,69632,1536,4096,1536,4096,1,4);
    fixture_interfaces={Row(11,"A",0x100000f00ULL,71680)}; Sample(host);
    return Expect(host,0x100000f00ULL,71680,2560,2048,4096,6144,4,6) && ok;
}
bool MultipleDwordDelta() {
    fixture_interfaces={Row(11,"A",0x1000ULL,0x2000ULL)};
    CPulseBarDlg host; if(!Start(host,"A")) return false;
    fixture_interfaces={Row(11,"A",0x200003000ULL,0x100006000ULL)}; Sample(host);
    return Expect(host,0x200003000ULL,0x100006000ULL,0x200002000ULL,0x100004000ULL,
        0x200002000ULL,0x100004000ULL,8388616,4194320);
}
bool DirectionReset(bool incoming) {
    fixture_interfaces={Row(11,"A",100000,200000)};
    CPulseBarDlg host; if(!Start(host,"A")) return false;
    fixture_interfaces={incoming?Row(11,"A",512,204096):Row(11,"A",104096,512)};
    Sample(host);
    bool ok=incoming?Expect(host,512,204096,0,4096,0,4096,0,4)
        :Expect(host,104096,512,4096,0,4096,0,4,0);
    fixture_interfaces={incoming?Row(11,"A",2560,206144):Row(11,"A",106144,2560)};
    Sample(host);
    const bool recovered=incoming?Expect(host,2560,206144,2048,2048,2048,6144,2,6)
        :Expect(host,106144,2560,2048,2048,6144,2048,6,2);
    return recovered && ok;
}
bool Reordered(bool filtered) {
    fixture_interfaces={Row(11,"A",10000,20000),Row(22,"B",100000,200000)};
    CPulseBarDlg host; if(!Start(host,filtered?"B":"A",filtered)) return false;
    const auto capacity=host.m_dwSize;
    fixture_interfaces={Row(22,"B",104096,208192),Row(11,"A",12048,24096)};
    Sample(host);
    bool ok=filtered?Expect(host,104096,208192,4096,8192,4096,8192,4,8)
        :Expect(host,12048,24096,2048,4096,2048,4096,2,4);
    return Check(host.m_dwSize==capacity,"fixture reorders without a buffer resize") && ok;
}
bool ShorterTable(bool old_ordinal_outside) {
    fixture_interfaces={Row(11,"A",10000,20000),Row(22,"B",100000,200000),Row(33,"C",300000,400000)};
    CPulseBarDlg host; if(!Start(host,old_ordinal_outside?"C":"B")) return false;
    const auto capacity=host.m_dwSize;
    // B's cached ordinal 1 now points at C; C's cached ordinal 2 is outside
    // dwNumEntries although that memory remains inside the allocated buffer.
    fixture_interfaces={Row(22,"B",101024,202048),Row(33,"C",304096,408192)};
    Sample(host);
    bool ok=old_ordinal_outside?Expect(host,304096,408192,4096,8192,4096,8192,4,8)
        :Expect(host,101024,202048,1024,2048,1024,2048,1,2);
    return Check(host.m_dwSize==capacity && host.m_pIfTable->dwNumEntries==2,
                 "fixture shrinks the logical table, not its capacity") && ok;
}
bool RemovedInterface() {
    fixture_interfaces={Row(11,"A",10000,20000),Row(22,"B",100000,200000),Row(33,"C",300000,400000)};
    CPulseBarDlg host; if(!Start(host,"B")) return false;
    fixture_interfaces={Row(11,"A",12048,24096),Row(33,"C",304096,408192)};
    Sample(host);
    return Expect(host,0,0,0,0,0,0,0,0);
}
bool SelectAll64() {
    fixture_interfaces={Row(11,"A",0xfffffc00ULL,100000),Row(22,"B",0x1fffffc00ULL,200000)};
    CPulseBarDlg host; if(!Start(host,"A",false,true)) return false;
    fixture_interfaces={Row(11,"A",0x100000400ULL,104096),Row(22,"B",0x200000400ULL,208192)};
    Sample(host);
    return Expect(host,0x300000800ULL,312288,4096,12288,4096,12288,4,12);
}
bool ReadFailureRebaseline(bool selected_all) {
    fixture_interfaces={Row(11,"A",10000,20000),Row(22,"B",100000,200000)};
    CPulseBarDlg host; if(!Start(host,"A",false,selected_all)) return false;
    fixture_failed_identity=11;
    fixture_interfaces={Row(11,"A",50000,60000),Row(22,"B",101024,202048)};
    Sample(host);
    bool ok=selected_all?Expect(host,101024,202048,1024,2048,1024,2048,1,2)
        :Expect(host,0,0,0,0,0,0,0,0);
    fixture_failed_identity=0;
    fixture_interfaces={Row(11,"A",70000,80000),Row(22,"B",102048,204096)};
    Sample(host);
    ok=(selected_all?Expect(host,172048,284096,1024,2048,2048,4096,2,4)
        :Expect(host,70000,80000,0,0,0,0,0,0)) && ok;
    fixture_interfaces={Row(11,"A",72048,84096),Row(22,"B",103072,206144)};
    Sample(host);
    return (selected_all?Expect(host,175120,290240,3072,6144,5120,10240,5,10)
        :Expect(host,72048,84096,2048,4096,2048,4096,2,4)) && ok;
}
bool AllDirectionResets() {
    fixture_interfaces={Row(11,"A",100000,200000),Row(22,"B",300000,400000)};
    CPulseBarDlg host; if(!Start(host,"A",false,true)) return false;
    // Opposite directions reset on different interfaces: sum-then-subtract loses both valid deltas.
    fixture_interfaces={Row(11,"A",512,204096),Row(22,"B",308192,256)};
    Sample(host);
    bool ok=Expect(host,308704,204352,8192,4096,8192,4096,8,4);
    fixture_interfaces={Row(11,"A",2560,206144),Row(22,"B",309216,1280)};
    Sample(host);
    return Expect(host,311776,207424,3072,3072,11264,7168,11,7) && ok;
}
bool ReusedInterfaceIndex() {
    fixture_interfaces={Row(11,"A",10000,20000)}; fixture_interfaces[0].luid=111;
    CPulseBarDlg host; if(!Start(host,"A")) return false;
    fixture_interfaces={Row(11,"replacement",900000,1000000)}; fixture_interfaces[0].luid=211;
    Sample(host);
    bool ok=Expect(host,0,0,0,0,0,0,0,0);
    // A requested UI refresh adopts the new identity, but must not reuse old baseline.
    CAdapterCommon::GetAllIfTableInfo(host.m_connections,host.m_pIfTable);
    host.m_connection_change_flag=true;
    Sample(host);
    ok=Expect(host,900000,1000000,0,0,0,0,0,0) && ok;
    fixture_interfaces[0].received=901024; fixture_interfaces[0].sent=1002048;
    Sample(host);
    return Expect(host,901024,1002048,1024,2048,1024,2048,1,2) && ok;
}
bool Unavailable64BitApi() {
    fixture_entry_api_available=false;
    fixture_interfaces={Row(11,"A",10000,20000)};
    CPulseBarDlg host; if(!Start(host,"A")) return false;
    fixture_interfaces={Row(11,"A",12048,24096)}; Sample(host);
    return Expect(host,0,0,0,0,0,0,0,0);
}
bool ZeroBaseline() {
    fixture_interfaces={Row(11,"A",0,0)};
    CPulseBarDlg host; if(!Start(host,"A")) return false;
    fixture_interfaces={Row(11,"A",1024,2048)}; Sample(host);
    return Expect(host,1024,2048,1024,2048,1024,2048,1,2);
}
bool BufferGrowth() {
    fixture_interfaces={Row(11,"A",10000,20000)};
    CPulseBarDlg host; if(!Start(host,"A")) return false;
    const auto old_capacity=host.m_dwSize;
    fixture_interfaces={Row(11,"A",12048,24096),Row(22,"B",100000,200000),Row(33,"C",300000,400000)};
    Sample(host);
    return Check(host.m_dwSize>old_capacity && host.refresh_requests>0,
                 "legacy buffer growth must remain bounded and request a mapping refresh")
        && Expect(host,0,0,0,0,0,0,0,0);
}
int wmain(int argc,wchar_t** argv) {
    if(argc!=3) return 2;
    const wstring mode=argv[1];
    bool ok=false;
    if(mode==L"baseline-normal") ok=Normal();
    else if(mode==L"cross-dword") ok=CrossDword();
    else if(mode==L"multiple-dword-delta") ok=MultipleDwordDelta();
    else if(mode==L"reset-in") ok=DirectionReset(true);
    else if(mode==L"reset-out") ok=DirectionReset(false);
    else if(mode==L"same-capacity-reorder") ok=Reordered(false);
    else if(mode==L"filtered-reorder") ok=Reordered(true);
    else if(mode==L"shorter-table-shift") ok=ShorterTable(false);
    else if(mode==L"shorter-table-old-ordinal-outside") ok=ShorterTable(true);
    else if(mode==L"selected-interface-removed") ok=RemovedInterface();
    else if(mode==L"select-all-64bit") ok=SelectAll64();
    else if(mode==L"read-failure-rebaseline") ok=ReadFailureRebaseline(false);
    else if(mode==L"select-all-read-failure") ok=ReadFailureRebaseline(true);
    else if(mode==L"select-all-direction-resets") ok=AllDirectionResets();
    else if(mode==L"reused-interface-index") ok=ReusedInterfaceIndex();
    else if(mode==L"unavailable-64bit-api") ok=Unavailable64BitApi();
    else if(mode==L"zero-baseline") ok=ZeroBaseline();
    else if(mode==L"buffer-growth") ok=BufferGrowth();
    printf("%s network %ls\n",ok?"PASS":"FAIL",mode.c_str());
    return ok?0:1;
}
'''
    return code


def main():
    run, env = native.build(make_code(), "network")
    modes = ("baseline-normal", "cross-dword", "multiple-dword-delta", "reset-in", "reset-out",
             "same-capacity-reorder", "filtered-reorder", "shorter-table-shift",
             "shorter-table-old-ordinal-outside", "selected-interface-removed", "select-all-64bit",
             "read-failure-rebaseline", "select-all-read-failure", "select-all-direction-resets",
             "reused-interface-index", "unavailable-64bit-api", "zero-baseline", "buffer-growth")
    results = [native.execute(run, env, mode) for mode in modes]
    print(f"RESULT {sum(results)}/{len(results)}")
    print(f"ARTIFACTS {run}")
    return int(not all(results))


if __name__ == "__main__":
    raise SystemExit(main())
