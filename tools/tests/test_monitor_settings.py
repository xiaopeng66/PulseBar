"""F02 native behavior regressions against the current sampling/settings sources.

Only test executables are built/run. OS, sensor, network and plugin boundaries
are fixture doubles; the sampling entry, settings publication/store and enable
sync are production code. No Bin/AppData/config/history files are read.
"""
from pathlib import Path
import importlib.util
import sys

SUPPORT = Path(__file__).resolve().with_name("native_test_support.py")
spec = importlib.util.spec_from_file_location("monitor_native_support", SUPPORT)
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)


def make_code():
    common = native.source("CommonData.h")
    app_header = native.source("PulseBar.h")
    app = native.source("PulseBar.cpp")
    dlg = native.source("PulseBarDlg.cpp")
    has_snapshot = (native.SOURCE / "MonitorSettings.h").is_file()
    code = r'''#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>
using namespace std;
using namespace std::chrono_literals;
// 生产代码里由 PluginManager.cpp 定义；夹具只提取调用点，这里给它一个空实现
inline void LogPluginCallbackFailure(const wchar_t*) noexcept {}
struct LanguageInfo { int language_id{}; };
using StringSet = set<wstring>;
'''
    code += native.method(common, "enum HardwareItem") + ";\n"
    code += native.method(common, "struct GeneralSettingData") + ";\n"
    if has_snapshot:
        code += '#include "MonitorSettings.h"\n'
    code += r'''
struct CString : wstring {
    using wstring::wstring; using wstring::operator=; using wstring::operator+=;
    CString(const wstring& value):wstring(value){}
    const wchar_t* GetString() const { return c_str(); }
    operator const wchar_t*() const { return c_str(); }
    void Replace(const wchar_t*, const wstring&) {}
    CString& operator+=(const char*) { return *this; }
};
constexpr int IDS_CONNECTION_NUM_CHANGED=1, IDS_CONNECTION_NOT_MATCH=2;
constexpr int IDS_HARDWARE_INFO_ACQUIRE_FAILED_ERROR=3, IDS_AVREAGE_TEMPERATURE=4;
constexpr UINT ID_SELETE_CONNECTION=10, WM_MONITOR_INFO_UPDATED=WM_APP+1;
#define _T(value) L##value
class CCommon {public:
    static CString LoadText(int id) { return id==4 ? L"Average" : L"fixture"; }
    static wstring IntToString(unsigned int v) { return to_wstring(v); }
    static void WriteLog(const wchar_t*, const wchar_t*) {}
};
struct CCriticalSection { recursive_mutex mutex; };
CCriticalSection* hardware_section{};
function<void()> before_hardware_lock;
class CSingleLock {
    unique_lock<recursive_mutex> lock;
public:
    CSingleLock(CCriticalSection* section, BOOL):lock(section->mutex, defer_lock) {
        if(section==hardware_section && before_hardware_lock) before_hardware_lock();
        lock.lock();
    }
    void Unlock() { lock.unlock(); }
};
void TestLocalTime(SYSTEMTIME* t) { *t={2026,11,0,5,12,0,0,0}; }
BOOL TestMemory(MEMORYSTATUSEX* t) { t->dwMemoryLoad=50; t->ullTotalPhys=8192; t->ullAvailPhys=4096; return TRUE; }
DWORD TestNumberOfInterfaces(DWORD* n) { *n=1; return NO_ERROR; }
#define GetLocalTime TestLocalTime
#define GlobalMemoryStatusEx TestMemory
#define GetNumberOfInterfaces TestNumberOfInterfaces
struct Sensor {
    map<wstring,float> cpu, temperatures, usage;
    bool cpu_enabled{}, gpu_enabled{}, hdd_enabled{}, mainboard_enabled{};
    function<void()> on_cpu_enable;
    void SetCpuEnable(bool v) { cpu_enabled=v; if(on_cpu_enable) on_cpu_enable(); }
    void SetGpuEnable(bool v) { gpu_enabled=v; }
    void SetHddEnable(bool v) { hdd_enabled=v; }
    void SetMainboardEnable(bool v) { mainboard_enabled=v; }
    void GetHardwareInfo() {}
    const auto& AllCpuTemperature() const { return cpu; }
    const auto& AllHDDTemperature() const { return temperatures; }
    const auto& AllHDDUsage() const { return usage; }
    float CpuTemperature() { return 51; }
    float GpuTemperature() { return 52; }
    float MainboardTemperature() { return 53; }
    int GpuUsage() { return 40; }
    float CpuFreq() { return 3.5f; }
};
namespace OpenHardwareMonitorApi {
    function<void()> on_create;
    shared_ptr<Sensor> CreateInstance() { if(on_create) on_create(); return make_shared<Sensor>(); }
    wstring GetErrorMessage() { return {}; }
}
struct ITMPlugin {
    struct MonitorInfo {
        unsigned long long up_speed{}, down_speed{};
        int cpu_usage{}, memory_usage{}, gpu_usage{}, hdd_usage{};
        float cpu_temperature{}, gpu_temperature{}, hdd_temperature{}, cpu_freq{}, main_board_temperature{};
    };
    void DataRequired() {} void OnMonitorInfo(const MonitorInfo&) {}
};
struct Plugins {
    struct Item { ITMPlugin* plugin{}; };
    vector<Item> items;
    const auto& GetPlugins() { return items; }
};
class CPulseBarApp { public:
'''
    code += app_header[app_header.index("    unsigned __int64 m_in_speed"):app_header.index("    bool m_cannot_save_config_warning")]
    code += r'''
    GeneralSettingData m_general_data{};
    struct Config { bool m_select_all{}, m_auto_select{}; string m_connection_name="fixture"; } m_cfg_data;
    bool m_debug_log{};
    wstring m_log_path, m_config_dir;
    Plugins m_plugins;
    CCriticalSection m_minitor_lib_critical;
    enum MonitorState { MONITOR_NONE=0, MONITOR_INITIALIZING=1, MONITOR_READY=2 };
    atomic<int> m_monitor_state{MONITOR_READY};
    shared_ptr<Sensor> m_pMonitor=make_shared<Sensor>();
    bool IsHardwareMonitorReady() const { return m_monitor_state==MONITOR_READY; }
    void RefreshTemperatureNameSnapshotLocked() {}
    void UpdateOpenHardwareMonitorEnableState();
    static UINT InitOpenHardwareMonitorLibThreadFunc(LPVOID);
    GeneralSettingData startup_data{};
    void LoadConfig() { m_general_data=startup_data; }
    void StartupSettingsForTest();
'''
    if has_snapshot:
        # The real store is included, and the real app methods are compiled below.
        code += '''    MonitorSettingsStore m_monitor_settings;
    void PublishMonitorSettings();
    shared_ptr<const MonitorSettings> GetMonitorSettingsSnapshot() const;
'''
    code += "} theApp;\n"
    if has_snapshot:
        for signature in ("void CPulseBarApp::PublishMonitorSettings()",
                          "std::shared_ptr<const MonitorSettings> CPulseBarApp::GetMonitorSettingsSnapshot() const"):
            code += native.method(app, signature) + "\n"
    for signature in ("void CPulseBarApp::UpdateOpenHardwareMonitorEnableState()",
                      "UINT CPulseBarApp::InitOpenHardwareMonitorLibThreadFunc("):
        code += native.method(app, signature) + "\n"
    # Extract the real UI commit/startup publication seams, not a replacement publisher.
    apply = native.method(dlg, "void CPulseBarDlg::ApplySettings(")
    commit = apply[apply.index("    theApp.m_general_data ="):apply.index("    theApp.m_cfg_data.m_notify_icon_selected =")]
    code += "void CommitUiSettings(const GeneralSettingData& value) {\n"
    code += "    struct { struct { GeneralSettingData m_data; } m_tab3_dlg; } optionsDlg{{value}};\n"
    code += commit + "}\n"
    startup = native.method(app, "BOOL CPulseBarApp::InitInstance()")
    code += "void CPulseBarApp::StartupSettingsForTest() {\n"
    code += startup[startup.index("    LoadConfig();"):startup.index("    m_taskbar_default_style.LoadConfig();")]
    code += "}\n"
    code += r'''
struct NetworkCounterSample {
    unsigned int interface_index{}; unsigned long long interface_luid{}, in_bytes{}, out_bytes{};
};
struct FixtureConnection { unsigned int interface_index=1; unsigned long long interface_luid=1; };
class CAdapterCommon { public:
    static bool GetInterfaceCounters(unsigned int index,unsigned long long luid,NetworkCounterSample& sample) {
        sample={index,luid,1000,2000}; return true;
    }
};
struct CpuUsage {
    bool used_cpu_time{}; function<void()> on_read;
    int GetCpuUsage(bool cpu_time) { used_cpu_time=cpu_time; if(on_read) on_read(); return 17; }
};
struct CpuFreq { bool GetCpuFreq(float&) { return false; } };
struct GpuUsage { bool GetGpuUsage(int& v) { v=22; return true; } };
struct GpuMemory {
    bool GetNvidiaGpuStatus(int&,int&,int&) { return false; }
    bool GetDedicatedGpuMemory(int&,unsigned long long&,unsigned long long&) { return false; }
};
struct DiskUsage {
    vector<wstring> names{L"_Total"};
    int FindDiskIndex(const wstring& value) { auto i=find(names.begin(),names.end(),value); return i==names.end()?-1:static_cast<int>(i-names.begin()); }
    const auto& GetDiskNames() const { return names; }
    bool GetDiskUsage(int index,int& value) { if(index<0) return false; value=10+index; return true; }
};
struct DiskSpeed { bool GetDiskSpeed(unsigned long long&,unsigned long long&) { return false; } };
struct Traffic {
    int day=5; unsigned long long up_kBytes{}, down_kBytes{};
    unsigned long long kBytes() const { return up_kBytes+down_kBytes; }
};
struct History {
    Traffic data;
    const Traffic& GetTodayTraffic() const { return data; }
    void SetTodayTrafficKBytes(unsigned long long up,unsigned long long down) { data.up_kBytes=up; data.down_kBytes=down; }
    void OnDateChanged() {}
};
struct Interface { unsigned long long dwInOctets=1000, dwOutOctets=2000; char bDescr[32]="fixture"; };
class CPulseBarDlg { public:
    CCriticalSection m_connection_lock, m_history_lock;
    vector<FixtureConnection> m_connections{FixtureConnection{}};
    int m_connection_selected{}, m_zero_speed_cnt{}, m_restart_cnt{};
    unsigned long long m_in_bytes{},m_out_bytes{};
    map<unsigned long long,NetworkCounterSample> m_network_baselines;
    bool m_connection_change_flag{},m_close_pending{},m_get_disk_usage_by_pdh{};
    unsigned int m_monitor_time_cnt{};
    History m_history_traffic;
    CpuUsage m_cpu_usage_helper; CpuFreq m_cpu_freq_helper; GpuUsage m_gpu_usage_helper;
    GpuMemory m_gpu_mem_helper; DiskUsage m_disk_usage_helper; DiskSpeed m_disk_speed_helper;
    bool UpdateIfTable(bool* resized) { *resized=false; return true; }
    Interface GetConnectIfTable(int) { return {}; }
    void RequestConnectionRefresh() {} bool SaveHistoryTraffic() { return true; }
    void PostMessage(UINT,WPARAM=0) {}
    void DoMonitorAcquisition();
    bool IsTemperatureNeeded() const;
'''
    if has_snapshot:
        code += "    static bool IsTemperatureNeeded(const MonitorSettings& settings);\n"
    code += "};\n"
    code += native.method(dlg, "bool CPulseBarDlg::IsTemperatureNeeded() const") + "\n"
    if has_snapshot:
        code += native.method(dlg, "bool CPulseBarDlg::IsTemperatureNeeded(const MonitorSettings&") + "\n"
    code += native.method(dlg, "static int GetMonitorTimerCount(") + "\n"
    code += native.method(dlg, "void CPulseBarDlg::DoMonitorAcquisition()") + "\n"
    code += r'''
bool Check(bool ok,const char* why) { if(!ok) printf("  %s\n",why); return ok; }
GeneralSettingData Settings(bool short_names) {
    GeneralSettingData v{};
    v.monitor_time_span=short_names?200:30000;
    v.cpu_usage_acquire_method=short_names?GeneralSettingData::CA_CPU_TIME:GeneralSettingData::CA_PDH;
    v.hardware_monitor_item=short_names?(HI_CPU|HI_MBD):(HI_GPU|HI_HDD);
    v.hard_disk_name=short_names?L"d":wstring(8192,L'D');
    v.cpu_core_name=short_names?L"c":wstring(4096,L'C');
    return v;
}
template<class Snapshot> bool Matches(const Snapshot& p) {
    const bool odd=(p->version%2)==1;
    return p->version>0 && p->monitor_time_span==(odd?200:30000)
        && p->cpu_usage_acquire_method==(odd?GeneralSettingData::CA_CPU_TIME:GeneralSettingData::CA_PDH)
        && p->hardware_monitor_item==static_cast<unsigned>(odd?(HI_CPU|HI_MBD):(HI_GPU|HI_HDD))
        && p->hard_disk_name==(odd?L"d":wstring(8192,L'D'))
        && p->cpu_core_name==(odd?L"c":wstring(4096,L'C'));
}
template<class App> bool SnapshotConcurrency(App& app) {
    if constexpr(requires { app.GetMonitorSettingsSnapshot(); }) {
        CommitUiSettings(Settings(true));
        const auto held=app.GetMonitorSettingsSnapshot();
        static_assert(is_const_v<remove_reference_t<decltype(*held)>>,"readers must not mutate a published snapshot");
        atomic<bool> done=false, ok=true;
        atomic<unsigned> ready=0, reads=0;
        vector<thread> readers;
        for(int r=0;r<4;++r) readers.emplace_back([&] {
            ++ready;
            do {
                auto p=app.GetMonitorSettingsSnapshot();
                if(!Matches(p)) ok=false;
                this_thread::yield();
                if(!Matches(p)) ok=false;
                ++reads;
            } while(!done.load());
        });
        while(ready!=4) this_thread::yield();
        for(int v=2;v<=4000;++v) CommitUiSettings(Settings((v%2)==1));
        done=true;
        for(auto& reader:readers) reader.join();
        const auto current=app.GetMonitorSettingsSnapshot();
        return Check(ok && reads>=4 && Matches(held) && held->version==1 && current->version==4000,
                     "published long/short strings, duration, flags and version must stay coherent and immutable");
    } else return Check(false,"production has no independently published sampling snapshot");
}
template<class App> bool PublishDoesNotWait(App& app) {
    if constexpr(requires { app.GetMonitorSettingsSnapshot(); }) {
        unique_lock<recursive_mutex> hardware(app.m_minitor_lib_critical.mutex);
        auto ui=async(launch::async,[&] { CommitUiSettings(Settings(true)); return Matches(app.GetMonitorSettingsSnapshot()); });
        const bool completed=ui.wait_for(1s)==future_status::ready;
        hardware.unlock();
        return Check(completed && ui.get(),"UI publication/read must finish while another thread owns the hardware lock");
    } else return Check(false,"UI cannot publish/read an independent snapshot");
}
template<class App> bool TimerSnapshot(App& app) {
    if constexpr(requires { app.GetMonitorSettingsSnapshot(); }) {
        CommitUiSettings(Settings(true));
        const auto held=app.GetMonitorSettingsSnapshot();
        CommitUiSettings(Settings(false));
        return Check(GetMonitorTimerCount(30,*held)==150 && GetMonitorTimerCount(3,*held)==15
            && GetMonitorTimerCount(3,*app.GetMonitorSettingsSnapshot())==1,
            "timer counts must use the caller's round, not re-read the UI interval");
    } else return Check(false,"timer helper cannot consume a pinned sampling interval");
}
bool Fallback(const wstring& mode) {
    GeneralSettingData selected{};
    selected.hard_disk_name=L"saved missing disk"; selected.cpu_core_name=L"saved missing core";
    if(mode==L"cpu-fallback") selected.hardware_monitor_item=HI_CPU;
    if(mode==L"hdd-temperature" || mode==L"hdd-usage") selected.hardware_monitor_item=HI_HDD;
    CommitUiSettings(selected);
    CPulseBarDlg host;
    if(mode==L"pdh-first") host.m_disk_usage_helper.names={L"first"};
    if(mode==L"cpu-fallback") theApp.m_pMonitor->cpu={{L"first core",47}};
    if(mode==L"hdd-temperature") {
        theApp.m_pMonitor->temperatures={{L"first disk",58}};
        theApp.m_pMonitor->usage={{L"first disk",36}};
    }
    if(mode==L"hdd-usage") theApp.m_pMonitor->usage={{L"first disk",68}};
    host.DoMonitorAcquisition();
    bool values=true;
    if(mode==L"pdh-total" || mode==L"pdh-first") values=theApp.m_hdd_usage==10;
    if(mode==L"cpu-fallback") values=theApp.m_cpu_temperature==47;
    if(mode==L"hdd-temperature") values=theApp.m_hdd_temperature==58 && theApp.m_hdd_usage==36;
    if(mode==L"hdd-usage") values=theApp.m_hdd_usage==68;
    return Check(values,"missing selected sensor must still produce fallback measurements")
        && Check(theApp.m_general_data.hard_disk_name==selected.hard_disk_name
                 && theApp.m_general_data.cpu_core_name==selected.cpu_core_name,
                 "worker fallback must not overwrite the UI's saved device/core choice");
}
bool OneRound() {
    auto old=Settings(true); old.hardware_monitor_item=HI_CPU|HI_GPU|HI_HDD|HI_MBD;
    old.hard_disk_name=L"old disk"; old.cpu_core_name=L"old core";
    auto newer=Settings(false); newer.hardware_monitor_item=0;
    CommitUiSettings(old);
    theApp.m_pMonitor->cpu={{old.cpu_core_name,45},{newer.cpu_core_name,85}};
    theApp.m_pMonitor->temperatures={{old.hard_disk_name,55},{newer.hard_disk_name,95}};
    theApp.m_pMonitor->usage={{old.hard_disk_name,65},{newer.hard_disk_name,95}};
    CPulseBarDlg host;
    // A UI commit lands after CPU sampling begins, before all other settings reads.
    host.m_cpu_usage_helper.on_read=[&] { CommitUiSettings(newer); };
    host.DoMonitorAcquisition();
    return Check(host.m_cpu_usage_helper.used_cpu_time && theApp.m_cpu_temperature==45
        && theApp.m_hdd_temperature==55 && theApp.m_hdd_usage==65 && theApp.m_gpu_usage==40,
        "a mid-round UI commit must not change that round's masks, method or selected sensors")
        && Check(theApp.m_general_data.hard_disk_name==newer.hard_disk_name,"worker must preserve the newer UI commit");
}
bool EnableSnapshot() {
    auto old=Settings(true); old.hardware_monitor_item=HI_CPU|HI_HDD;
    auto newer=Settings(false); newer.hardware_monitor_item=HI_GPU|HI_MBD;
    CommitUiSettings(old);
    theApp.m_general_data.hardware_monitor_item=0; // UI assignment is not published yet.
    auto sensor=theApp.m_pMonitor;
    sensor->on_cpu_enable=[&] { CommitUiSettings(newer); };
    theApp.UpdateOpenHardwareMonitorEnableState();
    const bool coherent=sensor->cpu_enabled && !sensor->gpu_enabled && sensor->hdd_enabled && !sensor->mainboard_enabled;
    sensor->on_cpu_enable={};
    theApp.UpdateOpenHardwareMonitorEnableState();
    const bool latest=!sensor->cpu_enabled && sensor->gpu_enabled && !sensor->hdd_enabled && sensor->mainboard_enabled;
    return Check(coherent && latest,"enable sync must pin one committed version without holding its lock across sensor APIs");
}
bool EnableLatestAfterWait() {
    auto old=Settings(true); auto newer=Settings(false);
    CommitUiSettings(old);
    hardware_section=&theApp.m_minitor_lib_critical;
    unique_lock<recursive_mutex> hardware(theApp.m_minitor_lib_critical.mutex);
    promise<void> waiting;
    before_hardware_lock=[&] { waiting.set_value(); };
    thread worker([] { theApp.UpdateOpenHardwareMonitorEnableState(); });
    waiting.get_future().wait();
    CommitUiSettings(newer);
    hardware.unlock(); worker.join(); before_hardware_lock={};
    const auto sensor=theApp.m_pMonitor;
    return Check(!sensor->cpu_enabled && sensor->gpu_enabled && sensor->hdd_enabled && !sensor->mainboard_enabled,
                 "enable sync must acquire the latest publication after waiting for hardware");
}
template<class App> bool StartupPublication(App& app) {
    if constexpr(requires { app.GetMonitorSettingsSnapshot(); }) {
        app.startup_data=Settings(true);
        app.StartupSettingsForTest();
        bool before_hardware=false;
        OpenHardwareMonitorApi::on_create=[&] {
            before_hardware=Matches(app.GetMonitorSettingsSnapshot());
            CommitUiSettings(Settings(false)); // Apply during slow construction.
        };
        app.InitOpenHardwareMonitorLibThreadFunc(nullptr);
        const auto sensor=app.m_pMonitor;
        return Check(before_hardware && app.IsHardwareMonitorReady() && !sensor->cpu_enabled
            && sensor->gpu_enabled && sensor->hdd_enabled && !sensor->mainboard_enabled,
            "startup must publish before hardware creation; initialization must apply the latest committed flags");
    } else return Check(false,"startup has no published snapshot before hardware initialization");
}
bool TemperatureSnapshot() {
    auto selected=Settings(true); selected.hardware_monitor_item=HI_CPU;
    CommitUiSettings(selected);
    theApp.m_general_data.hardware_monitor_item=0;
    return Check(CPulseBarDlg().IsTemperatureNeeded(),"temperature-needed must read the committed snapshot, not UI-owned data");
}
int wmain(int argc,wchar_t** argv) {
    if(argc!=3) return 2;
    const wstring mode=argv[1];
    bool ok=false;
    if(mode==L"snapshot-concurrency") ok=SnapshotConcurrency(theApp);
    else if(mode==L"publish-no-hardware-wait") ok=PublishDoesNotWait(theApp);
    else if(mode==L"timer-snapshot") ok=TimerSnapshot(theApp);
    else if(mode==L"one-round") ok=OneRound();
    else if(mode==L"enable-snapshot") ok=EnableSnapshot();
    else if(mode==L"enable-latest-after-wait") ok=EnableLatestAfterWait();
    else if(mode==L"startup-publication") ok=StartupPublication(theApp);
    else if(mode==L"temperature-snapshot") ok=TemperatureSnapshot();
    else ok=Fallback(mode);
    printf("%s monitor-settings %ls\n",ok?"PASS":"FAIL",mode.c_str());
    return ok?0:1;
}
'''
    return code


def main():
    run, env = native.build(make_code(), "monitor-settings")
    modes = ("pdh-total", "pdh-first", "cpu-fallback", "hdd-temperature", "hdd-usage",
             "one-round", "snapshot-concurrency", "publish-no-hardware-wait", "timer-snapshot",
             "enable-snapshot", "enable-latest-after-wait", "startup-publication", "temperature-snapshot")
    results = [native.execute(run, env, mode, timeout=20) for mode in modes]
    print(f"RESULT {sum(results)}/{len(results)}")
    print(f"ARTIFACTS {run}")
    return int(not all(results))


if __name__ == "__main__":
    raise SystemExit(main())
