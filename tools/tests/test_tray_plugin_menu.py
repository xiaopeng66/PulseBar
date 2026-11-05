"""Tray 'Plugins' submenu: one submenu per plugin with Options AND Details commands.

Run with: python -X utf8 -I -B <absolute path to this file>
Every run extracts the CURRENT CPulseBarApp::UpdateTrayPluginMenu and
CPulseBarDlg::OnPluginTrayCommand. CMenu is an in-memory HMENU model that
mirrors MFC ownership semantics (AppendMenu MF_POPUP adopts the handle,
DeleteMenu destroys it, Detach releases it); plugins, dialogs, icons and
LoadText are stubs. No real window, Explorer, plugin dll or application.

Scenarios:
- submenus: every successfully loaded plugin gets a submenu with exactly two
  commands: Options (START + index*2) and Details (START + index*2 + 1).
- skip-disabled: plugins that are disabled or failed to load are skipped while
  surviving commands keep mapping to the original vector index.
- name-fallback: a plugin that returns a null TMI_NAME falls back to its file name.
- overflow: more plugins than the ID band supports are truncated to the band.
- rebuild-destroys: a rebuild destroys every previously created submenu handle.
- empty-grayed: with no usable plugin the tray entry is grayed out.
- route-options/route-detail: OnPluginTrayCommand routes by (nID-START)/2 with
  %2 choosing Options or Details, validates the plugin state, and handles the
  OR_OPTION_* results; out-of-band IDs do nothing.

On the previous one-command-per-plugin implementation the submenu/route-detail
expectations fail - that is the RED this suite is written against.
"""
from pathlib import Path
import hashlib
import importlib.util
import sys

SUPPORT = Path(__file__).resolve().with_name("native_test_support.py")
spec = importlib.util.spec_from_file_location("tray_native_support", SUPPORT)
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)


def define_value(lines, name):
    hits = [line for line in lines if line.startswith("#define " + name + " ")]
    if len(hits) != 1:
        raise ValueError(f"cannot extract {name}: {len(hits)} hits")
    return hits[0]


def make_code():
    cpp = native.source("PulseBar.cpp")
    dlg_cpp = native.source("PulseBarDlg.cpp")
    resource_lines = native.source("resource.h").splitlines()
    language_lines = native.source("language.h").splitlines()
    extracted = []

    def take(name, signature, text=None):
        text = text if text is not None else cpp
        fragment = native.method(text, signature)
        extracted.append({"file": name, "signature": signature,
                          "sha256": hashlib.sha256(fragment.encode("utf-8")).hexdigest()})
        return "\n// CURRENT " + name + " :: " + signature + "\n" + fragment + "\n"

    code = r'''#include <cwchar>
#include <map>
#include <memory>
#include <string>
#include <vector>
using namespace std;
using UINT = unsigned int;
using BOOL = int;
using DWORD = unsigned long;
using HICON = void*;
using HMENU = void*;
using UINT_PTR = unsigned long long;
constexpr BOOL TRUE = 1, FALSE = 0;
constexpr UINT MF_BYPOSITION = 0x400, MF_BYCOMMAND = 0, MF_POPUP = 0x10,
    MF_STRING = 0, MF_ENABLED = 0, MF_GRAYED = 1, MF_SEPARATOR = 0x800;
constexpr UINT MB_OK = 0, MB_ICONINFORMATION = 0x30;
'''
    code += define_value(resource_lines, "ID_DISPLAY_SETTINGS") + "\n"
    code += define_value(resource_lines, "ID_PLUGIN_MANAGE") + "\n"
    code += define_value(resource_lines, "ID_PLUGIN_TRAY_START") + "\n"
    code += define_value(resource_lines, "ID_PLUGIN_TRAY_MAX") + "\n"
    code += define_value(language_lines, "IDS_PLUGIN_NO_OPTIONS_INFO") + "\n"
    code += define_value(language_lines, "IDS_PLUGIN_OPTIONS") + "\n"
    code += define_value(language_lines, "IDS_PLUGIN_INFO") + "\n"

    code += r'''
#define _T(x) L##x
// ---- in-memory CMenu model mirroring MFC handle ownership ----
struct MenuData { bool alive{}; bool grayed{}; vector<int> command_ids; vector<HMENU> popups; vector<wstring> texts; };
struct MenuRegistry { map<HMENU, unique_ptr<MenuData>> menus; int destroyed{}; } g_menus;
int g_alive_handles = 0;

class CMenu {
public:
    HMENU m_hMenu{};
    CMenu() = default;
    ~CMenu() { if (m_hMenu) DestroyMenu(m_hMenu); }
    BOOL CreatePopupMenu() {
        m_hMenu = reinterpret_cast<HMENU>(static_cast<intptr_t>(++g_alive_handles));
        g_menus.menus[m_hMenu] = make_unique<MenuData>();
        g_menus.menus[m_hMenu]->alive = true;
        return TRUE;
    }
    BOOL AppendMenu(UINT flags, UINT_PTR id, const wchar_t* text) {
        auto& data = *g_menus.menus.at(m_hMenu);
        if (flags & MF_POPUP) {
            data.command_ids.push_back(-1);
            data.popups.push_back(reinterpret_cast<HMENU>(id));
            data.texts.push_back(text ? text : L"");
        } else if (flags & MF_SEPARATOR) {
            data.command_ids.push_back(0);
            data.popups.push_back(nullptr);
            data.texts.push_back(L"");
        } else {
            data.command_ids.push_back(static_cast<int>(id));
            data.popups.push_back(nullptr);
            data.texts.push_back(text ? text : L"");
        }
        return TRUE;
    }
    BOOL DeleteMenu(int position, UINT) {
        auto& data = *g_menus.menus.at(m_hMenu);
        if (position < 0 || position >= static_cast<int>(data.command_ids.size())) return FALSE;
        HMENU popup = data.popups[position];
        data.command_ids.erase(data.command_ids.begin() + position);
        data.popups.erase(data.popups.begin() + position);
        data.texts.erase(data.texts.begin() + position);
        if (popup) DestroyMenu(popup);   // MFC semantics: deleting a popup item destroys it
        return TRUE;
    }
    BOOL DestroyMenu(HMENU handle) {
        auto found = g_menus.menus.find(handle);
        if (found == g_menus.menus.end() || !found->second->alive) return FALSE;
        for (HMENU popup : found->second->popups)
            if (popup) DestroyMenu(popup);
        found->second->alive = false;
        ++g_menus.destroyed;
        return TRUE;
    }
    // Borrowed view of a handle owned elsewhere; never destroys it.
    static CMenu* View(HMENU handle);
    HMENU Detach() { HMENU handle = m_hMenu; m_hMenu = nullptr; m_owned = false; return handle; }
    int GetMenuItemCount() const { return static_cast<int>(g_menus.menus.at(m_hMenu)->command_ids.size()); }
    CMenu* GetSubMenu(int position) const {
        HMENU popup = g_menus.menus.at(m_hMenu)->popups[position];
        return popup ? View(popup) : nullptr;
    }
    HMENU GetSafeHmenu() const { return m_hMenu; }
    BOOL EnableMenuItem(int item, UINT flags) {
        if (flags & MF_BYPOSITION) g_menus.menus.at(m_hMenu)->grayed = (flags & MF_GRAYED) != 0;
        return TRUE;
    }
private:
    bool m_owned{};
};
map<HMENU, unique_ptr<CMenu>> g_views;
// Out-of-line because make_unique<CMenu> needs the complete type.
CMenu* CMenu::View(HMENU handle) {
    auto& view = g_views[handle];
    if (!view) {
        view = make_unique<CMenu>();
        view->m_hMenu = handle;
        view->m_owned = false;
    }
    return view.get();
}
// Stand-in for the production icon helper; records nothing that affects routing.
struct CMenuIcon { static void AddIconToMenuItem(HMENU, UINT, BOOL, HICON) {} };
// Mimics CString's implicit conversion to LPCTSTR for extracted call sites.
struct FakeCString : wstring {
    using wstring::wstring;
    FakeCString(const wstring& value) : wstring(value) {}
    operator const wchar_t*() const { return c_str(); }
};
struct CCommon {
    static FakeCString LoadText(const wchar_t* id, const wchar_t* suffix = L"") { return FakeCString(wstring(id) + suffix); }
};
struct CFilePathHelper {
    wstring path;
    explicit CFilePathHelper(const wstring& value) : path(value) {}
    wstring GetFileName() const { return path; }
};

// ---- plugin model mirroring CPluginManager/ITMPlugin ----
struct PluginStub {
    wstring name;          // empty means the plugin returns nullptr for TMI_NAME
    bool loaded{ true };
    int api_version{ 8 };
    int options_result{ 2 }; // 0=CHANGED, 1=UNCHANGED, 2=NOT_PROVIDED
    int options_calls{};
};
struct ITMPlugin {
    enum PluginInfoIndex { TMI_NAME };
    enum OptionReturn { OR_OPTION_CHANGED, OR_OPTION_UNCHANGED, OR_OPTION_NOT_PROVIDED };
    PluginStub stub;
    explicit ITMPlugin(PluginStub value) : stub(value) {}
    const wchar_t* GetInfo(PluginInfoIndex) { return stub.name.empty() ? nullptr : stub.name.c_str(); }
    int GetAPIVersion() { return stub.api_version; }
    void* GetPluginIcon() { return nullptr; }
    OptionReturn ShowOptionsDialog(void*) { ++stub.options_calls; return static_cast<OptionReturn>(stub.options_result); }
};
struct CPluginManager {
    enum class PluginState { PS_SUCCEED, PS_MUDULE_LOAD_FAILED, PS_FUNCTION_GET_FAILED,
                             PS_VERSION_NOT_SUPPORT, PS_INSTANCE_NULL, PS_DISABLE };
    struct PluginInfo {
        wstring file_path;
        ITMPlugin* plugin{};
        PluginState state{};
    };
    vector<PluginInfo> plugins;
    const vector<PluginInfo>& GetPlugins() const { return plugins; }
};

vector<int> g_plugin_info_dialogs;
struct CPluginInfoDlg {
    explicit CPluginInfoDlg(int plugin_index) { g_plugin_info_dialogs.push_back(plugin_index); }
    void DoModal() {}
};

// ---- host/app model ----
int g_width_changed{};
int g_message_boxes{};
struct CTaskBarDlg { void WidthChanged() { ++g_width_changed; } };
struct CWndBase {
    virtual ~CWndBase() = default;
    void* GetSafeHwnd() { return this; }
    int MessageBox(const wstring&, const wchar_t*, UINT) { ++g_message_boxes; return 1; }
};
bool g_taskbar_valid{ true };
struct CPulseBarDlg : public CWndBase {
    CTaskBarDlg* m_tBarDlg{ new CTaskBarDlg() };
    bool IsTaskbarWndValid() const { return g_taskbar_valid; }
    void OnPluginTrayCommand(UINT nID);
};
struct CPulseBarApp {
    CPluginManager m_plugins;
    CMenu m_taskbar_menu;
    CMenu m_taskbar_menu_tray_plugins;
    void UpdateTrayPluginMenu();
} theApp;

// The fixture pre-builds the state InitMenuResourse creates: the tray popup is
// already inserted after ID_PLUGIN_MANAGE inside the menu that contains
// ID_DISPLAY_SETTINGS; production code relocates it by handle only.
CMenu* FindMenuContainingCommand(CMenu* menu, UINT id) {
    if (!menu || !menu->GetSafeHmenu()) return nullptr;
    auto& data = *g_menus.menus.at(menu->GetSafeHmenu());
    for (int item : data.command_ids) if (item == static_cast<int>(id)) return menu;
    for (HMENU popup : data.popups) {
        if (popup) {
            auto found = g_menus.menus.find(popup);
            if (found == g_menus.menus.end() || !found->second->alive) continue;
            CMenu* nested = FindMenuContainingCommand(CMenu::View(popup), id);
            if (nested) return nested;
        }
    }
    return nullptr;
}
'''
    code += take("PulseBar.cpp", "void CPulseBarApp::UpdateTrayPluginMenu()")
    code += take("PulseBarDlg.cpp", "void CPulseBarDlg::OnPluginTrayCommand(UINT nID)", dlg_cpp)
    code += r'''
int wmain(int argc, wchar_t** argv) {
    if (argc != 3) return 2;
    const wstring mode = argv[1];
    int checks{}, failures{};
    auto check = [&](bool condition, const char* label) {
        ++checks; if (!condition) ++failures;
        printf("%s %s\n", condition ? "PASS" : "FAIL", label);
    };
    auto add_plugin = [&](const PluginStub& stub, CPluginManager::PluginState state, const wchar_t* file) {
        CPluginManager::PluginInfo info;
        info.file_path = file;
        info.state = state;
        if (state == CPluginManager::PluginState::PS_SUCCEED) info.plugin = new ITMPlugin(stub);
        theApp.m_plugins.plugins.push_back(info);
    };
    auto build_menu = [&]() {
        theApp.m_taskbar_menu.CreatePopupMenu();
        theApp.m_taskbar_menu.AppendMenu(MF_STRING, ID_DISPLAY_SETTINGS, L"display");
        theApp.m_taskbar_menu.AppendMenu(MF_STRING, ID_PLUGIN_MANAGE, L"manage");
        theApp.m_taskbar_menu_tray_plugins.CreatePopupMenu();
        theApp.m_taskbar_menu.AppendMenu(MF_POPUP,
            reinterpret_cast<UINT_PTR>(theApp.m_taskbar_menu_tray_plugins.GetSafeHmenu()), L"Plugins");
        g_menus.destroyed = 0;
    };
    auto count_submenu_commands = [&](int index) {
        HMENU handle = g_menus.menus.at(theApp.m_taskbar_menu_tray_plugins.GetSafeHmenu())->popups[index];
        auto& submenu = *g_menus.menus.at(handle);
        return make_pair(submenu.command_ids, submenu.texts);
    };

    if (mode == L"submenus") {
        add_plugin({L"Hardware Monitor"}, CPluginManager::PluginState::PS_SUCCEED, L"a.dll");
        add_plugin({L"Weather"}, CPluginManager::PluginState::PS_SUCCEED, L"b.dll");
        add_plugin({L"Notes"}, CPluginManager::PluginState::PS_SUCCEED, L"c.dll");
        build_menu();
        theApp.UpdateTrayPluginMenu();
        auto& tray = *g_menus.menus.at(theApp.m_taskbar_menu_tray_plugins.GetSafeHmenu());
        check(tray.popups.size() == 3, "one-submenu-per-plugin");
        check(tray.texts[0] == L"Hardware Monitor" && tray.texts[1] == L"Weather" && tray.texts[2] == L"Notes",
              "submenu-titles-are-plugin-names");
        auto first = count_submenu_commands(0);
        check(first.first[0] == ID_PLUGIN_TRAY_START + 0 * 2 && first.first[1] == ID_PLUGIN_TRAY_START + 0 * 2 + 1,
              "options-and-details-ids-map-to-plugin-index");
        check(first.second[0] == L"IDS_PLUGIN_OPTIONS..." && first.second[1] == L"IDS_PLUGIN_INFO...",
              "submenu-carries-options-and-details-entries");
        auto second = count_submenu_commands(1);
        check(second.first[0] == ID_PLUGIN_TRAY_START + 1 * 2, "second-plugin-keeps-index-based-ids");
        check(!tray.grayed, "usable-plugins-keep-entry-enabled");
    } else if (mode == L"skip-disabled") {
        add_plugin({L"Dead dll"}, CPluginManager::PluginState::PS_MUDULE_LOAD_FAILED, L"dead.dll");
        add_plugin({L"Hardware Monitor"}, CPluginManager::PluginState::PS_SUCCEED, L"a.dll");
        add_plugin({L""}, CPluginManager::PluginState::PS_DISABLE, L"disabled.dll");
        add_plugin({L"Weather"}, CPluginManager::PluginState::PS_SUCCEED, L"b.dll");
        build_menu();
        theApp.UpdateTrayPluginMenu();
        auto& tray = *g_menus.menus.at(theApp.m_taskbar_menu_tray_plugins.GetSafeHmenu());
        check(tray.popups.size() == 2, "failed-and-disabled-plugins-are-skipped");
        check(tray.texts[0] == L"Hardware Monitor" && tray.texts[1] == L"Weather",
              "surviving-plugins-keep-their-names");
        auto second = count_submenu_commands(1);
        check(second.first[0] == ID_PLUGIN_TRAY_START + 3 * 2,
              "command-ids-map-to-original-vector-index-not-added-count");
    } else if (mode == L"name-fallback") {
        add_plugin({L""}, CPluginManager::PluginState::PS_SUCCEED, L"unnamed.dll");
        build_menu();
        theApp.UpdateTrayPluginMenu();
        auto& tray = *g_menus.menus.at(theApp.m_taskbar_menu_tray_plugins.GetSafeHmenu());
        check(tray.texts[0] == L"unnamed.dll", "null-plugin-name-falls-back-to-file-name");
    } else if (mode == L"overflow") {
        for (int i = 0; i < 33; ++i) add_plugin({L"P" + to_wstring(i)}, CPluginManager::PluginState::PS_SUCCEED, L"x.dll");
        build_menu();
        theApp.UpdateTrayPluginMenu();
        auto& tray = *g_menus.menus.at(theApp.m_taskbar_menu_tray_plugins.GetSafeHmenu());
        check(tray.popups.size() == 32, "plugins-are-truncated-to-the-command-id-band");
    } else if (mode == L"rebuild-destroys") {
        add_plugin({L"Hardware Monitor"}, CPluginManager::PluginState::PS_SUCCEED, L"a.dll");
        build_menu();
        theApp.UpdateTrayPluginMenu();
        const int alive_after_first = g_alive_handles - g_menus.destroyed;
        const int destroyed_before_rebuild = g_menus.destroyed;
        theApp.UpdateTrayPluginMenu();
        check(g_menus.menus.at(theApp.m_taskbar_menu_tray_plugins.GetSafeHmenu())->popups.size() == 1,
              "rebuild-recreates-the-submenu");
        check(g_menus.destroyed > destroyed_before_rebuild, "rebuild-destroys-previous-submenu-handle");
        check(g_alive_handles - g_menus.destroyed == alive_after_first,
              "live-handle-count-is-preserved-across-rebuilds");
    } else if (mode == L"empty-grayed") {
        add_plugin({L"Broken"}, CPluginManager::PluginState::PS_INSTANCE_NULL, L"broken.dll");
        build_menu();
        theApp.UpdateTrayPluginMenu();
        // The grayed flag belongs to the item inside the root menu (MF_BYPOSITION on the parent).
        check(g_menus.menus.at(theApp.m_taskbar_menu.GetSafeHmenu())->grayed,
              "no-usable-plugin-grays-out-the-entry");
    } else if (mode == L"route-options") {
        add_plugin({L"Hardware Monitor"}, CPluginManager::PluginState::PS_SUCCEED, L"a.dll");
        auto& plugin = *theApp.m_plugins.plugins[0].plugin;
        plugin.stub.options_result = ITMPlugin::OR_OPTION_CHANGED;
        CPulseBarDlg dlg;
        dlg.OnPluginTrayCommand(ID_PLUGIN_TRAY_START + 0 * 2);
        check(plugin.stub.options_calls == 1, "options-command-shows-plugin-options-dialog");
        check(g_width_changed == 1, "option-changed-refreshes-taskbar-width");
        check(g_plugin_info_dialogs.empty() && g_message_boxes == 0, "options-route-does-not-open-info-or-warn");
        plugin.stub.options_result = ITMPlugin::OR_OPTION_UNCHANGED;
        dlg.OnPluginTrayCommand(ID_PLUGIN_TRAY_START + 0 * 2);
        check(g_width_changed == 1, "unchanged-options-do-not-refresh-width");
        plugin.stub.options_result = ITMPlugin::OR_OPTION_NOT_PROVIDED;
        dlg.OnPluginTrayCommand(ID_PLUGIN_TRAY_START + 0 * 2);
        check(g_message_boxes == 1, "missing-options-informs-the-user");
    } else if (mode == L"route-detail") {
        add_plugin({L"Hardware Monitor"}, CPluginManager::PluginState::PS_SUCCEED, L"a.dll");
        CPulseBarDlg dlg;
        dlg.OnPluginTrayCommand(ID_PLUGIN_TRAY_START + 0 * 2 + 1);
        check(g_plugin_info_dialogs == vector<int>{0}, "details-command-opens-plugin-info-by-index");
        check(theApp.m_plugins.plugins[0].plugin->stub.options_calls == 0,
              "details-route-does-not-show-options-dialog");
        dlg.OnPluginTrayCommand(ID_PLUGIN_TRAY_START + 99);
        dlg.OnPluginTrayCommand(ID_PLUGIN_TRAY_START - 3);
        check(g_plugin_info_dialogs == vector<int>{0} && g_message_boxes == 0 && g_width_changed == 0,
              "out-of-band-ids-do-nothing");
        g_plugin_info_dialogs.clear();
        theApp.m_plugins.plugins[0].state = CPluginManager::PluginState::PS_DISABLE;
        dlg.OnPluginTrayCommand(ID_PLUGIN_TRAY_START + 0 * 2 + 1);
        check(g_plugin_info_dialogs.empty(), "non-succeed-state-is-rejected-at-route-time");
    } else {
        return 2;
    }
    printf("CHECKS %d PASS %d FAIL %d\n", checks, checks - failures, failures);
    return failures ? 1 : 0;
}
'''
    return code, extracted


def main():
    code, extracted = make_code()
    run, env = native.build(code, "tray-plugin-menu")
    (run / "source-provenance.json").write_text("", encoding="utf-8")
    cases = ("submenus", "skip-disabled", "name-fallback", "overflow",
             "rebuild-destroys", "empty-grayed", "route-options", "route-detail")
    results = {case: native.execute(run, env, case, timeout=10) for case in cases}
    passed = sum(results.values())
    print(f"RESULT native {passed}/{len(cases)} cases")
    return int(passed != len(cases))


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    raise SystemExit(main())
