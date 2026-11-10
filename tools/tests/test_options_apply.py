"""F11: current native Options Apply/Cancel methods, NOT a desktop/MFC test.

Run: python -X utf8 -I -B <absolute test_options_apply.py> --verify-mutations
Raw RED: append --mutation owner recreate (exit 1 is the expected regression).

The production breaks this catches are a replaceable taskbar owner, a missing
post-Apply update_source baseline, and a missing Cancel restore. Expectations
use literal values (update_source: GitHub=0), not another implementation of Apply.

Every invocation extracts CURRENT production methods. The complete host
ApplySettings, its message consumer, _OnOptions, Close/OpenTaskBarWnd and the
actual taskbar OnCancel owner-enumeration loop are compiled unchanged (except
explicit in-memory mutants). No mock implements the host's commit/rebuild or
its decision to cancel Options. General/Options state transitions are real too.

Boundaries: MFC class shells, modal driver, HWND/message dispatch, OS/rendering,
prevalidated subpage UI collection, plugin delivery and persistence sinks.
Full production settings value declarations are used. The modal driver supplies
post-OnInit state; it is not OnInitDialog, DDX, a real message pump or a real
window. No HTTP/update check, actual settings files, registry, Explorer, GUI or
application executable is used. WITHOUT_TEMPERATURE excludes hardware startup.
Handle tombstones expose lifetime/invalid-window observations without accessing
freed C++ objects; these are method-level checks, not proof of MFC/OS lifetime.
"""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import sys

SUPPORT = Path(__file__).resolve().with_name("native_test_support.py")
spec = importlib.util.spec_from_file_location("options_native_support", SUPPORT)
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)

MODES = ("owner", "recreate", "recreate-fails", "unchanged", "cancel_only", "general_cancel",
         "unready", "missing_host", "general_uncreated", "null_payload")
MUTANTS = {
    "owner": ("recreate", "FAIL recreate.options-live-on-apply-return"),
    "baseline": ("recreate", "FAIL recreate.cancel-keeps-applied-source"),
    "restore": ("cancel_only", "FAIL cancel_only.preview-restored"),
}


def make_code(sources, mutation="current"):
    extracted = []
    mutations = []

    def text(name):
        if name not in sources:
            sources[name] = native.source(name)
        return sources[name]

    def take(name, signature):
        fragment = native.method(text(name), signature)
        original = fragment
        replacement = None
        if mutation == "owner" and signature == "void CPulseBarDlg::OnOptions2()":
            replacement = ("_OnOptions(0, this);",
                           "_OnOptions(0, IsTaskbarWndValid() ? static_cast<CWnd*>(m_tBarDlg) : static_cast<CWnd*>(this));")
        elif mutation == "baseline" and signature == "void CGeneralSettingsDlg::OnSettingsApplied()":
            replacement = ("m_update_source_ori = m_data.update_source;", "(void)m_data.update_source;")
        elif mutation == "restore" and signature == "void COptionsDlg::OnCancel()":
            replacement = ("m_tab3_dlg.RestoreUpdateSource();", "(void)m_tab3_dlg;")
        if replacement:
            old, new = replacement
            if fragment.count(old) != 1:
                raise ValueError(f"Mutation anchor drift: {name}: {signature}")
            fragment = fragment.replace(old, new)
            mutations.append({"signature": signature, "before": old, "after": new,
                              "sha256": hashlib.sha256(fragment.encode("utf-8")).hexdigest()})
        extracted.append({"file": str(native.SOURCE / name), "signature": signature,
                          "sha256": hashlib.sha256(original.encode("utf-8")).hexdigest()})
        return f"\n// CURRENT {name} :: {signature}\n{fragment}\n"

    code = r'''#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
using namespace std;
using BOOL = int;
using UINT = unsigned int;
using DWORD = unsigned long;
using WORD = unsigned short;
using BYTE = unsigned char;
using COLORREF = unsigned long;
using INT_PTR = intptr_t;
// 生产代码在配置目录迁移后弹结果框；夹具里不能真的弹窗，按“已确认”返回
inline int AfxMessageBox(const wchar_t*, unsigned int = 0, unsigned int = 0) { return 1; }
using LRESULT = intptr_t;
using WPARAM = uintptr_t;
using LPARAM = intptr_t;
using WCHAR = wchar_t;
using LPCTSTR = const wchar_t*;
#define afx_msg
#define WITHOUT_TEMPERATURE
#define _T(x) L##x
#define RGB(r,g,b) COLORREF((r) | ((g) << 8) | ((b) << 16))
constexpr int TRUE=1, FALSE=0, IDOK=1, IDCANCEL=2, SW_SHOW=5, SW_RESTORE=9;
constexpr UINT WM_USER=0x400, WM_COMMAND=0x111;
constexpr UINT MB_OK=0, MB_ICONWARNING=0x30, MB_ICONINFORMATION=0x40;
// Template/resource identities and warning text are UI boundary tokens only.
constexpr int IDD_OPTIONS_DIALOG=147, IDD_GENERAL_SETTINGS_DIALOG=155,
    IDD_TASKBAR_SETTINGS_DIALOG=150, IDD_TASK_BAR_DIALOG=135,
    IDD_TASK_BAR_DIALOG_NOREDIRECTIONBITMAP=340;
constexpr int IDS_SET_AUTO_RUN_FAILED_WARNING=1, IDS_PATH=2, IDS_SET_AUTO_RUN_SUCCESS=3;
constexpr int FW_BOLD=700, FW_NORMAL=400, DEFAULT_CHARSET=1, OUT_DEFAULT_PRECIS=0,
    CLIP_DEFAULT_PRECIS=0, DEFAULT_QUALITY=0, DEFAULT_PITCH=0, FF_SWISS=0;
[[noreturn]] void Unexpected(const char* operation) { throw logic_error(operation); }
struct CString : wstring {
    using wstring::wstring;
    CString(const wstring& v): wstring(v) {}
    const wchar_t* GetString() const { return c_str(); }
    bool IsEmpty() const { return empty(); }
    operator const wchar_t*() const { return c_str(); }
    template<class... T> void Format(const wchar_t*, T...) { Unexpected("unexpected formatting/UI branch"); }
};
struct CFont {
    template<class... T> void CreateFont(T...) { Unexpected("real font creation excluded"); }
};
int FontSizeToLfHeight(int, int) { Unexpected("font metrics excluded"); }
struct IPluginItem {};
class CCommon {
public:
    static CString LoadText(int, const wchar_t* = L"") { Unexpected("autorun warning branch excluded"); }
    static CString LoadTextFormat(int, initializer_list<const wchar_t*>) { Unexpected("autorun message excluded"); }
    static void TransparentColorConvert(COLORREF&);
};
constexpr BYTE GetRValue(COLORREF c) { return BYTE(c); }
constexpr BYTE GetGValue(COLORREF c) { return BYTE(c >> 8); }
constexpr BYTE GetBValue(COLORREF c) { return BYTE(c >> 16); }
DWORD GetEnvironmentVariable(const wchar_t*, wchar_t*, DWORD) { Unexpected("environment query excluded"); }
'''
    # These are declarations, not assertion text-match tests.
    for name, symbols in {
        "stdafx.h": ("WM_SETTINGS_APPLIED", "MONITOR_TIMER", "MAX_NOTIFY_ICON_STYLE",
                      "TASKBAR_TRANSPARENT_COLOR1", "TASKBAR_TRANSPARENT_COLOR2"),
        "OptionsDlg.h": ("OPTION_DLG_NAME",),
    }.items():
        for symbol in symbols:
            lines = [line for line in text(name).splitlines() if line.startswith("#define " + symbol + " ")]
            if len(lines) != 1:
                raise ValueError(f"Cannot extract current macro {symbol}")
            code += lines[0] + "\n"
    code += native.no_includes(text("DisplayItem.h")) + "\n"
    for signature in ("enum class SpeedUnit", "enum HardwareItem", "struct DispStrings",
                      "enum class DoubleClickAction", "struct FontInfo", "enum class HistoryTrafficViewType",
                      "struct StringSet", "struct LanguageInfo", "enum NotifyIconStyle", "struct MainConfigData",
                      "enum class MemoryDisplay", "struct PublicSettingData", "struct TaskbarItemColor"):
        code += take("CommonData.h", signature) + ";\n"
    code += take("TaskbarItemOrderHelper.h", "class CTaskbarItemOrderHelper") + ";\n"
    for signature in ("struct TaskBarSettingData :", "struct GeneralSettingData"):
        code += take("CommonData.h", signature) + ";\n"
    code += native.no_includes(text("MonitorSettings.h")) + "\n"
    code += r'''
class CWnd;
class COptionsDlg;
struct WindowState {
    CWnd* object{};
    WindowState* parent{};
    bool alive{}, child{};
    int cancels{};
    string kind;
};
using HWND = WindowState*;
static_assert(is_pointer_v<HWND> && sizeof(WPARAM) >= sizeof(void*));
vector<unique_ptr<WindowState>> windows;
vector<string> events;
HWND active_options{};
int apply_messages{}, cancel_messages{}, taskbar_creates{}, taskbar_deletes{}, stale_accesses{};
bool in_apply_send{};
bool fail_taskbar_create{};

// No owner-based auto-cancel policy exists here. Production TaskBarDlg::OnCancel
// must itself inspect GetParent and send IDCANCEL. Only child-window destruction
// (not owned top-level dialogs) is an explicit MFC boundary assumption.
void DestroyMemoryWindow(HWND h) {
    if (!h || !h->alive) return;
    h->alive = false;
    events.push_back("destroy:" + h->kind);
    for (const auto& child : windows)
        if (child->child && child->parent == h) DestroyMemoryWindow(child.get());
}
BOOL IsWindow(HWND h) { return h && h->alive; }
HWND GetParent(HWND h) { return IsWindow(h) ? h->parent : nullptr; }
LRESULT SendMessage(HWND, UINT, WPARAM, LPARAM);
BOOL ShowWindow(HWND h, int) { if (!IsWindow(h)) ++stale_accesses; return TRUE; }
BOOL SetForegroundWindow(HWND h) { if (!IsWindow(h)) ++stale_accesses; return TRUE; }
class CWnd {
public:
    HWND token;
    explicit CWnd(CWnd* parent=nullptr, string kind="host", bool child=false) {
        windows.push_back(make_unique<WindowState>());
        token = windows.back().get();
        token->object=this; token->parent=parent ? parent->token : nullptr;
        token->kind=std::move(kind); token->child=child;
    }
    virtual ~CWnd() {
        if (token->kind == "taskbar") ++taskbar_deletes;
        token->alive=false; token->object=nullptr;
    }
    HWND GetSafeHwnd() const { return token->alive ? token : nullptr; }
    BOOL DestroyWindow() { DestroyMemoryWindow(token); return TRUE; }
    void RequireWindow() const { if (!token->alive) ++stale_accesses; }
    int MessageBox(const CString&, const wchar_t*, UINT) { Unexpected("GUI messages excluded"); }
    void ShowWindow(int command) { ::ShowWindow(GetSafeHwnd(), command); }
};
class CDialog : public CWnd {
public:
    inline static function<INT_PTR(CDialog&)> modal_driver;
    int modal_result{};
    CDialog(UINT id, CWnd* parent=nullptr): CWnd(parent,
        id==IDD_OPTIONS_DIALOG ? "options" : id==IDD_GENERAL_SETTINGS_DIALOG ? "general-page" :
        id==IDD_TASKBAR_SETTINGS_DIALOG ? "taskbar-page" : "taskbar",
        id==IDD_GENERAL_SETTINGS_DIALOG || id==IDD_TASKBAR_SETTINGS_DIALOG) {}
    virtual void OnCancel() {
        ++token->cancels; modal_result=IDCANCEL;
        events.push_back("cancel:" + token->kind); DestroyWindow();
    }
    virtual void OnOK() { modal_result=IDOK; DestroyWindow(); }
    INT_PTR DoModal() { return modal_driver(*this); }
    BOOL Create(UINT, CWnd* owner) {
        if (token->kind=="taskbar" && fail_taskbar_create) {
            token->alive=false;
            return FALSE;
        }
        token->parent=owner ? owner->token : nullptr; token->alive=true;
        if (token->kind=="taskbar") ++taskbar_creates;
        events.push_back("create:" + token->kind); return TRUE;
    }
};
class CBaseDialog : public CDialog {
public:
    using CDialog::CDialog;
    inline static map<CString, HWND> m_unique_hwnd;
    static HWND GetUniqueHandel(LPCTSTR);
    static const map<CString, HWND>& AllUniqueHandels();
    virtual CString GetDialogName() const { return {}; }
    INT_PTR DoModal();
};
class CTabDlg : public CBaseDialog {
public:
    using CBaseDialog::CBaseDialog;
    virtual void OnSettingsApplied() {}
};
struct ButtonBoundary { void SetCheck(bool) {} };
class CGeneralSettingsDlg : public CTabDlg {
public:
    explicit CGeneralSettingsDlg(CWnd* parent): CTabDlg(IDD_GENERAL_SETTINGS_DIALOG, parent) {}
    GeneralSettingData m_data{};
    int m_notify_icon_selected{}, m_monitor_time_span_ori{}, m_update_source_ori{};
    bool m_auto_run_modified{}, m_show_all_interface_modified{};
    // Validated UI draft supplied by the fixture. Does NOT alter any setting,
    // original-value baseline, Apply routing, or Cancel/restore behavior.
    void CollectSettings() { RequireWindow(); }
    static void CheckTaskbarDisplayItem();
    void OnSettingsApplied() override;
    void RestoreUpdateSource();
    void OnCancel() override;
    void OnBnClickedGithubRadio();
    bool IsMonitorTimeSpanModified() const;
'''
    for signature in ("bool IsAutoRunModified() const", "bool IsShowAllInterfaceModified() const"):
        code += take("GeneralSettingsDlg.h", signature)
    code += r'''
};
class CTaskBarSettingsDlg : public CTabDlg {
public:
    explicit CTaskBarSettingsDlg(CWnd* parent): CTabDlg(IDD_TASKBAR_SETTINGS_DIALOG, parent) {}
    TaskBarSettingData m_data{};
    map<int, TaskBarSettingData> m_pending_presets;
    bool m_style_modified{};
    ButtonBoundary m_background_transparent_chk, m_auto_set_back_color_chk;
    bool CollectSettings() { RequireWindow(); return true; } // UI boundary, not validation coverage.
    void DrawStaticColor() { RequireWindow(); }
    void RefreshThemeControls() { RequireWindow(); }
    bool IsStyleModified();
    void ModifyDefaultStyle(int);
    void SaveColorSettingToDefaultStyle();
    void OnSettingsApplied() override;
};
class COptionsDlg : public CBaseDialog {
public:
    COptionsDlg(int tab=0, CWnd* parent=nullptr);
    CTaskBarSettingsDlg m_tab2_dlg{this};
    CGeneralSettingsDlg m_tab3_dlg{this};
    int m_notify_icon_selected{}, m_tab_selected{};
    bool m_ready{}, m_tab3_created{};
    CString GetDialogName() const override;
    bool CollectSettings();
    void SelectPage(int) { Unexpected("invalid-control navigation outside supplied draft"); }
    void OnCancel() override;
    void OnBnClickedApplyButton();
};
struct PersistedCommit {
    TaskBarSettingData taskbar;
    GeneralSettingData general;
    bool inside_send{}, options_alive{}, options_object_alive{}, general_alive{};
};
struct PersistenceBoundary {
    int style_saves{}, global_saves{};
    vector<PersistedCommit> commits;
} persistence;
struct StyleBoundary {
    void ModifyDefaultStyle(int, const TaskBarSettingData&) { Unexpected("preset changes excluded"); }
    void SaveConfig() { ++persistence.style_saves; events.push_back("save-style"); }
};
struct WindowsVersionBoundary {
    bool IsWine() const { return false; }
    bool IsWindows8Or8point1() const { return false; }
};
struct CWindowsSettingHelper { static bool IsWindows10LightTheme() { return false; } };
class CPulseBarApp {
public:
    TaskBarSettingData m_taskbar_data{};
    GeneralSettingData m_general_data{};
    MainConfigData m_cfg_data{};
    CWnd* m_pMainWnd{};
    StyleBoundary m_taskbar_default_style;
    WindowsVersionBoundary m_win_version;
    MonitorSettingsStore m_monitor_settings;
    wstring m_log_path, m_module_path;
    struct Plugins {
        int GetItemIndex(IPluginItem*) const { Unexpected("plugins not loaded"); }
    } m_plugins;
    static int NormalizeNotifyIconSelection(int);
    bool IsForceShowNotifyIcon();
    void PublishMonitorSettings();
    bool IsWindows11Taskbar() const { return true; }
    void CheckWindows11Taskbar() {} // OS detection boundary only.
    void SendSettingsToPlugin() { events.push_back("plugin-delivery"); }
    bool SetAutoRun(bool, bool) { Unexpected("autostart/registry excluded"); }
    void SaveConfig() {
        auto* options = active_options ? dynamic_cast<COptionsDlg*>(active_options->object) : nullptr;
        persistence.commits.push_back({m_taskbar_data, m_general_data, in_apply_send,
            active_options && active_options->alive, options != nullptr,
            options && IsWindow(options->m_tab3_dlg.GetSafeHwnd())});
        events.push_back("save-config");
    }
    void SaveGlobalConfig() { ++persistence.global_saves; events.push_back("save-global"); }
    //配置目录迁移会真的搬文件，夹具里不执行：返回“未迁移、无提示”，只验证调用顺序
    bool MigrateConfigDir(bool, CString& message) { message.clear(); return false; }
    wstring m_history_traffic_path;
};
CPulseBarApp theApp;
namespace DrawCommonHelper { enum class RenderType { GDI, D2D1_WITH_DCOMPOSITION }; }
struct CSupportedRenderEnums {
    DrawCommonHelper::RenderType GetAutoFitEnum() const { return DrawCommonHelper::RenderType::GDI; }
};
struct CSpeedGraphTip {
    static CSpeedGraphTip& Instance() { static CSpeedGraphTip instance; return instance; }
    void ForceHide() {} // Other-window rendering boundary, no Apply/owner logic.
};
class CTaskBarDlg : public CDialog {
public:
    CTaskBarDlg(): CDialog(IDD_TASK_BAR_DIALOG) {}
    bool m_graph_tip_showing{}, m_is_width_changed{};
    bool NeedsWindowRecreation() const { return false; } // Backend capability probe is an external boundary.
    bool IsReadyToShow() const { return IsWindow(GetSafeHwnd()); }
    void ApplySettings() { RequireWindow(); } // Rendering/layout boundary only.
    void ApplyWindowTransparentColor() { RequireWindow(); }
    static void DisableRenderFeatureIfNecessary(CSupportedRenderEnums&) {}
    void ResetTaskbarPos() {} // Explorer positioning excluded.
    void OnCancel() override;
    void WidthChanged();
};
struct CWineTaskbarDlg : CTaskBarDlg {};
struct CWin11TaskbarDlg : CTaskBarDlg {};
struct CClassicalTaskbarDlg : CTaskBarDlg {};
class CPulseBarDlg : public CWnd {
public:
    CTaskBarDlg* m_tBarDlg{};
    //迁移后要同步历史文件对象里的路径；夹具只记录调用，不碰真实历史
    struct HistoryTrafficBoundary { void SetFilePath(const wstring&) {} } m_history_traffic;
    int m_taskbar_open_failures{};      //面板创建失败计数（自愈限次用）
    void OnOptions2();
    void _OnOptions(int, CWnd*);
    void ApplySettings(COptionsDlg&);
    LRESULT OnSettingsApplied(WPARAM, LPARAM);
    bool IsTaskbarWndValid() const;
    void CloseTaskBarWnd();
    void OpenTaskBarWnd();
    void UpdateNotifyIconTip(bool) { Unexpected("tray-style changes excluded"); }
    void AddNotifyIcon() { Unexpected("tray creation excluded"); }
    void DeleteNotifyIcon() { Unexpected("tray deletion excluded"); }
    void IniConnection() { Unexpected("network interface enumeration excluded"); }
    void KillTimer(UINT timer) {
        if (timer != MONITOR_TIMER) Unexpected("unexpected timer identity");
        events.push_back("kill-monitor-timer");
    }
    void SetTimer(UINT timer, int interval, void* callback) {
        if (timer != MONITOR_TIMER || interval != 1000 || callback != nullptr)
            Unexpected("unexpected monitor timer parameters");
        events.push_back("set-monitor-timer");
    }
};

// Explicit synchronous message transport. Production chooses the destination,
// payload and whether to send anything; this is not an MFC message-map test.
LRESULT SendMessage(HWND target, UINT msg, WPARAM wp, LPARAM lp) {
    if (!IsWindow(target) || !target->object) Unexpected("message to invalid fixture window");
    if (msg == WM_SETTINGS_APPLIED) {
        if (!theApp.m_pMainWnd || target != theApp.m_pMainWnd->GetSafeHwnd() ||
            !active_options || wp != reinterpret_cast<WPARAM>(active_options->object) || lp != 0)
            Unexpected("wrong production Apply route/payload");
        ++apply_messages; in_apply_send=true; events.push_back("send-apply");
        const auto result = static_cast<CPulseBarDlg*>(target->object)->OnSettingsApplied(wp, lp);
        events.push_back("return-apply"); in_apply_send=false;
        return result;
    }
    if (msg == WM_COMMAND && wp == IDCANCEL && lp == 0) {
        ++cancel_messages;
        static_cast<CDialog*>(target->object)->OnCancel();
        return 0;
    }
    Unexpected("unsupported fixture message");
}
// Windows event parameters and the compiled-out hardware flag are unused in
// production Lite methods; suppress only those warnings in extracted bodies.
#pragma warning(push)
#pragma warning(disable: 4100 4189)
'''
    groups = {
        "DisplayItem.cpp": ("CommonDisplayItem::CommonDisplayItem(DisplayItem item)",
                            "bool CommonDisplayItem::operator<(", "bool CommonDisplayItem::operator==(",
                            "DisplayItemSet::DisplayItemSet(", "void DisplayItemSet::Remove("),
        "TaskbarItemOrderHelper.cpp": ("CTaskbarItemOrderHelper::CTaskbarItemOrderHelper(",),
        "CommonData.cpp": ("std::set<std::wstring>& StringSet::data()", "bool TaskBarSettingData::IsTaskbarTransparent() const",
                           "void TaskBarSettingData::SetTaskabrTransparent("),
        "Common.cpp": ("void CCommon::TransparentColorConvert(",),
        "BaseDialog.cpp": ("HWND CBaseDialog::GetUniqueHandel(", "const std::map<CString, HWND>& CBaseDialog::AllUniqueHandels()",
                           "INT_PTR CBaseDialog::DoModal()"),
        "OptionsDlg.cpp": ("COptionsDlg::COptionsDlg(", "CString COptionsDlg::GetDialogName() const",
                           "bool COptionsDlg::CollectSettings()", "void COptionsDlg::OnBnClickedApplyButton()",
                           "void COptionsDlg::OnCancel()"),
        "GeneralSettingsDlg.cpp": ("void CGeneralSettingsDlg::CheckTaskbarDisplayItem()",
                                   "void CGeneralSettingsDlg::OnSettingsApplied()", "void CGeneralSettingsDlg::OnCancel()",
                                   "void CGeneralSettingsDlg::RestoreUpdateSource()",
                                   "void CGeneralSettingsDlg::OnBnClickedGithubRadio()",
                                   "bool CGeneralSettingsDlg::IsMonitorTimeSpanModified() const"),
        "TaskBarSettingsDlg.cpp": ("bool CTaskBarSettingsDlg::IsStyleModified()", "void CTaskBarSettingsDlg::ModifyDefaultStyle(",
                                   "void CTaskBarSettingsDlg::SaveColorSettingToDefaultStyle()",
                                   "void CTaskBarSettingsDlg::OnSettingsApplied()"),
        "TaskBarDlg.cpp": ("void CTaskBarDlg::OnCancel()", "void CTaskBarDlg::WidthChanged()"),
        "PulseBar.cpp": ("int CPulseBarApp::NormalizeNotifyIconSelection(", "bool CPulseBarApp::IsForceShowNotifyIcon()",
                         "void CPulseBarApp::PublishMonitorSettings()"),
        "PulseBarDlg.cpp": ("bool CPulseBarDlg::IsTaskbarWndValid() const", "void CPulseBarDlg::OnOptions2()",
                            "void CPulseBarDlg::_OnOptions(", "void CPulseBarDlg::ApplySettings(",
                            "afx_msg LRESULT CPulseBarDlg::OnSettingsApplied(",
                            "void CPulseBarDlg::CloseTaskBarWnd()", "void CPulseBarDlg::OpenTaskBarWnd()"),
    }
    for name, signatures in groups.items():
        for signature in signatures:
            code += take(name, signature)
    code += r'''
#pragma warning(pop)
int checks{}, failures{};
string mode;
bool Check(bool condition, const string& label) {
    ++checks;
    if (!condition) ++failures;
    printf("%s %s.%s\n", condition ? "PASS" : "FAIL", mode.c_str(), label.c_str());
    return condition;
}
size_t Event(const char* name) {
    return static_cast<size_t>(find(events.begin(), events.end(), name)-events.begin());
}
bool Live(HWND h) { return h && h->alive && h->object; }

void Scenario(COptionsDlg& options, CPulseBarDlg& host, HWND old_taskbar) {
    auto& page = options.m_tab3_dlg;
    if (mode=="owner") {
        Check(GetParent(options.GetSafeHwnd())==host.GetSafeHwnd(), "stable-owner-reaches-modal-constructor");
        options.OnCancel();
        Check(persistence.commits.empty(), "owner-observation-does-not-apply");
        return;
    }
    if (mode=="cancel_only") {
        page.OnBnClickedGithubRadio();
        Check(theApp.m_general_data.update_source==0 && page.m_data.update_source==0, "real-radio-previews-source");
        options.OnCancel();
        Check(theApp.m_general_data.update_source==1, "preview-restored");
        Check(persistence.commits.empty() && apply_messages==0, "cancel-does-not-submit");
        return;
    }
    if (mode=="general_uncreated") {
        options.OnBnClickedApplyButton();
        Check(persistence.commits.size()==1 && persistence.commits[0].general.update_source==1,
              "uncreated-general-draft-commits-without-controls");
        Check(page.m_update_source_ori==0, "uncreated-page-has-no-applied-baseline");
        options.OnCancel();
        Check(theApp.m_general_data.update_source==1, "uncreated-page-cancel-does-not-restore-default-baseline");
        return;
    }
    //只剩 GitHub 一个更新源：直接构造"已改动"的草稿，预览语义与真实单选按钮一致
    page.m_data.update_source=1;
    theApp.m_general_data.update_source=1;
    Check(theApp.m_general_data.update_source==1 && page.m_data.update_source==1, "real-radio-previews-source");
    Check(page.m_update_source_ori==0, "preview-does-not-change-baseline");
    if (mode=="unready" || mode=="missing_host") {
        if (mode=="unready") options.m_ready=false;
        else theApp.m_pMainWnd=nullptr;
        options.OnBnClickedApplyButton();
        Check(persistence.commits.empty() && apply_messages==0 && page.m_update_source_ori==0,
              "blocked-apply-does-not-submit-or-advance-baseline");
        options.OnCancel();
        Check(theApp.m_general_data.update_source==0, "blocked-apply-remains-cancellable");
        theApp.m_pMainWnd=&host;
        return;
    }
    const bool recreate = mode=="recreate" || mode=="recreate-fails";
    if (recreate) options.m_tab2_dlg.m_data.tb_theme=1; // A production rebuild predicate, not a fake callback.
    fail_taskbar_create = mode=="recreate-fails";
    options.OnBnClickedApplyButton();
    Check(Live(options.token) && Live(page.token) && Live(options.m_tab2_dlg.token), "options-live-on-apply-return");
    Check(options.token->cancels==0 && page.token->cancels==0 && cancel_messages==0 && options.modal_result==0,
          "apply-does-not-cancel-options");
    Check(apply_messages==1 && persistence.commits.size()==1 && persistence.global_saves==1,
          "exactly-one-synchronous-submit");
    if (!persistence.commits.empty()) {
        const auto& commit = persistence.commits.front();
        Check(commit.inside_send && commit.options_alive && commit.options_object_alive && commit.general_alive,
              "options-live-through-real-host-commit");
        Check(commit.general.update_source==1 && theApp.m_general_data.update_source==1,
              "commit-keeps-preview-source");
        Check(commit.taskbar.tb_theme==(recreate ? 1 : 0), "real-host-consumes-taskbar-draft");
    }
    Check(Event("send-apply") < Event("save-config") && Event("save-config") < Event("save-global") &&
          Event("save-global") < Event("return-apply"), "host-finishes-before-send-returns");
    Check(page.m_update_source_ori==1, "real-onsettingsapplied-advances-baseline");
    Check(theApp.m_monitor_settings.Read()->version==1, "real-host-publishes-one-commit");
    if (mode=="recreate-fails") {
        Check(host.m_tBarDlg==nullptr && !host.IsTaskbarWndValid() && !old_taskbar->alive && !old_taskbar->object,
              "failed-taskbar-create-is-cleaned-without-dereferencing-null");
    } else if (recreate) {
        Check(!old_taskbar->alive && !old_taskbar->object && taskbar_deletes==1 && taskbar_creates==2 &&
              host.IsTaskbarWndValid() && host.m_tBarDlg->GetSafeHwnd()!=old_taskbar,
              "real-host-replaces-taskbar-during-apply");
    } else {
        Check(Live(old_taskbar) && host.m_tBarDlg->GetSafeHwnd()==old_taskbar &&
              taskbar_deletes==0 && taskbar_creates==1 && host.m_tBarDlg->m_is_width_changed,
              "unchanged-settings-use-real-width-refresh-not-rebuild");
    }
    // A failed owner mutant has already closed the dialog. Do not pretend a
    // user can keep interacting with dead controls; observations above kill it.
    if (!Live(options.token)) return;
    page.OnBnClickedGithubRadio();
    Check(theApp.m_general_data.update_source==0 && page.m_update_source_ori==1, "later-preview-is-uncommitted");
    if (mode=="general_cancel") page.OnCancel();
    else options.OnCancel();
    Check(theApp.m_general_data.update_source==1, "cancel-keeps-applied-source");
    Check(persistence.commits.size()==1 && persistence.commits[0].general.update_source==1 &&
          persistence.global_saves==1 && apply_messages==1, "cancel-neither-reapplies-nor-rewrites-commit");
    if (mode=="general_cancel") {
        Check(page.token->cancels==1 && options.token->cancels==0, "real-general-cancel-reaches-only-page-boundary");
        options.OnCancel();
        Check(theApp.m_general_data.update_source==1, "parent-cancel-skips-already-closed-page");
    }
}
int wmain(int argc, wchar_t** argv) {
    if (argc!=3) return 2;
    const wstring selected=argv[1];
    mode.assign(selected.begin(), selected.end());
    try {
        CPulseBarDlg host;
        host.token->alive=true;
        theApp.m_pMainWnd=&host;
        theApp.m_general_data.update_source=(mode=="cancel_only" || mode=="general_uncreated") ? 1 : 0;
        host.OpenTaskBarWnd(); // Real routing; Create/Show and platform detection are boundaries.
        HWND old_taskbar=host.m_tBarDlg->GetSafeHwnd();
        HWND options_token=nullptr;
        if (mode=="null_payload") {
            Check(host.OnSettingsApplied(0,0)==0 && persistence.commits.empty() && taskbar_deletes==0,
                  "null-message-payload-does-not-submit");
        } else {
            CDialog::modal_driver = [&](CDialog& shell) -> INT_PTR {
                auto& options=static_cast<COptionsDlg&>(shell);
                active_options=options.token; options_token=options.token;
                // Explicit post-OnInit fixture; baseline INPUT is the opening
                // source, never advanced here after Apply. No real window opens.
                options.token->alive=true; options.m_ready=true;
                options.m_tab2_dlg.token->alive=true;
                options.m_tab3_created=mode!="general_uncreated";
                options.m_tab3_dlg.token->alive=options.m_tab3_created;
                if (options.m_tab3_created) {
                    options.m_tab3_dlg.m_update_source_ori=theApp.m_general_data.update_source;
                    options.m_tab3_dlg.m_monitor_time_span_ori=1000;
                }
                CBaseDialog::m_unique_hwnd[OPTION_DLG_NAME]=options.GetSafeHwnd();
                Scenario(options, host, old_taskbar);
                Check(options.modal_result==IDCANCEL, "modal-exits-only-after-cancel");
                Check(options.token->object==&options, "cpp-dialog-lives-until-modal-unwinds");
                CBaseDialog::m_unique_hwnd[OPTION_DLG_NAME]=nullptr;
                active_options=nullptr;
                return options.modal_result;
            };
            host.OnOptions2(); // Real OnOptions2 -> _OnOptions -> constructor -> modal driver.
            Check(options_token && !options_token->alive && !options_token->object,
                  "stack-dialog-retires-after-real-onoptions-returns");
            Check(persistence.commits.size()<=1, "cancelled-modal-does-not-trigger-second-host-apply");
        }
        Check(stale_accesses==0, "no-invalid-window-access-at-observed-boundaries");
        printf("OBSERVED %s apply_messages=%d cancel_messages=%d commits=%zu source=%d creates=%d deletes=%d stale=%d\n",
               mode.c_str(), apply_messages, cancel_messages, persistence.commits.size(),
               theApp.m_general_data.update_source, taskbar_creates, taskbar_deletes, stale_accesses);
        host.CloseTaskBarWnd(); // Own fixture cleanup, still real host close; never Explorer.
        theApp.m_pMainWnd=nullptr;
        printf("CHECKS %d PASS %d FAIL %d\n", checks, checks-failures, failures);
        return failures ? 1 : 0;
    } catch (const exception& error) {
        printf("ERROR %s\n", error.what());
        return 2;
    }
}
'''
    if mutation != "current" and len(mutations) != 1:
        raise ValueError(f"Expected exactly one in-memory mutation, got {len(mutations)}")
    provenance = {
        "variant": mutation, "source_kind": "current production read once per Python invocation",
        "hash_encoding": "SHA256 of decoded/re-encoded UTF-8 source text",
        "source_files": {str(native.SOURCE / name): hashlib.sha256(value.encode("utf-8")).hexdigest()
                         for name, value in sources.items()},
        "extracted": extracted, "test_build_only_mutations": mutations,
        "scope": __doc__,
    }
    return code, provenance


def run_variant(sources, variant, modes):
    code, provenance = make_code(sources, variant)
    run, env = native.build(code, "options-apply-" + variant)
    (run / "current-source-provenance.json").write_text(
        json.dumps(provenance, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"BUILD PASS {variant}; ARTIFACTS {run}", flush=True)
    results = {}
    logs = {}
    for mode in modes:
        results[mode] = native.execute(run, env, mode)
        logs[mode] = (run / f"{mode}.log").read_text(encoding="utf-8")
    summary = f"RESULT {variant}: {sum(results.values())}/{len(results)} cases passed"
    full_log = [__doc__, "\nBUILD PASS\n", (run / "build.log").read_text(encoding="utf-8")]
    full_log.extend(f"\n== {mode} ==\n{logs[mode]}" for mode in modes)
    full_log.append("\n" + summary + "\n")
    (run / "options-apply.log").write_text("".join(full_log), encoding="utf-8")
    print(summary)
    print(f"LOG {run / 'options-apply.log'}", flush=True)
    return run, results, logs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mutation", choices=("current", *MUTANTS), default="current")
    parser.add_argument("--verify-mutations", action="store_true")
    parser.add_argument("modes", nargs="*", metavar="MODE")
    args = parser.parse_args()
    if any(mode not in MODES for mode in args.modes):
        parser.error("unknown mode; choose from " + ", ".join(MODES))
    if args.verify_mutations and (args.mutation != "current" or args.modes):
        parser.error("--verify-mutations runs its fixed RED checks and the complete current suite")
    sources = {}
    evidence = []
    if args.verify_mutations:
        # RED first. Only a normal assertion failure on the named behavior kills
        # a mutant: compile errors, exceptions, timeout and crash are NOT evidence.
        for variant, (mode, marker) in MUTANTS.items():
            run, results, logs = run_variant(sources, variant, (mode,))
            killed = (not results[mode] and marker in logs[mode] and
                      logs[mode].endswith("EXIT 1\n") and "ERROR " not in logs[mode])
            evidence.append({"variant": variant, "killed": killed, "marker": marker, "run": str(run)})
            print(f"MUTATION {variant} {'KILLED' if killed else 'NOT PROVEN'}", flush=True)
    run, results, _ = run_variant(sources, args.mutation, tuple(args.modes) or MODES)
    if args.verify_mutations:
        (run / "mutation-evidence.json").write_text(
            json.dumps(evidence, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(f"MUTATIONS {sum(row['killed'] for row in evidence)}/{len(evidence)} killed")
    return 0 if all(results.values()) and all(row["killed"] for row in evidence) else 1


if __name__ == "__main__":
    raise SystemExit(main())
