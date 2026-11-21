"""Every failed clipboard operation must release local ownership and close the clipboard."""
from pathlib import Path
import importlib.util

spec = importlib.util.spec_from_file_location("native_support", Path(__file__).with_name("native_test_support.py"))
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)


def main():
    method = native.method(native.source("Common.cpp"), "bool CCommon::CopyStringToClipboard(const wstring& str)")
    code = r'''#include <windows.h>
#include <string>
#include <cstring>
#include <limits>
#include <memory>
#include <cstdio>
using std::wstring;
using namespace std;
enum class Failure {none, open, empty, alloc, lock, set};
Failure failure=Failure::none; bool opened=false, transferred=false; int close_count=0, free_count=0, unlock_count=0; UINT alloc_flags{};
HGLOBAL memory{}; wchar_t clipboard_text[32]{};
BOOL ProbeOpenClipboard(HWND) {if(failure==Failure::open)return FALSE;opened=true;return TRUE;}
BOOL ProbeCloseClipboard() {++close_count;opened=false;return TRUE;}
BOOL ProbeEmptyClipboard() {return failure!=Failure::empty;}
HGLOBAL ProbeGlobalAlloc(UINT flags,SIZE_T bytes) {alloc_flags=flags;if(failure==Failure::alloc)return nullptr;memory=::GlobalAlloc(GMEM_MOVEABLE,bytes);return memory;}
LPVOID ProbeGlobalLock(HGLOBAL h) {return failure==Failure::lock ? nullptr : ::GlobalLock(h);}
BOOL ProbeGlobalUnlock(HGLOBAL h) {++unlock_count;return ::GlobalUnlock(h);}
HGLOBAL ProbeGlobalFree(HGLOBAL h) {++free_count;return ::GlobalFree(h);}
HANDLE ProbeSetClipboardData(UINT,HANDLE h) {if(failure==Failure::set)return nullptr;transferred=true;return h;}
#define OpenClipboard ProbeOpenClipboard
#define CloseClipboard ProbeCloseClipboard
#define EmptyClipboard ProbeEmptyClipboard
#define GlobalAlloc ProbeGlobalAlloc
#define GlobalLock ProbeGlobalLock
#define GlobalUnlock ProbeGlobalUnlock
#define GlobalFree ProbeGlobalFree
#define SetClipboardData ProbeSetClipboardData
class CCommon {public: static bool CopyStringToClipboard(const wstring& str);};
'''+method+r'''
int wmain(int argc,wchar_t** argv) {
    if(argc!=3 || std::wstring(argv[2]).find(PULSEBAR_TEST_ROOT)!=0)return 2;
    const std::wstring mode=argv[1];
    if(mode==L"open")failure=Failure::open;
    else if(mode==L"empty")failure=Failure::empty;
    else if(mode==L"alloc")failure=Failure::alloc;
    else if(mode==L"lock")failure=Failure::lock;
    else if(mode==L"set")failure=Failure::set;
    const bool ok=CCommon::CopyStringToClipboard(L"probe");
    if(mode==L"success")return ok&&transferred&&opened==false&&close_count==1&&free_count==0&&
        (alloc_flags&GMEM_MOVEABLE)!=0?0:1;
    if(mode==L"open")return !ok&&!opened&&close_count==0&&free_count==0?0:1;
    if(mode==L"alloc")return !ok&&!opened&&close_count==1&&free_count==0?0:1;
    if(mode==L"empty")return !ok&&!opened&&close_count==1&&free_count==1?0:1;
    return !ok&&!opened&&close_count==1&&free_count==1?0:1;
}
'''
    run, env = native.build(code, "clipboard-cleanup")
    outcomes = [native.execute(run, env, mode) for mode in ("open", "empty", "alloc", "lock", "set", "success")]
    print(f"ARTIFACTS {run}")
    return int(not all(outcomes))


if __name__ == "__main__":
    raise SystemExit(main())
