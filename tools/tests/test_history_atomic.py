"""History commit fault injection, using current production serialization and Win32 I/O."""
from pathlib import Path
import importlib.util

spec = importlib.util.spec_from_file_location("history_regressions", Path(__file__).with_name("test_history_regressions.py"))
history = importlib.util.module_from_spec(spec)
spec.loader.exec_module(history)
native = history.native


def make_code():
    code = history.make_code().split("int wmain(", 1)[0].replace('#include "AtomicFile.h"\n', '')
    seam = r'''
#include <type_traits>
#include <iterator>
HANDLE injected_file=INVALID_HANDLE_VALUE;
int fault=0, fault_hits=0, write_calls=0;
HANDLE WINAPI FaultCreateFileW(LPCWSTR path,DWORD access,DWORD share,LPSECURITY_ATTRIBUTES security,
                              DWORD disposition,DWORD flags,HANDLE template_file) {
    HANDLE value=::CreateFileW(path,access,share,security,disposition,flags,template_file);
    if(disposition==CREATE_NEW && value!=INVALID_HANDLE_VALUE) injected_file=value;
    return value;
}
BOOL WINAPI FaultWriteFile(HANDLE file,LPCVOID data,DWORD count,LPDWORD written,LPOVERLAPPED overlapped) {
    if(file==injected_file && fault>=1 && fault<=3) {
        ++fault_hits;
        if(fault==2 && write_calls++==0) return ::WriteFile(file,data,count/2,written,overlapped);
        *written=0; SetLastError(ERROR_DISK_FULL); return fault==3?TRUE:FALSE;
    }
    return ::WriteFile(file,data,count,written,overlapped);
}
BOOL WINAPI FaultFlushFileBuffers(HANDLE file) {
    if(file==injected_file && fault==4) {++fault_hits;SetLastError(ERROR_WRITE_FAULT);return FALSE;}
    return ::FlushFileBuffers(file);
}
BOOL WINAPI FaultCloseHandle(HANDLE file) {
    bool fail=file==injected_file && fault==5;
    BOOL result=::CloseHandle(file);
    if(file==injected_file) injected_file=INVALID_HANDLE_VALUE;
    if(fail) {++fault_hits;SetLastError(ERROR_WRITE_FAULT);return FALSE;}
    return result;
}
BOOL WINAPI FaultReplaceFileW(LPCWSTR path,LPCWSTR replacement,LPCWSTR backup,DWORD flags,LPVOID exclude,LPVOID reserved) {
    if(fault==6) {++fault_hits;SetLastError(ERROR_ACCESS_DENIED);return FALSE;}
    return ::ReplaceFileW(path,replacement,backup,flags,exclude,reserved);
}
BOOL WINAPI FaultMoveFileExW(LPCWSTR from,LPCWSTR to,DWORD flags) {
    if(fault==7) {++fault_hits;SetLastError(ERROR_ACCESS_DENIED);return FALSE;}
    return ::MoveFileExW(from,to,flags);
}
#define CreateFileW FaultCreateFileW
#define WriteFile FaultWriteFile
#define FlushFileBuffers FaultFlushFileBuffers
#define CloseHandle FaultCloseHandle
#define ReplaceFileW FaultReplaceFileW
#define MoveFileExW FaultMoveFileExW
'''
    if (native.SOURCE / "AtomicFile.h").exists():
        seam += '#include "AtomicFile.h"\n'
    code = code.replace("class CHistoryTrafficFile", seam + "\nclass CHistoryTrafficFile", 1)
    for signature in ("CPulseBarDlg::SaveHistoryTrafficFull()", "CPulseBarDlg::BackupHistoryTrafficFile()"):
        app = native.source("PulseBarDlg.cpp")
        start = app.rfind("\n", 0, app.index(signature)) + 1
        body = native.method(app, app[start:app.index(signature)] + signature)
        declaration = body[:body.index("{")].strip().replace("CPulseBarDlg::", "") + ";"
        code = code.replace("    void LoadHistoryTraffic();", "    void LoadHistoryTraffic();\n    " + declaration, 1)
        code += body + "\n"
    code = code.replace("class CPulseBarDlg {public:", "class CSingleLock {public: CSingleLock(void*,BOOL) {} void Unlock() {}};\nclass CPulseBarDlg {public:\n    int m_history_lock=0;")
    code += r'''
#undef CreateFileW
#undef WriteFile
#undef FlushFileBuffers
#undef CloseHandle
#undef ReplaceFileW
#undef MoveFileExW
string ReadBytes(const wstring& path) {ifstream in(path,ios::binary);return string(istreambuf_iterator<char>(in),{});}
template<class File> bool SaveResult(File& file) {
    if constexpr(is_void_v<decltype(file.Save())>) {file.Save();return true;}
    else return file.Save();
}
template<class File> bool BackupResult(File& file) {
    if constexpr(requires {file.SaveBackup();}) return file.SaveBackup();
    else return CopyFileW(file.GetFilePath().c_str(),(file.GetFilePath()+L".bak").c_str(),FALSE)!=FALSE;
}
int wmain(int argc,wchar_t** argv) {
    if(argc!=3) return 2;
    const wstring mode=argv[1],root=argv[2];
    if(root.find(L"E:\\AI\\Zcode\\tmp\\pulsebar-fix-20261105\\")!=0) return 3;
    const wstring path=root+L"\\atomic-"+mode+L".dat";
    const string original="lines: \"1\"\n2026/11/04 100/200\n";
    Seed(path,original);CHistoryTrafficFile file(path);file.Load();
    file.SetTodayTrafficKBytes(999,200);
    bool ok=false;
    if(mode==L"success") {
        ok=SaveResult(file)&&ReadBytes(path).find("999/200")!=string::npos;
    } else if(mode==L"same-snapshot") {
        ok=SaveResult(file);const string committed=ReadBytes(path);
        file.SetTodayTrafficKBytes(777,200);
        ok=ok&&BackupResult(file)&&ReadBytes(path+L".bak")==committed;
    } else if(mode==L"format") {
        ok=SaveResult(file)&&ReadBytes(path)=="lines: \"1\"\r\n2026/11/04 999/200\r\n";
    } else if(mode==L"blocked-replace") {
        HANDLE lock=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,
                                OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(lock==INVALID_HANDLE_VALUE) return 4;
        bool saved=SaveResult(file);
        CloseHandle(lock);
        ok=!saved&&ReadBytes(path)==original;
    } else if(mode==L"host-failed-save") {
        Seed(path+L".bak",original);theApp.m_history_traffic_path=path;
        CPulseBarDlg host(path);host.m_history_traffic.Load();host.m_history_traffic.SetTodayTrafficKBytes(999,200);
        fault=1;host.SaveHistoryTrafficFull();fault=0;host.BackupHistoryTrafficFile();
        ok=fault_hits>0&&ReadBytes(path)==original&&ReadBytes(path+L".bak")==original;
    } else if(mode==L"backup") {
        Seed(path+L".bak",original);
        ok=SaveResult(file);fault=6;
        bool result=BackupResult(file);
        ok=ok&&!result&&fault_hits>0&&ReadBytes(path+L".bak")==original
            &&ReadBytes(path).find("999/200")!=string::npos;
    } else {
        fault=_wtoi(mode.c_str());
        if(fault==7) DeleteFileW(path.c_str());
        bool saved=SaveResult(file);
        ok=!saved&&fault_hits>0&&(fault==7?!CCommon::FileExist(path.c_str()):ReadBytes(path)==original);
    }
    WIN32_FIND_DATAW entry{};HANDLE find=FindFirstFileW((root+L"\\.PulseBar.*.tmp").c_str(),&entry);
    if(find!=INVALID_HANDLE_VALUE) {ok=false;FindClose(find);}
    printf("%s atomic %ls fault_hits=%d\n",ok?"PASS":"FAIL",mode.c_str(),fault_hits);
    return ok?0:1;
}
'''
    return code


def main():
    run, env = native.build(make_code(), "history-atomic")
    modes = ["success", "blocked-replace", "1", "2", "3", "4", "5", "6", "7", "backup", "host-failed-save", "same-snapshot", "format"]
    results = [native.execute(run, env, mode) for mode in modes]
    print(f"ARTIFACTS {run}")
    return int(not all(results))


if __name__ == "__main__":
    raise SystemExit(main())
