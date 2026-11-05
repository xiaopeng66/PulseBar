"""F06/F07 RED: failed peer -> refresh handler -> mapping init -> next delta.

Base generator extracts current production AdapterCommon and acquisition prefix.
This generator restores the actual RequestConnectionRefresh/OnRefreshConnectionList/
IniConnection/AutoSelect methods. OS interface APIs and clocks use memory fixtures;
UI menus/timers are no-ops and PostMessage queues a command drained between rounds.
No real adapters, application settings, network I/O or window APIs are accessed.
F07 detail mode extracts current OnNetworkInfo snapshot and detail row reader.
"""
from pathlib import Path
import importlib.util

HERE = Path(__file__).resolve().parent
TESTS = HERE if (HERE / "native_test_support.py").is_file() else Path("E:/AI/Zcode/projects/traffic-monitor/src/tools/tests")
spec = importlib.util.spec_from_file_location("network_base", TESTS / "test_network_regressions.py")
network = importlib.util.module_from_spec(spec)
spec.loader.exec_module(network)
native = network.native
if "review-" in HERE.name:
    native.RUN_ROOT = HERE


def make_code():
    code = network.make_code().split("int wmain(", 1)[0]
    # Change only external OS and UI scaffolding; production reader/delta code is unchanged.
    code = code.replace("DWORD fixture_failed_identity{};", "DWORD fixture_failed_identity{}, fixture_down_identity{};")
    code = code.replace("row->OperStatus=IfOperStatusUp;", "row->OperStatus=source.identity==fixture_down_identity?IfOperStatusDown:IfOperStatusUp;")
    code = code.replace("class CCommon { public:", r'''
struct CString : wstring {
    using wstring::wstring; using wstring::operator=; using wstring::operator+=;
    CString(const wstring& value):wstring(value){}
    const wchar_t* GetString() const {return c_str();}
    operator const wchar_t*() const {return c_str();}
    CString& operator+=(const char*) {return *this;}
};
constexpr UINT ID_REFRESH_CONNECTION_LIST=11, DELAY_TIMER=12;
class CCommon { public:
    static wstring IntToString(unsigned int value) {return to_wstring(value);}
    static void WriteLog(const wchar_t*,const wchar_t*) {}
''')
    code = code.replace("struct App {", r'''
struct FixtureHidden {bool Contains(const wstring&) const {return false;}};
struct App {
    struct General {bool show_all_interface=false; FixtureHidden connections_hide;} m_general_data;
    bool m_debug_log=false;
    wstring m_config_dir;
''')
    seam = "    void RequestConnectionRefresh() { ++refresh_requests; }\n    void PostMessage(UINT,WPARAM=0) {}"
    assert seam in code, "base network UI fixture seam changed"
    code = code.replace(seam, r'''
    atomic_bool m_connection_refresh_pending{false};
    bool m_close_pending=false;
    int m_restart_cnt=-1;
    string m_connection_name_preferd="A";
    vector<WPARAM> queued_commands;
    void RequestConnectionRefresh();
    void OnRefreshConnectionList();
    void IniConnection();
    void AutoSelect();
    void IniTaskBarConnectionMenu() {}
    void InsertDynamicMenuItems() {}
    void KillTimer(UINT) {}
    void SetTimer(UINT,UINT,void*) {}
    BOOL PostMessage(UINT,WPARAM command=0) {queued_commands.push_back(command);++refresh_requests;return TRUE;}
    DWORD DetailSnapshotIdentity();
''')
    dlg = native.source("PulseBarDlg.cpp")
    for signature in ("void CPulseBarDlg::RequestConnectionRefresh()", "void CPulseBarDlg::OnRefreshConnectionList()",
                      "void CPulseBarDlg::IniConnection()", "void CPulseBarDlg::AutoSelect()"):
        code += native.method(dlg, signature) + "\n"
    code += r'''
class CNetworkInfoDlg {public:
    vector<NetWorkConection> m_connections;
    vector<MIB_IFROW> m_if_rows;
    int m_connection_selected{};
    CNetworkInfoDlg(const vector<NetWorkConection>& a,const vector<MIB_IFROW>& r,int i):m_connections(a),m_if_rows(r),m_connection_selected(i){}
    MIB_IFROW& GetConnectIfTable(int);
};
'''
    code += native.method(native.source("NetworkInfoDlg.cpp"), "MIB_IFROW& CNetworkInfoDlg::GetConnectIfTable(") + "\n"
    snap = native.method(dlg, "void CPulseBarDlg::OnNetworkInfo()")
    first = snap.index("    vector<NetWorkConection> connections;")
    end = snap.index("    CNetworkInfoDlg aDlg(")
    end = snap.index(";", end) + 1
    code += "DWORD CPulseBarDlg::DetailSnapshotIdentity() {\n" + snap[first:end] + "\n    return aDlg.GetConnectIfTable(connection_selected).dwIndex;\n}\n"
    code += r'''
void DrainRefresh(CPulseBarDlg& host) {
    auto commands=std::move(host.queued_commands); host.queued_commands.clear();
    for(auto command:commands) if(command==ID_REFRESH_CONNECTION_LIST) host.OnRefreshConnectionList();
}
int wmain(int argc,wchar_t** argv) {
    if(argc!=3) return 2;
    const wstring mode=argv[1];
    fixture_interfaces={Row(11,"A",10000,20000),Row(22,"B",100000,200000)};
    CPulseBarDlg host;
    if(mode==L"detail-reorder" || mode==L"detail-normal" || mode==L"detail-removed") {
        if(!Start(host,"A")) return 3;
        if(mode==L"detail-reorder")
            fixture_interfaces={Row(22,"B",101024,202048),Row(11,"A",14096,28192)};
        else if(mode==L"detail-removed")
            fixture_interfaces={Row(22,"B",101024,202048)};
        Sample(host);
        const DWORD stable=host.GetConnectIfTable(host.m_connection_selected).dwIndex;
        const DWORD detail=host.DetailSnapshotIdentity();
        const DWORD expected=mode==L"detail-removed"?0:11;
        const bool ok=stable==expected && detail==expected;
        printf("%s mode=%ls sampled_identity=%lu detail_identity=%lu refresh_commands=%zu\n",
            ok?"PASS":"FAIL",mode.c_str(),stable,detail,host.queued_commands.size());
        return ok?0:1;
    }
    if(!Start(host,"A",false,true)) return 3;
    const bool down=mode==L"down-with-ui" || mode==L"down-without-ui";
    const bool recover=mode==L"recovery-with-ui";
    const bool failed=mode==L"failure-with-ui" || recover;
    const bool refresh=mode!=L"down-without-ui";
    if(down) fixture_down_identity=22;
    if(failed) fixture_failed_identity=22;
    for(int sample=0;sample<3;++sample) {
        fixture_interfaces[0].received+=4096;
        fixture_interfaces[0].sent+=8192;
        Sample(host);
        printf("mode=%ls round=%d live_speed=%llu/%llu cumulative=%llu/%llu queued=%zu\n",mode.c_str(),sample+1,
            theApp.m_in_speed,theApp.m_out_speed,theApp.m_today_down_traffic,theApp.m_today_up_traffic,host.queued_commands.size());
        if(refresh) DrainRefresh(host);
    }
    const unsigned long long expected_in=recover?13312:12288;
    const unsigned long long expected_out=recover?26624:24576;
    if(recover) {
        fixture_failed_identity=0;
        Sample(host); DrainRefresh(host); // Recovered interface establishes a new baseline.
        fixture_interfaces[1].received+=1024; fixture_interfaces[1].sent+=2048;
        Sample(host); DrainRefresh(host);
    }
    const bool ok=theApp.m_today_down_traffic==expected_in && theApp.m_today_up_traffic==expected_out;
    printf("%s mode=%ls expected_cumulative=%llu/%llu observed=%llu/%llu\n",
        ok?"PASS":"FAIL",mode.c_str(),expected_in,expected_out,theApp.m_today_down_traffic,theApp.m_today_up_traffic);
    return ok?0:1;
}
'''
    return code


def main():
    run, env = native.build(make_code(), "network-refresh-chain")
    modes = ("healthy-with-ui", "down-without-ui", "down-with-ui", "failure-with-ui", "recovery-with-ui",
             "detail-normal", "detail-reorder", "detail-removed")
    results = [native.execute(run, env, mode) for mode in modes]
    print(f"RESULT {sum(results)}/{len(results)}")
    print(f"ARTIFACTS {run}")
    return int(not all(results))


if __name__ == "__main__":
    raise SystemExit(main())
