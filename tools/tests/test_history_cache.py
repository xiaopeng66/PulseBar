"""History views must observe today's updates after a cache has been populated."""
from pathlib import Path
import importlib.util

spec = importlib.util.spec_from_file_location("history_regressions", Path(__file__).with_name("test_history_regressions.py"))
history = importlib.util.module_from_spec(spec)
spec.loader.exec_module(history)
native = history.native


def main():
    code = history.make_code().split("int wmain(", 1)[0] + r'''
template<class File> void SetToday(File& file,unsigned long long up,unsigned long long down) {
    if constexpr(requires {file.SetTodayTrafficKBytes(up,down);}) file.SetTodayTrafficKBytes(up,down);
    else {file.GetTodayTraffic().up_kBytes=up;file.GetTodayTraffic().down_kBytes=down;}
}
int wmain(int argc,wchar_t** argv) {
    if(argc!=3)return 2;wstring root=argv[2];
    if(root.find(L"E:\\AI\\Zcode\\tmp\\pulsebar-fix-20261105\\")!=0)return 3;
    wstring path=root+L"\\cache.dat";
    Seed(path,"lines: \"2\"\n2026/11/04 100/200\n2026/11/03 300/400\n");
    CHistoryTrafficFile file(path);file.Load();auto initial=file.GetTraffics();
    SetToday(file,123456,654321);const auto& updated=file.GetTraffics();
    bool ok=initial.front().up_kBytes==100&&updated.front().up_kBytes==123456
        &&updated.front().down_kBytes==654321&&updated.size()==2&&Has(file,3,300);
    SetToday(file,123457,654322);
    ok=ok&&file.GetTraffics().front().up_kBytes==123457;
    printf("%s history-cache updates\n",ok?"PASS":"FAIL");return ok?0:1;
}
'''
    run, env = native.build(code, "history-cache")
    ok = native.execute(run, env, "cache")
    print(f"ARTIFACTS {run}")
    return int(not ok)


if __name__ == "__main__":
    raise SystemExit(main())
