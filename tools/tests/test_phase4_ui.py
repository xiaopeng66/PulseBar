"""Phase 4 UI/resource regressions.

This is a bounded static gate: it reads the current dialog implementations and
UTF-16 resource template, then checks the geometry and defensive branches that
are difficult to exercise without a desktop session.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2] / "PulseBar"


def read(name, encoding="utf-8-sig"):
    return (ROOT / name).read_text(encoding=encoding)


def method(text, signature):
    start = text.index(signature)
    pos = text.index("{", start)
    depth = 0
    state = "code"
    quote = ""
    while pos < len(text):
        c = text[pos]
        nxt = text[pos:pos + 2]
        if state == "line":
            if c == "\n":
                state = "code"
        elif state == "block":
            if nxt == "*/":
                state = "code"
                pos += 1
        elif state == "string":
            if c == "\\":
                pos += 1
            elif c == quote:
                state = "code"
        elif nxt == "//":
            state = "line"
            pos += 1
        elif nxt == "/*":
            state = "block"
            pos += 1
        elif c in "\"'":
            quote = c
            state = "string"
        elif c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return text[start:pos + 1]
        pos += 1
    raise AssertionError(f"unterminated method: {signature}")


def dialog_block(rc, dialog_id):
    start = rc.index(dialog_id + " DIALOGEX")
    end = rc.index("\nEND", start)
    return rc[start:end]


def coords(block, control_id):
    lines = [line.strip() for line in block.splitlines() if control_id in line]
    if len(lines) != 1:
        raise AssertionError(f"expected one {control_id}, got {len(lines)}")
    numbers = [int(value) for value in re.findall(r"(?<![A-Za-z_])(-?\d+)(?![A-Za-z_])", lines[0])]
    if len(numbers) < 4:
        raise AssertionError(f"no coordinates for {control_id}: {lines[0]}")
    return tuple(numbers[-4:])


def overlap(a, b):
    ax, ay, aw, ah = a
    bx, by, bw, bh = b
    return max(ax, bx) < min(ax + aw, bx + bw) and max(ay, by) < min(ay + ah, by + bh)


def require(value, message):
    if not value:
        raise AssertionError(message)


def main():
    rc = read("PulseBar.rc", "utf-16")
    history = read("HistoryTrafficDlg.cpp")
    history_h = read("HistoryTrafficDlg.h")
    history_list = read("HistoryTrafficListDlg.cpp")
    calendar = read("HistoryTrafficCalendarDlg.cpp")
    display = read("DisplayTextSettingDlg.cpp")
    display_h = read("DisplayTextSettingDlg.h")
    plugin = read("PluginManagerDlg.cpp")
    plugin_h = read("PluginManagerDlg.h")
    info = read("PluginInfoDlg.cpp")
    info_h = read("PluginInfoDlg.h")
    colors = read("TaskbarColorDlg.cpp")
    colors_h = read("TaskbarColorDlg.h")
    taskbar = read("TaskBarSettingsDlg.cpp")
    tab = read("TabDlg.cpp")
    general = read("GeneralSettingsDlg.cpp")
    base = read("BaseDialog.cpp")
    options = read("OptionsDlg.cpp")
    pulse = read("PulseBarDlg.cpp")

    require("int m_tab_selected{ 0 };" in history_h,
            "history tab selection must have a deterministic initial value")
    history_init = method(history, "BOOL CHistoryTrafficDlg::OnInitDialog()")
    require("Create(IDD_HISTORY_TRAFFIC_LIST_DIALOG" in history_init and
            "Create(IDD_HISTORY_TRAFFIC_CALENDAR_DIALOG" in history_init and
            "if (!tab1_created || !tab2_created)" in history_init,
            "history child dialog creation must be checked")
    require("m_tab.SetCurSel(m_tab_selected)" in history_init,
            "history tab must select the current item, not only focus it")
    history_switch = method(history, "void CHistoryTrafficDlg::OnTcnSelchangeTab1(")
    require(history_switch.index("m_tab1_dlg.ShowWindow(SW_HIDE)") <
            history_switch.index("selected->ShowWindow(SW_SHOW)"),
            "history tab switch must hide the old page before showing the new page")
    require("RDW_INVALIDATE" in history_switch and "RDW_ERASE" in history_switch and
            "RDW_UPDATENOW" in history_switch,
            "history tab switch must erase and redraw the tab area")

    history_resource = dialog_block(rc, "IDD_HISTORY_TRAFFIC_DIALOG")
    require(not overlap(coords(history_resource, "IDC_TAB1"),
                         coords(history_resource, "IDOK")),
            "history tab and OK button still overlap")

    require("max_traffic == 0" in history_list,
            "history list must guard zero traffic before scaling")
    require("for (const auto& data : list_data)" in history_list and
            "if (cur_traffic > max_traffic)" in history_list,
            "aggregated history view must include its final group in the maximum")
    require("m_history_traffics.empty()" in calendar and
            "GetClientRect" in calendar and
            ("IntersectRect" in calendar or "min(" in calendar),
            "calendar must handle empty data and clip to a small client area")

    display_resource = dialog_block(rc, "IDD_DISPLAY_TEXT_SETTINGS_DIALOG")
    display_buttons = ["IDC_RESTORE_DEFAULT_BUTTON", "IDOK", "IDCANCEL"]
    require(all(coords(display_resource, control)[3] >= 18 for control in display_buttons),
            "display-text button row must use standard DLU height")
    require(not overlap(coords(display_resource, "IDC_LIST1"),
                         coords(display_resource, "IDC_RESTORE_DEFAULT_BUTTON")),
            "display-text list overlaps restore button")
    require(not overlap(coords(display_resource, "IDC_RESTORE_DEFAULT_BUTTON"),
                         coords(display_resource, "IDOK")),
            "display-text restore button overlaps OK")
    require(not overlap(coords(display_resource, "IDOK"),
                         coords(display_resource, "IDCANCEL")),
            "display-text OK overlaps Cancel")
    require("ON_WM_SIZE()" in display and "SetColumnWidth" in display and
            "afx_msg void OnSize" in display_h,
            "display-text list must resize its columns")

    plugin_resource = dialog_block(rc, "IDD_PLUGIN_MANAGER_DIALOG")
    require(not overlap(coords(plugin_resource, "IDC_LIST1"),
                         coords(plugin_resource, "IDC_PLUGIN_DOWNLOAD_STATIC")),
            "plugin list overlaps download link")
    for link in ("IDC_PLUGIN_DOWNLOAD_STATIC", "IDC_PLUGIN_DEV_GUID_STATIC", "IDC_OPEN_PLUGIN_DIR_STATIC"):
        require(not overlap(coords(plugin_resource, link),
                            coords(plugin_resource, "IDC_PLUGIN_INFO_BUTTON")),
                f"plugin link {link} overlaps button row")
    require(all(coords(plugin_resource, control)[3] == 14
                for control in ("IDC_PLUGIN_INFO_BUTTON", "IDC_OPTINS_BUTTON", "IDCANCEL")),
            "plugin button row must retain the requested compact DLU height")
    require("SetBackgroundColor(GetSysColor(COLOR_WINDOW))" in plugin,
            "plugin links need a background matching the dialog")
    require("ON_WM_SIZE()" in plugin and "SetColumnWidth" in plugin and
            "afx_msg void OnSize" in plugin_h,
            "plugin list must resize its columns")
    require("ON_WM_SIZE()" in info and "SetColumnWidth" in info and
            "afx_msg void OnSize" in info_h,
            "plugin-info list must resize its columns")
    require("ON_WM_SIZE()" in colors and "SetColumnWidth" in colors and
            "afx_msg void OnSize" in colors_h,
            "taskbar-color list must resize its columns")

    require("EDITTEXT IDC_SETTINGS_IMAGE_PATH" in rc,
            "background image path is written by code but absent from the resource")
    require("IDC_SETTINGS_IMAGE_PATH" in taskbar and
            "item.is_edit" in taskbar,
            "background image path must be present in the template control table")

    require("RDW_INVALIDATE" in tab and "RDW_ERASE" in tab and "RDW_UPDATENOW" in tab,
            "dynamic visibility changes must redraw the affected rectangle")
    scrollbar = method(tab, "bool CTabDlg::IsScrollBarVisible()")
    require("cbSize = sizeof" in scrollbar,
            "scrollbar visibility query must initialize SCROLLINFO.cbSize")
    fill = method(general, "void CGeneralSettingsDlg::FillTemperatureNameCombos()")
    require(fill.count("SetRedraw(FALSE)") >= 2 and fill.count("SetRedraw(TRUE)") >= 2,
            "temperature combo rebuilds must suppress redraw and restore it")
    require("SetCurSel(cur_index)" in fill,
            "temperature combo rebuilds must restore the selected item")

    menu = method(pulse, "void CPulseBarDlg::IniConnectionMenu(CMenu* pMenu)")
    menu_state = method(pulse, "void CPulseBarDlg::SetConnectionMenuState(CMenu* pMenu)")
    require("dynamic_start" in menu and "InsertMenu(insert_pos" in menu,
            "dynamic network items must be inserted after the preserved separator")
    require("GetMenuItemID" in menu_state and "m_connection_selected + 2" not in menu_state,
            "network menu selection must not address the separator by fixed offset")

    require("if (!dialog_name.IsEmpty())" in base and
            "if (!GetDialogName().IsEmpty()" in base,
            "empty-name tab pages must not register or focus as unique dialogs")
    require("DT_END_ELLIPSIS" in options,
            "long localized options navigation labels need an explicit ellipsis policy")

    print("PASS phase4 static UI/resource regressions")


if __name__ == "__main__":
    main()
