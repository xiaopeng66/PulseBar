"""F11-F15 native RED regressions, reading CURRENT production methods on every run.

Run with: python -X utf8 -I -B <absolute path to this file>
Only one native fixture is built per invocation, under native.RUN_ROOT. No real
Windows/MFC headers or APIs are linked: handles are pointers to in-memory window
state; dialog results, font/text measurement, rendering and SaveConfig are the
explicit boundaries. This does not launch PulseBar, open windows, touch registry,
configure autostart, or read user settings.

F11 executes OnOptions2 and observes the _OnOptions owner argument, NOT a copied
Apply/recreation model. F12 executes the ENTIRE CalculateWindowSize twice and the
real CheckClickedItem, including real item comparison/visibility/order helpers.
F13 keeps the entire host OnDisplaySettings result consumer. F14 keeps the real
ApplySettings, Hide, ForceHide and HideSpeedGraphTip. F15 extracts the CPU/GPU
build_pts lambda plus its usable-height declaration; rasterization is out of scope.
"""
from pathlib import Path
import hashlib
import importlib.util
import json
import re
import sys

SUPPORT = Path(__file__).resolve().with_name("native_test_support.py")
spec = importlib.util.spec_from_file_location("ui_native_support", SUPPORT)
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)


def make_code():
    sources = {}
    extracted = []

    def text(name):
        if name not in sources:
            sources[name] = native.source(name)
        return sources[name]

    def take(name, signature):
        fragment = native.method(text(name), signature)
        extracted.append({"file": str(native.SOURCE / name), "signature": signature,
                          "sha256": hashlib.sha256(fragment.encode("utf-8")).hexdigest()})
        return "\n// CURRENT " + name + " :: " + signature + "\n" + fragment + "\n"

    code = r'''#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
using namespace std;
using BOOL = int;
using UINT = unsigned int;
using DWORD = unsigned long;
using COLORREF = unsigned long;
using INT_PTR = intptr_t;
using LRESULT = intptr_t;
using WPARAM = uintptr_t;
using LPARAM = intptr_t;
#define CALLBACK
constexpr BOOL TRUE = 1, FALSE = 0;
constexpr int IDOK = 1, IDCANCEL = 2, SW_HIDE = 0;
#define RGB(r,g,b) (COLORREF((r) | ((g) << 8) | ((b) << 16)))

// Pointer handles, never integer stand-ins and never OS window handles.
struct WindowState { bool alive{true}, visible{true}; int hide_calls{}; };
using HWND = WindowState*;
static_assert(is_pointer_v<HWND>);
BOOL IsWindow(HWND h) { return h != nullptr && h->alive; }
BOOL IsWindowVisible(HWND h) { return IsWindow(h) && h->visible; }
BOOL ShowWindow(HWND h, int command) {
    if (!IsWindow(h) || command != SW_HIDE)
        throw logic_error("fixture attempted unsupported window operation");
    const bool was_visible = h->visible;
    h->visible = false;
    ++h->hide_calls;
    return was_visible;
}
struct CWnd {
    WindowState window;
    CWnd* parent{};
    explicit CWnd(CWnd* owner = nullptr): parent(owner) {}
    HWND GetSafeHwnd() const { return const_cast<WindowState*>(&window); }
    CWnd* GetParent() const { return parent; }
};
struct CPoint { int x{}, y{}; };
struct CRect {
    int left{}, top{}, right{}, bottom{};
    CRect() = default;
    CRect(int l, int t, int r, int b): left(l), top(t), right(r), bottom(b) {}
    int Width() const { return right - left; }
    int Height() const { return bottom - top; }
    bool PtInRect(CPoint p) const {
        return p.x >= left && p.x < right && p.y >= top && p.y < bottom;
    }
    void SetRect(int l, int t, int r, int b) { *this = CRect(l,t,r,b); }
    void MoveToX(int x) { right = x + Width(); left = x; }
};
struct CString : wstring {
    using wstring::wstring;
    CString(const wstring& value): wstring(value) {}
    const wchar_t* GetString() const { return c_str(); }
    operator const wchar_t*() const { return c_str(); }
};
struct CFont {
    struct Token {} token;
    const void* GetSafeHandle() const { return &token; }
};
struct CSize { int cx{}, cy{}; };
struct CDC {
    int extent_calls{};
    CFont* SelectObject(CFont* font) { return font; }
    CSize GetTextExtent(const wchar_t* value) {
        ++extent_calls;
        return {static_cast<int>(wcslen(value)) * 8, 12};
    }
};
struct CToolTipCtrl : CWnd {
    int pop_calls{};
    void Pop() { ++pop_calls; window.visible = false; }
};
struct FontInfo {}; // Font creation is a UI boundary, not exercised here.
struct CSupportedRenderEnums {};
struct IPluginItem {
    const wchar_t* GetItemId() const { return L"fixture-plugin"; }
    const wchar_t* GetItemName() const { return L"Fixture plugin"; }
    const wchar_t* GetItemValueSampleText() const { return L"v"; }
    bool IsCustomDraw() const { return false; }
    int IsDoubleLineExclusive() const { return 0; }
};
struct ITMPlugin { int GetAPIVersion() const { return 8; } };
class CCommon {
public:
    template<class T> static void RemoveVectorDuplicateItem(vector<T>& values) {
        vector<T> unique;
        for (const T& value : values)
            if (find(unique.begin(), unique.end(), value) == unique.end())
                unique.push_back(value);
        values = unique;
    }
};
namespace Gdiplus { struct Point { int X{}, Y{}; }; }
'''
    # Real value/container declarations: comparisons and display filtering must
    # not be replaced by a fixture that conveniently favors the new plugin.
    code += native.no_includes(text("DisplayItem.h")) + "\n"
    for signature in ("enum class SpeedUnit", "enum HardwareItem", "struct DispStrings",
                      "enum class DoubleClickAction", "enum class MemoryDisplay",
                      "struct StringSet", "struct TaskbarItemColor"):
        code += take("CommonData.h", signature) + ";\n"
    code += take("TaskbarItemOrderHelper.h", "class CTaskbarItemOrderHelper") + ";\n"
    code += take("CommonData.h", "struct PublicSettingData") + ";\n"
    code += take("CommonData.h", "struct TaskBarSettingData") + ";\n"
    # Only test access changes; the graph state fields and inline accessors are
    # current production declarations, and all state transitions below are real.
    code += "#define private public\n"
    code += take("SpeedGraphTip.h", "class CSpeedGraphTip") + ";\n"
    code += "#undef private\n"
    code += r'''
struct FixturePlugins {
    vector<CommonDisplayItem> items;
    ITMPlugin api;
    const auto& AllDisplayItemsWithPlugins() const { return items; }
    int GetItemIndex(IPluginItem* plugin) const {
        for (size_t i=0; i<items.size(); ++i)
            if (items[i].IsPlugin() && items[i].PluginItem() == plugin)
                return static_cast<int>(i);
        return -1;
    }
    int GetItemWidth(IPluginItem*, CDC*) const { return 16; }
    ITMPlugin* GetPluginByItem(IPluginItem* item) { return item ? &api : nullptr; }
};
class CPulseBarApp {
public:
    TaskBarSettingData m_taskbar_data{}, saved{};
    FixturePlugins m_plugins;
    struct General { bool IsHardwareEnable(HardwareItem) const { return true; } } m_general_data;
    CWnd* m_pMainWnd{};
    int saves{};
    // Persistence boundary: record exactly what the real host commits.
    void SaveConfig() { ++saves; saved = m_taskbar_data; }
    bool IsTaksbarItemDisplayed(CommonDisplayItem item) const;
};
CPulseBarApp theApp;
class CTaskBarDlg : public CWnd {
public:
    using CWnd::CWnd;
'''
    code += take("TaskBarDlg.h", "struct ItemWidth") + ";\n"
    code += take("TaskBarDlg.h", "struct ItemWidthInfo") + ";\n"
    code += r'''
    vector<ItemWidthInfo> m_item_widths;
    map<CommonDisplayItem, CRect> m_item_rects;
    CommonDisplayItem m_clicked_item;
    CRect m_rect, m_rcTaskbar{0,0,800,32};
    int m_window_width{}, m_window_height{};
    bool m_taskbar_on_top_or_bottom{true}, m_graph_tip_showing{}, m_is_width_changed{};
    UINT m_taskbar_dpi{96};
    size_t m_last_layout_signature{};
    CFont m_font;
    CDC dc;
    CDC* m_pDC{&dc};
    CToolTipCtrl m_tool_tips;
    CSupportedRenderEnums m_supported_render_enums;
    int font_requests{};
    int adjust_calls{}, invalidate_calls{}, update_window_calls{};
    void SetTextFont() { ++font_requests; } // No real fonts/DCs/rendering.
    bool AdjustWindowPos(bool) { ++adjust_calls; return true; }
    void Invalidate(BOOL) { ++invalidate_calls; }
    void UpdateWindow() { ++update_window_calls; }
    static void DisableRenderFeatureIfNecessary(CSupportedRenderEnums&) {}
    void ApplySettings();
    void CalculateWindowSize();
    bool CheckClickedItem(CPoint point);
    void WidthChanged();
    int DPI(int pixel) const;
'''
    code += take("TaskBarDlg.h", "bool IsTasksbarOnTopOrBottom() const")
    code += take("TaskBarDlg.h", "CommonDisplayItem GetClickedItem() const")
    code += take("TaskBarDlg.h", "void HideSpeedGraphTip()")
    code += "};\n"
    code += r'''
class CPulseBarDlg : public CWnd {
public:
    CTaskBarDlg* m_tBarDlg{};
    CWnd* options_owner{};
    int options_tab{-1}, options_calls{};
    bool IsTaskbarWndValid() const;
    void OnOptions2();
    void OnDisplaySettings();
    // A sink for the actual call argument, NOT an Apply/cancellation model.
    void _OnOptions(int tab, CWnd* owner) {
        ++options_calls; options_tab = tab; options_owner = owner;
    }
};
class CSetItemOrderDlg {
public:
    inline static function<void(CSetItemOrderDlg&)> on_modal;
    inline static int modal_result{IDOK};
    CWnd* owner;
    vector<int> order;
    DisplayItemSet displayed;
    StringSet plugins;
    DispStrings texts;
    map<CommonDisplayItem, TaskbarItemColor> colors;
    bool colors_modified{};
    explicit CSetItemOrderDlg(CWnd* parent): owner(parent) {}
    void SetAppearance(const DispStrings& value, const map<CommonDisplayItem, TaskbarItemColor>& c) {
        texts = value; colors = c;
    }
    void SetItemOrder(const vector<int>& value) { order = value; }
    const vector<int>& GetItemOrder() const { return order; }
    void SetDisplayItem(const DisplayItemSet& value) { displayed = value; }
    DisplayItemSet GetDisplayItem() const { return displayed; }
    void SetPluginDisplayItem(const StringSet& value) { plugins = value; }
    const StringSet& GetPluginDisplayItem() const { return plugins; }
    const DispStrings& GetDisplayTexts() const { return texts; }
    const map<CommonDisplayItem, TaskbarItemColor>& GetColors() const { return colors; }
    bool ColorsModified() const { return colors_modified; }
    int DoModal() { if (on_modal) on_modal(*this); return modal_result; }
};
// Only localized names/sample text are substituted; identity and ordering below
// remain production methods. The DC supplies deterministic character metrics.
CString CommonDisplayItem::GetItemName() const {
    return IsPlugin() ? CString(PluginItem()->GetItemName()) : CString(to_wstring(ItemType()));
}
CString CommonDisplayItem::GetItemValueSampleText(bool) const { return L"v"; }
'''
    for signature in (
            "CommonDisplayItem::CommonDisplayItem(DisplayItem item)",
            "CommonDisplayItem::CommonDisplayItem(IPluginItem* item)",
            "bool CommonDisplayItem::operator<(", "bool CommonDisplayItem::operator==(",
            "bool CommonDisplayItem::IsPlugin() const", "DisplayItem CommonDisplayItem::ItemType() const",
            "IPluginItem* CommonDisplayItem::PluginItem() const", "bool CommonDisplayItem::IsDoubleLineExclusive() const",
            "DisplayItemSet::DisplayItemSet(", "void DisplayItemSet::Add(", "void DisplayItemSet::Remove(",
            "bool DisplayItemSet::Contains(", "int DisplayItemSet::ToInt() const", "bool DisplayItemSet::IsEmpty() const"):
        code += take("DisplayItem.cpp", signature)
    for signature in (
            "wstring& DispStrings::Get(", "const wstring& DispStrings::GetConst(",
            "const std::map<CommonDisplayItem, wstring>& DispStrings::GetAllItems() const",
            "bool DispStrings::operator==(", "bool StringSet::Contains(",
            "void StringSet::SetStrContained(", "std::wstring StringSet::ToString() const",
            "std::set<std::wstring>& StringSet::data()"):
        code += take("CommonData.cpp", signature)
    for signature in (
            "CTaskbarItemOrderHelper::CTaskbarItemOrderHelper(", "void CTaskbarItemOrderHelper::Init()",
            "std::vector<CommonDisplayItem> CTaskbarItemOrderHelper::GetAllDisplayItemsWithOrder() const",
            "std::wstring CTaskbarItemOrderHelper::ToString() const", "void CTaskbarItemOrderHelper::SetOrder(",
            "const vector<int>& CTaskbarItemOrderHelper::GetItemOrderConst() const",
            "bool CTaskbarItemOrderHelper::IsItemDisplayed(", "void CTaskbarItemOrderHelper::NormalizeItemOrder()"):
        code += take("TaskbarItemOrderHelper.cpp", signature)
    code += take("PulseBar.cpp", "bool CPulseBarApp::IsTaksbarItemDisplayed(")
    for signature in ("CSpeedGraphTip::CSpeedGraphTip()", "CSpeedGraphTip& CSpeedGraphTip::Instance()",
                      "void CSpeedGraphTip::Hide()", "void CSpeedGraphTip::ForceHide()"):
        code += take("SpeedGraphTip.cpp", signature)
    code += "void CSpeedGraphTip::Refresh() {}\n"
    height_macro = re.search(r"^#define TASKBAR_WND_HEIGHT[^\n]*", text("TaskBarDlg.h"), re.MULTILINE)
    if height_macro is None:
        raise ValueError("Cannot locate current taskbar layout height declaration")
    code += height_macro.group() + "\n"
    for signature in ("void CTaskBarDlg::ApplySettings()", "void CTaskBarDlg::CalculateWindowSize()",
                      "bool CTaskBarDlg::CheckClickedItem(", "void CTaskBarDlg::WidthChanged()",
                      "int CTaskBarDlg::DPI(int pixel) const"):
        code += take("TaskBarDlg.cpp", signature)
    for signature in ("bool CPulseBarDlg::IsTaskbarWndValid() const", "void CPulseBarDlg::OnOptions2()",
                      "void CPulseBarDlg::OnDisplaySettings()"):
        code += take("PulseBarDlg.cpp", signature)

    # Take the actual current CPU/GPU geometry (not the network-speed lambda).
    draw = native.method(text("SpeedGraphTip.cpp"), "void CSpeedGraphTip::Draw(")
    cpu = draw[draw.index("// ---- CPU/GPU"):]
    usable = re.search(r"\b(?:const\s+)?int\s+usable_h\s*=[^;]+;", cpu)
    if usable is None:
        raise ValueError("Cannot locate current CPU/GPU usable-height declaration")
    points = native.method(cpu, "auto build_pts = [&](int* vals)") + ";"
    extracted.append({"file": str(native.SOURCE / "SpeedGraphTip.cpp"),
                      "signature": "Draw CPU/GPU usable_h + build_pts(int*)",
                      "sha256": hashlib.sha256((usable.group() + points).encode("utf-8")).hexdigest()})
    code += r'''
constexpr int SAMPLE_COUNT = CSpeedGraphTip::SAMPLE_COUNT;
array<Gdiplus::Point, SAMPLE_COUNT + 2> CpuGpuPoints(array<int, SAMPLE_COUNT> values, CRect curve) {
    Gdiplus::Point pts[SAMPLE_COUNT + 2]{};
'''
    code += usable.group() + "\n" + points + "\n"
    code += r'''
    build_pts(values.data());
    array<Gdiplus::Point, SAMPLE_COUNT + 2> result{};
    copy(begin(pts), end(pts), result.begin());
    return result;
}

// Fixtures set INPUTS only. No fixture clears m_item_rects, selects a hit,
// enables per-item color, or implements the production panel transitions.
struct Fixture {
    CPulseBarDlg host;
    CTaskBarDlg taskbar{&host};
    IPluginItem plugin;
    WindowState panel;
    Fixture() {
        theApp.m_taskbar_data = TaskBarSettingData{};
        theApp.saved = TaskBarSettingData{};
        theApp.saves = 0;
        theApp.m_plugins.items.clear();
        for (auto item : AllDisplayItems)
            theApp.m_plugins.items.emplace_back(item);
        theApp.m_plugins.items.emplace_back(&plugin);
        auto& data = theApp.m_taskbar_data;
        data.item_order.Init();
        data.item_order.SetOrder({});
        data.display_item = DisplayItemSet{TDI_UP};
        data.disp_str.Get(TDI_UP) = L"u";
        data.disp_str.Get(&plugin) = L"p";
        data.text_colors[TDI_UP] = {0x010203, 0x040506};
        host.m_tBarDlg = &taskbar;
        theApp.m_pMainWnd = &host;
        CSetItemOrderDlg::on_modal = {};
        CSetItemOrderDlg::modal_result = IDOK;
        CSpeedGraphTip::Instance().m_hWnd = &panel;
        CSpeedGraphTip::Instance().m_pinned = false;
        panel.visible = false;
    }
    ~Fixture() {
        CSpeedGraphTip::Instance().m_hWnd = nullptr;
        theApp.m_pMainWnd = nullptr;
    }
    void OnlyPlugin() {
        theApp.m_taskbar_data.display_item.Remove(TDI_UP);
        theApp.m_taskbar_data.plugin_display_item.SetStrContained(plugin.GetItemId(), true);
    }
    void PinPanel() {
        CSpeedGraphTip::Instance().m_pinned = true;
        taskbar.m_graph_tip_showing = true;
        panel.visible = true;
    }
};
bool Check(bool value, const char* label) {
    printf("%s %s\n", value ? "PASS" : "FAIL", label);
    return value;
}
bool SameRect(const CRect& a, const CRect& b) {
    return a.left==b.left && a.top==b.top && a.right==b.right && a.bottom==b.bottom;
}
void EditColors(CSetItemOrderDlg& dlg) {
    dlg.colors[TDI_UP] = {0x112233, 0x445566};
    dlg.texts.Get(TDI_UP) = L"changed upload";
    dlg.colors_modified = true;
}

bool F11Owner() {
    Fixture f;
    f.host.OnOptions2();
    bool ok = Check(f.host.options_calls == 1 && f.host.options_tab == 0,
                    "F11 current OnOptions2 reached _OnOptions exactly once");
    printf("OBSERVED F11 owner=%s expected=stable-host\n",
           f.host.options_owner == &f.host ? "stable-host" :
           f.host.options_owner == &f.taskbar ? "replaceable-taskbar" : "other");
    ok &= Check(f.host.options_owner == &f.host,
                "F11 taskbar entry must give _OnOptions the stable host owner");
    return ok;
}

bool F12LayoutHit() {
    Fixture f;
    f.taskbar.CalculateWindowSize();
    bool setup = Check(f.taskbar.CheckClickedItem({4,4}) && f.taskbar.GetClickedItem() == TDI_UP,
                       "F12 first REAL layout routes point to the visible builtin");
    if (!setup) return false;
    const CRect old_rect = f.taskbar.m_item_rects.at(TDI_UP);
    const size_t old_signature = f.taskbar.m_last_layout_signature;
    f.OnlyPlugin();
    f.taskbar.CalculateWindowSize();
    auto plugin_rect = f.taskbar.m_item_rects.find(&f.plugin);
    setup &= Check(f.taskbar.m_last_layout_signature != old_signature
                   && f.taskbar.m_item_widths.size() == 1
                   && f.taskbar.m_item_widths.front().PluginItem() == &f.plugin,
                   "F12 changed inputs caused REAL layout rebuild with only the new plugin");
    setup &= Check(plugin_rect != f.taskbar.m_item_rects.end()
                   && SameRect(plugin_rect->second, old_rect),
                   "F12 REAL new plugin rectangle occupies exactly the old builtin rectangle");
    if (!setup) return false;
    const bool hit = f.taskbar.CheckClickedItem({4,4});
    printf("OBSERVED F12 hit=%s rectangles=%zu old_hidden_present=%d\n",
           f.taskbar.GetClickedItem().IsPlugin() ? "new-plugin" : "old-builtin",
           f.taskbar.m_item_rects.size(), int(f.taskbar.m_item_rects.count(TDI_UP) != 0));
    bool ok = Check(f.taskbar.m_item_rects.count(TDI_UP) == 0,
                    "F12 rebuilt hit map must discard the now-hidden builtin");
    ok &= Check(hit && f.taskbar.GetClickedItem().IsPlugin()
                && f.taskbar.GetClickedItem().PluginItem() == &f.plugin,
                "F12 colliding point must route to the currently visible plugin");
    return ok;
}

bool F13Colors() {
    Fixture f;
    CSetItemOrderDlg::on_modal = EditColors;
    f.host.OnDisplaySettings();
    const auto& data = theApp.m_taskbar_data;
    bool ok = Check(data.text_colors.at(TDI_UP) == TaskbarItemColor{0x112233,0x445566}
                    && data.disp_str.GetConst(TDI_UP) == L"changed upload",
                    "F13 real host consumes edited dialog colors and texts");
    ok &= Check(theApp.saves == 1 && f.taskbar.font_requests == 1 && f.taskbar.m_is_width_changed,
                "F13 accepted dialog reaches refresh and SaveConfig boundaries");
    printf("OBSERVED F13 live_per_item=%d saved_per_item=%d\n",
           int(data.specify_each_item_color), int(theApp.saved.specify_each_item_color));
    ok &= Check(data.specify_each_item_color,
                "F13 ColorsModified must enable per-item colors in live settings");
    ok &= Check(theApp.saved.specify_each_item_color
                && theApp.saved.text_colors.at(TDI_UP) == TaskbarItemColor{0x112233,0x445566},
                "F13 SaveConfig must receive enabled per-item colors, not an inert palette");
    ok &= Check(f.taskbar.adjust_calls == 1 && f.taskbar.invalidate_calls == 1
                && f.taskbar.update_window_calls == 1,
                "F13 ApplySettings immediately adjusts, invalidates and redraws the taskbar HWND");
    return ok;
}

bool F14DisablePinned() {
    bool ok = true;
    for (bool ordinary_tooltip : {false, true}) {
        Fixture f;
        f.PinPanel();
        theApp.m_taskbar_data.show_speed_graph_tip = false;
        theApp.m_taskbar_data.show_tool_tip = ordinary_tooltip;
        f.taskbar.ApplySettings();
        auto& tip = CSpeedGraphTip::Instance();
        printf("OBSERVED F14 mode=%s showing=%d pinned=%d visible=%d\n",
               ordinary_tooltip ? "ordinary-tooltip" : "none", int(f.taskbar.m_graph_tip_showing),
               int(tip.IsPinned()), int(tip.IsVisible()));
        ok &= Check(!f.taskbar.m_graph_tip_showing,
                    "F14 disabling graph through REAL ApplySettings clears showing");
        ok &= Check(!tip.IsPinned(), "F14 disabling graph through REAL ApplySettings unpins");
        ok &= Check(!tip.IsVisible(), "F14 disabling graph through REAL ApplySettings hides the panel");
    }
    return ok;
}

bool F15Geometry() {
    // Height 38 with a 30px usable plot area. All expected coordinates are
    // hand-derived literals, not a second copy of the production formula.
    struct Case { int percent, x, y; };
    const Case cases[] = {{0,10,50}, {20,12,44}, {80,14,26}, {90,16,23}, {100,18,20},
                          {-5,20,50}, {120,22,20}};
    array<int, SAMPLE_COUNT> values{};
    for (size_t i=0; i<size(cases); ++i) values[i] = cases[i].percent;
    const auto points = CpuGpuPoints(values, CRect(10,12,128,50));
    bool ok = true;
    for (size_t i=0; i<size(cases); ++i) {
        printf("OBSERVED F15 percent=%d actual=(%d,%d) expected=(%d,%d)\n",
               cases[i].percent, points[i].X, points[i].Y, cases[i].x, cases[i].y);
        ok &= Check(points[i].X == cases[i].x && points[i].Y == cases[i].y,
                    "F15 CPU/GPU percentage must scale over usable height without top clipping");
    }
    return ok;
}

bool Controls() {
    bool ok = true;
    {
        Fixture f;
        f.host.m_tBarDlg = nullptr;
        f.host.OnOptions2();
        ok &= Check(f.host.options_owner == &f.host && f.host.options_calls == 1,
                    "CONTROL F11 absent taskbar uses stable host");
    }
    {
        Fixture f;
        f.taskbar.window.alive = false;
        f.host.OnOptions2();
        ok &= Check(f.host.options_owner == &f.host && f.host.options_calls == 1,
                    "CONTROL F11 invalid taskbar uses stable host");
    }
    {
        Fixture f;
        f.OnlyPlugin();
        f.taskbar.CalculateWindowSize();
        ok &= Check(f.taskbar.m_item_rects.size() == 1 && f.taskbar.CheckClickedItem({4,4})
                    && f.taskbar.GetClickedItem().PluginItem() == &f.plugin,
                    "CONTROL F12 fresh real layout and hit test reach the plugin");
        const int measurements = f.taskbar.dc.extent_calls;
        f.taskbar.CalculateWindowSize();
        ok &= Check(f.taskbar.dc.extent_calls == measurements && f.taskbar.CheckClickedItem({4,4})
                    && f.taskbar.GetClickedItem().PluginItem() == &f.plugin,
                    "CONTROL F12 unchanged cached layout preserves active hit rectangles");
        ok &= Check(!f.taskbar.CheckClickedItem({200,200}),
                    "CONTROL F12 outside click does not hit an item");
    }
    {
        Fixture f;
        CSetItemOrderDlg::on_modal = EditColors;
        CSetItemOrderDlg::modal_result = IDCANCEL;
        f.host.OnDisplaySettings();
        ok &= Check(!theApp.m_taskbar_data.specify_each_item_color
                    && theApp.m_taskbar_data.text_colors.at(TDI_UP) == TaskbarItemColor{0x010203,0x040506}
                    && theApp.m_taskbar_data.disp_str.GetConst(TDI_UP) == L"u"
                    && theApp.saves == 0 && f.taskbar.font_requests == 0 && !f.taskbar.m_is_width_changed,
                    "CONTROL F13 cancel consumes none of the edited dialog draft");
    }
    {
        Fixture f;
        CSetItemOrderDlg::on_modal = [](CSetItemOrderDlg& dlg) { dlg.texts.Get(TDI_UP) = L"label only"; };
        f.host.OnDisplaySettings();
        ok &= Check(!theApp.m_taskbar_data.specify_each_item_color && !theApp.saved.specify_each_item_color
                    && theApp.saves == 1 && theApp.saved.disp_str.GetConst(TDI_UP) == L"label only",
                    "CONTROL F13 accepted unchanged colors must not enable per-item mode");
    }
    {
        Fixture f;
        theApp.m_taskbar_data.specify_each_item_color = true;
        CSetItemOrderDlg::on_modal = EditColors;
        f.host.OnDisplaySettings();
        ok &= Check(theApp.saved.specify_each_item_color && theApp.saves == 1
                    && theApp.saved.text_colors.at(TDI_UP) == TaskbarItemColor{0x112233,0x445566},
                    "CONTROL F13 already-enabled colors stay enabled and commit");
    }
    {
        Fixture f;
        f.PinPanel();
        auto& tip = CSpeedGraphTip::Instance();
        tip.Hide();
        ok &= Check(tip.IsPinned() && tip.IsVisible() && f.panel.hide_calls == 0,
                    "CONTROL F14 REAL Hide preserves an explicitly pinned panel");
        tip.ForceHide();
        ok &= Check(!tip.IsPinned() && !tip.IsVisible() && f.panel.hide_calls == 1,
                    "CONTROL F14 REAL ForceHide unpins and calls the window-hide boundary");
    }
    {
        Fixture f;
        f.PinPanel();
        f.taskbar.ApplySettings();
        ok &= Check(f.taskbar.m_graph_tip_showing && CSpeedGraphTip::Instance().IsPinned()
                    && CSpeedGraphTip::Instance().IsVisible() && f.taskbar.m_tool_tips.pop_calls == 1,
                    "CONTROL F14 enabled REAL ApplySettings preserves pin and removes ordinary tooltip");
        f.taskbar.HideSpeedGraphTip();
        ok &= Check(!f.taskbar.m_graph_tip_showing && !CSpeedGraphTip::Instance().IsPinned()
                    && !CSpeedGraphTip::Instance().IsVisible(),
                    "CONTROL F14 REAL explicit HideSpeedGraphTip clears both state owners");
    }
    {
        array<int, SAMPLE_COUNT> values{};
        values[1] = 100;
        const auto points = CpuGpuPoints(values, CRect(10,12,128,50));
        ok &= Check(points[0].X == 10 && points[0].Y == 50
                    && points[1].X == 12 && points[1].Y == 20,
                    "CONTROL F15 real formula retains literal zero/full-scale endpoints");
        ok &= Check(points[SAMPLE_COUNT].X == 128 && points[SAMPLE_COUNT].Y == 50
                    && points[SAMPLE_COUNT+1].X == 10 && points[SAMPLE_COUNT+1].Y == 50,
                    "CONTROL F15 real polygon closes along the bottom edge");
    }
    return ok;
}
int wmain(int argc, wchar_t** argv) {
    if (argc != 3) return 2;
    const wstring mode = argv[1];
    bool ok;
    if (mode == L"controls") ok = Controls();
    else if (mode == L"f11_owner") ok = F11Owner();
    else if (mode == L"f12_layout_hit") ok = F12LayoutHit();
    else if (mode == L"f13_colors") ok = F13Colors();
    else if (mode == L"f14_disable_pinned") ok = F14DisablePinned();
    else if (mode == L"f15_percent_geometry") ok = F15Geometry();
    else return 2;
    return ok ? 0 : 1;
}
'''
    provenance = {"source_kind": "current production, never audit-frozen snapshots",
                  "source_files": {str(native.SOURCE / name): hashlib.sha256(value.encode("utf-8")).hexdigest()
                                   for name, value in sources.items()},
                  "hash_encoding": "SHA256 of UTF-8-decoded/re-encoded source text",
                  "extracted": extracted}
    return code, provenance


def render_contracts():
    """Method-level contracts for the taskbar blank-frame/geometry fixes.

    These assertions read current production methods; they do not claim a real
    desktop GUI or compositor run.
    """
    def source(name):
        raw = (native.SOURCE / name).read_bytes()
        try:
            return raw.decode("utf-8-sig")
        except UnicodeDecodeError:
            return raw.decode("gbk")

    taskbar_cpp = source("TaskBarDlg.cpp")
    win11_cpp = source("Win11TaskbarDlg.cpp")
    wine_cpp = source("WineTaskbarDlg.cpp")
    classical_cpp = source("ClassicalTaskbarDlg.cpp")
    tip_cpp = source("SpeedGraphTip.cpp")
    erase = native.method(taskbar_cpp, "BOOL CTaskBarDlg::OnEraseBkgnd(")
    apply = native.method(taskbar_cpp, "void CTaskBarDlg::ApplySettings()")
    set_dpi = native.method(taskbar_cpp, "void CTaskBarDlg::SetDPI(")
    win11 = native.method(win11_cpp, "void CWin11TaskbarDlg::AdjustTaskbarWndPos(")
    wine = native.method(wine_cpp, "void CWineTaskbarDlg::AdjustTaskbarWndPos(")
    classical = native.method(classical_cpp, "void CClassicalTaskbarDlg::AdjustTaskbarWndPos(")
    wnd_proc = native.method(tip_cpp, "LRESULT CSpeedGraphTip::WndProc(")
    if "ON_WM_ERASEBKGND()" not in taskbar_cpp or "return TRUE;" not in erase:
        raise AssertionError("taskbar must own WM_ERASEBKGND without MFC background erase")
    for token in ("AdjustWindowPos(true);", "Invalidate(FALSE);", "UpdateWindow();"):
        if token not in apply:
            raise AssertionError(f"ApplySettings missing immediate redraw step: {token}")
    if "OnDpiChanged(dpi, taskbar_rect)" not in set_dpi:
        raise AssertionError("taskbar DPI changes must refresh the hover panel")
    if "m_rcNotify.left - m_rcTaskbar.left" not in win11:
        raise AssertionError("Win11 notify position must be taskbar-relative")
    if "m_rect.left = max(0" not in win11 or "m_rect.top = max(0" not in win11:
        raise AssertionError("Win11 final rectangle must be clamped")
    if "GetWindowRect(GetSafeHwnd(), &current_rect)" not in wine:
        raise AssertionError("Wine must compare the current rectangle")
    if "current_rect != rect" not in wine:
        raise AssertionError("Wine must avoid unchanged MoveWindow calls")
    if "max(0, m_window_width)" not in classical or "max(0, m_window_height)" not in classical:
        raise AssertionError("Classical geometry must reject negative dimensions")
    paint = wnd_proc[wnd_proc.index("case WM_PAINT:"):wnd_proc.index("case WM_ERASEBKGND:")]
    if "buffer_graphics.Clear" not in paint or paint.index("buffer_graphics.Clear") > paint.index("self->Draw"):
        raise AssertionError("SpeedGraphTip must clear its first memory frame before drawing")
    legend_start = tip_cpp.index("void DrawChartLegend(")
    if "COLORREF text_color" not in tip_cpp[legend_start:legend_start + 260]:
        raise AssertionError("SpeedGraphTip legend must accept a theme contrast color")
    if "dc.SetTextColor(RGB(215, 215, 220))" in tip_cpp:
        raise AssertionError("SpeedGraphTip legend must not use a fixed text color")
    print("SOURCE RENDER CONTRACTS PASS", flush=True)


def main():
    render_contracts()
    timer = native.method(native.source("TaskBarDlg.cpp"),
                          "void CTaskBarDlg::OnTimer(UINT_PTR nIDEvent)")
    if "CalculateWindowSize()" in timer and "AdjustWindowPos()" not in timer:
        raise AssertionError(
            "taskbar timer must apply a changed custom-draw width to the window")
    available = ("controls", "f11_owner", "f12_layout_hit", "f13_colors",
                 "f14_disable_pinned", "f15_percent_geometry")
    selected = sys.argv[1:]
    if any(mode not in available for mode in selected):
        raise SystemExit("Unknown UI regression mode")
    modes = tuple(dict.fromkeys(["controls", *selected])) if selected else available
    code, provenance = make_code()
    run, env = native.build(code, "ui-f11-f15")
    (run / "current-source-provenance.json").write_text(
        json.dumps(provenance, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"BUILD PASS; ARTIFACTS {run}", flush=True)
    results = {mode: native.execute(run, env, mode) for mode in modes}
    log = ["BUILD PASS (native.build returned successfully)\n",
           "CURRENT source paths/hashes: current-source-provenance.json\n",
           "SCOPE: isolated native methods; no real windows/registry/autostart/application.\n"]
    log.append("\n== build ==\n" + (run / "build.log").read_text(encoding="utf-8"))
    for mode in modes:
        log.append(f"\n== {mode} ==\n" + (run / f"{mode}.log").read_text(encoding="utf-8"))
    summary = "RESULT " + ", ".join(f"{mode}={'PASS' if ok else 'FAIL'}" for mode, ok in results.items())
    log.append("\n" + summary + "\n")
    (run / "ui-regressions.log").write_text("".join(log), encoding="utf-8")
    print(summary)
    print(f"LOG {run / 'ui-regressions.log'}")
    return 0 if all(results.values()) else 1


if __name__ == "__main__":
    raise SystemExit(main())
