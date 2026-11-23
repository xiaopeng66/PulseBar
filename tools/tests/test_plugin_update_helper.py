"""Exercise PulseBar's catalog worker, displayed versions, and plugin file replacement."""
import importlib.util
from pathlib import Path
import sys

spec = importlib.util.spec_from_file_location("support", Path(__file__).with_name("native_test_support.py"))
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)


def make_code():
    source = native.source("PluginUpdateHelper.cpp")
    source = source.replace(native.method(source, "static bool ReadPluginCatalog("), "")
    source = source.replace('#pragma comment(lib, "wininet.lib")', "")
    source = source.replace(native.method(source, "    bool DownloadBinaryFile("), "")
    replace_signature = "PluginInstallResult CommitPluginUpdate("
    replacement = native.method(source, replace_signature)
    source = source.replace(replacement, replacement.replace("::MoveFileExW(", "MovePluginFile("))
    return r'''#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <atomic>
#include <future>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <climits>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include "tinyxml2/tinyxml2.cpp"
#include "language.h"
using namespace std;
using namespace std::chrono_literals;

struct CCommon {
    static void StringSplit(const string& text, char delimiter, vector<string>& out) {
        size_t start = 0;
        for (size_t end = 0; end <= text.size(); ++end) {
            if (end == text.size() || text[end] == delimiter) {
                if (end > start) out.push_back(text.substr(start, end - start));
                start = end + 1;
            }
        }
    }
    static string AsciiToStr(const wstring& text) { return string(text.begin(), text.end()); }
    static wstring AsciiToUnicode(const string& text) { return wstring(text.begin(), text.end()); }
    static wstring StrToUnicode(const string& text, bool utf8 = false) {
        if (text.empty()) return {};
        const UINT code_page = utf8 ? CP_UTF8 : CP_ACP;
        const int length = MultiByteToWideChar(code_page, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        wstring result(length, L'\0');
        MultiByteToWideChar(code_page, 0, text.data(), static_cast<int>(text.size()), result.data(), length);
        return result;
    }
    static char* GetFileContent(const wchar_t*, size_t&) { return nullptr; }
};

static string response;
static bool response_ok = true;
static atomic<bool> pause_fetch{false};
static atomic<int> fetches{0};
static bool ReadPluginCatalog(const wstring&, string& body) {
    ++fetches;
    while (pause_fetch) this_thread::sleep_for(1ms);
    if (!response_ok) return false;
    body = response;
    return true;
}

static wstring package_source;
static bool DownloadBinaryFile(const wstring& url, const wstring& path) {
    return url == L"https://fixture.invalid/package.dll"
        && ::CopyFileW(package_source.c_str(), path.c_str(), FALSE);
}

static bool deny_staged_move = false;
static int denied_moves = 0;
static BOOL MovePluginFile(const wchar_t* from, const wchar_t* to, DWORD flags) {
    if (deny_staged_move && wstring(from).ends_with(L".tmp")) {
        ++denied_moves;
        SetLastError(ERROR_SHARING_VIOLATION);
        return FALSE;
    }
    return ::MoveFileExW(from, to, flags);
}
''' + "\n".join(native.no_includes(content) for content in (
        native.source("TinyXml2Helper.h"), native.source("PluginUpdateHelper.h"),
        source, native.source("TinyXml2Helper.cpp"),
    )) + r'''

using Status = CPluginUpdateHelper::Status;
static int failures = 0;
static bool Check(bool ok, const char* label) {
    printf("%s %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++failures;
    return ok;
}

static auto WaitForCatalog(CPluginUpdateHelper& helper) {
    for (int i = 0; i < 1500 && helper.GetStatus() == Status::Checking; ++i)
        this_thread::sleep_for(1ms);
    return helper.GetCatalog();
}

static const char* k_valid =
    "<plugins><plugin file_name=\"MagpieTaskbar.dll\" version=\"1.3.1\" "
    "download=\"Plugin.dll\" arch=\"x64\" description=\"自动刷新\" official=\"true\"/></plugins>";

static void CatalogFetch() {
    CPluginUpdateHelper helper;
    Check(helper.GetCatalog().status == Status::Idle && !helper.GetCatalog().has_snapshot,
          "no successful catalog before the first request");
    response = k_valid;
    pause_fetch = true;
    helper.RefreshAsync();
    for (int i = 0; i < 10; ++i) helper.RefreshAsync();
    Check(helper.GetCatalog().status == Status::Checking && !helper.GetCatalog().has_snapshot,
          "checking is visible while the network is held");
    pause_fetch = false;
    auto first = WaitForCatalog(helper);
    Check(fetches == 1, "simultaneous management and store requests share a single fetch");
    Check(first.status == Status::Ready && first.has_snapshot && first.items.size() == 1,
          "successful fetch publishes the complete catalog");
    const auto* item = FindPluginStoreItem(first.items, L"magpietaskbar.DLL");
    Check(item && item->version.GetVersionWString() == L"1.3.1" && item->description == L"自动刷新" && item->official,
          "lookup uses the Windows file name and decodes UTF-8 metadata");
    response = "<plugins/>";
    helper.RefreshAsync();
    const auto empty = WaitForCatalog(helper);
    Check(empty.status == Status::Ready && empty.has_snapshot && empty.items.empty(), "valid empty catalog is distinct from failure");
    Check(first.items.size() == 1 && first.items.front().file_name == L"MagpieTaskbar.dll",
          "a later refresh leaves the existing UI snapshot intact");
}

static void FailedRefresh() {
    CPluginUpdateHelper helper;
    response_ok = false;
    helper.RefreshAsync();
    auto snapshot = WaitForCatalog(helper);
    Check(snapshot.status == Status::Failed && !snapshot.has_snapshot, "offline first launch reports failure");
    response = k_valid;
    response_ok = true;
    helper.RefreshAsync();
    Check(WaitForCatalog(helper).status == Status::Ready, "manual retry can recover from a failed request");
    for (const char* bad : {"", "<plugins><plugin", "not XML", "<html/>",
                             "<plugins><plugin file_name=\"broken.dll\"/></plugins>",
                             "<plugins><plugin file_name=\"broken.dll\" version=\"1.beta.2\"/></plugins>",
                             "<plugins><plugin file_name=\"broken.dll\" version=\"1..2\"/></plugins>"}) {
        response = bad;
        helper.RefreshAsync();
        snapshot = WaitForCatalog(helper);
        Check(snapshot.status == Status::Failed && snapshot.has_snapshot && snapshot.items.size() == 1
            && snapshot.items.front().version.GetVersionWString() == L"1.3.1",
            "invalid response reports failure and preserves the last good catalog");
    }
    response_ok = false;
    helper.RefreshAsync();
    snapshot = WaitForCatalog(helper);
    Check(snapshot.status == Status::Failed && snapshot.items.size() == 1, "offline refresh preserves update results");
}

static void StoreStatus() {
    PluginStoreItem item;
    item.file_name = L"MagpieTaskbar.dll";
    item.version = PluginVersion(L"1.3.1");
    item.download_path = L"MagpieTaskbar.dll";
    item.arch = L"x64";
    using State = PluginStoreState;
    Check(GetPluginStoreState(item, true, PluginVersion(L"1.3.0"), {}) == State::UpdateAvailable, "1.3.0 to 1.3.1 is an update");
    Check(GetPluginStoreState(item, true, PluginVersion(L"1.3.1"), {}) == State::Installed, "equivalent versions do not offer an update");
    Check(GetPluginStoreState(item, true, PluginVersion(L"1.10.0"), {}) == State::NewerLocally, "numeric version comparison prevents downgrade");
    Check(!(PluginVersion("1.beta.2") == PluginVersion("1.2")) &&
          !(PluginVersion("1..2") == PluginVersion("1.2")) &&
          !(PluginVersion("2147483648") == PluginVersion("2147483647")),
          "malformed, empty, and overflowing version components are rejected");
    Check(GetPluginStoreState(item, true, {}, {}) == State::Installed, "unknown local version is not a claimed update");
    Check(GetPluginStoreState(item, false, {}, {}) == State::Available, "not installed is distinct from an update");
    item.arch = L"x86";
    Check(GetPluginStoreState(item, true, PluginVersion(L"1.2.0"), {}) == State::Incompatible, "incompatible package is not offered");
    CPluginUpdateHelper helper;
    helper.MarkPendingRestart(item.file_name, item.version);
    Check(helper.HasPendingRestart() && GetPluginStoreState(item, true, PluginVersion(L"1.2.0"),
          helper.GetPendingVersion(L"magpietaskbar.DLL")) == State::PendingRestart,
          "an installed update stays pending across dialogs until the app restarts");
}

static void Write(const wstring& path, char marker) {
    FILE* file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"wb") == 0) { fputc(marker, file); fclose(file); }
}

static int Read(const wstring& path) {
    FILE* file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"rb") != 0) return EOF;
    const int value = fgetc(file);
    fclose(file);
    return value;
}

static void FileReplacement(const wstring& folder) {
    const auto target = folder + L"\\fixture.dll";
    const auto staged = folder + L"\\fixture.tmp";
    const auto backup = target + L".pb-old";
    Write(target, 'a'); Write(staged, 'b');
    deny_staged_move = true;
    Check(CommitPluginUpdate(staged, target) == PluginInstallResult::Failed && denied_moves == 2,
          "injected failure occurs both before and after the old DLL is moved aside");
    Check(Read(target) == 'a' && Read(staged) == 'b' && Read(backup) == EOF,
          "commit failure puts the original back and leaves the staged file for cleanup");
    deny_staged_move = false;
    Check(CommitPluginUpdate(staged, target) == PluginInstallResult::Installed && Read(target) == 'b' && Read(staged) == EOF,
          "a retry installs the new file without losing the original before replacement");
}

static void SharedInstall(const wstring& folder) {
    const auto directory = folder + L"\\plugins\\";
    ::CreateDirectoryW(directory.c_str(), nullptr);
    const auto target = directory + L"fixture.dll";
    Write(target, 'a');

    wchar_t system_directory[MAX_PATH]{};
    if (!Check(::GetSystemDirectoryW(system_directory, MAX_PATH) != 0, "locate an offline x64 DLL fixture")) return;
    package_source = wstring(system_directory) + L"\\kernel32.dll";
    CPluginUpdateHelper helper;
    CPluginPackageInstall install;
    PluginStoreItem item;
    item.file_name = L".\\fixture.dll";
    item.download_path = L"https://fixture.invalid/package.dll";
    item.version = PluginVersion(L"1.4.1");
    const auto* invalid = install.Start(item, directory);
    Check(invalid && wcscmp(invalid, IDS_STORE_INVALID_NAME) == 0 && !install.IsActive(),
          "invalid catalog name is rejected before staging");

    item.file_name = L"fixture.dll";
    item.sha256 = wstring(64, L'0');
    if (!Check(install.Start(item, directory) == nullptr, "begin download with fixture checksum")) return;
    auto wait = [&]() {
        for (int i = 0; i < 5000 && !install.IsReady(); ++i) this_thread::sleep_for(1ms);
        return install.IsReady();
    };
    if (!Check(wait(), "checksum test download completes")) return;
    const auto bad = install.Finish(helper);
    Check(bad.error && wcscmp(bad.error, IDS_STORE_SHA_MISMATCH) == 0 && !install.IsActive()
        && !helper.HasPendingRestart() && Read(target) == 'a',
        "checksum mismatch preserves the installed DLL and allows retry");

    item.sha256.clear();
    if (!Check(install.Start(item, directory) == nullptr, "start the retry")) return;
    item.file_name = L"other.dll";
    if (!Check(wait(), "retry completes")) return;
    Check(Read(target) == 'a' && !helper.HasPendingRestart(), "worker only stages the file");
    const auto installed = install.Finish(helper);
    Check(installed.error == nullptr && installed.version.GetVersionWString() == L"1.4.1"
        && helper.GetPendingVersion(L"fixture.dll").GetVersionWString() == L"1.4.1"
        && Read(target) == 'M' && Read(directory + L"other.dll") == EOF,
        "commit installs the original selection and marks it pending restart");
}

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) return 2;
    const wstring mode = argv[1];
    if (mode == L"catalog-fetch") CatalogFetch();
    else if (mode == L"failed-refresh") FailedRefresh();
    else if (mode == L"store-status") StoreStatus();
    else if (mode == L"file-replacement") FileReplacement(argv[2]);
    else if (mode == L"shared-install") SharedInstall(argv[2]);
    else return 2;
    return failures ? 1 : 0;
}
'''


def main():
    cases = ("catalog-fetch", "failed-refresh", "store-status", "file-replacement", "shared-install")
    run, env = native.build(make_code(), "plugin-update-helper")
    passed = sum(native.execute(run, env, case, timeout=15) for case in cases)
    print(f"RESULT {passed}/{len(cases)} scenarios")
    return int(passed != len(cases))


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    raise SystemExit(main())
