"""Native regressions for the PluginManager third-party ABI boundary.

The fixture extracts the current PluginManager methods on every run. It does not
load a DLL or start PulseBar; plugin items and the host DPI/logging boundary are
small in-memory stand-ins.

RED cases cover negative/oversized custom-draw widths, zero-width semantics,
throwing plugin callbacks, and null item metadata. The command-name case is
covered by test_tray_plugin_menu.py because command IDs belong to the menu host,
not PluginManager's item registry.
"""
from pathlib import Path
import hashlib
import importlib.util
import sys

SUPPORT = Path(__file__).resolve().with_name("native_test_support.py")
spec = importlib.util.spec_from_file_location("plugin_manager_native_support", SUPPORT)
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)

EXPECTED_MIN_WIDTH = 1
EXPECTED_MAX_WIDTH = 4096


def make_code():
    source = native.source("PluginManager.cpp")
    logger = native.method(source, "static void LogPluginCallbackFailure(")
    width = native.method(source, "int CPluginManager::GetItemWidth(")
    valid = native.method(source, "static bool IsPluginItemValid(")

    code = r'''#include <climits>
#include <cstdio>
#include <map>
#include <stdexcept>
#include <string>
using namespace std;
using HDC = void*;

int g_log_calls{};
struct CCommon {
    static void WriteLog(const wchar_t*, const wchar_t*) { ++g_log_calls; }
};

struct CDC {
    void* handle{this};
    void* GetSafeHdc() const { return handle; }
};

struct IPluginItem {
    int ex_width{};
    int legacy_width{};
    bool throw_ex{};
    bool throw_legacy{};
    const wchar_t* id{L"fixture-id"};
    const wchar_t* name{L"fixture-name"};
    const wchar_t* label{L"fixture-label"};

    const wchar_t* GetItemId() const {
        if (id == reinterpret_cast<const wchar_t*>(1)) throw runtime_error("id callback");
        return id;
    }
    const wchar_t* GetItemName() const {
        if (name == reinterpret_cast<const wchar_t*>(1)) throw runtime_error("name callback");
        return name;
    }
    const wchar_t* GetItemLableText() const {
        if (label == reinterpret_cast<const wchar_t*>(1)) throw runtime_error("label callback");
        return label;
    }
    int GetItemWidthEx(void*) const {
        if (throw_ex) throw runtime_error("width ex callback");
        return ex_width;
    }
    int GetItemWidth() const {
        if (throw_legacy) throw runtime_error("width callback");
        return legacy_width;
    }
};

struct ITMPlugin {
    int api_version{8};
    bool throw_api{};
    int GetAPIVersion() const {
        if (throw_api) throw runtime_error("api version callback");
        return api_version;
    }
};

struct App {
    int dpi{96};
    wstring m_log_path{L"fixture.log"};
    int DPI(int pixel) const { return dpi * pixel / 96; }
};
App theApp;

class CPluginManager {
public:
    static constexpr int MIN_PLUGIN_ITEM_WIDTH = 1;
    static constexpr int MAX_PLUGIN_ITEM_WIDTH = 4096;
    map<IPluginItem*, ITMPlugin*> bindings;
    ITMPlugin* GetPluginByItem(IPluginItem* item) {
        auto found = bindings.find(item);
        return found == bindings.end() ? nullptr : found->second;
    }
    int GetItemWidth(IPluginItem* pItem, CDC* pDC);
};

'''
    code += "// CURRENT PluginManager.cpp :: LogPluginCallbackFailure\n" + logger + "\n"
    code += "// CURRENT PluginManager.cpp :: IsPluginItemValid\n" + valid + "\n"
    code += "// CURRENT PluginManager.cpp :: GetItemWidth\n" + width + "\n"
    code += rf'''
bool Check(bool value, const char* label) {{
    printf("%s %s\n", value ? "PASS" : "FAIL", label);
    return value;
}}

bool WidthBoundaries() {{
    CPluginManager manager;
    CDC dc;
    ITMPlugin ex_plugin{{3, false}};
    ITMPlugin legacy_plugin{{2, false}};

    IPluginItem negative_ex;
    negative_ex.ex_width = -17;
    negative_ex.legacy_width = 73;
    manager.bindings[&negative_ex] = &ex_plugin;
    const int negative_ex_width = manager.GetItemWidth(&negative_ex, &dc);
    bool ok = Check(negative_ex_width == {EXPECTED_MIN_WIDTH}
                    && negative_ex_width > 0,
                    "negative GetItemWidthEx becomes a visible positive width");

    IPluginItem negative_legacy;
    negative_legacy.legacy_width = -19;
    manager.bindings[&negative_legacy] = &legacy_plugin;
    const int negative_legacy_width = manager.GetItemWidth(&negative_legacy, &dc);
    ok &= Check(negative_legacy_width == {EXPECTED_MIN_WIDTH}
                && negative_legacy_width > 0,
                "negative GetItemWidth becomes a visible positive width");

    IPluginItem huge_ex;
    huge_ex.ex_width = INT_MAX;
    manager.bindings[&huge_ex] = &ex_plugin;
    const int huge_ex_width = manager.GetItemWidth(&huge_ex, &dc);
    ok &= Check(huge_ex_width == {EXPECTED_MAX_WIDTH}
                && huge_ex_width > 0,
                "oversized GetItemWidthEx is capped");

    IPluginItem huge_legacy;
    huge_legacy.legacy_width = INT_MAX;
    manager.bindings[&huge_legacy] = &legacy_plugin;
    theApp.dpi = 192;
    const int huge_legacy_width = manager.GetItemWidth(&huge_legacy, &dc);
    theApp.dpi = 96;
    ok &= Check(huge_legacy_width == {EXPECTED_MAX_WIDTH}
                && huge_legacy_width > 0,
                "oversized DPI-scaled GetItemWidth is capped without overflow");

    IPluginItem zero;
    zero.ex_width = 0;
    zero.legacy_width = 0;
    manager.bindings[&zero] = &ex_plugin;
    const int zero_width = manager.GetItemWidth(&zero, &dc);
    ok &= Check(zero_width == 0,
                "zero custom-draw width retains its legal zero-width meaning");

    IPluginItem fallback;
    fallback.ex_width = 0;
    fallback.legacy_width = 50;
    manager.bindings[&fallback] = &ex_plugin;
    theApp.dpi = 192;
    const int fallback_width = manager.GetItemWidth(&fallback, &dc);
    theApp.dpi = 96;
    ok &= Check(fallback_width == 100,
                "zero GetItemWidthEx still falls back to DPI-scaled GetItemWidth");

    // A taskbar rectangle built from a clamped width must not be empty. This
    // models the m_rect guard without extracting TaskBarDlg or changing it.
    ok &= Check(negative_ex_width > 0 && huge_ex_width > 0,
                "clamped plugin widths cannot create an empty taskbar rectangle");
    return ok;
}}

bool CallbackGuards() {{
    CPluginManager manager;
    CDC dc;
    ITMPlugin api_throws{{3, true}};
    IPluginItem api_throw_item;
    api_throw_item.ex_width = 12;
    api_throw_item.legacy_width = 14;
    manager.bindings[&api_throw_item] = &api_throws;

    bool ok = true;
    try {{
        const int width = manager.GetItemWidth(&api_throw_item, &dc);
        ok &= Check(width == 14,
                    "GetAPIVersion exception falls back to the legacy width");
    }} catch (...) {{
        ok &= Check(false, "GetAPIVersion exception is contained at the host boundary");
    }}

    ITMPlugin ex_plugin{{3, false}};
    IPluginItem ex_throws;
    ex_throws.throw_ex = true;
    ex_throws.legacy_width = 23;
    manager.bindings[&ex_throws] = &ex_plugin;
    try {{
        const int width = manager.GetItemWidth(&ex_throws, &dc);
        ok &= Check(width == 23,
                    "GetItemWidthEx exception falls back to GetItemWidth");
    }} catch (...) {{
        ok &= Check(false, "GetItemWidthEx exception is contained at the host boundary");
    }}

    ITMPlugin legacy_plugin{{2, false}};
    IPluginItem legacy_throws;
    legacy_throws.throw_legacy = true;
    manager.bindings[&legacy_throws] = &legacy_plugin;
    try {{
        const int width = manager.GetItemWidth(&legacy_throws, &dc);
        ok &= Check(width == 0,
                    "GetItemWidth exception becomes a safe zero width");
    }} catch (...) {{
        ok &= Check(false, "GetItemWidth exception is contained at the host boundary");
    }}

    return ok && Check(g_log_calls >= 3,
                       "plugin callback failures are logged instead of escaping");
}}

bool NullMetadata() {{
    IPluginItem valid{{}};
    bool ok = Check(IsPluginItemValid(&valid), "complete plugin item metadata is accepted");

    IPluginItem null_id{{}};
    null_id.id = nullptr;
    ok &= Check(!IsPluginItemValid(&null_id), "null plugin item ID is rejected");

    IPluginItem null_name{{}};
    null_name.name = nullptr;
    ok &= Check(!IsPluginItemValid(&null_name), "null plugin item name is rejected");

    IPluginItem null_label{{}};
    null_label.label = nullptr;
    ok &= Check(!IsPluginItemValid(&null_label), "null plugin item label is rejected");

    IPluginItem throwing_name{{}};
    throwing_name.name = reinterpret_cast<const wchar_t*>(1);
    try {{
        ok &= Check(!IsPluginItemValid(&throwing_name),
                    "throwing plugin item name is rejected without escaping");
    }} catch (...) {{
        ok &= Check(false, "throwing plugin item metadata is contained at the host boundary");
    }}
    return ok;
}}

int wmain(int argc, wchar_t** argv) {{
    if (argc != 3) return 2;
    const wstring mode = argv[1];
    bool ok = false;
    if (mode == L"widths") ok = WidthBoundaries();
    else if (mode == L"callbacks") ok = CallbackGuards();
    else if (mode == L"null-metadata") ok = NullMetadata();
    else return 2;
    return ok ? 0 : 1;
}}
'''
    return code, {
        "source_files": {"PluginManager.cpp": hashlib.sha256(source.encode("utf-8")).hexdigest()},
        "extracted": [
            {"signature": "static bool IsPluginItemValid(",
             "sha256": hashlib.sha256(valid.encode("utf-8")).hexdigest()},
            {"signature": "int CPluginManager::GetItemWidth(",
             "sha256": hashlib.sha256(width.encode("utf-8")).hexdigest()},
        ],
    }


def main():
    code, provenance = make_code()
    run, env = native.build(code, "plugin-manager-boundaries")
    (run / "source-provenance.json").write_text(
        __import__("json").dumps(provenance, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8")
    modes = ("widths", "callbacks", "null-metadata")
    results = {mode: native.execute(run, env, mode, timeout=10) for mode in modes}
    summary = "RESULT " + ", ".join(
        f"{mode}={'PASS' if passed else 'FAIL'}" for mode, passed in results.items())
    print(summary)
    return 0 if all(results.values()) else 1


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    raise SystemExit(main())
