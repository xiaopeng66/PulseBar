"""INI section enumeration must be bounded and never span a malformed section line."""
from pathlib import Path
import importlib.util

spec = importlib.util.spec_from_file_location("native_support", Path(__file__).with_name("native_test_support.py"))
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)


def make_code():
    declaration = native.method(native.source("IniHelper.cpp"), "vector<wstring> CIniHelper::GetAllAppName(")
    signature = declaration[:declaration.index("{")].strip().replace("CIniHelper::", "")
    if "bool* valid" in signature:
        signature = signature.replace("bool* valid", "bool* valid = nullptr")
    code = r'''#include <string>
#include <vector>
#include <cstdio>
using namespace std;
class CIniHelper {public: wstring m_ini_str;
'''+signature+';\n};\n'+declaration+r'''
int wmain(int argc,wchar_t** argv) {
    if(argc!=3) return 2;
    wstring mode=argv[1];CIniHelper ini;vector<wstring> expected;wstring prefix;
    if(mode==L"valid") {ini.m_ini_str=L"\n[general]\na=1\n[skin_blue]\n";expected={L"general",L"skin_blue"};}
    if(mode==L"first-line") {ini.m_ini_str=L"[general]\na=1\n";expected={L"general"};}
    if(mode==L"truncated") {ini.m_ini_str=L"\n[general";expected={};}
    if(mode==L"next-section") {ini.m_ini_str=L"\n[broken\n[good]\nx=1\n";expected={L"good"};}
    if(mode==L"prefix") {ini.m_ini_str=L"\n[other]\n[skin_blue]\n[skin_broken\n";prefix=L"skin_";expected={L"blue"};}
    bool ok=ini.GetAllAppName(prefix)==expected;
    printf("%s ini-sections %ls\n",ok?"PASS":"FAIL",mode.c_str());return ok?0:1;
}
'''
    return code


def main():
    run, env = native.build(make_code(), "ini-sections")
    results = [native.execute(run, env, mode, timeout=2) for mode in ("valid", "first-line", "truncated", "next-section", "prefix")]
    print(f"ARTIFACTS {run}")
    return int(not all(results))


if __name__ == "__main__":
    raise SystemExit(main())
