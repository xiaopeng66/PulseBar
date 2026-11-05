"""Bounded response collection uses the current production request helper, with fake HTTP I/O."""
from pathlib import Path
import importlib.util

spec = importlib.util.spec_from_file_location("native_support", Path(__file__).with_name("native_test_support.py"))
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)


def main():
    text = native.source("NetworkInfoDlg.cpp")
    method = native.method(text,"static void GetInternetIPForLookup(")
    code = r'''#include <windows.h>
#include <string>
#include <memory>
#include <cstdio>
#include <cstring>
#include <algorithm>
using namespace std;
constexpr wchar_t VERSION[]=L"test";
constexpr DWORD HTTP_STATUS_OK=200;
int chunks=1,reads=0;
class CInternetException {public:void Delete(){delete this;}};
class CHttpFile {public:
    void QueryInfoStatusCode(DWORD& value){value=HTTP_STATUS_OK;}
    UINT Read(void* data,UINT size){++reads;if(chunks--<=0)return 0;memset(data,'a',size);return size;}
};
class CInternetSession {public:CInternetSession(const wchar_t*){} CHttpFile* OpenURL(const wchar_t*){return new CHttpFile;}};
class CCommon {public:
    static wstring StrToUnicode(const char*,bool){return L"fixture";}
    static wstring GetJsonValueSimple(const wstring&,const wstring& key){return key==L"ip"?L"203.0.113.7":L"test";}
};
'''+method+r'''
int wmain(int argc,wchar_t** argv){
    if(argc!=3)return 2;wstring mode=argv[1];bool large=mode==L"oversize";chunks=large?256:1;
    wstring address,location;GetInternetIPForLookup(address,location,false);
    bool ok=large?(address.empty()&&location.empty()&&reads<=17):(address==L"203.0.113.7"&&reads==2);
    printf("%s ip-response %ls reads=%d\n",ok?"PASS":"FAIL",mode.c_str(),reads);return ok?0:1;
}
'''
    run, env = native.build(code, "ip-response")
    results = [native.execute(run, env, mode) for mode in ("normal", "oversize")]
    print(f"ARTIFACTS {run}")
    return int(not all(results))


if __name__ == "__main__":
    raise SystemExit(main())
