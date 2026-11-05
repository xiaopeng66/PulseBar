"""Real simple-JSON parsing rejects truncated/invalid values without throwing."""
from pathlib import Path
import importlib.util

spec = importlib.util.spec_from_file_location("native_support", Path(__file__).with_name("native_test_support.py"))
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)


def main():
    text = native.source("Common.cpp")
    start = text.index("wstring CCommon::GetJsonValueSimple(")
    end = text.index("bool CCommon::GetURL(", start)
    method = text[start:end]
    code = r'''#include <string>
#include <vector>
#include <cstdio>
#include <stdexcept>
using namespace std;
class CCommon {public: static wstring GetJsonValueSimple(const wstring&,const wstring&);};
'''+method+r'''
int wmain(int argc,wchar_t** argv) {
    if(argc!=3) return 2;
    struct Case {const wchar_t* input;const wchar_t* key;const wchar_t* want;};
    const Case cases[]={
        {L"{\"ip\":\"203.0.113.7\"}",L"ip",L"203.0.113.7"},
        {L"{\"ip\":",L"ip",L""},
        {L"{\"ip\": \"",L"ip",L""},
        {L"{\"ip\":\"unterminated}",L"ip",L""},
        {L"{\"ip\":\"\",\"location\":\"somewhere\"}",L"ip",L""},
        {L"{\"location\":\"A\\\"B\"}",L"location",L"A\"B"},
        {L"{\"ip\":null}",L"ip",L""},
        {L"{\"unrelated\":\"x\"}",L"ip",L""}
    };
    int failures=0,index=0;
    for(const auto& item:cases) {
        bool ok=false;try {ok=CCommon::GetJsonValueSimple(item.input,item.key)==item.want;} catch(...) {}
        printf("%s json case=%d\n",ok?"PASS":"FAIL",++index);if(!ok) ++failures;
    }
    return failures?1:0;
}
'''
    run, env = native.build(code, "json")
    ok = native.execute(run, env, "parse")
    print(f"ARTIFACTS {run}")
    return int(not ok)


if __name__ == "__main__":
    raise SystemExit(main())
