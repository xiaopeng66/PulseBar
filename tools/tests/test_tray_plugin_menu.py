"""Tray plugin menu regression tests.

Run with: python -X utf8 -I -B <absolute path to this file>
Every run extracts the CURRENT CPulseBarApp::UpdateTrayPluginMenu and
CPulseBarDlg::OnPluginTrayCommand. The C++ fixture is an in-memory HMENU model
that mirrors MFC popup ownership (a parent that adopts a popup owns its handle),
plus small plugin/config/dialog stubs. It deliberately does not build the full
application.

The desired tray shape is direct plugin-name popups after ID_PLUGIN_MANAGE.
Each popup contains Detail, Options, Disable/Enable, a separator, and the
plugin's non-null custom commands. The dynamic registry, rather than vector
positions or taskbar click state, owns command routing.
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

    tray_fragment = native.method(cpp, "void CPulseBarApp::UpdateTrayPluginMenu()")
    command_fragment = native.method(dlg_cpp, "void CPulseBarDlg::OnPluginTrayCommand(UINT nID)")
    if "m_clicked_item" in command_fragment or "m_item_selected" in command_fragment:
        raise AssertionError("tray command routing must not use taskbar/dialog click state")

    code = r'''#include <algorithm>
#include <cstdint>
#include <cwchar>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
using namespace std;
using UINT = unsigned int;
using BOOL = int;
using DWORD = unsigned long;
using HICON = void*;
using HMENU = void*;
using HWND = void*;
using UINT_PTR = unsigned long long;
constexpr BOOL TRUE = 1, FALSE = 0;
constexpr UINT MF_BYPOSITION = 0x400, MF_BYCOMMAND = 0, MF_POPUP = 0x10,
    MF_STRING = 0, MF_ENABLED = 0, MF_GRAYED = 1, MF_SEPARATOR = 0x800,
    MF_CHECKED = 0x8, MF_UNCHECKED = 0;
constexpr UINT MB_OK = 0, MB_ICONINFORMATION = 0x30;
'''
    code += define_value(resource_lines, "ID_DISPLAY_SETTINGS") + "\n"
    code += define_value(resource_lines, "ID_PLUGIN_MANAGE") + "\n"
    code += define_value(resource_lines, "ID_PLUGIN_TRAY_START") + "\n"
    code += define_value(resource_lines, "ID_PLUGIN_TRAY_MAX") + "\n"
    code += define_value(resource_lines, "IDI_INFO") + "\n"
    code += define_value(resource_lines, "IDI_SETTINGS") + "\n"
    code += define_value(language_lines, "IDS_PLUGIN_NO_OPTIONS_INFO") + "\n"
    code += define_value(language_lines, "IDS_PLUGIN_OPTIONS") + "\n"
    code += define_value(language_lines, "IDS_PLUGIN_INFO") + "\n"
    code += define_value(language_lines, "IDS_RESTART_TO_APPLY_CHANGE_INFO") + "\n"

    code += r'''
#define _T(x) L##x

// ---- in-memory CMenu model mirroring MFC handle ownership ----
struct MenuData {
    bool alive{};
    bool grayed{};
    vector<int> command_ids;
    vector<HMENU> popups;
    vector<wstring> texts;
    map<UINT, bool> checked;
};
struct MenuRegistry {
    map<HMENU, unique_ptr<MenuData>> menus;
    int destroyed{};
} g_menus;
int g_next_handle{};

class CMenu {
public:
    HMENU m_hMenu{};
    bool m_owned{};
    CMenu() = default;
    ~CMenu() { if (m_hMenu && m_owned) DestroyMenu(m_hMenu); }
    BOOL CreatePopupMenu() {
        m_hMenu = reinterpret_cast<HMENU>(static_cast<intptr_t>(++g_next_handle));
        g_menus.menus[m_hMenu] = make_unique<MenuData>();
        g_menus.menus[m_hMenu]->alive = true;
        m_owned = true;
        return TRUE;
    }
    BOOL AppendMenu(UINT flags, UINT_PTR id = 0, const wchar_t* text = nullptr) {
        return InsertMenu(GetMenuItemCount(), flags, id, text);
    }
    BOOL InsertMenu(int position, UINT flags, UINT_PTR id, const wchar_t* text) {
        auto found = g_menus.menus.find(m_hMenu);
        if (found == g_menus.menus.end() || !found->second->alive) return FALSE;
        auto& data = *found->second;
        position = max(0, min(position, static_cast<int>(data.command_ids.size())));
        int command_id = (flags & MF_POPUP) ? -1 : ((flags & MF_SEPARATOR) ? 0 : static_cast<int>(id));
        HMENU popup = (flags & MF_POPUP) ? reinterpret_cast<HMENU>(id) : nullptr;
        data.command_ids.insert(data.command_ids.begin() + position, command_id);
        data.popups.insert(data.popups.begin() + position, popup);
        data.texts.insert(data.texts.begin() + position, text ? text : L"");
        return TRUE;
    }
    BOOL DeleteMenu(int position, UINT) {
        auto found = g_menus.menus.find(m_hMenu);
        if (found == g_menus.menus.end() || !found->second->alive) return FALSE;
        auto& data = *found->second;
        if (position < 0 || position >= static_cast<int>(data.command_ids.size())) return FALSE;
        HMENU popup = data.popups[position];
        data.command_ids.erase(data.command_ids.begin() + position);
        data.popups.erase(data.popups.begin() + position);
        data.texts.erase(data.texts.begin() + position);
        if (popup) DestroyMenu(popup);   // parent owns and destroys adopted popup
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
    HMENU Detach() {
        HMENU handle = m_hMenu;
        m_hMenu = nullptr;
        m_owned = false;
        return handle;
    }
    int GetMenuItemCount() const {
        return static_cast<int>(g_menus.menus.at(m_hMenu)->command_ids.size());
    }
    int GetMenuItemID(int position) const {
        const auto& data = *g_menus.menus.at(m_hMenu);
        return data.command_ids[position];
    }
    CMenu* GetSubMenu(int position) const {
        const auto& data = *g_menus.menus.at(m_hMenu);
        HMENU popup = data.popups[position];
        if (!popup) return nullptr;
        auto found = g_menus.menus.find(popup);
        if (found == g_menus.menus.end() || !found->second->alive) return nullptr;
        return View(popup);
    }
    HMENU GetSafeHmenu() const { return m_hMenu; }
    BOOL EnableMenuItem(int item, UINT flags) {
        if (flags & MF_BYPOSITION)
            g_menus.menus.at(m_hMenu)->grayed = (flags & MF_GRAYED) != 0;
        return TRUE;
    }
    BOOL CheckMenuItem(UINT item, UINT flags) {
        g_menus.menus.at(m_hMenu)->checked[item] = (flags & MF_CHECKED) != 0;
        return TRUE;
    }
    static CMenu* View(HMENU handle);
};
map<HMENU, unique_ptr<CMenu>> g_views;
CMenu* CMenu::View(HMENU handle) {
    auto& view = g_views[handle];
    if (!view) {
        view = make_unique<CMenu>();
        view->m_hMenu = handle;
        view->m_owned = false;
    }
    return view.get();
}

struct CMenuIcon { static void AddIconToMenuItem(HMENU, UINT, BOOL, HICON) {} };
struct FakeCString : wstring {
    using wstring::wstring;
    FakeCString(const wstring& value) : wstring(value) {}
    operator const wchar_t*() const { return c_str(); }
    const wchar_t* GetString() const { return c_str(); }
};
int callback_failures_logged{};
struct CException { virtual ~CException()=default; void Delete(){delete this;} };
struct CFlagLocker {
    bool& flag; explicit CFlagLocker(bool& value):flag(value){flag=true;} ~CFlagLocker(){flag=false;}
};
struct CCommon {
    static void WriteLog(const char*,const wchar_t*) {++callback_failures_logged;}
    static FakeCString LoadText(const wchar_t* id, const wchar_t* suffix = L"") {
        return FakeCString(wstring(id) + suffix);
    }
};
struct CFilePathHelper {
    wstring path;
    explicit CFilePathHelper(const wstring& value) : path(value) {}
    wstring GetFileName() const {
        const size_t slash = path.find_last_of(L"\\/");
        return path.substr(slash == wstring::npos ? 0 : slash + 1);
    }
};

// ---- plugin/config model ----
struct StringSet {
    set<wstring> values;
    bool Contains(const wstring& value) const { return values.count(value) != 0; }
    void SetStrContained(const wstring& value, bool contained) {
        if (contained) values.insert(value); else values.erase(value);
    }
};
struct PluginStub {
    wstring name;
    bool loaded{ true };
    int api_version{ 8 };
    int options_result{ 2 };
    int options_calls{};
    void* options_parent{};
    vector<wstring> command_names;
    vector<bool> command_null;
    int command_count_override{ -1 };
    vector<int> command_calls;
    vector<int> command_icon_calls;
    bool scratch_names{}, checked{}, throw_info{}, throw_command{};
    wstring scratch;
};
struct ITMPlugin {
    enum PluginInfoIndex { TMI_NAME };
    enum OptionReturn { OR_OPTION_CHANGED, OR_OPTION_UNCHANGED, OR_OPTION_NOT_PROVIDED };
    PluginStub stub;
    explicit ITMPlugin(PluginStub value) : stub(std::move(value)) {}
    const wchar_t* GetInfo(PluginInfoIndex) {
        if (stub.throw_info) throw runtime_error("injected GetInfo failure");
        return stub.name.empty() ? nullptr : stub.name.c_str();
    }
    int GetAPIVersion() const { return stub.api_version; }
    void* GetPluginIcon() { return nullptr; }
    OptionReturn ShowOptionsDialog(void* parent) {
        ++stub.options_calls;
        stub.options_parent = parent;
        return static_cast<OptionReturn>(stub.options_result);
    }
    int GetCommandCount() {
        return stub.command_count_override >= 0
            ? stub.command_count_override : static_cast<int>(stub.command_names.size());
    }
    const wchar_t* GetCommandName(int index) {
        if (index < 0 || index >= static_cast<int>(stub.command_names.size())) return nullptr;
        if (index < static_cast<int>(stub.command_null.size()) && stub.command_null[index]) return nullptr;
        if (stub.scratch_names) {
            stub.scratch = stub.command_names[index];
            return stub.scratch.c_str();
        }
        return stub.command_names[index].c_str();
    }
    void* GetCommandIcon(int index) {
        stub.command_icon_calls.push_back(index);
        return nullptr;
    }
    int IsCommandChecked(int) { return stub.checked ? 1 : 0; }
    void OnPluginCommand(int index, void*, void*) {
        if (stub.throw_command) throw runtime_error("injected OnPluginCommand failure");
        stub.command_calls.push_back(index);
    }
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
    int GetPluginIndex(ITMPlugin* plugin) const {
        for (int i = 0; i < static_cast<int>(plugins.size()); ++i)
            if (plugins[i].plugin == plugin) return i;
        return -1;
    }
};

enum class TrayPluginActionKind { DETAIL, OPTIONS, DISABLE, PLUGIN_COMMAND };
struct TrayPluginAction {
    wstring file_path;
    ITMPlugin* plugin{};
    TrayPluginActionKind kind{};
    int plugin_command_index{ -1 };
    wstring command_label;
};

struct CWnd {
    static CWnd* FromHandle(HWND h) {return reinterpret_cast<CWnd*>(h);}
};
HWND g_plugin_info_owner{};
vector<int> g_plugin_info_dialogs;
struct CPluginInfoDlg {
    explicit CPluginInfoDlg(int plugin_index,CWnd* owner=nullptr) {
        g_plugin_info_dialogs.push_back(plugin_index);
        g_plugin_info_owner=reinterpret_cast<HWND>(owner);
    }
    void DoModal() {}
};

// ---- host/app model ----
int g_width_changed{};
int g_message_boxes{};
bool g_taskbar_valid{ true };
HWND g_foreground{};
struct CTaskBarDlg { void WidthChanged() { ++g_width_changed; } };
struct CWndBase {
    HWND GetSafeHwnd() { return this; }
    int MessageBox(const FakeCString&, const wchar_t*, UINT) { ++g_message_boxes; return 1; }
};
struct CBaseDialog {
    static vector<pair<int, HWND>>& AllUniqueHandels() { static vector<pair<int, HWND>> dialogs; return dialogs; }
};
HWND GetForegroundWindow() { return g_foreground; }
BOOL IsWindow(HWND window) { return window != nullptr; }
BOOL IsWindowVisible(HWND window) { return window != nullptr; }
BOOL IsWindowEnabled(HWND window) { return window != nullptr; }
constexpr UINT GA_ROOT=2;
HWND GetAncestor(HWND window,UINT) {return window;}
HWND GetLastActivePopup(HWND window) {return window;}

struct CPulseBarDlg : public CWndBase {
    CTaskBarDlg* m_tBarDlg{ new CTaskBarDlg() };
    bool IsTaskbarWndValid() const { return g_taskbar_valid; }
    void OnPluginTrayCommand(UINT nID);
};
struct CPulseBarApp {
    struct MenuStrings { wstring LoadMenuText(const wchar_t* key) const { return key; } } m_str_table;
    CPluginManager m_plugins;
    CMenu m_taskbar_menu;
    // Kept only so the pre-change implementation still compiles; the new menu
    // must not use this intermediate popup.
    CMenu m_taskbar_menu_tray_plugins;
    StringSet plugin_disabled_unused;
    struct Config { StringSet plugin_disabled; } m_cfg_data;
    map<UINT, TrayPluginAction> m_tray_plugin_actions;
    vector<HMENU> m_tray_plugin_popup_handles;
    HMENU m_tray_plugin_parent_menu{};
    map<wstring,UINT> m_tray_plugin_command_ids;
    bool m_tray_plugin_menu_active{},m_tray_plugin_menu_building{};
    wstring m_log_path;
    HWND GetDialogOwner(HWND fallback) const;
    HICON GetMenuIcon(UINT) { return nullptr; }
    void UpdateTrayPluginMenu();
} theApp;

CMenu* FindMenuContainingCommand(CMenu* menu, UINT id) {
    if (!menu || !menu->GetSafeHmenu()) return nullptr;
    for (int i = 0; i < menu->GetMenuItemCount(); ++i)
        if (menu->GetMenuItemID(i) == static_cast<int>(id)) return menu;
    for (int i = 0; i < menu->GetMenuItemCount(); ++i) {
        CMenu* child = menu->GetSubMenu(i);
        if (child) {
            CMenu* found = FindMenuContainingCommand(child, id);
            if (found) return found;
        }
    }
    return nullptr;
}
'''
    code += take("PulseBar.cpp", "HWND CPulseBarApp::GetDialogOwner(HWND fallback) const")
    code += take("PulseBar.cpp", "void CPulseBarApp::UpdateTrayPluginMenu()")
    code += take("PulseBarDlg.cpp", "void CPulseBarDlg::OnPluginTrayCommand(UINT nID)", dlg_cpp)
    code += r'''

HMENU popup_for_handle(const vector<HMENU>& handles, int index) {
    return handles.at(index);
}
const MenuData& menu_data(HMENU handle) { return *g_menus.menus.at(handle); }
UINT action_id(TrayPluginActionKind kind, int command_index = -1) {
    for (const auto& item : theApp.m_tray_plugin_actions) {
        if (item.second.kind == kind &&
            (command_index < 0 || item.second.plugin_command_index == command_index))
            return item.first;
    }
    return 0;
}
void add_plugin(const PluginStub& stub, CPluginManager::PluginState state, const wchar_t* file) {
    CPluginManager::PluginInfo info;
    info.file_path = file;
    info.state = state;
    if (state == CPluginManager::PluginState::PS_SUCCEED) info.plugin = new ITMPlugin(stub);
    theApp.m_plugins.plugins.push_back(info);
}
void build_menu() {
    theApp.m_taskbar_menu.CreatePopupMenu();
    theApp.m_taskbar_menu.AppendMenu(MF_STRING, ID_DISPLAY_SETTINGS, L"display");
    theApp.m_taskbar_menu.AppendMenu(MF_STRING, ID_PLUGIN_MANAGE, L"manage");
    theApp.m_taskbar_menu.AppendMenu(MF_STRING, 40000, L"exit");
    g_menus.destroyed = 0;
}
void root_dynamic_names(vector<wstring>& names) {
    const auto& root = menu_data(theApp.m_taskbar_menu.GetSafeHmenu());
    for (HMENU handle : theApp.m_tray_plugin_popup_handles) {
        for (int i = 0; i < static_cast<int>(root.popups.size()); ++i)
            if (root.popups[i] == handle) names.push_back(root.texts[i]);
    }
}

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) return 2;
    const wstring mode = argv[1];
    int checks{}, failures{};
    auto check = [&](bool condition, const char* label) {
        ++checks; if (!condition) ++failures;
        printf("%s %s\n", condition ? "PASS" : "FAIL", label);
    };

    if (mode == L"direct-menu") {
        add_plugin({L"Hardware Monitor", true, 8, 2, 0, nullptr,
                    {L"Open", L"Open"}, {false, false}},
                   CPluginManager::PluginState::PS_SUCCEED, L"plugins\\a.dll");
        add_plugin({L"Weather"}, CPluginManager::PluginState::PS_SUCCEED, L"plugins\\b.dll");
        build_menu();
        theApp.UpdateTrayPluginMenu();
        vector<wstring> names;
        root_dynamic_names(names);
        check(names == vector<wstring>{L"Hardware Monitor", L"Weather"},
              "plugin-names-are-direct-root-popups");
        const bool has_first_popup = !theApp.m_tray_plugin_popup_handles.empty();
        check(has_first_popup, "direct-root-plugin-popup-is-created");
        if (has_first_popup) {
            const auto& first = menu_data(popup_for_handle(theApp.m_tray_plugin_popup_handles, 0));
            check(first.command_ids.size() == 6, "popup-has-three-base-actions-separator-and-commands");
            check(first.texts[0] == L"TXT_PLUGIN_DETAIL" && first.texts[1] == L"TXT_PLUGIN_OPTIONS" &&
                  first.texts[2] == L"TXT_PLUGIN_DISABLE", "popup-uses-requested-plugin-labels-in-order");
            check(first.command_ids[3] == 0 && first.command_ids[4] == ID_PLUGIN_TRAY_START + 3 &&
                  first.command_ids[5] == ID_PLUGIN_TRAY_START + 4,
                  "custom-commands-follow-separator-with-unique-ids");
            check(theApp.m_tray_plugin_actions.at(ID_PLUGIN_TRAY_START).kind == TrayPluginActionKind::DETAIL &&
                  theApp.m_tray_plugin_actions.at(ID_PLUGIN_TRAY_START + 1).kind == TrayPluginActionKind::OPTIONS &&
                  theApp.m_tray_plugin_actions.at(ID_PLUGIN_TRAY_START + 2).kind == TrayPluginActionKind::DISABLE,
                  "registry-records-detail-options-disable-actions");
            check(theApp.m_tray_plugin_actions.at(ID_PLUGIN_TRAY_START + 4).plugin_command_index == 1,
                  "registry-preserves-plugin-command-index");
        }
    } else if (mode == L"skip-disabled") {
        add_plugin({L"Dead dll"}, CPluginManager::PluginState::PS_MUDULE_LOAD_FAILED, L"dead.dll");
        add_plugin({L"Hardware Monitor"}, CPluginManager::PluginState::PS_SUCCEED, L"a.dll");
        add_plugin({L"Disabled"}, CPluginManager::PluginState::PS_DISABLE, L"disabled.dll");
        add_plugin({L"Weather"}, CPluginManager::PluginState::PS_SUCCEED, L"b.dll");
        build_menu();
        theApp.UpdateTrayPluginMenu();
        vector<wstring> names;
        root_dynamic_names(names);
        check(names == vector<wstring>{L"Hardware Monitor", L"Weather"},
              "failed-and-disabled-plugins-have-no-root-popup");
        const bool has_second_action = theApp.m_tray_plugin_actions.count(ID_PLUGIN_TRAY_START + 3) != 0;
        check(has_second_action && theApp.m_tray_plugin_actions.at(ID_PLUGIN_TRAY_START + 3).file_path == L"b.dll",
              "surviving-plugin-identity-uses-file-path-not-vector-slot");
        check(has_second_action && theApp.m_tray_plugin_actions.at(ID_PLUGIN_TRAY_START + 3).plugin_command_index == -1,
              "base-actions-have-no-command-index");
    } else if (mode == L"name-fallback") {
        add_plugin({L""}, CPluginManager::PluginState::PS_SUCCEED, L"C:\\plugins\\unnamed.dll");
        build_menu();
        theApp.UpdateTrayPluginMenu();
        vector<wstring> names;
        root_dynamic_names(names);
        check(names == vector<wstring>{L"unnamed.dll"}, "null-plugin-name-falls-back-to-dll-file-name");
    } else if (mode == L"commands") {
        PluginStub sparse{L"Commands"};
        sparse.command_names = {L"first", L"null", L"duplicate", L"duplicate"};
        sparse.command_null = {false, true, false, false};
        add_plugin(sparse, CPluginManager::PluginState::PS_SUCCEED, L"commands.dll");
        build_menu();
        theApp.UpdateTrayPluginMenu();
        const bool has_command_popup = !theApp.m_tray_plugin_popup_handles.empty();
        check(has_command_popup, "custom-command-plugin-popup-is-created");
        if (has_command_popup) {
            const auto& popup = menu_data(popup_for_handle(theApp.m_tray_plugin_popup_handles, 0));
            check(popup.command_ids.size() == 7, "null-command-name-is-skipped-without-consuming-id");
            check(popup.command_ids[4] == ID_PLUGIN_TRAY_START + 3 &&
                  popup.command_ids[5] == ID_PLUGIN_TRAY_START + 4 &&
                  popup.command_ids[6] == ID_PLUGIN_TRAY_START + 5,
                  "sparse-and-duplicate-command-names-get-distinct-ids");
                check(action_id(TrayPluginActionKind::PLUGIN_COMMAND, 0) == ID_PLUGIN_TRAY_START + 3 &&
                  action_id(TrayPluginActionKind::PLUGIN_COMMAND, 2) == ID_PLUGIN_TRAY_START + 4 &&
                  action_id(TrayPluginActionKind::PLUGIN_COMMAND, 3) == ID_PLUGIN_TRAY_START + 5,
                  "registry-retains-sparse-plugin-command-indices");
            check(theApp.m_plugins.plugins[0].plugin->stub.command_icon_calls == vector<int>{0, 2, 3},
                  "custom-command-icons-use-the-original-plugin-indices");
        }

        theApp.m_plugins.plugins.clear();
        PluginStub overflow{L"Overflow"};
        overflow.command_count_override = ID_PLUGIN_TRAY_MAX - ID_PLUGIN_TRAY_START + 100;
        overflow.command_names.resize(overflow.command_count_override, L"cmd");
        add_plugin(overflow, CPluginManager::PluginState::PS_SUCCEED, L"overflow.dll");
        build_menu();
        theApp.UpdateTrayPluginMenu();
        check(theApp.m_tray_plugin_actions.size() == 3 + min(257, ID_PLUGIN_TRAY_MAX-ID_PLUGIN_TRAY_START-2),
              "command-overflow-is-bounded-by-plugin-command-cap-and-id-band");
        bool ids_in_band = true;
        for (const auto& item : theApp.m_tray_plugin_actions)
            ids_in_band = ids_in_band && item.first >= ID_PLUGIN_TRAY_START && item.first <= ID_PLUGIN_TRAY_MAX;
        check(ids_in_band, "all-dynamic-command-ids-stay-in-band");
    } else if (mode == L"rebuild") {
        add_plugin({L"Hardware Monitor", true, 8, 2, 0, nullptr, {L"one"}, {false}},
                   CPluginManager::PluginState::PS_SUCCEED, L"a.dll");
        build_menu();
        theApp.UpdateTrayPluginMenu();
        const vector<HMENU> old_handles = theApp.m_tray_plugin_popup_handles;
        const int destroyed_before_rebuild = g_menus.destroyed;
        const UINT old_id = ID_PLUGIN_TRAY_START;
        theApp.UpdateTrayPluginMenu();
        const bool had_old_popup = !old_handles.empty();
        check(had_old_popup && g_menus.destroyed > destroyed_before_rebuild,
              "rebuild-destroys-old-parent-owned-popup");
        check(had_old_popup && !g_menus.menus.at(old_handles[0])->alive,
              "old-popup-handle-is-dead-after-rebuild");
        check(theApp.m_tray_plugin_popup_handles.size() == 1 && had_old_popup &&
              theApp.m_tray_plugin_popup_handles[0] != old_handles[0],
              "rebuild-installs-a-fresh-popup-handle");
        check(theApp.m_tray_plugin_actions.count(old_id) == 1,
              "rebuild-repopulates-registry-for-current-popup-only");
    } else if (mode == L"empty") {
        add_plugin({L"Broken"}, CPluginManager::PluginState::PS_INSTANCE_NULL, L"broken.dll");
        add_plugin({L"Disabled"}, CPluginManager::PluginState::PS_DISABLE, L"disabled.dll");
        build_menu();
        theApp.UpdateTrayPluginMenu();
        const auto& root = menu_data(theApp.m_taskbar_menu.GetSafeHmenu());
        bool has_group_popup = false;
        for (const auto& text : root.texts) has_group_popup = has_group_popup || text == L"IDS_PLUGIN_MENU";
        check(theApp.m_tray_plugin_popup_handles.empty(), "no-usable-plugin-hides-direct-plugin-entries");
        check(!has_group_popup && theApp.m_tray_plugin_actions.empty(),
              "empty-menu-has-no-intermediate-plugin-入口");
    } else if (mode == L"route") {
        PluginStub stub{L"Hardware Monitor", true, 8, 0, 0, nullptr, {L"Run command"}, {false}};
        add_plugin(stub, CPluginManager::PluginState::PS_SUCCEED, L"a.dll");
        build_menu();
        theApp.UpdateTrayPluginMenu();
        HWND modal = reinterpret_cast<HWND>(static_cast<intptr_t>(0x55));
        g_foreground = modal;
        CBaseDialog::AllUniqueHandels().push_back({1, modal});
        CPulseBarDlg dlg;
        dlg.OnPluginTrayCommand(action_id(TrayPluginActionKind::DETAIL));
        check(g_plugin_info_dialogs == vector<int>{0} && g_plugin_info_owner==modal, "detail-action-opens-plugin-info-with-current-modal-owner");
        auto* plugin = theApp.m_plugins.plugins[0].plugin;
        plugin->stub.options_result = ITMPlugin::OR_OPTION_CHANGED;
        dlg.OnPluginTrayCommand(action_id(TrayPluginActionKind::OPTIONS));
        check(plugin->stub.options_parent == modal, "options-use-current-foreground-modal-as-parent");
        check(g_width_changed == 1, "changed-options-refresh-width-when-taskbar-is-valid");
        g_taskbar_valid = false;
        dlg.OnPluginTrayCommand(action_id(TrayPluginActionKind::OPTIONS));
        check(g_width_changed == 1, "options-recheck-taskbar-validity-after-callback");
        g_taskbar_valid = true;
        dlg.OnPluginTrayCommand(action_id(TrayPluginActionKind::DISABLE));
        check(theApp.m_cfg_data.plugin_disabled.Contains(L"a.dll"), "disable-only-updates-config");
        check(plugin->stub.options_calls == 2 && g_message_boxes == 1,
              "disable-prompts-restart-without-unloading-plugin");
        dlg.OnPluginTrayCommand(action_id(TrayPluginActionKind::DISABLE));
        check(!theApp.m_cfg_data.plugin_disabled.Contains(L"a.dll") && g_message_boxes == 2,
              "disable-action-toggles-enable-with-restart-prompt");
        dlg.OnPluginTrayCommand(action_id(TrayPluginActionKind::PLUGIN_COMMAND));
        check(plugin->stub.command_calls == vector<int>{0}, "custom-command-calls-plugin-index");
    } else if (mode == L"many-plugins") {
        PluginStub first{L"Large"}; first.command_names.resize(61, L"bulk");
        add_plugin(first, CPluginManager::PluginState::PS_SUCCEED, L"large.dll");
        add_plugin({L"Second"}, CPluginManager::PluginState::PS_SUCCEED, L"second.dll");
        build_menu(); theApp.UpdateTrayPluginMenu();
        vector<wstring> names; root_dynamic_names(names);
        check(names==vector<wstring>{L"Large",L"Second"}, "first-plugin-cannot-consume-another-plugin-base-actions");
    } else if (mode == L"scratch-names") {
        PluginStub plugin{L"Scratch"}; plugin.command_names={L"one",L"two"}; plugin.scratch_names=true;
        add_plugin(plugin,CPluginManager::PluginState::PS_SUCCEED,L"scratch.dll");
        build_menu(); theApp.UpdateTrayPluginMenu();
        const auto& popup=menu_data(theApp.m_tray_plugin_popup_handles.at(0));
        check(popup.texts.size()==6 && popup.texts[4]==L"one" && popup.texts[5]==L"two",
              "command-names-are-copied-before-plugin-reuses-scratch-buffer");
    } else if (mode == L"checked-refresh") {
        PluginStub plugin{L"Toggle"}; plugin.command_names={L"old"};
        add_plugin(plugin,CPluginManager::PluginState::PS_SUCCEED,L"toggle.dll");
        build_menu(); theApp.UpdateTrayPluginMenu();
        theApp.m_plugins.plugins[0].plugin->stub.checked=true;
        theApp.m_plugins.plugins[0].plugin->stub.command_names[0]=L"new";
        theApp.UpdateTrayPluginMenu();
        const auto& popup=menu_data(theApp.m_tray_plugin_popup_handles.at(0));
        const UINT id=action_id(TrayPluginActionKind::PLUGIN_COMMAND,0);
        const auto checked=popup.checked.find(id);
        check(popup.texts.back()==L"new", "reopening-refreshes-plugin-command-label");
        check(checked!=popup.checked.end() && checked->second, "reopening-refreshes-IsCommandChecked");
    } else if (mode == L"rebound-stale") {
        add_plugin({L"A"}, CPluginManager::PluginState::PS_SUCCEED,L"a.dll");
        build_menu(); theApp.UpdateTrayPluginMenu();
        const UINT old_id=action_id(TrayPluginActionKind::OPTIONS);
        theApp.m_plugins.plugins.clear();
        add_plugin({L"B"}, CPluginManager::PluginState::PS_SUCCEED,L"b.dll");
        theApp.UpdateTrayPluginMenu();
        CPulseBarDlg dlg; dlg.OnPluginTrayCommand(old_id);
        check(theApp.m_plugins.plugins[0].plugin->stub.options_calls==0,
              "old-action-id-cannot-rebind-to-an-unrelated-new-plugin");
    } else if (mode == L"callback-exceptions") {
        PluginStub plugin{L"Throwing"}; plugin.throw_info=true; plugin.command_names={L"run"};
        add_plugin(plugin,CPluginManager::PluginState::PS_SUCCEED,L"throwing.dll");
        build_menu(); bool escaped=false;
        try { theApp.UpdateTrayPluginMenu(); } catch (...) { escaped=true; }
        check(!escaped,"GetInfo-exception-is-contained-by-menu-builder");
        if (!escaped) {
            CPulseBarDlg dlg; theApp.m_plugins.plugins[0].plugin->stub.throw_command=true;
            bool command_escaped=false;
            try { dlg.OnPluginTrayCommand(action_id(TrayPluginActionKind::PLUGIN_COMMAND)); } catch (...) {command_escaped=true;}
            check(!command_escaped,"OnPluginCommand-exception-does-not-escape-window-handler");
        }
    } else if (mode == L"stale") {
        add_plugin({L"Hardware Monitor"}, CPluginManager::PluginState::PS_SUCCEED, L"a.dll");
        build_menu();
        theApp.UpdateTrayPluginMenu();
        CPulseBarDlg dlg;
        const UINT stale_id = ID_PLUGIN_TRAY_START;
        auto* plugin = theApp.m_plugins.plugins[0].plugin;
        theApp.m_plugins.plugins.clear();
        theApp.UpdateTrayPluginMenu();
        dlg.OnPluginTrayCommand(stale_id);
        dlg.OnPluginTrayCommand(ID_PLUGIN_TRAY_MAX + 1);
        check(plugin->stub.options_calls == 0 && g_plugin_info_dialogs.empty() &&
              g_message_boxes == 0, "stale-and-out-of-band-ids-are-no-ops");
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
    cases = ("direct-menu", "skip-disabled", "name-fallback", "commands",
             "rebuild", "empty", "route", "stale", "many-plugins", "scratch-names",
             "checked-refresh", "rebound-stale", "callback-exceptions")
    results = {case: native.execute(run, env, case, timeout=10) for case in cases}
    passed = sum(results.values())
    print(f"RESULT native {passed}/{len(cases)} cases")
    return int(passed != len(cases))


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    raise SystemExit(main())
