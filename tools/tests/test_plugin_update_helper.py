"""CPluginUpdateHelper: the version table must be swapped atomically, never rebuilt in place.

Run with: python -X utf8 -I -B <absolute path to this file>
Every run extracts the CURRENT PluginUpdateHelper.cpp/h and the real
CTinyXml2Helper iteration plus the vendored tinyxml2 parser. CCommon::GetURL is
a controlled stub, so no network is touched. Scenarios:

- parse: a successful download fills the table and lookups hit.
- failure-keeps-previous: a FAILED download must keep the previous table
  (the old implementation cleared the map before downloading and lost it).
- copy-isolated: lookups return a value copy; a later background swap does
  not change an already returned copy.
- concurrency: writers swap the table while readers query copies; with the
  mutex the run must finish cleanly. This is hardening, not a crash oracle.

The Python side also asserts extraction contracts on the CURRENT source
(value return type, mutex use, no in-place clear) - on the old code these are
the expected-to-fail RED assertions, not compile errors.
"""
from pathlib import Path
import hashlib
import importlib.util
import re
import sys

SUPPORT = Path(__file__).resolve().with_name("native_test_support.py")
spec = importlib.util.spec_from_file_location("update_native_support", SUPPORT)
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)


def extraction_contracts(cpp_text, h_text):
    problems = []
    signature = "PluginVersion CPluginUpdateHelper::GetPluginLatestVersions"
    if signature not in cpp_text:
        problems.append("GetPluginLatestVersions must return PluginVersion by value, not a reference into the map")
    if "const PluginVersion& CPluginUpdateHelper::GetPluginLatestVersions" in cpp_text:
        problems.append("GetPluginLatestVersions still returns a reference into a concurrently rebuilt map")
    implementation = cpp_text.split(signature)[1].split("{", 1)[1].split("\n}", 1)[0] if signature in cpp_text else ""
    if "m_version_mutex" not in implementation:
        problems.append("GetPluginLatestVersions must query the table under the version mutex")
    if signature in cpp_text and "std::lock_guard" not in cpp_text.split(signature)[1].split("\n}", 1)[0]:
        problems.append("GetPluginLatestVersions must lock m_version_mutex")
    check = cpp_text.split("bool CPluginUpdateHelper::CheckForUpdate")[1].split("\n}", 1)[0]
    if "m_latest_versions.clear()" in check:
        problems.append("CheckForUpdate must not clear the shared table before the download; build a local table and swap")
    if "m_latest_versions.swap(" not in check:
        problems.append("CheckForUpdate must publish the new table with one atomic swap under the mutex")
    if "#include <mutex>" not in h_text and "std::mutex" not in h_text:
        problems.append("PluginUpdateHelper.h must declare the shared-table mutex")
    return problems


def make_code(cpp_text, h_text, helper_text):
    code = r'''#include <atomic>
#include <cstdio>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "tinyxml2/tinyxml2.cpp"
using namespace std;

// ---- controlled stubs for the extraction's external dependencies ----
struct CCommon {
    static bool g_url_ok;
    static thread_local string g_url_body; //每个下载桩有自己的响应，不在测试桩引入另一场数据竞争。
    static bool GetURL(const wstring&, string& result, const wstring& = wstring()) {
        if (!g_url_ok) return false;
        result = g_url_body;
        return true;
    }
    static void StringSplit(const string& str, char div_ch, vector<string>& results,
                            bool skip_empty = true, bool trim = true) {
        results.clear();
        string current;
        for (char ch : str) {
            if (ch == div_ch) { results.push_back(current); current.clear(); }
            else current.push_back(ch);
        }
        results.push_back(current);
        if (skip_empty) {
            vector<string> kept;
            for (auto& item : results) if (!item.empty()) kept.push_back(item);
            results.swap(kept);
        }
    }
    static string AsciiToStr(const wstring& text) { return string(text.begin(), text.end()); }
    static wstring AsciiToUnicode(const string& text) { return wstring(text.begin(), text.end()); }
    static wstring StrToUnicode(const string& text) { return AsciiToUnicode(text); }
    // LoadXmlFile is extracted for completeness but never invoked here.
    static char* GetFileContent(const wchar_t*, size_t&) { return nullptr; }
};
bool CCommon::g_url_ok = false;
thread_local string CCommon::g_url_body;

struct App {
    struct { int update_source{ 0 }; } m_general_data;
} theApp;

'''
    code += native.no_includes((native.SOURCE / "TinyXml2Helper.h").read_text(encoding="utf-8-sig")) + "\n"
    code += native.no_includes(h_text) + "\n"
    code += native.no_includes(cpp_text) + "\n"
    code += native.no_includes(helper_text) + "\n"
    code += r'''
#pragma warning(push)
#pragma warning(disable: 4100 4189)
static int checks{}, failures{};
static bool Check(bool condition, const string& label) {
    ++checks;
    if (!condition) ++failures;
    printf("%s %s\n", condition ? "PASS" : "FAIL", label.c_str());
    return condition;
}

static const char* k_xml_two_plugins =
    "<plugins>\n"
    "  <plugin file_name=\"hardware_monitor.dll\" version=\"1.5.2\"/>\n"
    "  <plugin file_name=\"weather.dll\" version=\"0.9.1\"/>\n"
    "</plugins>\n";
static const char* k_xml_swapped =
    "<plugins>\n"
    "  <plugin file_name=\"hardware_monitor.dll\" version=\"2.0.0\"/>\n"
    "</plugins>\n";

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) return 2;
    const wstring mode = argv[1];
    CPluginUpdateHelper helper;
    if (mode == L"parse") {
        CCommon::g_url_ok = true; CCommon::g_url_body = k_xml_two_plugins;
        Check(helper.CheckForUpdate(), "successful-download-succeeds");
        Check(helper.GetPluginLatestVersions(L"hardware_monitor.dll").GetVersionString() == "1.5.2",
              "lookup-hits-downloaded-version");
        Check(helper.GetPluginLatestVersions(L"missing.dll").GetVersionString().empty(),
              "unknown-plugin-returns-empty-version");
        Check(helper.GetPluginLatestVersions(L"weather.dll").GetVersionString() == "0.9.1",
              "second-plugin-version-hits");
    } else if (mode == L"failure-keeps-previous") {
        CCommon::g_url_ok = true; CCommon::g_url_body = k_xml_two_plugins;
        Check(helper.CheckForUpdate(), "first-download-succeeds");
        CCommon::g_url_ok = false;
        Check(!helper.CheckForUpdate(), "failed-download-reports-failure");
        Check(helper.GetPluginLatestVersions(L"hardware_monitor.dll").GetVersionString() == "1.5.2",
              "failed-download-keeps-previous-table");
    } else if (mode == L"malformed-keeps-previous") {
        CCommon::g_url_ok = true; CCommon::g_url_body = k_xml_two_plugins;
        helper.CheckForUpdate();
        for (const char* invalid : {"", "<plugins><plugin", "not XML"}) {
            CCommon::g_url_body = invalid;
            Check(!helper.CheckForUpdate(), "malformed-response-reports-failure");
            Check(helper.GetPluginLatestVersions(L"hardware_monitor.dll").GetVersionString() == "1.5.2",
                  "malformed-response-preserves-last-successful-table");
        }
    } else if (mode == L"copy-isolated") {
        CCommon::g_url_ok = true; CCommon::g_url_body = k_xml_two_plugins;
        helper.CheckForUpdate();
        const PluginVersion copy = helper.GetPluginLatestVersions(L"hardware_monitor.dll");
        Check(copy.GetVersionString() == "1.5.2", "copy-hits-current-table");
        CCommon::g_url_body = k_xml_swapped;
        helper.CheckForUpdate();     // background-style rebuild of the table
        Check(copy.GetVersionString() == "1.5.2", "returned-copy-is-not-invalidated-by-swap");
        Check(helper.GetPluginLatestVersions(L"hardware_monitor.dll").GetVersionString() == "2.0.0",
              "later-lookup-sees-new-table");
    } else if (mode == L"concurrency") {
        CCommon::g_url_ok = true;
        atomic<bool> stop{ false };
        atomic<int> anomalies{ 0 };
        vector<thread> writers;
        for (int w = 0; w < 2; ++w) {
            writers.emplace_back([&stop, &helper, w]() {
                for (int round = 0; round < 800 && !stop.load(); ++round) {
                    CCommon::g_url_body = (round % 2 == w) ? k_xml_two_plugins : k_xml_swapped;
                    helper.CheckForUpdate();
                }
            });
        }
        vector<thread> readers;
        for (int r = 0; r < 4; ++r) {
            readers.emplace_back([&stop, &anomalies, &helper]() {
                while (!stop.load()) {
                    PluginVersion version = helper.GetPluginLatestVersions(L"hardware_monitor.dll");
                    const string text = version.GetVersionString();
                    if (text != "1.5.2" && text != "2.0.0" && !text.empty()) ++anomalies;
                }
            });
        }
        for (auto& thread : writers) thread.join();
        stop = true;
        for (auto& thread : readers) thread.join();
        Check(anomalies.load() == 0, "concurrent-readers-see-only-whole-tables");
        Check(!helper.GetPluginLatestVersions(L"hardware_monitor.dll").GetVersionString().empty(),
              "table-survives-concurrent-rebuilds");
    } else {
        return 2;
    }
    printf("CHECKS %d PASS %d FAIL %d\n", checks, checks - failures, failures);
    return failures ? 1 : 0;
}
'''
    return code


def main():
    cpp_path = native.SOURCE / "PluginUpdateHelper.cpp"
    h_path = native.SOURCE / "PluginUpdateHelper.h"
    helper_path = native.SOURCE / "TinyXml2Helper.cpp"
    cpp_text = cpp_path.read_text(encoding="utf-8-sig")
    h_text = h_path.read_text(encoding="utf-8-sig")
    helper_text = helper_path.read_text(encoding="utf-8-sig")

    contract_problems = extraction_contracts(cpp_text, h_text)
    contract_failures = 0
    for problem in contract_problems:
        print(f"FAIL extraction-contract: {problem}")
        contract_failures += 1
    if not contract_problems:
        print("PASS extraction-contract: value-returning locked lookups and atomic swap present")

    code = make_code(cpp_text, h_text, helper_text)
    run, env = native.build(code, "plugin-update-helper")
    cases = ("parse", "failure-keeps-previous", "copy-isolated", "concurrency", "malformed-keeps-previous")
    results = {case: native.execute(run, env, case, timeout=30) for case in cases}
    passed = sum(results.values())
    summary = f"RESULT native {passed}/{len(cases)} cases; contracts {'pass' if not contract_failures else str(contract_failures) + ' problems'}"
    print(summary)
    return int(passed != len(cases) or bool(contract_failures))


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    raise SystemExit(main())
