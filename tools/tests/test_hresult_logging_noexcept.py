"""Failures while recording an HRESULT must not escape device-recovery callbacks."""
from pathlib import Path
import importlib.util

spec = importlib.util.spec_from_file_location("native_support", Path(__file__).with_name("native_test_support.py"))
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)


def main():
    method = native.method(native.source("HResultException.cpp"), "void LogHResultException(CHResultException& ex)")
    code = r'''#include <memory>
#include <string>
#include <stdexcept>
using HRESULT=long; using BSTR=wchar_t*;
constexpr HRESULT S_OK=0, E_FAIL=static_cast<HRESULT>(0x80004005L);
#define FAILED(hr) ((hr)<0)
#define SUCCEEDED(hr) ((hr)>=0)
int writes=0, fail_write=0, description_hr=E_FAIL, freed=0;
void SysFreeString(BSTR p) { if(p) {++freed; delete[] p;} }
struct IErrorInfo { HRESULT GetDescription(BSTR* out) { if(description_hr==S_OK) *out=new wchar_t[2]{L'o',L'k'}; return description_hr; } };
template<class T> struct FakeComPtr { T* Get(){return reinterpret_cast<T*>(1);} };
struct CHResultException { HRESULT GetHResult(){return E_FAIL;} const char* what(){return "failure";} FakeComPtr<IErrorInfo> GetError(){return {};} };
struct CCommon { static void WriteLog(const char*, const char*) { if(++writes==fail_write) throw std::runtime_error("log failed"); }
    static void WriteLog(const wchar_t*, const char*) { if(++writes==fail_write) throw std::runtime_error("log failed"); } };
struct App { std::string m_log_path="test.log"; } theApp;
void ThrowIfFailed(HRESULT hr,const char*) { if(FAILED(hr)) throw std::runtime_error("description failed"); }
#define TRAFFICMONITOR_ERROR_STR(message) message
'''+method+r'''
int wmain(int argc,wchar_t** argv) {
    if(argc!=3 || std::wstring(argv[2]).find(PULSEBAR_TEST_ROOT)!=0) return 2;
    CHResultException ex;
    if(std::wstring(argv[1])==L"write") { fail_write=1; try {LogHResultException(ex);} catch(...) {return 1;} return 0; }
    if(std::wstring(argv[1])==L"description") { description_hr=E_FAIL; try {LogHResultException(ex);} catch(...) {return 1;} return 0; }
    if(std::wstring(argv[1])==L"cleanup") { description_hr=S_OK; fail_write=3; try {LogHResultException(ex);} catch(...) {return 1;} return freed==1?0:1; }
    return 3;
}
'''
    run, env = native.build(code, "hresult-logging-noexcept")
    outcomes = [native.execute(run, env, mode) for mode in ("write", "description", "cleanup")]
    print(f"ARTIFACTS {run}")
    return int(not all(outcomes))


if __name__ == "__main__":
    raise SystemExit(main())
