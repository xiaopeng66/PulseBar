"""Exercise the real worker exception boundary without HTTP, dialogs or global app state."""
from pathlib import Path
import importlib.util

spec = importlib.util.spec_from_file_location("native_support", Path(__file__).with_name("native_test_support.py"))
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)


def main():
    text = native.source("NetworkInfoDlg.cpp")
    structs = text[text.index("struct InternetIPLookupState"):text.index("// Unlike CCommon::GetURL")]
    code = r'''#include <windows.h>
#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <stdexcept>
#include <cstdio>
using namespace std;
class CException {public: void Delete(){delete this;}};
class CInternetException:public CException {};
class CCommon {public: static void SetThreadLanguage(WORD) {}};
class CNetworkInfoDlg {public: static UINT GetInternetIPThreadFunc(LPVOID);};
int request_mode=0;
static void GetInternetIPForLookup(wstring& address,wstring& location,bool ipv6) {
    if(request_mode==1) throw out_of_range("truncated response");
    if(request_mode==2) throw new CInternetException;
    address=ipv6?L"2001:db8::1":L"203.0.113.7";location=L"test";
}
'''+structs+native.method(text,"UINT CNetworkInfoDlg::GetInternetIPThreadFunc(")+r'''
int wmain(int argc,wchar_t** argv) {
    if(argc!=3)return 2;
    request_mode=_wtoi(argv[1]);auto state=make_shared<InternetIPLookupState>();
    bool escaped=false;
    try {CNetworkInfoDlg::GetInternetIPThreadFunc(new InternetIPThreadData{state,0});}
    catch(CException* error){error->Delete();escaped=true;} catch(...){escaped=true;}
    bool ok=!escaped&&state->complete;
    if(request_mode==0)ok=ok&&state->ipv4_address==L"203.0.113.7"&&state->ipv6_address==L"2001:db8::1";
    else ok=ok&&state->ipv4_address.empty()&&state->ipv6_address.empty();
    printf("%s ip-worker mode=%d escaped=%d complete=%d\n",ok?"PASS":"FAIL",request_mode,escaped,state->complete);
    return ok?0:1;
}
'''
    run, env = native.build(code, "ip-errors")
    results = [native.execute(run, env, mode) for mode in ("0", "1", "2")]
    print(f"ARTIFACTS {run}")
    return int(not all(results))


if __name__ == "__main__":
    raise SystemExit(main())
