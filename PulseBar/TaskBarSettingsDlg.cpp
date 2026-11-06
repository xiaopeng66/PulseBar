// TaskBarSettingsDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "PulseBar.h"
#include "TaskBarSettingsDlg.h"
#include "afxdialogex.h"
#include "CMFCColorDialogEx.h"
#include "CAutoAdaptSettingsDlg.h"
#include "DisplayTextSettingDlg.h"
#include "SetItemOrderDlg.h"
#include "WindowsSettingHelper.h"
#include "PulseBarDlg.h"
#include "FileDialogEx.h"
#include "Win11TaskbarSettingDlg.h"
#include "TaskbarHelper.h"
#include "DrawCommon.h"
#include "TaskbarThemeGeometry.h"

// CTaskBarSettingsDlg 对话框

IMPLEMENT_DYNAMIC(CTaskBarSettingsDlg, CTabDlg)

CTaskBarSettingsDlg::CTaskBarSettingsDlg(CWnd* pParent /*=NULL*/)
    : CTabDlg(IDD_TASKBAR_SETTINGS_DIALOG, pParent)
{

}

CTaskBarSettingsDlg::~CTaskBarSettingsDlg()
{
}

bool CTaskBarSettingsDlg::IsStyleModified()
{
    bool modified{};
    modified |= (theApp.m_taskbar_data.text_colors != m_data.text_colors);
    modified |= (theApp.m_taskbar_data.back_color != m_data.back_color);
    modified |= (theApp.m_taskbar_data.transparent_color != m_data.transparent_color);
    modified |= (theApp.m_taskbar_data.status_bar_color != m_data.status_bar_color);
    modified |= (theApp.m_taskbar_data.specify_each_item_color != m_data.specify_each_item_color);
    return modified && m_style_modified;
}

void CTaskBarSettingsDlg::DrawStaticColor()
{
    //CCommon::FillStaticColor(m_text_color_static, m_data.text_color);
    //CCommon::FillStaticColor(m_back_color_static, m_data.back_color);
    if (m_data.specify_each_item_color)
    {
        int color_num{ static_cast<int>(m_data.text_colors.size()) * 2 };
        if (color_num > 16)
            color_num = 16;
        int i{};
        m_text_color_static.SetColorNum(color_num);
        for (const auto& item : m_data.text_colors)
        {
            m_text_color_static.SetFillColor(i, item.second.label);
            m_text_color_static.SetFillColor(i + 1, item.second.value);
            i += 2;
        }
        m_text_color_static.Invalidate();
    }
    else if (!m_data.text_colors.empty())
    {
        m_text_color_static.SetFillColor(m_data.text_colors.begin()->second.label);
    }
    m_back_color_static.SetFillColor(m_data.back_color);
    //m_trans_color_static.SetFillColor(m_data.transparent_color);
    m_status_bar_color_static.SetFillColor(m_data.GetUsageGraphColor());
}

void CTaskBarSettingsDlg::IniUnitCombo()
{
    m_unit_combo.SetRedraw(FALSE);
    m_unit_combo.ResetContent();
    m_unit_combo.AddString(CCommon::LoadText(IDS_AUTO));
    if (m_data.unit_byte)
    {
        m_unit_combo.AddString(CCommon::LoadText(IDS_FIXED_AS, _T(" KB/s")));
        m_unit_combo.AddString(CCommon::LoadText(IDS_FIXED_AS, _T(" MB/s")));
    }
    else
    {
        m_unit_combo.AddString(CCommon::LoadText(IDS_FIXED_AS, _T(" Kb/s")));
        m_unit_combo.AddString(CCommon::LoadText(IDS_FIXED_AS, _T(" Mb/s")));
    }
    m_unit_combo.SetRedraw(TRUE);
    m_unit_combo.SetCurSel(static_cast<int>(m_data.speed_unit));
}

void CTaskBarSettingsDlg::ApplyDefaultStyle(int index)
{
    auto pending = m_pending_presets.find(index);
    if (pending != m_pending_presets.end())
    {
        const auto& data = pending->second;
        m_data.text_colors = data.text_colors;
        m_data.back_color = data.back_color;
        m_data.transparent_color = data.transparent_color;
        m_data.status_bar_color = data.status_bar_color;
        m_data.specify_each_item_color = data.specify_each_item_color;
    }
    else
        theApp.m_taskbar_default_style.ApplyDefaultStyle(index, m_data);
    m_data.tb_theme = 0;
    m_data.tb_use_background_image = false;
    m_style_modified = true;
    RefreshThemeControls();
    DrawStaticColor();
    ((CButton*)GetDlgItem(IDC_SPECIFY_EACH_ITEM_COLOR_CHECK))->SetCheck(m_data.specify_each_item_color);
    m_background_transparent_chk.SetCheck(m_data.IsTaskbarTransparent());
}

void CTaskBarSettingsDlg::ModifyDefaultStyle(int index)
{
    m_pending_presets[index] = m_data;
}

void CTaskBarSettingsDlg::EnableControl()
{
    bool exe_path_enable = (m_data.double_click_action == DoubleClickAction::SEPCIFIC_APP);
    const bool show_exe = exe_path_enable && (m_section == 2 || m_section < 0);
    ShowDlgCtrl(IDC_EXE_PATH_STATIC, show_exe);
    ShowDlgCtrl(IDC_EXE_PATH_EDIT, show_exe);
    ShowDlgCtrl(IDC_BROWSE_BUTTON, show_exe);
    EnableDlgCtrl(IDC_SHOW_DASHED_BOX, m_data.show_status_bar || m_data.show_netspeed_figure);
    m_status_bar_color_static.EnableWindow(m_data.show_status_bar || m_data.show_netspeed_figure);
    EnableDlgCtrl(IDC_CM_GRAPH_BAR_RADIO, m_data.show_status_bar || m_data.show_netspeed_figure);
    EnableDlgCtrl(IDC_CM_GRAPH_PLOT_RADIO, m_data.show_status_bar || m_data.show_netspeed_figure);
    EnableDlgCtrl(IDC_NET_SPEED_FIGURE_MAX_VALUE_EDIT, m_data.show_netspeed_figure);
    EnableDlgCtrl(IDC_NET_SPEED_FIGURE_MAX_VALUE_UNIT_COMBO, m_data.show_netspeed_figure);
    //Win11下，任务栏左对齐时禁用“任务栏窗口显示在任务栏左侧”的选项
    EnableDlgCtrl(IDC_TASKBAR_WND_ON_LEFT_CHECK, !theApp.IsWindows11Taskbar() || CWindowsSettingHelper::IsTaskbarCenterAlign());
    EnableDlgCtrl(IDC_ENABLE_COLOR_EMOJI_CHECK, !m_data.disable_d2d && !m_data.IsTaskbarThemePanel());
    EnableDlgCtrl(IDC_WIN11_SETTINGS_BUTTON, theApp.IsWindows11Taskbar());
    const bool classic = m_data.tb_theme == 0;
    //经典专属的颜色行只在外观页(m_section==1)显示，避免把其他分区已隐藏的控件又显示出来。
    //隐藏后把控件原矩形在父窗口标脏并立即重绘，否则 DWM 表面会残留旧画面。
    auto hide_and_clear = [this](UINT id) {
        CWnd* ctrl = GetDlgItem(id);
        if (ctrl == nullptr || ctrl->GetSafeHwnd() == nullptr)
            return;
        if (::IsWindowVisible(ctrl->GetSafeHwnd()))
        {
            CRect rect;
            ctrl->GetWindowRect(&rect);
            ScreenToClient(rect);
            ctrl->ShowWindow(SW_HIDE);
            InvalidateRect(rect, TRUE);
        }
    };
    const bool show_classic_rows = classic && m_section == 1;
    if (show_classic_rows)
    {
        ShowDlgCtrl(IDC_TXT_COLOR_STATIC, true);
        ShowDlgCtrl(IDC_TEXT_COLOR_STATIC1, true);
        ShowDlgCtrl(IDC_SPECIFY_EACH_ITEM_COLOR_CHECK, true);
        ShowDlgCtrl(IDC_BACK_COLOR_STATIC, true);
        ShowDlgCtrl(IDC_TEXT_COLOR_STATIC2, true);
        ShowDlgCtrl(IDC_BACKGROUND_TRANSPARENT_CHECK, true);
        ShowDlgCtrl(IDC_AUTO_SET_BACK_COLOR_CHECK, true);
        ShowDlgCtrl(IDC_AUTO_ADAPT_LIGHT_THEME_CHECK, true);
        ShowDlgCtrl(IDC_AUTO_ADAPT_SETTINGS_BUTTON, true);
        ShowDlgCtrl(IDC_DEFAULT_STYLE_BUTTON, true);
    }
    else
    {
        hide_and_clear(IDC_TXT_COLOR_STATIC);
        hide_and_clear(IDC_TEXT_COLOR_STATIC1);
        hide_and_clear(IDC_SPECIFY_EACH_ITEM_COLOR_CHECK);
        hide_and_clear(IDC_BACK_COLOR_STATIC);
        hide_and_clear(IDC_TEXT_COLOR_STATIC2);
        hide_and_clear(IDC_BACKGROUND_TRANSPARENT_CHECK);
        hide_and_clear(IDC_AUTO_SET_BACK_COLOR_CHECK);
        hide_and_clear(IDC_AUTO_ADAPT_LIGHT_THEME_CHECK);
        hide_and_clear(IDC_AUTO_ADAPT_SETTINGS_BUTTON);
        hide_and_clear(IDC_DEFAULT_STYLE_BUTTON);
    }
    EnableDlgCtrl(IDC_BACKGROUND_TRANSPARENT_CHECK, classic);
    EnableDlgCtrl(IDC_AUTO_SET_BACK_COLOR_CHECK, classic && theApp.m_win_version.IsWindows8OrLater());
    EnableDlgCtrl(IDC_AUTO_ADAPT_LIGHT_THEME_CHECK, classic && theApp.m_win_version.GetMajorVersion() >= 10);
    EnableDlgCtrl(IDC_AUTO_ADAPT_SETTINGS_BUTTON, classic && m_data.auto_adapt_light_theme);
    EnableDlgCtrl(IDC_DEFAULT_STYLE_BUTTON, classic);
    m_back_color_static.EnableWindow(classic);
    m_text_color_static.EnableWindow(classic || m_data.specify_each_item_color);
    EnableDlgCtrl(IDC_GDI_RADIO, !m_data.IsTaskbarThemePanel());
    EnableDlgCtrl(IDC_D2D_RADIO, !m_data.IsTaskbarThemePanel() && CTaskBarDlgDrawCommonSupport::CheckSupport());
    RefreshSectionVisibility();
}


void CTaskBarSettingsDlg::SetControlMouseWheelEnable(bool enable)
{
    m_unit_combo.SetMouseWheelEnable(enable);
    m_double_click_combo.SetMouseWheelEnable(enable);
    m_digit_number_combo.SetMouseWheelEnable(enable);
    m_font_size_edit.SetMouseWheelEnable(enable);
    m_memory_display_combo.SetMouseWheelEnable(enable);
    m_item_space_edit.SetMouseWheelEnable(enable);
    m_vertical_margin_edit.SetMouseWheelEnable(enable);
    m_net_speed_figure_max_val_edit.SetMouseWheelEnable(enable);
    m_net_speed_figure_max_val_unit_combo.SetMouseWheelEnable(enable);
    m_displays_combo.SetMouseWheelEnable(enable);
    m_theme_combo.SetMouseWheelEnable(enable);
    m_hover_combo.SetMouseWheelEnable(enable);
    m_opacity_edit.SetMouseWheelEnable(enable);
    m_corner_edit.SetMouseWheelEnable(enable);
}

bool CTaskBarSettingsDlg::InitializeControls()
{
    return false;
}

void CTaskBarSettingsDlg::DoDataExchange(CDataExchange* pDX)
{
    DDX_Control(pDX, IDC_TEXT_COLOR_STATIC1, m_text_color_static);
    DDX_Control(pDX, IDC_TEXT_COLOR_STATIC2, m_back_color_static);
    DDX_Control(pDX, IDC_TEXT_COLOR_STATIC3, m_status_bar_color_static);
    CTabDlg::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_UNIT_COMBO, m_unit_combo);
    DDX_Control(pDX, IDC_HIDE_UNIT_CHECK, m_hide_unit_chk);
    DDX_Control(pDX, IDC_FONT_SIZE_EDIT1, m_font_size_edit);
    DDX_Control(pDX, IDC_DOUBLE_CLICK_COMBO, m_double_click_combo);
    DDX_Control(pDX, IDC_DIGIT_NUMBER_COMBO, m_digit_number_combo);
    //DDX_Control(pDX, IDC_TRANSPARENT_COLOR_STATIC, m_trans_color_static);
    DDX_Control(pDX, IDC_BACKGROUND_TRANSPARENT_CHECK, m_background_transparent_chk);
    DDX_Control(pDX, IDC_AUTO_ADAPT_LIGHT_THEME_CHECK, m_atuo_adapt_light_theme_chk);
    DDX_Control(pDX, IDC_AUTO_SET_BACK_COLOR_CHECK, m_auto_set_back_color_chk);
    DDX_Control(pDX, IDC_MEMORY_DISPLAY_COMBO, m_memory_display_combo);
    DDX_Control(pDX, IDC_ITEM_SPACE_EDIT, m_item_space_edit);
    DDX_Control(pDX, IDC_VERTICAL_MARGIN_EDIT, m_vertical_margin_edit);
    DDX_Control(pDX, IDC_NET_SPEED_FIGURE_MAX_VALUE_EDIT, m_net_speed_figure_max_val_edit);
    DDX_Control(pDX, IDC_NET_SPEED_FIGURE_MAX_VALUE_UNIT_COMBO, m_net_speed_figure_max_val_unit_combo);
    DDX_Control(pDX, IDC_DISPLAY_TO_SHOW_TASKBAR_WND_COMBO, m_displays_combo);
    DDX_Control(pDX, IDC_SETTINGS_THEME_COMBO, m_theme_combo);
    DDX_Control(pDX, IDC_SETTINGS_HOVER_COMBO, m_hover_combo);
    DDX_Control(pDX, IDC_SETTINGS_OPACITY_EDIT, m_opacity_edit);
    DDX_Control(pDX, IDC_SETTINGS_CORNER_EDIT, m_corner_edit);
    DDX_Control(pDX, IDC_SETTINGS_THEME_PREVIEW, m_theme_preview);
}


BEGIN_MESSAGE_MAP(CTaskBarSettingsDlg, CTabDlg)
    ON_BN_CLICKED(IDC_SET_FONT_BUTTON1, &CTaskBarSettingsDlg::OnBnClickedSetFontButton1)
    ON_BN_CLICKED(IDC_CUSTOM_THEME_BTN, &CTaskBarSettingsDlg::OnBnClickedCustomThemeBtn)
    ON_BN_CLICKED(IDC_TASKBAR_WND_ON_LEFT_CHECK, &CTaskBarSettingsDlg::OnBnClickedTaskbarWndOnLeftCheck)
    ON_BN_CLICKED(IDC_SPEED_SHORT_MODE_CHECK, &CTaskBarSettingsDlg::OnBnClickedSpeedShortModeCheck)
    ON_CBN_SELCHANGE(IDC_UNIT_COMBO, &CTaskBarSettingsDlg::OnCbnSelchangeUnitCombo)
    ON_BN_CLICKED(IDC_HIDE_UNIT_CHECK, &CTaskBarSettingsDlg::OnBnClickedHideUnitCheck)
    ON_BN_CLICKED(IDC_VALUE_RIGHT_ALIGN_CHECK, &CTaskBarSettingsDlg::OnBnClickedValueRightAlignCheck)
    ON_BN_CLICKED(IDC_HIDE_PERCENTAGE_CHECK, &CTaskBarSettingsDlg::OnBnClickedHidePercentageCheck)
    ON_MESSAGE(WM_STATIC_CLICKED, &CTaskBarSettingsDlg::OnStaticClicked)
    ON_BN_CLICKED(IDC_SPECIFY_EACH_ITEM_COLOR_CHECK, &CTaskBarSettingsDlg::OnBnClickedSpecifyEachItemColorCheck)
    ON_CBN_SELCHANGE(IDC_DOUBLE_CLICK_COMBO, &CTaskBarSettingsDlg::OnCbnSelchangeDoubleClickCombo)
    ON_BN_CLICKED(IDC_HORIZONTAL_ARRANGE_CHECK, &CTaskBarSettingsDlg::OnBnClickedHorizontalArrangeCheck)
    ON_BN_CLICKED(IDC_SHOW_STATUS_BAR_CHECK, &CTaskBarSettingsDlg::OnBnClickedShowStatusBarCheck)
    ON_BN_CLICKED(IDC_SEPARATE_VALUE_UNIT_CHECK, &CTaskBarSettingsDlg::OnBnClickedSeparateValueUnitCheck)
    ON_BN_CLICKED(IDC_UNIT_BYTE_RADIO, &CTaskBarSettingsDlg::OnBnClickedUnitByteRadio)
    ON_BN_CLICKED(IDC_UNIT_BIT_RADIO, &CTaskBarSettingsDlg::OnBnClickedUnitBitRadio)
    ON_BN_CLICKED(IDC_DEFAULT_STYLE_BUTTON, &CTaskBarSettingsDlg::OnBnClickedDefaultStyleButton)
    ON_WM_DESTROY()
    ON_BN_CLICKED(IDC_BROWSE_BUTTON, &CTaskBarSettingsDlg::OnBnClickedBrowseButton)
    ON_BN_CLICKED(IDC_CM_GRAPH_BAR_RADIO, &CTaskBarSettingsDlg::OnBnClickedCMGraphBarRadio)
    ON_BN_CLICKED(IDC_CM_GRAPH_PLOT_RADIO, &CTaskBarSettingsDlg::OnBnClickedCMGraphPLOTRadio)
    ON_BN_CLICKED(IDC_BACKGROUND_TRANSPARENT_CHECK, &CTaskBarSettingsDlg::OnBnClickedBackgroundTransparentCheck)
    ON_BN_CLICKED(IDC_AUTO_ADAPT_SETTINGS_BUTTON, &CTaskBarSettingsDlg::OnBnClickedAutoAdaptSettingsButton)
    ON_BN_CLICKED(IDC_AUTO_ADAPT_LIGHT_THEME_CHECK, &CTaskBarSettingsDlg::OnBnClickedAutoAdaptLightThemeCheck)
    ON_BN_CLICKED(IDC_AUTO_SET_BACK_COLOR_CHECK, &CTaskBarSettingsDlg::OnBnClickedAutoSetBackColorCheck)
    ON_CBN_SELCHANGE(IDC_MEMORY_DISPLAY_COMBO, &CTaskBarSettingsDlg::OnCbnSelchangeMemoryDisplayCombo)
    ON_BN_CLICKED(IDC_SHOW_DASHED_BOX, &CTaskBarSettingsDlg::OnBnClickedShowDashedBox)
    ON_BN_CLICKED(IDC_SET_ORDER_BUTTON, &CTaskBarSettingsDlg::OnBnClickedSetOrderButton)
    ON_EN_CHANGE(IDC_ITEM_SPACE_EDIT, &CTaskBarSettingsDlg::OnEnChangeItemSpaceEdit)
    ON_EN_CHANGE(IDC_VERTICAL_MARGIN_EDIT, &CTaskBarSettingsDlg::OnEnChangeVerticalMarginEdit)
    ON_BN_CLICKED(IDC_SHOW_NET_SPEED_FIGURE_CHECK, &CTaskBarSettingsDlg::OnBnClickedShowNetSpeedFigureCheck)
    ON_CBN_SELCHANGE(IDC_NET_SPEED_FIGURE_MAX_VALUE_UNIT_COMBO, &CTaskBarSettingsDlg::OnCbnSelchangeNetSpeedFigureMaxValueUnitCombo)
    ON_EN_CHANGE(IDC_NET_SPEED_FIGURE_MAX_VALUE_EDIT, &CTaskBarSettingsDlg::OnEnChangeNetSpeedFigureMaxValueEdit)
    ON_BN_CLICKED(IDC_GDI_RADIO, &CTaskBarSettingsDlg::OnBnClickedGdiRadio)
    ON_BN_CLICKED(IDC_D2D_RADIO, &CTaskBarSettingsDlg::OnBnClickedD2dRadio)
    ON_BN_CLICKED(IDC_ENABLE_COLOR_EMOJI_CHECK, &CTaskBarSettingsDlg::OnBnClickedEnableColorEmojiCheck)
    ON_CBN_SELCHANGE(IDC_DIGIT_NUMBER_COMBO, &CTaskBarSettingsDlg::OnCbnSelchangeDigitNumberCombo)
    ON_BN_CLICKED(IDC_WIN11_SETTINGS_BUTTON, &CTaskBarSettingsDlg::OnBnClickedWin11SettingsButton)
    ON_CBN_SELCHANGE(IDC_DISPLAY_TO_SHOW_TASKBAR_WND_COMBO, &CTaskBarSettingsDlg::OnCbnSelchangeDisplayToShowTaskbarWndCombo)
    ON_BN_CLICKED(IDC_USAGE_GRAPH_FOLLOW_SYSTEM_CHECK, &CTaskBarSettingsDlg::OnBnClickedUsageGraphFollowSystemCheck)
    ON_EN_CHANGE(IDC_FONT_SIZE_EDIT1, &CTaskBarSettingsDlg::OnEnChangeFontSizeEdit1)
    ON_CBN_SELCHANGE(IDC_SETTINGS_THEME_COMBO, &CTaskBarSettingsDlg::OnThemeChanged)
    ON_CBN_SELCHANGE(IDC_SETTINGS_HOVER_COMBO, &CTaskBarSettingsDlg::OnHoverChanged)
    ON_BN_CLICKED(IDC_SETTINGS_IMAGE_CHECK, &CTaskBarSettingsDlg::OnBackgroundImageChanged)
    ON_BN_CLICKED(IDC_SETTINGS_IMAGE_BUTTON, &CTaskBarSettingsDlg::OnSelectBackgroundImage)
    ON_EN_CHANGE(IDC_SETTINGS_OPACITY_EDIT, &CTaskBarSettingsDlg::OnThemeDimensionsChanged)
    ON_EN_CHANGE(IDC_SETTINGS_CORNER_EDIT, &CTaskBarSettingsDlg::OnThemeDimensionsChanged)
    ON_WM_DRAWITEM()
    ON_BN_CLICKED(IDC_SETTINGS_RENDERING_BUTTON, &CTaskBarSettingsDlg::OnRenderingSettings)
END_MESSAGE_MAP()

// CTaskBarSettingsDlg 消息处理程序


BOOL CTaskBarSettingsDlg::OnInitDialog()
{
    CTabDlg::OnInitDialog();
    SetRedraw(FALSE);

    // TODO:  在此添加额外的初始化

    //打开速度优化：显示内容分区(0)保持立即初始化；外观(1)与位置与交互(2)分区
    //延迟到首次显示时再初始化（EnsureSectionReady，由外层 LayoutPages 调用）。
    //未进入分区的控件保持模板默认状态，CollectSettings 会跳过它们并保留草稿原值。
    m_section_ready[0] = true;

    //初始化各控件状态（显示内容分区）
    //wchar_t buff[16];
    //swprintf_s(buff, L"%d", m_data.font_size);
    //SetDlgItemText(IDC_FONT_SIZE_EDIT1, buff);
    ((CButton*)GetDlgItem(IDC_SPEED_SHORT_MODE_CHECK))->SetCheck(m_data.speed_short_mode);
    ((CButton*)GetDlgItem(IDC_VALUE_RIGHT_ALIGN_CHECK))->SetCheck(m_data.value_right_align);
    ((CButton*)GetDlgItem(IDC_HORIZONTAL_ARRANGE_CHECK))->SetCheck(m_data.horizontal_arrange);
    ((CButton*)GetDlgItem(IDC_SEPARATE_VALUE_UNIT_CHECK))->SetCheck(m_data.separate_value_unit_with_space);

    if (m_data.unit_byte)
        ((CButton*)GetDlgItem(IDC_UNIT_BYTE_RADIO))->SetCheck(TRUE);
    else
        ((CButton*)GetDlgItem(IDC_UNIT_BIT_RADIO))->SetCheck(TRUE);

    IniUnitCombo();

    m_hide_unit_chk.SetCheck(m_data.hide_unit);
    if (m_data.speed_unit == SpeedUnit::AUTO)
    {
        m_hide_unit_chk.SetCheck(FALSE);
        m_data.hide_unit = false;
        m_hide_unit_chk.EnableWindow(FALSE);
    }
    ((CButton*)GetDlgItem(IDC_HIDE_PERCENTAGE_CHECK))->SetCheck(m_data.hide_percent);

    m_digit_number_combo.SetRedraw(FALSE);
    m_digit_number_combo.AddString(_T("3"));
    m_digit_number_combo.AddString(_T("4"));
    m_digit_number_combo.AddString(_T("5"));
    m_digit_number_combo.AddString(_T("6"));
    m_digit_number_combo.AddString(_T("7"));
    //越界值会让 SetCurSel 得到 -1，下拉框显示为空白；这里先钳到列表范围内
    m_digit_number_combo.SetRedraw(TRUE);
    m_digit_number_combo.SetCurSel(max(0, min(m_data.digits_number - 3, m_digit_number_combo.GetCount() - 1)));

    m_item_space_edit.SetRange(0, 32);
    m_item_space_edit.SetValue(m_data.item_space);
    CPulseBarDlg* pMainWnd = CPulseBarDlg::Instance();
    CTaskBarDlg* taskbar_dlg{ pMainWnd != nullptr ? pMainWnd->GetTaskbarWindow() : nullptr };
    m_vertical_margin_edit.SetRange(-10, 10);
    m_vertical_margin_edit.SetValue(m_data.vertical_margin);
    if (taskbar_dlg != nullptr)
        m_vertical_margin_edit.EnableWindow(taskbar_dlg->IsTasksbarOnTopOrBottom());

    //初始化内存显示方式下拉列表
    m_memory_display_combo.SetRedraw(FALSE);
    m_memory_display_combo.AddString(CCommon::LoadText(IDS_USAGE_PERCENTAGE));
    m_memory_display_combo.AddString(CCommon::LoadText(IDS_MEMORY_USED));
    m_memory_display_combo.AddString(CCommon::LoadText(IDS_MEMORY_AVAILABLE));
    m_memory_display_combo.SetRedraw(TRUE);
    //配置里的值理论上已由 PulseBar.cpp 夹紧；这里再钳一次，避免枚举越界时
    //SetCurSel 返回 -1 让下拉框显示空白，用户看不到当前选项。
    const int memory_display_index = max(0, min(static_cast<int>(m_data.memory_display),
        m_memory_display_combo.GetCount() - 1));
    m_memory_display_combo.SetCurSel(memory_display_index);

    ((CButton*)GetDlgItem(IDC_SET_ORDER_BUTTON))->SetIcon(theApp.GetMenuIcon(IDI_ITEM));

    SetRedraw(TRUE);
    m_loading = false;
    InitializeSections();

    return TRUE;  // return TRUE unless you set the focus to a control
                  // 异常: OCX 属性页应返回 FALSE
}

void CTaskBarSettingsDlg::EnsureSectionReady(int section)
{
    //外观/位置与交互分区的初始化延迟到首次显示该分区；重复调用直接返回。
    if (section < 0 || section >= 3 || m_section_ready[section])
        return;
    m_section_ready[section] = true;
    const bool was_loading = m_loading;
    m_loading = true;
    if (section == 1)
        EnsureAppearanceSection();
    else if (section == 2)
        EnsureInteractionSection();
    m_loading = was_loading;
}

void CTaskBarSettingsDlg::EnsureAppearanceSection()
{
    //外观分区的控件状态、预设菜单与主题填充。
    theApp.m_taskbar_default_style.LoadConfig();

    SetDlgItemText(IDC_FONT_NAME_EDIT1, m_data.font.name);
    m_font_size_edit.SetRange(5, 72);
    m_font_size_edit.SetValue(m_data.font.size);

    m_text_color_static.SetLinkCursor();
    m_back_color_static.SetLinkCursor();
    //m_trans_color_static.SetLinkCursor();
    m_status_bar_color_static.SetLinkCursor();
    DrawStaticColor();

#ifdef COMPILE_FOR_WINXP
    m_background_transparent_chk.EnableWindow(FALSE);
#endif // COMPILE_FOR_WINXP

    if (theApp.m_win_version.IsWindows7())
        m_background_transparent_chk.EnableWindow(FALSE);

    ((CButton*)GetDlgItem(IDC_SPECIFY_EACH_ITEM_COLOR_CHECK))->SetCheck(m_data.specify_each_item_color);
    m_background_transparent_chk.SetCheck(m_data.IsTaskbarTransparent());
    m_atuo_adapt_light_theme_chk.SetCheck(m_data.auto_adapt_light_theme);
    m_auto_set_back_color_chk.SetCheck(m_data.auto_set_background_color);
    m_auto_set_back_color_chk.EnableWindow(theApp.m_win_version.IsWindows8OrLater());

    if (theApp.m_win_version.GetMajorVersion() < 10)
    {
        m_data.auto_adapt_light_theme = false;
        m_atuo_adapt_light_theme_chk.EnableWindow(FALSE);
    }

    //初始化“预设方案”菜单
    m_default_style_menu.CreatePopupMenu();
    m_modify_default_style_menu.CreatePopupMenu();
    for (int i{ 0 }; i < TASKBAR_DEFAULT_STYLE_NUM; i++)
    {
        CString item_name;
        item_name.Format(_T("%s %d"), CCommon::LoadText(IDS_PRESET).GetString(), i + 1);
        m_default_style_menu.AppendMenu(MF_STRING | MF_ENABLED, ID_DEFAULT_STYLE1 + i, item_name);
        m_modify_default_style_menu.AppendMenu(MF_STRING | MF_ENABLED, ID_MODIFY_DEFAULT_STYLE1 + i, item_name);
    }
    m_default_style_menu.AppendMenu(MF_SEPARATOR);
    m_default_style_menu.AppendMenu(MF_POPUP | MF_STRING, reinterpret_cast<UINT_PTR>(m_modify_default_style_menu.GetSafeHmenu()), CCommon::LoadText(IDS_MODIFY_PRESET));

    const wchar_t* theme_keys[] = {
        L"TXT_TB_THEME_CLASSIC", L"TXT_TB_THEME_ROUND_PANEL", L"TXT_TB_THEME_GRADIENT_BAR",
        L"TXT_TB_THEME_GLASS_CYAN", L"TXT_TB_THEME_DARK_NEON", L"TXT_TB_THEME_LIGHT_SUN",
        L"TXT_TB_THEME_MIST_GOLD", L"TXT_TB_THEME_INDIGO_AURORA", L"TXT_TB_THEME_PILL_BLACK_GOLD",
        L"TXT_TB_THEME_CUSTOM"
    };
    m_theme_combo.SetRedraw(FALSE);
    for (int i = 0; i < 10; ++i)
    {
        int index = m_theme_combo.AddString(CCommon::LoadText(theme_keys[i]));
        m_theme_combo.SetItemData(index, static_cast<DWORD_PTR>(i == 9 ? -1 : i));
    }
    m_theme_combo.SetRedraw(TRUE);
    m_opacity_edit.SetRange(1, 100);
    m_opacity_edit.SetValue(m_data.tb_panel_opacity);
    m_corner_edit.SetRange(0, 32);
    m_corner_edit.SetValue(m_data.tb_corner_radius);
    SetDlgItemText(IDC_SETTINGS_IMAGE_PATH, m_data.tb_background_image.c_str());
    CheckDlgButton(IDC_SETTINGS_IMAGE_CHECK, m_data.tb_use_background_image);
    RefreshThemeControls();     //内部已调用 EnableControl，不再重复一遍
}

void CTaskBarSettingsDlg::EnsureInteractionSection()
{
    //位置与交互分区的控件状态与显示器枚举。
    const std::pair<const wchar_t*, DoubleClickAction> actions[] = {
        { IDS_OPEN_CONNECTION_DETIAL, DoubleClickAction::CONNECTION_INFO },
        { IDS_OPEN_HISTORICAL_TRAFFIC, DoubleClickAction::HISTORY_TRAFFIC },
        { IDS_SHOW_HIDE_CPU_MEMORY, DoubleClickAction::SHOW_MORE_INFO },
        { IDS_OPEN_OPTION_SETTINGS, DoubleClickAction::OPTIONS },
        { IDS_OPEN_TASK_MANAGER, DoubleClickAction::TASK_MANAGER },
        { IDS_SPECIFIC_APP, DoubleClickAction::SEPCIFIC_APP },
        { IDS_NONE, DoubleClickAction::NONE }
    };
    int selected_action = 6;
    m_double_click_combo.SetRedraw(FALSE);
    for (const auto& action : actions)
    {
        int index = m_double_click_combo.AddString(CCommon::LoadText(action.first));
        m_double_click_combo.SetItemData(index, static_cast<DWORD_PTR>(action.second));
        if (m_data.double_click_action == action.second)
            selected_action = index;
    }
    m_double_click_combo.SetRedraw(TRUE);
    m_double_click_combo.SetCurSel(selected_action);
    m_data.double_click_action = actions[selected_action].second;

    SetDlgItemText(IDC_EXE_PATH_EDIT, m_data.double_click_exe.c_str());

    if (m_data.cm_graph_type)
        CheckDlgButton(IDC_CM_GRAPH_PLOT_RADIO, TRUE);
    else
        CheckDlgButton(IDC_CM_GRAPH_BAR_RADIO, TRUE);
    CheckDlgButton(IDC_SHOW_DASHED_BOX, m_data.show_graph_dashed_box);
    CheckDlgButton(IDC_USAGE_GRAPH_FOLLOW_SYSTEM_CHECK, m_data.graph_color_following_system);

    ((CButton*)GetDlgItem(IDC_TASKBAR_WND_ON_LEFT_CHECK))->SetCheck(m_data.tbar_wnd_on_left);
    ((CButton*)GetDlgItem(IDC_SHOW_STATUS_BAR_CHECK))->SetCheck(m_data.show_status_bar);

    CheckDlgButton(IDC_SHOW_NET_SPEED_FIGURE_CHECK, m_data.show_netspeed_figure);
    m_net_speed_figure_max_val_edit.SetRange(1, 1024);
    m_net_speed_figure_max_val_edit.SetValue(m_data.netspeed_figure_max_value);
    m_net_speed_figure_max_val_unit_combo.SetRedraw(FALSE);
    m_net_speed_figure_max_val_unit_combo.AddString(_T("KB"));
    m_net_speed_figure_max_val_unit_combo.AddString(_T("MB"));
    m_net_speed_figure_max_val_unit_combo.SetRedraw(TRUE);
    //同上：0=KB、1=MB，越界会让下拉框显示空白
    const int net_unit_index = max(0, min(m_data.netspeed_figure_max_value_unit,
        m_net_speed_figure_max_val_unit_combo.GetCount() - 1));
    m_net_speed_figure_max_val_unit_combo.SetCurSel(net_unit_index);

    //获取副显示器的数量
    std::vector<HWND> secondary_displays;
    CTaskbarHelper::GetAllSecondaryDisplayTaskbar(secondary_displays);
    //初始化“显示任务栏窗口的显示器”下拉列表
    m_displays_combo.SetRedraw(FALSE);
    m_displays_combo.AddString(CCommon::LoadText(IDS_PRIMARY_DISPLAY));
    for (size_t i = 0; i < secondary_displays.size(); i++)
    {
        m_displays_combo.AddString(CCommon::LoadTextFormat(IDS_SECONDARY_DISPLAY, { i + 1 }));
    }
    m_displays_combo.SetRedraw(TRUE);
    if (!m_data.show_taskbar_wnd_in_secondary_display)
    {
        m_displays_combo.SetCurSel(0);
    }
    else
    {
        int combo_index = m_data.secondary_display_index + 1;
        int combo_item_count = m_displays_combo.GetCount();
        if (combo_index >= combo_item_count)
            combo_index = combo_item_count - 1;
        m_displays_combo.SetCurSel(combo_index);
    }

    m_hover_combo.SetRedraw(FALSE);
    m_hover_combo.AddString(CCommon::LoadText(IDS_NONE));
    m_hover_combo.AddString(CCommon::LoadText(L"TXT_SETTINGS_HOVER_SIMPLE"));
    m_hover_combo.AddString(CCommon::LoadText(L"TXT_SETTINGS_HOVER_PANEL"));
    m_hover_combo.SetRedraw(TRUE);
    m_hover_combo.SetCurSel(m_data.show_speed_graph_tip ? 2 : (m_data.show_tool_tip ? 1 : 0));

    //设置是否禁用D2D
    if (!CTaskBarDlgDrawCommonSupport::CheckSupport())
    {
        m_data.disable_d2d = true;
        //不支持时禁用选项
        EnableDlgCtrl(IDC_D2D_RADIO, false);
    }

    if (m_data.disable_d2d)
        CheckDlgButton(IDC_GDI_RADIO, true);
    else
        CheckDlgButton(IDC_D2D_RADIO, true);

    CheckDlgButton(IDC_ENABLE_COLOR_EMOJI_CHECK, m_data.enable_colorful_emoji);
}

void CTaskBarSettingsDlg::EnsureTooltip()
{
    if (m_toolTip.GetSafeHwnd() != nullptr)
        return;
    //工具提示延迟到第一次鼠标移动时创建，缩短打开设置的首屏耗时。
    m_toolTip.Create(this);
    m_toolTip.SetMaxTipWidth(theApp.DPI(300));
    m_toolTip.AddTool(GetDlgItem(IDC_SPEED_SHORT_MODE_CHECK), CCommon::LoadText(IDS_SPEED_SHORT_MODE_TIP));
    m_toolTip.AddTool(&m_atuo_adapt_light_theme_chk, CCommon::LoadText(IDS_AUTO_ADAPT_TIP_INFO));
    m_toolTip.AddTool(GetDlgItem(IDC_SHOW_STATUS_BAR_CHECK), CCommon::LoadText(IDS_SHOW_RESOURCE_USAGE_GRAPH_TIP));
    m_toolTip.AddTool(GetDlgItem(IDC_SHOW_NET_SPEED_FIGURE_CHECK), CCommon::LoadText(IDS_SHOW_NET_SPEED_GRAPH_TIP));
}


void CTaskBarSettingsDlg::OnBnClickedSetFontButton1()
{
    // TODO: 在此添加控件通知处理程序代码
    LOGFONT lf{};
    lf.lfHeight = FontSizeToLfHeight(m_data.font.size);
    lf.lfWeight = (m_data.font.bold ? FW_BOLD : FW_NORMAL);
    lf.lfItalic = m_data.font.italic;
    lf.lfUnderline = m_data.font.underline;
    lf.lfStrikeOut = m_data.font.strike_out;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_SWISS;
    //wcsncpy_s(lf.lfFaceName, m_data.font.name.GetString(), 32);
    CCommon::WStringCopy(lf.lfFaceName, 32, m_data.font.name.GetString());
    CCommon::NormalizeFont(lf);
    CFontDialog fontDlg(&lf);   //构造字体对话框，初始选择字体为之前字体
    fontDlg.m_cf.Flags |= CF_NOVERTFONTS;   //过滤掉以 "@" 开头的竖排字体
    if (IDOK == fontDlg.DoModal())     // 显示字体对话框
    {
        //获取字体信息
        m_data.font.name = fontDlg.GetFaceName();
        m_data.font.size = fontDlg.GetSize() / 10;
        m_data.font.bold = (fontDlg.IsBold() != FALSE);
        m_data.font.italic = (fontDlg.IsItalic() != FALSE);
        m_data.font.underline = (fontDlg.IsUnderline() != FALSE);
        m_data.font.strike_out = (fontDlg.IsStrikeOut() != FALSE);
        //将字体信息显示出来
        SetDlgItemText(IDC_FONT_NAME_EDIT1, m_data.font.name);
        wchar_t buff[16];
        swprintf_s(buff, L"%d", m_data.font.size);
        SetDlgItemText(IDC_FONT_SIZE_EDIT1, buff);
    }
}


void CTaskBarSettingsDlg::OnBnClickedCustomThemeBtn()
{
    CCustomThemeDlg themeDlg(m_data, this);
    if (themeDlg.DoModal() == IDOK)
        RefreshThemeControls();
}


void CTaskBarSettingsDlg::OnBnClickedTaskbarWndOnLeftCheck()
{
    // TODO: 在此添加控件通知处理程序代码
    m_data.tbar_wnd_on_left = (((CButton*)GetDlgItem(IDC_TASKBAR_WND_ON_LEFT_CHECK))->GetCheck() != 0);
    EnableControl();
}


void CTaskBarSettingsDlg::OnBnClickedSpeedShortModeCheck()
{
    // TODO: 在此添加控件通知处理程序代码
    m_data.speed_short_mode = (((CButton*)GetDlgItem(IDC_SPEED_SHORT_MODE_CHECK))->GetCheck() != 0);
}


BOOL CTaskBarSettingsDlg::PreTranslateMessage(MSG* pMsg)
{
    // TODO: 在此添加专用代码和/或调用基类
    if (pMsg->message == WM_MOUSEMOVE)
    {
        EnsureTooltip();
        m_toolTip.RelayEvent(pMsg);
    }
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN && GetFocus() != nullptr)
    {
        TCHAR class_name[32]{};
        ::GetClassName(GetFocus()->GetSafeHwnd(), class_name, 32);
        if (_tcsicmp(class_name, _T("ComboBox")) == 0 &&
            (GetFocus()->SendMessage(CB_GETDROPPEDSTATE) != 0))
            return CBaseDialog::PreTranslateMessage(pMsg);
    }

    return CTabDlg::PreTranslateMessage(pMsg);
}


void CTaskBarSettingsDlg::OnCbnSelchangeUnitCombo()
{
    // TODO: 在此添加控件通知处理程序代码
    //GetCurSel 返回 -1 表示没有选中项，此时保留原值，不要把 -1 存进枚举
    if (m_unit_combo.GetCurSel() < 0)
        return;
    m_data.speed_unit = static_cast<SpeedUnit>(m_unit_combo.GetCurSel());
    if (m_data.speed_unit == SpeedUnit::AUTO)
    {
        m_hide_unit_chk.SetCheck(FALSE);
        m_data.hide_unit = false;
        m_hide_unit_chk.EnableWindow(FALSE);
    }
    else
    {
        m_hide_unit_chk.EnableWindow(TRUE);
    }
}


void CTaskBarSettingsDlg::OnBnClickedHideUnitCheck()
{
    // TODO: 在此添加控件通知处理程序代码
    m_data.hide_unit = (m_hide_unit_chk.GetCheck() != 0);
}


bool CTaskBarSettingsDlg::CollectSettings()
{
    //外观分区可能尚未进入（EnsureSectionReady 未运行，控件还是模板默认状态）：
    //跳过对应读取并保留草稿原值，不能用未初始化的控件状态覆盖用户配置。
    if (m_section_ready[1])
    {
        int font_size = m_font_size_edit.GetValue();
        if (font_size > MAX_FONT_SIZE || font_size < MIN_FONT_SIZE)
        {
            CString info;
            info.Format(CCommon::LoadText(IDS_FONT_SIZE_WARNING), MIN_FONT_SIZE, MAX_FONT_SIZE);
            MessageBox(info, nullptr, MB_OK | MB_ICONWARNING);
            return false;
        }
        m_data.font.size = font_size;
        GetDlgItemText(IDC_FONT_NAME_EDIT1, m_data.font.name);
        m_data.SetTaskabrTransparent(m_background_transparent_chk.GetCheck() != 0);
        m_data.tb_panel_opacity = max(1, min(m_opacity_edit.GetValue(), 100));
        m_data.tb_corner_radius = max(0, min(m_corner_edit.GetValue(), 32));
    }
    //位置与交互分区未进入时控件仍为模板默认，跳过；进入后手输/粘贴的路径
    //必须在此读回，否则“浏览”按钮以外的输入在确定后会静默丢失。
    if (m_section_ready[2])
    {
        CString exe_path;
        GetDlgItemText(IDC_EXE_PATH_EDIT, exe_path);
        m_data.double_click_exe = exe_path.GetString();
    }
    m_data.item_space = m_item_space_edit.GetValue();
    m_data.vertical_margin = m_vertical_margin_edit.GetValue();
    m_data.ValidItemSpace();
    m_data.ValidVerticalMargin();
    //交互分区未进入时两个下拉框为空，OnHoverChanged/OnCbnSelchangeDoubleClickCombo
    //在 GetCurSel 为 -1 时不会写草稿，保留原值。
    OnHoverChanged();
    OnCbnSelchangeDoubleClickCombo();
    return true;
}

void CTaskBarSettingsDlg::OnOK()
{
    if (CollectSettings())
        CTabDlg::OnOK();
}

void CTaskBarSettingsDlg::OnSettingsApplied()
{
    for (auto& preset : m_pending_presets)
        theApp.m_taskbar_default_style.ModifyDefaultStyle(preset.first, preset.second);
    theApp.m_taskbar_default_style.SaveConfig();
    m_pending_presets.clear();
    m_style_modified = false;
    m_data = theApp.m_taskbar_data;
    if (::IsWindow(GetSafeHwnd()))
    {
        m_background_transparent_chk.SetCheck(m_data.IsTaskbarTransparent());
        m_auto_set_back_color_chk.SetCheck(m_data.auto_set_background_color);
        DrawStaticColor();
        RefreshThemeControls();
    }
}

void CTaskBarSettingsDlg::SaveColorSettingToDefaultStyle()
{
    //如果开启了自动适应Windows10深色/浅色模式功能时，自动将当前配置保存到对应预设
    if (m_data.auto_save_taskbar_color_settings_to_preset && m_data.auto_adapt_light_theme && IsStyleModified())
    {
        int default_style_saved{ CWindowsSettingHelper::IsWindows10LightTheme() ? m_data.light_default_style : m_data.dark_default_style };
        ModifyDefaultStyle(default_style_saved);
    }
}

void CTaskBarSettingsDlg::OnBnClickedValueRightAlignCheck()
{
    // TODO: 在此添加控件通知处理程序代码
    m_data.value_right_align = (((CButton*)GetDlgItem(IDC_VALUE_RIGHT_ALIGN_CHECK))->GetCheck() != 0);
}


void CTaskBarSettingsDlg::OnBnClickedHidePercentageCheck()
{
    // TODO: 在此添加控件通知处理程序代码
    m_data.hide_percent = (((CButton*)GetDlgItem(IDC_HIDE_PERCENTAGE_CHECK))->GetCheck() != 0);
}


afx_msg LRESULT CTaskBarSettingsDlg::OnStaticClicked(WPARAM wParam, LPARAM lParam)
{
    switch (::GetDlgCtrlID(((CWnd*)wParam)->m_hWnd))
    {
    case IDC_TEXT_COLOR_STATIC1:        //点击“文本颜色”时
    {
        //设置文本颜色
        if (m_data.specify_each_item_color)
        {
            CTaskbarColorDlg colorDlg(m_data.text_colors);
            if (colorDlg.DoModal() == IDOK)
            {
                m_data.text_colors = colorDlg.GetColors();
                DrawStaticColor();
                m_style_modified = true;
            }
        }
        else if (!m_data.text_colors.empty())
        {
            CMFCColorDialogEx colorDlg(m_data.text_colors.begin()->second.label, 0, this);
            if (colorDlg.DoModal() == IDOK)
            {
                m_data.text_colors.begin()->second.label = colorDlg.GetColor();
                if (m_data.back_color == m_data.text_colors.begin()->second.label)
                    MessageBox(CCommon::LoadText(IDS_SAME_TEXT_BACK_COLOR_WARNING), NULL, MB_ICONWARNING);
                DrawStaticColor();
                m_style_modified = true;
            }
        }
        break;
    }
    case IDC_TEXT_COLOR_STATIC2:        //点击“背景颜色”时
    {
        //设置背景颜色
        CMFCColorDialogEx colorDlg(m_data.back_color, 0, this);
        if (colorDlg.DoModal() == IDOK)
        {
            bool background_transparent = m_data.IsTaskbarTransparent();
            m_data.back_color = colorDlg.GetColor();
            //与 STATIC1 一致：text_colors 可能为空，对 begin() 解引用会崩溃
            if (!m_data.text_colors.empty() && m_data.back_color == m_data.text_colors.begin()->second.label)
                MessageBox(CCommon::LoadText(IDS_SAME_BACK_TEXT_COLOR_WARNING), NULL, MB_ICONWARNING);
            if (background_transparent)
            {
                CCommon::TransparentColorConvert(m_data.back_color);
                //如果当前设置了背景透明，则更改了背景色后同时将透明色设置成和背景色一样的颜色，以保持背景透明
                m_data.transparent_color = m_data.back_color;
            }
            DrawStaticColor();
            m_style_modified = true;
        }
        break;
    }
    //case IDC_TRANSPARENT_COLOR_STATIC:        //点击“透明色”时
    //{
    //  CMFCColorDialogEx colorDlg(m_data.transparent_color, 0, this);
    //  if (colorDlg.DoModal() == IDOK)
    //  {
    //      m_data.transparent_color = colorDlg.GetColor();
    //      DrawStaticColor();
    //  }
    //  break;
    //}
    case IDC_TEXT_COLOR_STATIC3:        //点击“状态条颜色”时
    {
        CMFCColorDialogEx colorDlg(m_data.status_bar_color, 0, this);
        if (colorDlg.DoModal() == IDOK)
        {
            m_data.status_bar_color = colorDlg.GetColor();

            //更改了资源占用图的颜色后，去掉“跟随Windows主题颜色”的勾选
            CheckDlgButton(IDC_USAGE_GRAPH_FOLLOW_SYSTEM_CHECK, FALSE);
            m_data.graph_color_following_system = false;

            DrawStaticColor();
            m_style_modified = true;
        }
        break;
    }
    default:
        break;
    }
    return 0;
}


void CTaskBarSettingsDlg::OnBnClickedSpecifyEachItemColorCheck()
{
    // TODO: 在此添加控件通知处理程序代码
    m_data.specify_each_item_color = (((CButton*)GetDlgItem(IDC_SPECIFY_EACH_ITEM_COLOR_CHECK))->GetCheck() != 0);
    EnableControl();
    DrawStaticColor();
    m_style_modified = true;
}


void CTaskBarSettingsDlg::OnCbnSelchangeDoubleClickCombo()
{
    // TODO: 在此添加控件通知处理程序代码
    int index = m_double_click_combo.GetCurSel();
    if (index >= 0)
        m_data.double_click_action = static_cast<DoubleClickAction>(m_double_click_combo.GetItemData(index));
    EnableControl();
}


void CTaskBarSettingsDlg::OnBnClickedHorizontalArrangeCheck()
{
    // TODO: 在此添加控件通知处理程序代码
    m_data.horizontal_arrange = (((CButton*)GetDlgItem(IDC_HORIZONTAL_ARRANGE_CHECK))->GetCheck() != 0);
}


void CTaskBarSettingsDlg::OnBnClickedShowStatusBarCheck()
{
    // TODO: 在此添加控件通知处理程序代码
    m_data.show_status_bar = (((CButton*)GetDlgItem(IDC_SHOW_STATUS_BAR_CHECK))->GetCheck() != 0);
    EnableControl();
}


void CTaskBarSettingsDlg::OnBnClickedSeparateValueUnitCheck()
{
    // TODO: 在此添加控件通知处理程序代码
    m_data.separate_value_unit_with_space = (((CButton*)GetDlgItem(IDC_SEPARATE_VALUE_UNIT_CHECK))->GetCheck() != 0);
}


void CTaskBarSettingsDlg::OnBnClickedUnitByteRadio()
{
    // TODO: 在此添加控件通知处理程序代码
    m_data.unit_byte = true;
    IniUnitCombo();
}


void CTaskBarSettingsDlg::OnBnClickedUnitBitRadio()
{
    // TODO: 在此添加控件通知处理程序代码
    m_data.unit_byte = false;
    IniUnitCombo();
}


void CTaskBarSettingsDlg::OnBnClickedDefaultStyleButton()
{
    // TODO: 在此添加控件通知处理程序代码
    CWnd* pBtn = GetDlgItem(IDC_DEFAULT_STYLE_BUTTON);
    CPoint point;
    if (pBtn != nullptr)
    {
        CRect rect;
        pBtn->GetWindowRect(rect);
        point.x = rect.left;
        point.y = rect.bottom;
        m_default_style_menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
    }
}


void CTaskBarSettingsDlg::OnDestroy()
{
    CTabDlg::OnDestroy();

    //预设只在外层设置提交后保存。
}


void CTaskBarSettingsDlg::OnBnClickedBrowseButton()
{
    // TODO: 在此添加控件通知处理程序代码
    CString szFilter = CCommon::LoadText(IDS_EXE_FILTER);
    CFileDialogEx fileDlg(TRUE, NULL, szFilter);
    if (IDOK == fileDlg.DoModal())
    {
        m_data.double_click_exe = fileDlg.GetPathName();
        SetDlgItemText(IDC_EXE_PATH_EDIT, m_data.double_click_exe.c_str());
    }
}

void CTaskBarSettingsDlg::OnBnClickedCMGraphPLOTRadio()
{
    m_data.cm_graph_type = true;
}

void CTaskBarSettingsDlg::OnBnClickedCMGraphBarRadio()
{
    m_data.cm_graph_type = false;
}


void CTaskBarSettingsDlg::OnBnClickedBackgroundTransparentCheck()
{
    // TODO: 在此添加控件通知处理程序代码
    bool checked = (m_background_transparent_chk.GetCheck() != 0);
    m_data.SetTaskabrTransparent(checked);
    m_style_modified = true;
}


void CTaskBarSettingsDlg::OnBnClickedAutoAdaptSettingsButton()
{
    // TODO: 在此添加控件通知处理程序代码
    CAutoAdaptSettingsDlg dlg(m_data);
    dlg.DoModal();
}


void CTaskBarSettingsDlg::OnBnClickedAutoAdaptLightThemeCheck()
{
    // TODO: 在此添加控件通知处理程序代码
    m_data.auto_adapt_light_theme = (m_atuo_adapt_light_theme_chk.GetCheck() != 0);
    EnableControl();
}


void CTaskBarSettingsDlg::OnBnClickedAutoSetBackColorCheck()
{
    // TODO: 在此添加控件通知处理程序代码
    m_data.auto_set_background_color = (m_auto_set_back_color_chk.GetCheck() != 0);
}


void CTaskBarSettingsDlg::OnCbnSelchangeMemoryDisplayCombo()
{
    // TODO: 在此添加控件通知处理程序代码
    //没有选中项时保留原值，避免把 -1 存进枚举
    if (m_memory_display_combo.GetCurSel() < 0)
        return;
    m_data.memory_display = static_cast<MemoryDisplay>(m_memory_display_combo.GetCurSel());
}


void CTaskBarSettingsDlg::OnBnClickedShowDashedBox()
{
    // TODO: 在此添加控件通知处理程序代码
    m_data.show_graph_dashed_box = (IsDlgButtonChecked(IDC_SHOW_DASHED_BOX) != 0);
}


void CTaskBarSettingsDlg::OnBnClickedSetOrderButton()
{
    // TODO: 在此添加控件通知处理程序代码
    CSetItemOrderDlg dlg(this);
    dlg.SetAppearance(m_data.disp_str, m_data.text_colors);
    dlg.SetItemOrder(m_data.item_order.GetItemOrderConst());
    dlg.SetDisplayItem(m_data.display_item);
    dlg.SetPluginDisplayItem(m_data.plugin_display_item);
    if (dlg.DoModal() == IDOK)
    {
        m_data.item_order.SetOrder(dlg.GetItemOrder());
        m_data.display_item = dlg.GetDisplayItem();
        m_data.plugin_display_item = dlg.GetPluginDisplayItem();
        m_data.disp_str = dlg.GetDisplayTexts();
        m_data.text_colors = dlg.GetColors();
        if (dlg.ColorsModified() && !m_data.specify_each_item_color)
        {
            //用户刚设置了逐项颜色：自动启用，避免“设置了却没有效果”
            m_data.specify_each_item_color = true;
            CheckDlgButton(IDC_SPECIFY_EACH_ITEM_COLOR_CHECK, TRUE);
            EnableControl();
        }
        m_style_modified = true;
        DrawStaticColor();
    }
}


void CTaskBarSettingsDlg::OnEnChangeItemSpaceEdit()
{
    // TODO:  如果该控件是 RICHEDIT 控件，它将不
    // 发送此通知，除非重写 CTabDlg::OnInitDialog()
    // 函数并调用 CRichEditCtrl().SetEventMask()，
    // 同时将 ENM_CHANGE 标志“或”运算到掩码中。

    // TODO:  在此添加控件通知处理程序代码
    m_data.item_space = m_item_space_edit.GetValue();
    m_data.ValidItemSpace();
}

void CTaskBarSettingsDlg::OnEnChangeVerticalMarginEdit()
{
    m_data.vertical_margin = m_vertical_margin_edit.GetValue();
    m_data.ValidVerticalMargin();
}

BOOL CTaskBarSettingsDlg::OnCommand(WPARAM wParam, LPARAM lParam)
{
    // TODO: 在此添加专用代码和/或调用基类
    UINT cmd = LOWORD(wParam);

    if (cmd >= ID_DEFAULT_STYLE1 && cmd < ID_DEFAULT_STYLE_MAX)
    {
        int default_style = cmd - ID_DEFAULT_STYLE1;
        ApplyDefaultStyle(default_style);
    }
    if (cmd >= ID_MODIFY_DEFAULT_STYLE1 && cmd < ID_MODIFY_DEFAULT_STYLE_MAX)
    {
        int default_style = cmd - ID_MODIFY_DEFAULT_STYLE1;
        if (MessageBox(CCommon::LoadTextFormat(IDS_SAVE_DEFAULT_STYLE_INQUIRY, { default_style + 1 }), NULL, MB_ICONQUESTION | MB_YESNO) == IDYES)
        {
            ModifyDefaultStyle(default_style);
        }
    }

    return CTabDlg::OnCommand(wParam, lParam);
}


void CTaskBarSettingsDlg::OnBnClickedShowNetSpeedFigureCheck()
{
    m_data.show_netspeed_figure = (IsDlgButtonChecked(IDC_SHOW_NET_SPEED_FIGURE_CHECK) != 0);
    EnableControl();
}


void CTaskBarSettingsDlg::OnCbnSelchangeNetSpeedFigureMaxValueUnitCombo()
{
    if (m_net_speed_figure_max_val_unit_combo.GetCurSel() < 0)
        return;
    m_data.netspeed_figure_max_value_unit = m_net_speed_figure_max_val_unit_combo.GetCurSel();
}


void CTaskBarSettingsDlg::OnEnChangeNetSpeedFigureMaxValueEdit()
{
    m_data.netspeed_figure_max_value = m_net_speed_figure_max_val_edit.GetValue();
}


void CTaskBarSettingsDlg::OnBnClickedGdiRadio()
{
    m_data.disable_d2d = true;
    EnableControl();
}


void CTaskBarSettingsDlg::OnBnClickedD2dRadio()
{
    m_data.disable_d2d = false;
    EnableControl();
}


void CTaskBarSettingsDlg::OnBnClickedEnableColorEmojiCheck()
{
    m_data.enable_colorful_emoji = (IsDlgButtonChecked(IDC_ENABLE_COLOR_EMOJI_CHECK) != FALSE);
}


void CTaskBarSettingsDlg::OnCbnSelchangeDigitNumberCombo()
{
    //获取数据位数的设置
    //没有选中项时保留原值，避免写入 2（-1 + 3）
    if (m_digit_number_combo.GetCurSel() < 0)
        return;
    m_data.digits_number = m_digit_number_combo.GetCurSel() + 3;
}


void CTaskBarSettingsDlg::OnBnClickedWin11SettingsButton()
{
    CWin11TaskbarSettingDlg dlg(m_data);
    dlg.DoModal();
}



void CTaskBarSettingsDlg::OnCbnSelchangeDisplayToShowTaskbarWndCombo()
{
    
    int combo_index = m_displays_combo.GetCurSel();
    if (combo_index == 0)
    {
        m_data.show_taskbar_wnd_in_secondary_display = false;
    }
    else
    {
        m_data.show_taskbar_wnd_in_secondary_display = true;
        m_data.secondary_display_index = combo_index - 1;

    }
}


void CTaskBarSettingsDlg::OnBnClickedUsageGraphFollowSystemCheck()
{
    m_data.graph_color_following_system = (IsDlgButtonChecked(IDC_USAGE_GRAPH_FOLLOW_SYSTEM_CHECK) != FALSE);
    DrawStaticColor();
}


void CTaskBarSettingsDlg::OnEnChangeFontSizeEdit1()
{
    if (!m_loading)
        m_data.font.size = m_font_size_edit.GetValue();
}

//IDD_TASKBAR_SETTINGS_DIALOG 模板里每个控件的原始位置与分区，按 .rc 中的声明顺序排列。
//字段含义：x, y, cx, cy(DLU)，默认可见，所属分区（见 ShowSection 的分区划分）。
//分区重排时以这份数据为基准，所以它必须与 .rc 一一对应；改模板时要同步改这里。
static const TemplateControl k_taskbar_template_controls[] = {
    { 8, 5, 314, 58, true, 0 },  // IDC_STATIC
    { 18, 16, 294, 16, true, 0 },  // IDC_STATIC
    { 18, 36, 120, 20, true, 0 },  // IDC_SET_ORDER_BUTTON
    { 8, 70, 314, 116, true, 0 },  // IDC_STATIC
    { 18, 84, 140, 12, true, 0 },  // IDC_HORIZONTAL_ARRANGE_CHECK
    { 180, 84, 140, 12, true, 0 },  // IDC_VALUE_RIGHT_ALIGN_CHECK
    { 18, 100, 292, 12, true, 0 },  // IDC_SPEED_SHORT_MODE_CHECK
    { 18, 120, 90, 10, true, 0 },  // TXT_MEMORY_DISPLAY_MODE
    { 112, 117, 170, 70, true, 0 },  // IDC_MEMORY_DISPLAY_COMBO
    { 18, 140, 90, 10, true, 0 },  // IDC_NET_SPEED_WIDTH_STATIC
    { 112, 137, 54, 70, true, 0 },  // IDC_DIGIT_NUMBER_COMBO
    { 174, 140, 60, 10, true, 0 },  // IDC_CHARACTOR_STATIC
    { 18, 162, 58, 10, true, 0 },  // IDC_ITEM_SPACING_STATIC
    { 86, 159, 44, 14, true, 0 },  // IDC_ITEM_SPACE_EDIT
    { 136, 162, 30, 10, true, 0 },  // IDC_PIXELS_STATIC
    { 170, 162, 58, 10, true, 0 },  // IDC_VERTICAL_MARGIN_STATIC
    { 236, 159, 44, 14, true, 0 },  // IDC_VERTICAL_MARGIN_EDIT
    { 284, 162, 30, 10, true, 0 },  // IDC_PIXELS_STATIC1
    { 8, 194, 314, 94, true, 0 },  // IDC_STATIC
    { 18, 208, 140, 12, true, 0 },  // IDC_UNIT_BYTE_RADIO
    { 180, 208, 140, 12, true, 0 },  // IDC_UNIT_BIT_RADIO
    { 18, 230, 90, 10, true, 0 },  // IDC_STATIC
    { 112, 227, 170, 70, true, 0 },  // IDC_UNIT_COMBO
    { 18, 248, 140, 12, true, 0 },  // IDC_HIDE_UNIT_CHECK
    { 180, 248, 140, 12, true, 0 },  // IDC_HIDE_PERCENTAGE_CHECK
    { 18, 266, 292, 12, true, 0 },  // IDC_SEPARATE_VALUE_UNIT_CHECK
    { 8, 405, 314, 92, true, 1 },  // IDC_STATIC
    { 18, 420, 88, 10, true, 1 },  // IDC_STATIC
    { 112, 416, 198, 190, true, 1 },  // IDC_SETTINGS_THEME_COMBO
    { 18, 440, 292, 26, true, 1 },  // IDC_SETTINGS_THEME_PREVIEW
    { 18, 470, 84, 16, true, 1 },  // IDC_CUSTOM_THEME_BTN
    { 8, 503, 314, 130, true, 1 },  // IDC_COLOR_FONT_GROUP
    { 18, 519, 40, 10, true, 1 },  // IDC_FONT_STATIC
    { 64, 517, 152, 14, true, 1 },  // IDC_FONT_NAME_EDIT1
    { 224, 518, 34, 12, true, 1 },  // IDC_FONT_SIZE_EDIT1
    { 264, 516, 46, 16, true, 1 },  // IDC_SET_FONT_BUTTON1
    { 18, 519, 40, 10, false, 1 },  // IDC_FONT_SIZE_STATIC
    { 18, 540, 84, 10, true, 1 },  // IDC_TXT_COLOR_STATIC
    { 112, 537, 40, 17, true, 1 },  // IDC_TEXT_COLOR_STATIC1
    { 164, 539, 146, 12, true, 1 },  // IDC_SPECIFY_EACH_ITEM_COLOR_CHECK
    { 18, 562, 84, 10, true, 1 },  // IDC_BACK_COLOR_STATIC
    { 112, 558, 40, 17, true, 1 },  // IDC_TEXT_COLOR_STATIC2
    { 164, 561, 146, 12, true, 1 },  // IDC_BACKGROUND_TRANSPARENT_CHECK
    { 18, 579, 292, 12, true, 1 },  // IDC_AUTO_SET_BACK_COLOR_CHECK
    { 18, 595, 190, 12, true, 1 },  // IDC_AUTO_ADAPT_LIGHT_THEME_CHECK
    { 226, 593, 84, 16, true, 1 },  // IDC_AUTO_ADAPT_SETTINGS_BUTTON
    { 18, 612, 84, 15, true, 1 },  // IDC_DEFAULT_STYLE_BUTTON
    { 8, 638, 314, 86, true, 1 },  // IDC_STATIC
    { 18, 651, 186, 12, true, 1 },  // IDC_SETTINGS_IMAGE_CHECK
    { 210, 648, 100, 18, true, 1 },  // IDC_SETTINGS_IMAGE_BUTTON
    { 18, 670, 292, 14, true, 1 },  // IDC_SETTINGS_IMAGE_PATH
    { 18, 692, 90, 10, true, 1 },  // IDC_STATIC
    { 112, 690, 46, 14, true, 1 },  // IDC_SETTINGS_OPACITY_EDIT
    { 174, 692, 78, 10, true, 1 },  // IDC_STATIC
    { 264, 690, 46, 14, true, 1 },  // IDC_SETTINGS_CORNER_EDIT
    { 18, 706, 292, 14, true, 1 },  // IDC_STATIC
    { 8, 823, 314, 80, true, 2 },  // IDC_STATIC
    { 18, 838, 90, 10, true, 2 },  // IDC_DISPLAY_TO_SHOW_TASKBAR_WND_STATIC
    { 112, 834, 198, 80, true, 2 },  // IDC_DISPLAY_TO_SHOW_TASKBAR_WND_COMBO
    { 18, 859, 292, 12, true, 2 },  // IDC_TASKBAR_WND_ON_LEFT_CHECK
    { 18, 874, 140, 18, true, 2 },  // IDC_WIN11_SETTINGS_BUTTON
    { 8, 905, 314, 95, true, 2 },  // IDC_STATIC
    { 18, 920, 90, 10, true, 2 },  // IDC_STATIC
    { 112, 916, 198, 80, true, 2 },  // IDC_SETTINGS_HOVER_COMBO
    { 18, 941, 90, 10, true, 2 },  // IDC_DOUBLE_CLICK_ACTION_STATIC
    { 112, 938, 198, 140, true, 2 },  // IDC_DOUBLE_CLICK_COMBO
    { 18, 963, 90, 10, true, 2 },  // IDC_EXE_PATH_STATIC
    { 112, 961, 146, 14, true, 2 },  // IDC_EXE_PATH_EDIT
    { 264, 960, 46, 16, true, 2 },  // IDC_BROWSE_BUTTON
    { 8, 1007, 314, 128, true, 2 },  // IDC_STATIC
    { 18, 1021, 292, 12, true, 2 },  // IDC_SHOW_STATUS_BAR_CHECK
    { 18, 1041, 292, 12, true, 2 },  // IDC_SHOW_NET_SPEED_FIGURE_CHECK
    { 18, 1063, 90, 10, true, 2 },  // IDC_NET_SPEED_MAX_VALUE_STATIC
    { 112, 1061, 54, 14, true, 2 },  // IDC_NET_SPEED_FIGURE_MAX_VALUE_EDIT
    { 174, 1060, 60, 70, true, 2 },  // IDC_NET_SPEED_FIGURE_MAX_VALUE_UNIT_COMBO
    { 18, 1084, 90, 10, true, 2 },  // IDC_USAGE_GRAPH_COLOR_STATIC
    { 112, 1081, 40, 17, true, 2 },  // IDC_TEXT_COLOR_STATIC3
    { 164, 1084, 146, 12, true, 2 },  // IDC_USAGE_GRAPH_FOLLOW_SYSTEM_CHECK
    { 18, 1106, 90, 10, true, 2 },  // IDC_GRAPH_DISPLAY_MODE_STATIC
    { 112, 1103, 90, 12, true, 2 },  // IDC_CM_GRAPH_BAR_RADIO
    { 214, 1103, 96, 12, true, 2 },  // IDC_CM_GRAPH_PLOT_RADIO
    { 18, 1121, 292, 12, true, 2 },  // IDC_SHOW_DASHED_BOX
    { 8, 1139, 314, 20, true, 2 },  // IDC_SETTINGS_RENDERING_BUTTON
    { 8, 1162, 314, 57, true, 2 },  // IDC_SETTINGS_RENDERING_GROUP
    { 18, 1177, 130, 12, true, 2 },  // IDC_GDI_RADIO
    { 164, 1177, 146, 12, true, 2 },  // IDC_D2D_RADIO
    { 18, 1198, 292, 12, true, 2 },  // IDC_ENABLE_COLOR_EMOJI_CHECK
};


void CTaskBarSettingsDlg::InitializeSections()
{
    //按资源模板表建立控件基准位置。此前用运行时窗口坐标除以分区高度来推断
    //所属分区，一旦控件已被移动过就会永久归错类，导致整页控件拉散、重叠。
    m_section_controls.clear();
    int index = 0;
    const int count = static_cast<int>(sizeof(k_taskbar_template_controls) / sizeof(k_taskbar_template_controls[0]));
    for (CWnd* ctrl = GetWindow(GW_CHILD); ctrl != nullptr && index < count; ctrl = ctrl->GetWindow(GW_HWNDNEXT), ++index)
    {
        const TemplateControl& tc = k_taskbar_template_controls[index];
        SectionControl item;
        item.hwnd = ctrl->GetSafeHwnd();
        item.section = tc.section;
        //类名与控件 ID 一次性记入表项，ShowSection 每次重排不再逐控件查询
        wchar_t cls[32]{};
        ::GetClassName(item.hwnd, cls, 32);
        item.is_edit = _wcsicmp(cls, L"Edit") == 0;
        item.id = ::GetDlgCtrlID(item.hwnd);
        CRect rect(tc.x, tc.y, tc.x + tc.cx, tc.y + tc.cy);
        MapDialogRect(rect);
        const int section_offset_dlu = tc.section == 2 ? 818 : 400 * tc.section;
        CRect section_offset(0, 0, 0, section_offset_dlu);
        MapDialogRect(section_offset);
        rect.OffsetRect(0, -section_offset.bottom);
        item.rect = rect;
        item.visible = tc.visible;
        m_section_controls.push_back(item);
    }
}

void CTaskBarSettingsDlg::ShowSection(int section, int height)
{
    //每次都按模板中记录的矩形重新锚定所有控件，而不是只在切换分区时排布。
    //窗口尺寸、DPI 或滚动状态变化后，控件位置都会被校正回基准位置，
    //避免相对位移累积导致控件拉散和互相重叠。
    if (m_section != section)
    {
        //切换页面时回到顶部，避免带着上一页的滚动位置打开
        m_last_pos = 0;
        SetScrollbarInfo(height, height);
    }
    m_section = section;
    //分组框在子窗口移动后容易留下旧边框和旧文字：先暂停绘制，
    //统一移动/显隐所有控件，再整页擦除并立即重绘。
    SetRedraw(FALSE);
    //主题模式下经典配色行被隐藏，分组框和下方内容要跟着收起，避免留下大片空白。
    CRect collapse_dlu(0, 0, 0, 88);          //130 - 42 DLU
    MapDialogRect(collapse_dlu);
    CRect bg_top_dlu(0, 0, 0, 238);           //“背景与形状”分组在分区内的起点
    MapDialogRect(bg_top_dlu);
    const bool collapse = (section == 1 && m_data.tb_theme != 0);
    for (const auto& item : m_section_controls)
    {
        int top = item.rect.top;
        int item_height = item.rect.Height();
        if (item.is_edit)
            item_height = EditControlHeight();
        if (collapse && item.rect.top >= bg_top_dlu.bottom)
            top -= collapse_dlu.bottom;
        if (collapse && item.id == IDC_COLOR_FONT_GROUP)
            item_height -= collapse_dlu.bottom;
        ::SetWindowPos(item.hwnd, nullptr, item.rect.left, top,
            item.rect.Width(), item_height, SWP_NOZORDER | SWP_NOACTIVATE);
        bool show = item.section == section && item.visible;
        //ShowSection 会按模板重新显示控件，这里要同步隐藏空的背景图路径框
        if (item.id == IDC_SETTINGS_IMAGE_PATH && m_data.tb_background_image.empty())
            show = false;
        if (collapse)
        {
            //主题模式下这些经典配色项不适用，必须在这里一并隐藏，
            //否则本函数按模板重新显示控件时会把它们又显示出来。
            switch (item.id)
            {
            case IDC_TXT_COLOR_STATIC:
            case IDC_TEXT_COLOR_STATIC1:
            case IDC_SPECIFY_EACH_ITEM_COLOR_CHECK:
            case IDC_BACK_COLOR_STATIC:
            case IDC_TEXT_COLOR_STATIC2:
            case IDC_BACKGROUND_TRANSPARENT_CHECK:
            case IDC_AUTO_SET_BACK_COLOR_CHECK:
            case IDC_AUTO_ADAPT_LIGHT_THEME_CHECK:
            case IDC_AUTO_ADAPT_SETTINGS_BUTTON:
            case IDC_DEFAULT_STYLE_BUTTON:
                show = false;
                break;
            default:
                break;
            }
        }
        ::ShowWindow(item.hwnd, show ? SW_SHOW : SW_HIDE);
    }
    SetRedraw(TRUE);
    RepaintPageSwitch();
    EnableControl();
    CRect content(0, 0, 0, section == 2 ? (m_rendering_visible ? 420 : 350) : 300);
    MapDialogRect(content);
    SetScrollbarInfo(height, content.bottom);
    SetControlMouseWheelEnable(!IsScrollBarVisible());
    Invalidate();
}

void CTaskBarSettingsDlg::OnRenderingSettings()
{
    m_rendering_visible = !m_rendering_visible;
    SetDlgItemText(IDC_SETTINGS_RENDERING_BUTTON, CCommon::LoadText(m_rendering_visible
        ? L"TXT_SETTINGS_RENDERING_HIDE" : L"TXT_SETTINGS_RENDERING_SHOW"));
    CRect rect;
    GetClientRect(rect);
    ShowSection(m_section, rect.Height());
}

void CTaskBarSettingsDlg::RefreshSectionVisibility()
{
    if (m_section < 0)
        return;
    const bool show_exe = m_section == 2 && m_data.double_click_action == DoubleClickAction::SEPCIFIC_APP;
    ShowDlgCtrl(IDC_EXE_PATH_STATIC, show_exe);
    ShowDlgCtrl(IDC_EXE_PATH_EDIT, show_exe);
    ShowDlgCtrl(IDC_BROWSE_BUTTON, show_exe);
    const int rendering_controls[] = { IDC_SETTINGS_RENDERING_GROUP, IDC_GDI_RADIO,
        IDC_D2D_RADIO, IDC_ENABLE_COLOR_EMOJI_CHECK };
    for (int id : rendering_controls)
        ShowDlgCtrl(id, m_section == 2 && m_rendering_visible);
}

void CTaskBarSettingsDlg::RefreshThemeControls()
{
    const bool was_loading = m_loading;
    m_loading = true;
    for (int i = 0; i < m_theme_combo.GetCount(); ++i)
    {
        if (static_cast<int>(m_theme_combo.GetItemData(i)) == m_data.tb_theme)
            m_theme_combo.SetCurSel(i);
    }
    m_opacity_edit.SetValue(m_data.tb_panel_opacity);
    m_corner_edit.SetValue(m_data.tb_corner_radius);
    const bool panel = m_data.tb_theme != 0;
    EnableDlgCtrl(IDC_SETTINGS_OPACITY_EDIT, panel && !m_data.tb_use_background_image);
    EnableDlgCtrl(IDC_SETTINGS_CORNER_EDIT, panel && !m_data.tb_use_background_image);
    EnableDlgCtrl(IDC_SETTINGS_IMAGE_CHECK, panel);
    EnableDlgCtrl(IDC_SETTINGS_IMAGE_BUTTON, panel);
    CheckDlgButton(IDC_SETTINGS_IMAGE_CHECK, m_data.tb_use_background_image);
    //没选图片时这个只读路径框没有内容，隐藏掉更干净；选了图片才显示路径
    const bool has_background_image = !m_data.tb_background_image.empty();
    ShowDlgCtrl(IDC_SETTINGS_IMAGE_PATH, has_background_image);
    if (has_background_image)
        SetDlgItemText(IDC_SETTINGS_IMAGE_PATH, m_data.tb_background_image.c_str());
    m_theme_preview.Invalidate();
    m_loading = was_loading;
    EnableControl();
    //只标脏不强制同步重绘：UpdateWindow 会在初始化中途插一次完整绘制，
    //而调用方紧接着的 ShowSection/整页重绘本来就会画到同一批像素。
    Invalidate();
}

void CTaskBarSettingsDlg::OnThemeChanged()
{
    int selection = m_theme_combo.GetCurSel();
    if (m_loading || selection < 0)
        return;
    m_data.tb_theme = static_cast<int>(m_theme_combo.GetItemData(selection));
    if (m_data.tb_theme == 0)
        m_data.tb_use_background_image = false;
    RefreshThemeControls();
    //经典/主题模式切换会显示或隐藏整组控件，需要重新排布并收缩分组框
    CRect client;
    GetClientRect(client);
    ShowSection(m_section, client.Height());
}

void CTaskBarSettingsDlg::OnHoverChanged()
{
    int selection = m_hover_combo.GetCurSel();
    if (selection >= 0)
    {
        m_data.show_speed_graph_tip = selection == 2;
        m_data.show_tool_tip = selection == 1;
    }
}

void CTaskBarSettingsDlg::OnBackgroundImageChanged()
{
    m_data.tb_use_background_image = IsDlgButtonChecked(IDC_SETTINGS_IMAGE_CHECK) != 0;
    if (m_data.tb_use_background_image && m_data.tb_background_image.empty())
        OnSelectBackgroundImage();
    m_data.tb_use_background_image = m_data.tb_use_background_image && !m_data.tb_background_image.empty();
    RefreshThemeControls();
}

void CTaskBarSettingsDlg::OnSelectBackgroundImage()
{
    CFileDialog dlg(TRUE, nullptr, nullptr, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
        _T("Images (*.png;*.jpg;*.bmp)|*.png;*.jpg;*.jpeg;*.bmp|All files (*.*)|*.*||"), this);
    if (dlg.DoModal() == IDOK)
    {
        Gdiplus::Image image(dlg.GetPathName());
        if (image.GetLastStatus() != Gdiplus::Ok)
        {
            MessageBox(CCommon::LoadText(L"TXT_SETTINGS_IMAGE_ERROR"), nullptr, MB_OK | MB_ICONWARNING);
            return;
        }
        m_data.tb_background_image = dlg.GetPathName().GetString();
        m_data.tb_use_background_image = true;
        if (m_data.tb_theme == 0)
            m_data.tb_theme = 1;
        SetDlgItemText(IDC_SETTINGS_IMAGE_PATH, m_data.tb_background_image.c_str());
        RefreshThemeControls();
    }
}

void CTaskBarSettingsDlg::OnThemeDimensionsChanged()
{
    if (!m_loading)
    {
        m_data.tb_panel_opacity = max(1, min(m_opacity_edit.GetValue(), 100));
        m_data.tb_corner_radius = max(0, min(m_corner_edit.GetValue(), 32));
        m_theme_preview.Invalidate();
    }
}

void CTaskBarSettingsDlg::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT info)
{
    if (nIDCtl != IDC_SETTINGS_THEME_PREVIEW)
    {
        CTabDlg::OnDrawItem(nIDCtl, info);
        return;
    }
    CDC dc;
    dc.Attach(info->hDC);
    CRect rect(info->rcItem);
    dc.FillSolidRect(rect, RGB(246, 247, 249));
    COLORREF back = m_data.back_color;
    COLORREF label = m_data.text_colors.empty() ? RGB(220, 220, 220) : m_data.text_colors.begin()->second.label;
    COLORREF value = m_data.text_colors.empty() ? RGB(255, 255, 255) : m_data.text_colors.begin()->second.value;
    COLORREF accent = m_data.status_bar_color;
    int style = 0;
    int radius = m_data.tb_corner_radius;
    bool bold = m_data.font.bold;
    if (m_data.tb_theme == -1)
    {
        back = m_data.custom_bg;
        label = m_data.custom_label;
        value = m_data.custom_value;
        accent = m_data.custom_accent;
        style = m_data.custom_style;
        bold = m_data.custom_value_bold;
    }
    else if (const auto* theme = FindTaskbarTheme(m_data.tb_theme))
    {
        back = RGB(theme->bg_r, theme->bg_g, theme->bg_b);
        label = theme->label;
        value = theme->value;
        accent = theme->accent;
        style = theme->style;
        bold = theme->value_bold;
    }
    rect.DeflateRect(DPI(6), DPI(4));
    const int width = rect.Width();
    const int height = rect.Height();
    if (width <= 0 || height <= 0)
    {
        dc.Detach();
        return;
    }
    const BYTE alpha = static_cast<BYTE>(TaskbarThemeGeometry::PanelAlphaFromPercent(m_data.tb_panel_opacity));
    const int r = TaskbarThemeGeometry::ResolveCornerRadius(width, height, DPI(max(0, radius)), style == 4);
    Gdiplus::Graphics graphics(dc.GetSafeHdc());
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Rect rc(rect.left, rect.top, width, height);
    Gdiplus::GraphicsPath path;
    if (r > 0)
    {
        path.AddArc(rc.X, rc.Y, r * 2, r * 2, 180, 90);
        path.AddArc(rc.X + rc.Width - r * 2, rc.Y, r * 2, r * 2, 270, 90);
        path.AddArc(rc.X + rc.Width - r * 2, rc.Y + rc.Height - r * 2, r * 2, r * 2, 0, 90);
        path.AddArc(rc.X, rc.Y + rc.Height - r * 2, r * 2, r * 2, 90, 90);
        path.CloseFigure();
    }
    Gdiplus::Color bg(alpha, GetRValue(back), GetGValue(back), GetBValue(back));
    if (style == 1)
    {
        Gdiplus::Color end(alpha, (BYTE)((GetRValue(back) + GetRValue(accent)) / 2),
            (BYTE)((GetGValue(back) + GetGValue(accent)) / 2), (BYTE)((GetBValue(back) + GetBValue(accent)) / 2));
        Gdiplus::LinearGradientBrush brush(rc, bg, end, Gdiplus::LinearGradientModeVertical);
        if (r > 0)
            graphics.FillPath(&brush, &path);
        else
            graphics.FillRectangle(&brush, rc);
    }
    else
    {
        Gdiplus::SolidBrush brush(bg);
        if (r > 0)
            graphics.FillPath(&brush, &path);
        else
            graphics.FillRectangle(&brush, rc);
    }
    if (m_data.tb_use_background_image && !m_data.tb_background_image.empty())
    {
        Gdiplus::Image image(m_data.tb_background_image.c_str());
        if (image.GetLastStatus() == Gdiplus::Ok)
        {
            if (r > 0)
            {
                Gdiplus::GraphicsPath clip;
                clip.AddPath(&path, TRUE);
                graphics.SetClip(&clip);
                graphics.DrawImage(&image, rc.X, rc.Y, rc.Width, rc.Height);
                graphics.ResetClip();
            }
            else
            {
                graphics.DrawImage(&image, rc.X, rc.Y, rc.Width, rc.Height);
            }
        }
    }
    const BYTE accent_alpha = static_cast<BYTE>(alpha);
    const BYTE accent_fade_alpha = static_cast<BYTE>(TaskbarThemeGeometry::AccentGradientEndAlpha(alpha));
    if (style == 2)
    {
        TaskbarThemeGeometry::AccentBar accent_bar;
        if (TaskbarThemeGeometry::MakeBottomAccentBar(width, height, r, DPI(2), DPI(2), accent_bar))
        {
            Gdiplus::Rect bar(rect.left + accent_bar.left, rect.top + accent_bar.top,
                accent_bar.width, accent_bar.height);
            Gdiplus::LinearGradientBrush brush(bar,
                Gdiplus::Color(accent_alpha, GetRValue(accent), GetGValue(accent), GetBValue(accent)),
                Gdiplus::Color(accent_fade_alpha, GetRValue(accent), GetGValue(accent), GetBValue(accent)),
                Gdiplus::LinearGradientModeHorizontal);
            graphics.FillRectangle(&brush, bar);
        }
    }
    else if (style == 3)
    {
        Gdiplus::Pen pen(Gdiplus::Color(accent_alpha, GetRValue(accent), GetGValue(accent), GetBValue(accent)), 1.4f);
        if (r > 0)
            graphics.DrawPath(&pen, &path);
        else
            graphics.DrawRectangle(&pen, rc);
    }
    else if (style == 4)
    {
        const int inset = DPI(4);
        TaskbarThemeGeometry::AccentBar accent_bar;
        if (TaskbarThemeGeometry::MakeLeftAccentBar(width, height, inset, inset, DPI(3), accent_bar))
        {
            Gdiplus::Rect bar(rect.left + accent_bar.left, rect.top + accent_bar.top,
                accent_bar.width, accent_bar.height);
            Gdiplus::SolidBrush brush(Gdiplus::Color(accent_alpha, GetRValue(accent), GetGValue(accent), GetBValue(accent)));
            graphics.FillRectangle(&brush, bar);
        }
    }
    CFont preview_font;
    FontInfo preview_info;
    preview_info.name = m_data.font.name;
    preview_info.size = max(8, min(m_data.font.size, 14));
    preview_info.bold = bold;
    preview_info.Create(preview_font, theApp.GetDpi());
    CFont* old_font = dc.SelectObject(&preview_font);
    dc.SetBkMode(TRANSPARENT);
    //预览按真实任务栏面板的排布绘制：左对齐网速、右对齐占用率，留出内边距
    const int pad = DPI(10);
    CRect left(rect);
    left.left += pad;
    left.right = rect.CenterPoint().x;
    CRect right(rect);
    right.left = rect.CenterPoint().x;
    right.right -= pad;
    dc.SetTextColor(label);
    dc.DrawText(_T("↑ 128 KB/s   ↓ 2.4 MB/s"), left, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    dc.SetTextColor(value);
    dc.DrawText(_T("CPU 24%   RAM 48%"), right, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
    dc.SelectObject(old_font);
    dc.Detach();
}
