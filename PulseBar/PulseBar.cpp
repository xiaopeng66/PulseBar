
// PulseBar.cpp : 定义应用程序的类行为。
//

#include "stdafx.h"
#include "PulseBar.h"
#include "PulseBarDlg.h"
#include "crashtool.h"
#include "UpdateHelper.h"
#include "Test.h"
#include "WIC.h"
#include "auto_start_helper.h"
#include "AppAlreadyRuningDlg.h"
#include "WindowsSettingHelper.h"
#include "FilePathHelper.h"
#include "SettingsHelper.h"
#ifndef DISABLE_WINDOWS_WEB_EXPERIENCE_DETECTOR
#include "winrt/base.h"
#endif
#include <gdiplus.h>
#include <algorithm>
#include <cerrno>
#include <cwchar>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CPulseBarApp

BEGIN_MESSAGE_MAP(CPulseBarApp, CWinApp)
    //ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
    ON_COMMAND(ID_HELP, &CPulseBarApp::OnHelp)
    ON_COMMAND(ID_FREQUENTY_ASKED_QUESTIONS, &CPulseBarApp::OnFrequentyAskedQuestions)
    ON_COMMAND(ID_UPDATE_LOG, &CPulseBarApp::OnUpdateLog)
END_MESSAGE_MAP()


CPulseBarApp* CPulseBarApp::self = NULL;


// CPulseBarApp 构造
CPulseBarApp::CPulseBarApp()
{
    self = this;
    // 支持重新启动管理器
    //m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;

    // TODO: 在此处添加构造代码，
    // 将所有重要的初始化放置在 InitInstance 中
    CRASHREPORT::StartCrashReport();
#ifndef DISABLE_WINDOWS_WEB_EXPERIENCE_DETECTOR
    if (m_win_version.IsWindows11OrLater())
        winrt::init_apartment();
#endif

    CheckWindows11Taskbar();
    m_theme_color = CCommon::GetWindowsThemeColor();
}

void CPulseBarApp::LoadLanguageConfig()
{
    CIniHelper ini{ m_config_path };
    m_general_data.language.fromConfigString(ini.GetString(_T("general"), _T("language"), L""));
}

void CPulseBarApp::LoadConfig()
{
    CSettingsHelper ini;

    //常规设置
    m_general_data.check_update_when_start = ini.GetBool(_T("general"), _T("check_update_when_start"), true);
    //m_general_data.allow_skin_cover_font = ini.GetBool(_T("general"), _T("allow_skin_cover_font"), true);
    //m_general_data.allow_skin_cover_text = ini.GetBool(_T("general"), _T("allow_skin_cover_text"), true);
    m_general_data.show_all_interface = ini.GetBool(L"general", L"show_all_interface", false);
    bool is_chinese_language{ m_str_table.IsSimplifiedChinese() };     //当前语言是否为简体中文
    m_general_data.update_source = ini.GetInt(L"general", L"update_source", is_chinese_language ? 1 : 0);   //如果当前语言为简体，则默认更新源为Gitee，否则为GitHub
    m_general_data.cpu_usage_acquire_method = static_cast<GeneralSettingData::CpuUsageAcquireMethod>(ini.GetInt(L"general", L"cpu_usage_acquire_method", GeneralSettingData::CA_PDH));
    //只有 CA_CPU_TIME(0)/CA_PDH(1) 两种方式；越界值会让两个单选框都不选中，界面看起来像“没有设置”
    if (static_cast<int>(m_general_data.cpu_usage_acquire_method) < static_cast<int>(GeneralSettingData::CA_CPU_TIME)
        || static_cast<int>(m_general_data.cpu_usage_acquire_method) > static_cast<int>(GeneralSettingData::CA_PDH))
        m_general_data.cpu_usage_acquire_method = GeneralSettingData::CA_PDH;
    m_general_data.monitor_time_span = ini.GetInt(L"general", L"monitor_time_span", 1000);
    if (m_general_data.monitor_time_span < MONITOR_TIME_SPAN_MIN || m_general_data.monitor_time_span > MONITOR_TIME_SPAN_MAX)
        m_general_data.monitor_time_span = 1000;
    m_general_data.hard_disk_name = ini.GetString(L"general", L"hard_disk_name", L"");
    m_general_data.cpu_core_name = ini.GetString(L"general", L"cpu_core_name", L"Core Average");
    m_general_data.hardware_monitor_item = ini.GetInt(L"general", L"hardware_monitor_item", 0);
    std::vector<std::wstring> connections_hide;
    ini.GetStringList(L"general", L"connections_hide", connections_hide, std::vector<std::wstring>{});
    m_general_data.connections_hide.FromVector(connections_hide);

    //Windows10颜色模式设置
    bool is_windows10_light_theme = CWindowsSettingHelper::IsWindows10LightTheme();
    if (is_windows10_light_theme)
        CCommon::SetColorMode(ColorMode::Light);
    else
        CCommon::SetColorMode(ColorMode::Default);

    //主窗口已移除，以下仅保留与任务栏窗口/托盘图标共用的设置
    m_general_data.show_notify_icon = ini.GetBool(_T("config"), _T("show_notify_icon"), true);
    m_general_data.notify_icon_show_speed = ini.GetBool(_T("config"), _T("notify_icon_show_speed"), false);
    m_cfg_data.m_show_task_bar_wnd = ini.GetBool(_T("config"), _T("show_task_bar_wnd"), true);
    m_cfg_data.m_auto_select = ini.GetBool(_T("connection"), _T("auto_select"), true);
    m_cfg_data.m_select_all = ini.GetBool(_T("connection"), _T("select_all"), false);
    m_cfg_data.m_connection_name = CCommon::UnicodeToStr(ini.GetString(L"connection", L"connection_name", L"").c_str());

    //样式键 0=蓝 1=浅 2=深 3=自动（跟随任务栏）。旧六项索引与旧自动适应开关只在首次迁移时解释：
    //旧开关开启且样式非蓝色时迁移为“自动”（显示效果与旧逻辑一致）；蓝色固定，不受开关影响。
    const auto read_icon_index = [](const std::wstring& text) {
        if (text.empty())
            return -1;
        wchar_t* end{};
        errno = 0;
        const long value = std::wcstol(text.c_str(), &end, 10);
        return errno != ERANGE && end != text.c_str() && end == text.c_str() + text.size()
            && value >= 0 && value <= 5 ? static_cast<int>(value) : -1;
    };
    std::wstring notify_icon_style;
    if (ini.GetString(L"config", L"notify_icon_style", notify_icon_style))
    {
        const int style = read_icon_index(notify_icon_style);
        m_cfg_data.m_notify_icon_selected = (style >= 0 && style < MAX_NOTIFY_ICON_STYLE)
            ? style : NOTIFY_ICON_BLUE;
        if (ini.GetBool(L"config", L"notify_icon_auto_adapt", false)
            && m_cfg_data.m_notify_icon_selected != NOTIFY_ICON_BLUE)
            m_cfg_data.m_notify_icon_selected = NOTIFY_ICON_AUTO;
    }
    else
    {
        const int legacy_styles[] = { NOTIFY_ICON_LIGHT, NOTIFY_ICON_LIGHT, NOTIFY_ICON_BLUE,
            NOTIFY_ICON_BLUE, NOTIFY_ICON_DARK, NOTIFY_ICON_DARK };
        const int legacy_index = read_icon_index(ini.GetString(L"config", L"notify_icon_selected", L""));
        m_cfg_data.m_notify_icon_selected = (legacy_index >= 0 && legacy_index < static_cast<int>(_countof(legacy_styles)))
            ? legacy_styles[legacy_index] : NOTIFY_ICON_BLUE;
        if (ini.GetBool(L"config", L"notify_icon_auto_adapt", true)
            && m_cfg_data.m_notify_icon_selected != NOTIFY_ICON_BLUE)
            m_cfg_data.m_notify_icon_selected = NOTIFY_ICON_AUTO;
    }

    //插件接口兼容：主窗口已移除，这些值仅作为插件 EI_MAIN_WND_* 的默认值保留
    m_main_wnd_data.text_colors[TDI_UP] = RGB(255, 255, 255);
    m_main_wnd_data.specify_each_item_color = false;
    m_main_wnd_data.font.name = m_str_table.GetLanguageInfo().default_font_name.c_str();
    m_main_wnd_data.font.size = 10;
    m_main_wnd_data.disp_str.Get(TDI_UP) = CCommon::LoadText(IDS_UPLOAD_DISP, L": ");
    m_main_wnd_data.disp_str.Get(TDI_DOWN) = CCommon::LoadText(IDS_DOWNLOAD_DISP, L": ");
    m_main_wnd_data.disp_str.Get(TDI_CPU) = L"CPU: ";
    m_main_wnd_data.disp_str.Get(TDI_MEMORY) = CCommon::LoadText(IDS_MEMORY_DISP, _T(": "));
    m_main_wnd_data.separate_value_unit_with_space = true;
    m_main_wnd_data.unit_byte = true;
    m_main_wnd_data.speed_unit = SpeedUnit::AUTO;

    m_general_data.traffic_tip_enable = ini.GetBool(L"notify_tip", L"traffic_tip_enable", false);
    m_general_data.traffic_tip_value = ini.GetInt(L"notify_tip", L"traffic_tip_value", 200);
    if (m_general_data.traffic_tip_value < 1)
        m_general_data.traffic_tip_value = 1;
    if (m_general_data.traffic_tip_value > 32767)
        m_general_data.traffic_tip_value = 32767;
    m_general_data.traffic_tip_unit = ini.GetInt(L"notify_tip", L"traffic_tip_unit", 0);
    //提示单位只有 MB(0)/GB(1)；其它值会让下拉框空白，且提醒文案会落到 GB 分支
    if (m_general_data.traffic_tip_unit < 0 || m_general_data.traffic_tip_unit > 1)
        m_general_data.traffic_tip_unit = 0;
    m_general_data.memory_usage_tip.enable = ini.GetBool(L"notify_tip", L"memory_usage_tip_enable", false);
    m_general_data.memory_usage_tip.tip_value = ini.GetInt(L"notify_tip", L"memory_tip_value", 80);
    if (m_general_data.memory_usage_tip.tip_value < 1 || m_general_data.memory_usage_tip.tip_value > 100)
        m_general_data.memory_usage_tip.tip_value = 80;
    m_general_data.cpu_temp_tip.enable = ini.GetBool(L"notify_tip", L"cpu_temperature_tip_enable", false);
    m_general_data.cpu_temp_tip.tip_value = ini.GetInt(L"notify_tip", L"cpu_temperature_tip_value", 80);
    if (m_general_data.cpu_temp_tip.tip_value < 1 || m_general_data.cpu_temp_tip.tip_value > 120)
        m_general_data.cpu_temp_tip.tip_value = 80;
    m_general_data.gpu_temp_tip.enable = ini.GetBool(L"notify_tip", L"gpu_temperature_tip_enable", false);
    m_general_data.gpu_temp_tip.tip_value = ini.GetInt(L"notify_tip", L"gpu_temperature_tip_value", 80);
    if (m_general_data.gpu_temp_tip.tip_value < 1 || m_general_data.gpu_temp_tip.tip_value > 120)
        m_general_data.gpu_temp_tip.tip_value = 80;
    m_general_data.hdd_temp_tip.enable = ini.GetBool(L"notify_tip", L"hdd_temperature_tip_enable", false);
    m_general_data.hdd_temp_tip.tip_value = ini.GetInt(L"notify_tip", L"hdd_temperature_tip_value", 80);
    if (m_general_data.hdd_temp_tip.tip_value < 1 || m_general_data.hdd_temp_tip.tip_value > 120)
        m_general_data.hdd_temp_tip.tip_value = 80;
    m_general_data.mainboard_temp_tip.enable = ini.GetBool(L"notify_tip", L"mainboard_temperature_tip_enable", false);
    m_general_data.mainboard_temp_tip.tip_value = ini.GetInt(L"notify_tip", L"mainboard_temperature_tip_value", 80);
    if (m_general_data.mainboard_temp_tip.tip_value < 1 || m_general_data.mainboard_temp_tip.tip_value > 120)
        m_general_data.mainboard_temp_tip.tip_value = 80;

    //任务栏窗口设置
    m_taskbar_data.back_color = ini.GetInt(_T("task_bar"), _T("task_bar_back_color"), m_taskbar_data.dft_back_color);
    m_taskbar_data.transparent_color = ini.GetInt(_T("task_bar"), _T("transparent_color"), m_taskbar_data.dft_transparent_color);
    if (m_taskbar_data.IsTaskbarTransparent()) //如果任务栏背景透明，则需要将颜色转换一下
    {
        CCommon::TransparentColorConvert(m_taskbar_data.back_color);
        CCommon::TransparentColorConvert(m_taskbar_data.transparent_color);
    }
    m_taskbar_data.status_bar_color = ini.GetInt(_T("task_bar"), _T("status_bar_color"), m_taskbar_data.dft_status_bar_color);
    //m_taskbar_data.text_color = GetPrivateProfileInt(_T("task_bar"), _T("task_bar_text_color"), 0x00ffffffU, m_config_path.c_str());
    ini.LoadTaskbarWndColors(_T("task_bar"), _T("task_bar_text_color"), m_taskbar_data.text_colors, m_taskbar_data.dft_text_colors);
    m_taskbar_data.specify_each_item_color = ini.GetBool(L"task_bar", L"specify_each_item_color", false);
    //m_cfg_data.m_tbar_show_cpu_memory = ini.GetBool(_T("task_bar"), _T("task_bar_show_cpu_memory"), false);
    m_taskbar_data.display_item.FromInt(ini.GetInt(L"task_bar", L"tbar_display_item", DisplayItemSet{ TDI_UP, TDI_DOWN }.ToInt()));
    m_taskbar_data.show_taskbar_wnd_in_secondary_display = ini.GetBool(L"task_bar", L"show_taskbar_wnd_in_secondary_display", false);
    m_taskbar_data.secondary_display_index = ini.GetInt(L"task_bar", L"secondary_display_index", 0);

    //不含温度监控的版本，不显示温度监控相关项目
#ifdef WITHOUT_TEMPERATURE
    m_taskbar_data.display_item.Remove(TDI_CPU_TEMP);
    m_taskbar_data.display_item.Remove(TDI_GPU_TEMP);
    m_taskbar_data.display_item.Remove(TDI_HDD_TEMP);
    m_taskbar_data.display_item.Remove(TDI_MAIN_BOARD_TEMP);
#endif

    //如果选项设置中关闭了某个硬件监控，则不显示对应的温度监控相关项目
    if (!m_general_data.IsHardwareEnable(HI_CPU))
        m_taskbar_data.display_item.Remove(TDI_CPU_TEMP);
    if (!m_general_data.IsHardwareEnable(HI_GPU))
    {
        m_taskbar_data.display_item.Remove(TDI_GPU_TEMP);
    }
    if (!m_general_data.IsHardwareEnable(HI_HDD))
    {
        m_taskbar_data.display_item.Remove(TDI_HDD_TEMP);
    }
    if (!m_general_data.IsHardwareEnable(HI_MBD))
        m_taskbar_data.display_item.Remove(TDI_MAIN_BOARD_TEMP);

    //m_taskbar_data.swap_up_down = ini.GetBool(_T("task_bar"), _T("task_bar_swap_up_down"), false);

    if (m_taskbar_data.back_color == 0 && !m_taskbar_data.text_colors.empty() && m_taskbar_data.text_colors.begin()->second.label == 0)     //万一读取到的背景色和文本颜色都为0（黑色），则将文本色和背景色设置成默认颜色
    {
        m_taskbar_data.back_color = m_taskbar_data.dft_back_color;
        m_taskbar_data.text_colors.begin()->second.label = m_taskbar_data.dft_text_colors;
    }

    FontInfo default_font;
    default_font.name = m_str_table.GetLanguageInfo().default_font_name.c_str();
    default_font.size = 9;
    ini.LoadFontData(_T("task_bar"), m_taskbar_data.font, default_font);
    //配置文件可被手工编辑：载入时做范围校验，避免异常值进入绘制流程
    if (m_taskbar_data.font.size < MIN_FONT_SIZE || m_taskbar_data.font.size > MAX_FONT_SIZE)
        m_taskbar_data.font.size = 9;

    //载入显示文本设置
    ini.LoadDisplayStr(L"task_bar", m_taskbar_data.disp_str, false);
    ini.LoadPluginDisplayStr(L"plugin_display_str_taskbar_window", m_taskbar_data.disp_str, false);

    m_taskbar_data.tbar_wnd_on_left = ini.GetBool(_T("task_bar"), _T("task_bar_wnd_on_left"), false);
    m_taskbar_data.speed_short_mode = ini.GetBool(_T("task_bar"), _T("task_bar_speed_short_mode"), true);
    m_taskbar_data.tbar_wnd_snap = ini.GetBool(_T("task_bar"), _T("task_bar_wnd_snap"), false);
    m_taskbar_data.unit_byte = ini.GetBool(_T("task_bar"), _T("unit_byte"), true);
    //枚举值来自配置文本，必须校验：非法值会让 switch 无分支命中（数值/单位变成空串），
    //也会让下拉框 SetCurSel 得到 -1 而显示为空白
    {
        int speed_unit = ini.GetInt(_T("task_bar"), _T("task_bar_speed_unit"), 0);
        if (speed_unit < static_cast<int>(SpeedUnit::AUTO) || speed_unit > static_cast<int>(SpeedUnit::MBPS))
            speed_unit = static_cast<int>(SpeedUnit::AUTO);
        m_taskbar_data.speed_unit = static_cast<SpeedUnit>(speed_unit);
    }
    m_taskbar_data.hide_unit = ini.GetBool(_T("task_bar"), _T("task_bar_hide_unit"), false);
    m_taskbar_data.hide_percent = ini.GetBool(_T("task_bar"), _T("task_bar_hide_percent"), false);
    m_taskbar_data.value_right_align = ini.GetBool(_T("task_bar"), _T("value_right_align"), true);
    m_taskbar_data.horizontal_arrange = ini.GetBool(_T("task_bar"), _T("horizontal_arrange"), false);
    m_taskbar_data.show_status_bar = ini.GetBool(_T("task_bar"), _T("show_status_bar"), true);
    m_taskbar_data.separate_value_unit_with_space = ini.GetBool(_T("task_bar"), _T("separate_value_unit_with_space"), true);
    m_taskbar_data.show_tool_tip = ini.GetBool(_T("task_bar"), _T("show_tool_tip"), true);
    m_taskbar_data.digits_number = ini.GetInt(_T("task_bar"), _T("digits_number"), 4);
    //下拉框只提供 3–7，这里同步限制到 3–7；配置里更大的值会让 SetCurSel 越界而显示空白
    if (m_taskbar_data.digits_number < 3)
        m_taskbar_data.digits_number = 3;
    if (m_taskbar_data.digits_number > 7)
        m_taskbar_data.digits_number = 7;
    {
        int memory_display = ini.GetInt(L"task_bar", L"memory_display", static_cast<int>(MemoryDisplay::USAGE_PERCENTAGE));
        if (memory_display < static_cast<int>(MemoryDisplay::USAGE_PERCENTAGE) || memory_display > static_cast<int>(MemoryDisplay::MEMORY_AVAILABLE))
            memory_display = static_cast<int>(MemoryDisplay::USAGE_PERCENTAGE);
        m_taskbar_data.memory_display = static_cast<MemoryDisplay>(memory_display);
    }
    {
        int double_click_action = ini.GetInt(_T("task_bar"), _T("double_click_action"), static_cast<int>(DoubleClickAction::CONNECTION_INFO));
        if (double_click_action < static_cast<int>(DoubleClickAction::CONNECTION_INFO) || double_click_action > static_cast<int>(DoubleClickAction::NONE))
            double_click_action = static_cast<int>(DoubleClickAction::CONNECTION_INFO);
        m_taskbar_data.double_click_action = static_cast<DoubleClickAction>(double_click_action);
    }
    m_taskbar_data.double_click_exe = ini.GetString(L"task_bar", L"double_click_exe", (theApp.m_system_dir + L"\\Taskmgr.exe").c_str());
    m_taskbar_data.cm_graph_type = ini.GetBool(_T("task_bar"), _T("cm_graph_type"), true);
    m_taskbar_data.show_graph_dashed_box = ini.GetBool(L"task_bar", L"show_graph_dashed_box", false);
    m_taskbar_data.item_space = ini.GetInt(L"task_bar", L"item_space", 8);
    m_taskbar_data.vertical_margin = ini.GetInt(L"task_bar", L"vertical_margin", 0);
    m_taskbar_data.window_offset_top = ini.GetInt(L"task_bar", L"window_offset_top", 0);
    m_taskbar_data.window_offset_left = ini.GetInt(L"task_bar", L"window_offset_left", 0);
    m_taskbar_data.ValidItemSpace();
    m_taskbar_data.ValidWindowOffsetTop();
    m_taskbar_data.ValidWindowOffsetLeft();
    m_taskbar_data.avoid_overlap_with_widgets = ini.GetBool(_T("task_bar"), _T("avoid_overlap_with_widgets"), false);
    m_taskbar_data.taskbar_left_space_win11 = ini.GetInt(L"task_bar", L"taskbar_left_space_win11", 160);
    m_taskbar_data.taskbar_right_space_win11 = ini.GetInt(L"task_bar", L"taskbar_right_space_win11", 280);

    if (m_win_version.IsWindows10OrLater())     //只有Win10才支持自动适应系统深色/浅色主题
        m_taskbar_data.auto_adapt_light_theme = ini.GetBool(L"task_bar", L"auto_adapt_light_theme", false);
    else
        m_taskbar_data.auto_adapt_light_theme = false;
    m_taskbar_data.tb_theme = ini.GetInt(L"task_bar", L"tb_theme", 0);
    //历史版本 bug 曾把菜单命令 ID 原样存入(33668+)，这里钳制回有效主题编号; -1=自定义(S5)
    if (m_taskbar_data.tb_theme < -1 || m_taskbar_data.tb_theme > (ID_TB_THEME_PILL_BLACK_GOLD - ID_TB_THEME_CLASSIC))
        m_taskbar_data.tb_theme = 0;
    //S5: 自定义主题字段
    m_taskbar_data.custom_bg = ini.GetInt(L"task_bar", L"custom_bg", RGB(32, 34, 40));
    m_taskbar_data.custom_label = ini.GetInt(L"task_bar", L"custom_label", RGB(150, 150, 155));
    m_taskbar_data.custom_value = ini.GetInt(L"task_bar", L"custom_value", RGB(255, 255, 255));
    m_taskbar_data.custom_accent = ini.GetInt(L"task_bar", L"custom_accent", RGB(80, 160, 255));
    m_taskbar_data.custom_value_bold = ini.GetBool(L"task_bar", L"custom_bold", false);
    m_taskbar_data.custom_style = ini.GetInt(L"task_bar", L"custom_style", 0);
    if (m_taskbar_data.custom_style < 0 || m_taskbar_data.custom_style > 4)
        m_taskbar_data.custom_style = 0;
    m_taskbar_data.tb_corner_radius = ini.GetInt(L"task_bar", L"tb_corner_radius", 8);
    m_taskbar_data.tb_panel_opacity = ini.GetInt(L"task_bar", L"tb_panel_opacity", 70);
    if (m_taskbar_data.tb_corner_radius < 0 || m_taskbar_data.tb_corner_radius > 32)
        m_taskbar_data.tb_corner_radius = 8;
    if (m_taskbar_data.tb_panel_opacity < 1 || m_taskbar_data.tb_panel_opacity > 100)
        m_taskbar_data.tb_panel_opacity = 70;
    m_taskbar_data.tb_use_background_image = ini.GetBool(L"task_bar", L"tb_use_background_image", false);
    m_taskbar_data.tb_background_image = ini.GetString(L"task_bar", L"tb_background_image", L"");
    m_taskbar_data.percent_color_enabled = ini.GetBool(L"task_bar", L"percent_color", true);
    m_taskbar_data.dark_default_style = ini.GetInt(L"task_bar", L"dark_default_style", 0);
    m_taskbar_data.light_default_style = ini.GetInt(L"task_bar", L"light_default_style", TASKBAR_DEFAULT_LIGHT_STYLE_INDEX);

    if (m_win_version.IsWindows8OrLater())
        m_taskbar_data.auto_set_background_color = ini.GetBool(L"task_bar", L"auto_set_background_color", false);
    else
        m_taskbar_data.auto_set_background_color = false;

    m_taskbar_data.item_order.Init();
    m_taskbar_data.item_order.FromString(ini.GetString(L"task_bar", L"item_order", L""));
    m_taskbar_data.plugin_display_item.FromString(ini.GetString(L"task_bar", L"plugin_display_item", L""));
    m_taskbar_data.auto_save_taskbar_color_settings_to_preset = ini.GetBool(L"task_bar", L"auto_save_taskbar_color_settings_to_preset", true);

    m_taskbar_data.show_netspeed_figure = ini.GetBool(L"task_bar", L"show_netspeed_figure", true);
    m_taskbar_data.show_speed_graph_tip = ini.GetBool(L"task_bar", L"show_speed_graph_tip", true);
    m_taskbar_data.netspeed_figure_max_value = ini.GetInt(L"task_bar", L"netspeed_figure_max_value", 10);
    m_taskbar_data.netspeed_figure_max_value_unit = ini.GetInt(L"task_bar", L"netspeed_figure_max_value_unit", 1);
    //下拉框只有 KB(0)/MB(1) 两项；其它值会让下拉框显示空白，也会让 GetNetspeedFigureMaxValueInBytes 走错分支
    if (m_taskbar_data.netspeed_figure_max_value_unit < 0 || m_taskbar_data.netspeed_figure_max_value_unit > 1)
        m_taskbar_data.netspeed_figure_max_value_unit = 1;
    m_taskbar_data.graph_color_following_system = ini.GetBool(L"task_bar", L"graph_color_following_system", true);

    if (CTaskBarDlgDrawCommonSupport::CheckSupport())
        m_taskbar_data.disable_d2d = ini.GetBool(L"task_bar", L"disable_d2d", true);
    else
        m_taskbar_data.disable_d2d = true;
    m_taskbar_data.enable_colorful_emoji = ini.GetBool(L"task_bar", L"enable_colorful_emoji", true);

    //其他设置
    //m_cfg_data.m_show_internet_ip = ini.GetBool(L"connection_details", L"show_internet_ip", false);
    m_cfg_data.m_use_log_scale = ini.GetBool(_T("histroy_traffic"), _T("use_log_scale"), true);
    m_cfg_data.m_sunday_first = ini.GetBool(_T("histroy_traffic"), _T("sunday_first"), true);
    m_cfg_data.m_view_type = static_cast<HistoryTrafficViewType>(ini.GetInt(_T("histroy_traffic"), _T("view_type"), static_cast<int>(HistoryTrafficViewType::HV_DAY)));
    //历史列表下拉框只有 5 项；越界值会让 SetCurSel 得到 -1 而显示空白
    if (static_cast<int>(m_cfg_data.m_view_type) < static_cast<int>(HistoryTrafficViewType::HV_DAY)
        || static_cast<int>(m_cfg_data.m_view_type) > static_cast<int>(HistoryTrafficViewType::HV_YEAR))
        m_cfg_data.m_view_type = HistoryTrafficViewType::HV_DAY;

    m_notify_interval = ini.GetInt(_T("other"), _T("notify_interval"), 60);
    m_exit_when_start_by_restart_manager = ini.GetBool(_T("other"), _T("exit_when_start_by_restart_manager"), true);
    m_debug_log = ini.GetBool(_T("other"), _T("debug_log"), false);
    //由于Win7系统中设置任务栏窗口透明色会导致任务栏窗口不可见，因此默认在Win7中禁用透明色的设定
    m_taksbar_transparent_color_enable = ini.GetBool(L"other", L"taksbar_transparent_color_enable", !m_win_version.IsWindows7());
    m_last_light_mode = ini.GetBool(L"other", L"last_light_mode", CWindowsSettingHelper::IsWindows10LightTheme());
    m_show_mouse_panetrate_tip = ini.GetBool(L"other", L"show_mouse_panetrate_tip", true);
    m_show_dot_net_notinstalled_tip = ini.GetBool(L"other", L"show_dot_net_notinstalled_tip", true);
}

void CPulseBarApp::SaveConfig()
{
    CSettingsHelper ini;
    bool sections_valid{};
    const auto skin_sections = ini.GetAllAppName(L"skin_", &sections_valid);
    if (!sections_valid)
    {
        CCommon::WriteLog(L"Malformed INI section; configuration was not overwritten.", m_log_path.c_str());
        return;
    }

    //移除旧主窗口键，保留任务栏及第三方配置段。
    for (const wchar_t* key : { L"transparency", L"always_on_top", L"lock_window_pos", L"show_cpu_memory",
        L"mouse_penetrate", L"position_x", L"position_y", L"hide_main_window", L"skin_selected", L"swap_up_down",
        L"hide_main_wnd_when_fullscreen", L"speed_short_mode", L"separate_value_unit_with_space", L"show_tool_tip",
        L"memory_display", L"unit_byte", L"speed_unit", L"hide_unit", L"hide_percent", L"double_click_action",
        L"double_click_exe", L"alow_out_of_border", L"text_color", L"font_name", L"font_size", L"font_style",
        L"specify_each_item_color" })
        ini.RemoveKey(L"config", key);
    ini.RemoveSection(L"skins");
    ini.RemoveSection(L"plugin_display_str_main_window");
    for (const auto& skin_name : skin_sections)
        ini.RemoveSection(L"skin_" + skin_name);

    //常规设置
    ini.WriteBool(_T("general"), _T("check_update_when_start"), m_general_data.check_update_when_start);
    //ini.WriteBool(_T("general"), _T("allow_skin_cover_font"), m_general_data.allow_skin_cover_font);
    //ini.WriteBool(_T("general"), _T("allow_skin_cover_text"), m_general_data.allow_skin_cover_text);
    ini.WriteString(_T("general"), _T("language"), m_general_data.language.toConfigString());
    ini.WriteInt(L"general", L"update_source", m_general_data.update_source);
    ini.WriteBool(L"general", L"show_all_interface", m_general_data.show_all_interface);
    ini.WriteInt(L"general", L"cpu_usage_acquire_method", m_general_data.cpu_usage_acquire_method);
    ini.WriteInt(L"general", L"monitor_time_span", m_general_data.monitor_time_span);
    ini.WriteString(L"general", L"hard_disk_name", m_general_data.hard_disk_name);
    ini.WriteString(L"general", L"cpu_core_name", m_general_data.cpu_core_name);
    ini.WriteInt(L"general", L"hardware_monitor_item", m_general_data.hardware_monitor_item);
    ini.WriteStringList(L"general", L"connections_hide", m_general_data.connections_hide.ToVector());

    //主窗口已移除，仅保留与任务栏窗口/托盘图标共用的设置
    ini.WriteBool(L"config", L"show_notify_icon", m_general_data.show_notify_icon);
    ini.WriteBool(L"config", L"notify_icon_show_speed", m_general_data.notify_icon_show_speed);
    ini.WriteBool(L"config", L"show_task_bar_wnd", m_cfg_data.m_show_task_bar_wnd);
    ini.WriteBool(L"connection", L"auto_select", m_cfg_data.m_auto_select);
    ini.WriteBool(L"connection", L"select_all", m_cfg_data.m_select_all);
    ini.WriteString(L"connection", L"connection_name", CCommon::StrToUnicode(m_cfg_data.m_connection_name.c_str()));

    ini.WriteInt(L"config", L"notify_icon_style", NormalizeNotifyIconSelection(m_cfg_data.m_notify_icon_selected));
    ini.RemoveKey(L"config", L"notify_icon_selected");
    ini.RemoveKey(L"config", L"notify_icon_auto_adapt");    //已由 notify_icon_style=3（自动）取代

    ini.WriteBool(L"notify_tip", L"traffic_tip_enable", m_general_data.traffic_tip_enable);
    ini.WriteInt(L"notify_tip", L"traffic_tip_value", m_general_data.traffic_tip_value);
    ini.WriteInt(L"notify_tip", L"traffic_tip_unit", m_general_data.traffic_tip_unit);
    ini.WriteBool(L"notify_tip", L"memory_usage_tip_enable", m_general_data.memory_usage_tip.enable);
    ini.WriteInt(L"notify_tip", L"memory_tip_value", m_general_data.memory_usage_tip.tip_value);
    ini.WriteBool(L"notify_tip", L"cpu_temperature_tip_enable", m_general_data.cpu_temp_tip.enable);
    ini.WriteInt(L"notify_tip", L"cpu_temperature_tip_value", m_general_data.cpu_temp_tip.tip_value);
    ini.WriteBool(L"notify_tip", L"gpu_temperature_tip_enable", m_general_data.gpu_temp_tip.enable);
    ini.WriteInt(L"notify_tip", L"gpu_temperature_tip_value", m_general_data.gpu_temp_tip.tip_value);
    ini.WriteBool(L"notify_tip", L"hdd_temperature_tip_enable", m_general_data.hdd_temp_tip.enable);
    ini.WriteInt(L"notify_tip", L"hdd_temperature_tip_value", m_general_data.hdd_temp_tip.tip_value);
    ini.WriteBool(L"notify_tip", L"mainboard_temperature_tip_enable", m_general_data.mainboard_temp_tip.enable);
    ini.WriteInt(L"notify_tip", L"mainboard_temperature_tip_value", m_general_data.mainboard_temp_tip.tip_value);

    //任务栏窗口设置
    ini.WriteInt(L"task_bar", L"task_bar_back_color", m_taskbar_data.back_color);
    ini.WriteInt(L"task_bar", L"transparent_color", m_taskbar_data.transparent_color);
    ini.WriteInt(L"task_bar", L"status_bar_color", m_taskbar_data.status_bar_color);
    ini.SaveTaskbarWndColors(L"task_bar", L"task_bar_text_color", m_taskbar_data.text_colors);
    ini.WriteBool(L"task_bar", L"specify_each_item_color", m_taskbar_data.specify_each_item_color);
    //ini.WriteBool(L"task_bar", L"task_bar_show_cpu_memory", m_cfg_data.m_tbar_show_cpu_memory);
    ini.WriteInt(L"task_bar", L"tbar_display_item", m_taskbar_data.display_item.ToInt());
    ini.SaveFontData(L"task_bar", m_taskbar_data.font);
    //ini.WriteBool(L"task_bar", L"task_bar_swap_up_down", m_taskbar_data.swap_up_down);
    ini.WriteBool(L"task_bar", L"show_taskbar_wnd_in_secondary_display", m_taskbar_data.show_taskbar_wnd_in_secondary_display);
    ini.WriteInt(L"task_bar", L"secondary_display_index", m_taskbar_data.secondary_display_index);

    ini.SaveDisplayStr(L"task_bar", m_taskbar_data.disp_str);
    ini.SavePluginDisplayStr(L"plugin_display_str_taskbar_window", m_taskbar_data.disp_str);

    ini.WriteBool(L"task_bar", L"task_bar_wnd_on_left", m_taskbar_data.tbar_wnd_on_left);
    ini.WriteBool(L"task_bar", L"task_bar_wnd_snap", m_taskbar_data.tbar_wnd_snap);
    ini.WriteBool(L"task_bar", L"task_bar_speed_short_mode", m_taskbar_data.speed_short_mode);
    ini.WriteBool(L"task_bar", L"unit_byte", m_taskbar_data.unit_byte);
    ini.WriteInt(L"task_bar", L"task_bar_speed_unit", static_cast<int>(m_taskbar_data.speed_unit));
    ini.WriteBool(L"task_bar", L"task_bar_hide_unit", m_taskbar_data.hide_unit);
    ini.WriteBool(L"task_bar", L"task_bar_hide_percent", m_taskbar_data.hide_percent);
    ini.WriteBool(L"task_bar", L"value_right_align", m_taskbar_data.value_right_align);
    ini.WriteBool(L"task_bar", L"horizontal_arrange", m_taskbar_data.horizontal_arrange);
    ini.WriteBool(L"task_bar", L"show_status_bar", m_taskbar_data.show_status_bar);
    ini.WriteBool(L"task_bar", L"separate_value_unit_with_space", m_taskbar_data.separate_value_unit_with_space);
    ini.WriteBool(L"task_bar", L"show_tool_tip", m_taskbar_data.show_tool_tip);
    ini.WriteInt(L"task_bar", L"digits_number", m_taskbar_data.digits_number);
    ini.WriteInt(L"task_bar", L"memory_display", static_cast<int>(m_taskbar_data.memory_display));
    ini.WriteInt(L"task_bar", L"double_click_action", static_cast<int>(m_taskbar_data.double_click_action));
    ini.WriteString(L"task_bar", L"double_click_exe", m_taskbar_data.double_click_exe);
    ini.WriteBool(L"task_bar", L"cm_graph_type", m_taskbar_data.cm_graph_type);
    ini.WriteBool(L"task_bar", L"show_graph_dashed_box", m_taskbar_data.show_graph_dashed_box);
    ini.WriteInt(L"task_bar", L"item_space", m_taskbar_data.item_space);
    ini.WriteInt(L"task_bar", L"vertical_margin", m_taskbar_data.vertical_margin);
    ini.WriteInt(L"task_bar", L"window_offset_top", m_taskbar_data.window_offset_top);
    ini.WriteInt(L"task_bar", L"window_offset_left", m_taskbar_data.window_offset_left);
    ini.WriteBool(L"task_bar", L"avoid_overlap_with_widgets", m_taskbar_data.avoid_overlap_with_widgets);
    ini.WriteInt(L"task_bar", L"taskbar_left_space_win11", m_taskbar_data.taskbar_left_space_win11);
    ini.WriteInt(L"task_bar", L"taskbar_right_space_win11", m_taskbar_data.taskbar_right_space_win11);

    ini.WriteBool(L"task_bar", L"auto_adapt_light_theme", m_taskbar_data.auto_adapt_light_theme);
    ini.WriteInt(L"task_bar", L"tb_theme", m_taskbar_data.tb_theme);
    ini.WriteInt(L"task_bar", L"custom_bg", m_taskbar_data.custom_bg);
    ini.WriteInt(L"task_bar", L"custom_label", m_taskbar_data.custom_label);
    ini.WriteInt(L"task_bar", L"custom_value", m_taskbar_data.custom_value);
    ini.WriteInt(L"task_bar", L"custom_accent", m_taskbar_data.custom_accent);
    ini.WriteBool(L"task_bar", L"custom_bold", m_taskbar_data.custom_value_bold);
    ini.WriteInt(L"task_bar", L"custom_style", m_taskbar_data.custom_style);
    ini.WriteInt(L"task_bar", L"tb_corner_radius", m_taskbar_data.tb_corner_radius);
    ini.WriteInt(L"task_bar", L"tb_panel_opacity", m_taskbar_data.tb_panel_opacity);
    ini.WriteBool(L"task_bar", L"tb_use_background_image", m_taskbar_data.tb_use_background_image);
    ini.WriteString(L"task_bar", L"tb_background_image", m_taskbar_data.tb_background_image);
    ini.WriteBool(L"task_bar", L"percent_color", m_taskbar_data.percent_color_enabled);
    ini.WriteInt(L"task_bar", L"dark_default_style", m_taskbar_data.dark_default_style);
    ini.WriteInt(L"task_bar", L"light_default_style", m_taskbar_data.light_default_style);
    ini.WriteBool(L"task_bar", L"auto_set_background_color", m_taskbar_data.auto_set_background_color);

    ini.WriteString(L"task_bar", L"item_order", m_taskbar_data.item_order.ToString());
    ini.WriteString(L"task_bar", L"plugin_display_item", m_taskbar_data.plugin_display_item.ToString());
    ini.WriteBool(L"task_bar", L"auto_save_taskbar_color_settings_to_preset", m_taskbar_data.auto_save_taskbar_color_settings_to_preset);

    ini.WriteBool(L"task_bar", L"show_netspeed_figure", m_taskbar_data.show_netspeed_figure);
    ini.WriteBool(L"task_bar", L"show_speed_graph_tip", m_taskbar_data.show_speed_graph_tip);
    ini.WriteInt(L"task_bar", L"netspeed_figure_max_value", m_taskbar_data.netspeed_figure_max_value);
    ini.WriteInt(L"task_bar", L"netspeed_figure_max_value_unit", m_taskbar_data.netspeed_figure_max_value_unit);
    ini.WriteBool(L"task_bar", L"graph_color_following_system", m_taskbar_data.graph_color_following_system);

    ini.WriteBool(L"task_bar", L"disable_d2d", m_taskbar_data.disable_d2d);
    ini.WriteBool(L"task_bar", L"enable_colorful_emoji", m_taskbar_data.enable_colorful_emoji);

    //其他设置
    //ini.WriteBool(L"connection_details", L"show_internet_ip", m_cfg_data.m_show_internet_ip);
    ini.WriteBool(L"histroy_traffic", L"use_log_scale", m_cfg_data.m_use_log_scale);
    ini.WriteBool(L"histroy_traffic", L"sunday_first", m_cfg_data.m_sunday_first);
    ini.WriteInt(L"histroy_traffic", L"view_type", static_cast<int>(m_cfg_data.m_view_type));

    ini.WriteBool(_T("other"), _T("exit_when_start_by_restart_manager"), m_exit_when_start_by_restart_manager);
    ini.WriteBool(_T("other"), _T("debug_log"), m_debug_log);
    ini.WriteInt(_T("other"), _T("notify_interval"), m_notify_interval);
    ini.WriteBool(_T("other"), _T("taksbar_transparent_color_enable"), m_taksbar_transparent_color_enable);
    ini.WriteBool(_T("other"), _T("last_light_mode"), m_last_light_mode);
    ini.WriteBool(_T("other"), _T("show_mouse_panetrate_tip"), m_show_mouse_panetrate_tip);
    ini.WriteBool(_T("other"), _T("show_dot_net_notinstalled_tip"), m_show_dot_net_notinstalled_tip);

    ini.WriteString(L"config", L"plugin_disabled", m_cfg_data.plugin_disabled.ToString());

    ini.WriteString(L"app", L"version", VERSION);

    //检查是否保存成功
    if (!ini.Save())
    {
        if (m_cannot_save_config_warning)
        {
            CString info = CCommon::LoadText(IDS_CONNOT_SAVE_CONFIG_WARNING);
            info.Replace(_T("<%file_path%>"), m_config_path.c_str());
            AfxMessageBox(info, MB_ICONWARNING);
        }
        m_cannot_save_config_warning = false;
        return;
    }
}

namespace
{
    std::wstring WithTrailingSlash(std::wstring path)
    {
        if (!path.empty() && path.back() != L'\\' && path.back() != L'/')
            path += L'\\';
        return path;
    }

    //逐级创建目录，兼容 AppData 下中间目录尚不存在的情况
    bool EnsureDirectoryExists(const std::wstring& dir)
    {
        if (dir.empty())
            return false;
        const DWORD attributes = ::GetFileAttributesW(dir.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES)
            return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        for (size_t index = 1; index < dir.size(); ++index)
        {
            if (dir[index] != L'\\' && dir[index] != L'/')
                continue;
            const std::wstring partial = dir.substr(0, index);
            if (partial.size() > 2)
                ::CreateDirectoryW(partial.c_str(), nullptr);
        }
        if (::CreateDirectoryW(dir.c_str(), nullptr))
            return true;
        return ::GetLastError() == ERROR_ALREADY_EXISTS;
    }

    //写一个临时文件来确认目录可写；Program Files 等受保护目录在这里返回 false
    bool IsDirectoryWritable(const std::wstring& dir)
    {
        const std::wstring probe = WithTrailingSlash(dir) + L"~pulsebar_write_probe.tmp";
        HANDLE handle = ::CreateFileW(probe.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
            FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            return false;
        ::CloseHandle(handle);
        return true;
    }

    //语言文件缺少对应键时回退到英文，避免弹出空白提示
    CString LoadMigrationText(const wchar_t* key, const wchar_t* fallback)
    {
        CString text = CCommon::LoadText(key);
        if (text.IsEmpty())
            text = fallback;
        return text;
    }

    void CopyDirectoryContents(const std::wstring& source, const std::wstring& target, int depth = 0)
    {
        if (depth > 8)      //异常目录结构兜底，避免无限递归
            return;
        const std::wstring pattern = WithTrailingSlash(source) + L"*";
        WIN32_FIND_DATAW find_data{};
        HANDLE find_handle = ::FindFirstFileW(pattern.c_str(), &find_data);
        if (find_handle == INVALID_HANDLE_VALUE)
            return;
        do
        {
            if (wcscmp(find_data.cFileName, L".") == 0 || wcscmp(find_data.cFileName, L"..") == 0)
                continue;
            const std::wstring source_item = WithTrailingSlash(source) + find_data.cFileName;
            const std::wstring target_item = WithTrailingSlash(target) + find_data.cFileName;
            if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                ::CreateDirectoryW(target_item.c_str(), nullptr);
                CopyDirectoryContents(source_item, target_item, depth + 1);
            }
            else
            {
                ::CopyFileW(source_item.c_str(), target_item.c_str(), FALSE);
            }
        } while (::FindNextFileW(find_handle, &find_data));
        ::FindClose(find_handle);
    }
}

bool CPulseBarApp::MigrateConfigDir(bool to_program_dir, CString& message)
{
    message.Empty();

    std::wstring target_dir;
    if (to_program_dir)
    {
        target_dir = m_module_dir;
    }
    else
    {
        try
        {
            target_dir = CCommon::GetAppDataConfigDir();
        }
        catch (CException* exception)
        {
            exception->Delete();
            message = LoadMigrationText(IDS_CFG_DIR_CREATE_FAILED, L"Cannot create the configuration folder: <%path%>.");
            message.Replace(_T("<%path%>"), CString(m_appdata_dir.c_str()));
            return false;
        }
    }
    target_dir = WithTrailingSlash(target_dir);
    if (target_dir.empty())
        return false;

    const std::wstring source_dir = WithTrailingSlash(m_config_dir);
    if (_wcsicmp(target_dir.c_str(), source_dir.c_str()) == 0)
        return true;        //同一目录，无需迁移

    if (!EnsureDirectoryExists(target_dir) || !IsDirectoryWritable(target_dir))
    {
        message = LoadMigrationText(IDS_CFG_DIR_NOT_WRITABLE, L"Cannot move data to: <%path%>. The folder is not writable. If PulseBar is installed under Program Files, move it to a writable folder or run it as administrator.");
        message.Replace(_T("<%path%>"), CString(target_dir.c_str()));
        return false;
    }

    //目标已有配置时先留一份，避免覆盖用户数据后无迹可寻
    const std::wstring target_config = target_dir + L"config.ini";
    if (::GetFileAttributesW(target_config.c_str()) != INVALID_FILE_ATTRIBUTES)
        ::CopyFileW(target_config.c_str(), (target_config + L".old").c_str(), FALSE);

    const wchar_t* const data_files[] = { L"config.ini", L"history_traffic.dat", L"history_traffic.dat.bak" };
    for (const auto* file_name : data_files)
    {
        const std::wstring source_file = source_dir + file_name;
        if (::GetFileAttributesW(source_file.c_str()) != INVALID_FILE_ATTRIBUTES)
            ::CopyFileW(source_file.c_str(), (target_dir + file_name).c_str(), FALSE);
    }
    const wchar_t* const data_dirs[] = { L"plugins", L"plugin-backups" };
    for (const auto* dir_name : data_dirs)
    {
        const std::wstring source_sub = source_dir + dir_name;
        const DWORD attributes = ::GetFileAttributesW(source_sub.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
            continue;
        const std::wstring target_sub = target_dir + dir_name;
        ::CreateDirectoryW(target_sub.c_str(), nullptr);
        CopyDirectoryContents(source_sub, target_sub);
    }

    //切换运行期路径，之后的 SaveConfig 与历史保存都会落到新目录
    m_config_dir = target_dir;
    m_config_path = target_dir + L"config.ini";
    m_history_traffic_path = target_dir + L"history_traffic.dat";
    m_log_path = target_dir + L"error.log";

    //global_cfg.ini 固定在程序目录，决定下次启动从哪里读取数据
    CIniHelper ini{ m_module_dir + L"global_cfg.ini" };
    ini.WriteBool(L"config", L"portable_mode", to_program_dir);
    ini.Save();

    message = LoadMigrationText(IDS_CFG_DIR_MIGRATED_INFO, L"Settings, traffic history and plugins were moved to: <%path%>. PulseBar will use the new location from now on; restarting is recommended.");
    message.Replace(_T("<%path%>"), CString(target_dir.c_str()));
    return true;
}

void CPulseBarApp::LoadPluginDisabledSettings()
{
    CIniHelper ini{ m_config_path };
    m_cfg_data.plugin_disabled.FromString(ini.GetString(L"config", L"plugin_disabled", L""));
}

void CPulseBarApp::LoadGlobalConfig()
{
    bool portable_mode_default{ false };
    wstring global_cfg_path{ m_module_dir + L"global_cfg.ini" };
    if (!CCommon::FileExist(global_cfg_path.c_str()))       //如果global_cfg.ini不存在，则根据AppData/Roaming/TrafficMonitor目录下是否存在config.ini来判断配置文件的保存位置
    {
        portable_mode_default = !CCommon::FileExist((m_appdata_dir + L"config.ini").c_str());
    }

    CIniHelper ini{ global_cfg_path };
    m_general_data.portable_mode = ini.GetBool(L"config", L"portable_mode", portable_mode_default);

    //执行一次保存操作，以检查当前目录是否有写入权限
    m_module_dir_writable = ini.Save();

    //GetTemplateDir 在取不到临时目录时返回空串；空串不能用于子串匹配（find(L"") 恒为 0）
    const wstring temp_dir = CCommon::GetTemplateDir();
    if (!temp_dir.empty() && m_module_dir.find(temp_dir) != wstring::npos)      //如果当前路径是在Temp目录下，则强制将数据保存到Appdata
    {
        m_module_dir_writable = false;
    }

    if (!m_module_dir_writable)              //如果当前目录没有写入权限，则设置配置保存到AppData目录
    {
        m_general_data.portable_mode = false;
    }
}

void CPulseBarApp::SaveGlobalConfig()
{
    CIniHelper ini{ m_module_dir + L"global_cfg.ini" };
    ini.WriteBool(L"config", L"portable_mode", m_general_data.portable_mode);

    //检查是否保存成功
    if (!ini.Save())
    {
        //if (m_cannot_save_global_config_warning)
        //{
        //    CString info;
        //    info.LoadString(IDS_CONNOT_SAVE_CONFIG_WARNING);
        //    info.Replace(_T("<%file_path%>"), m_module_dir.c_str());
        //    AfxMessageBox(info, MB_ICONWARNING);
        //}
        //m_cannot_save_global_config_warning = false;
        //return;
    }
}

int CPulseBarApp::DPI(int pixel)
{
    return m_dpi * pixel / 96;
}

void CPulseBarApp::DPI(CRect& rect)
{
    rect.left = DPI(rect.left);
    rect.right = DPI(rect.right);
    rect.top = DPI(rect.top);
    rect.bottom = DPI(rect.bottom);
}

void CPulseBarApp::DPIFromWindow(CWnd* pWnd)
{
    CWindowDC dc(pWnd);
    HDC hDC = dc.GetSafeHdc();
    m_dpi = GetDeviceCaps(hDC, LOGPIXELSY);
}

void CPulseBarApp::CheckUpdate(bool message)
{
    if (m_checking_update)      //如果还在检查更新，则直接返回
        return;
    CFlagLocker update_locker(m_checking_update);
    CWaitCursor wait_cursor;

    wstring version;        //程序版本
    wstring link;           //下载链接
    wstring contents_zh_cn; //更新内容（简体中文）
    wstring contents_en;    //更新内容（English）
    wstring contents_zh_tw; //更新内容（繁体中文）
    CUpdateHelper update_helper;
    update_helper.SetUpdateSource(static_cast<CUpdateHelper::UpdateSource>(m_general_data.update_source));
    if (!update_helper.CheckForUpdate())
    {
        if (message)
            AfxMessageBox(CCommon::LoadText(IDS_CHECK_UPDATE_FAILD), MB_OK | MB_ICONWARNING);
        return;
    }
    version = update_helper.GetVersion();
#ifdef _M_X64
    link = update_helper.GetLink64();
#elif defined _M_ARM64EC
    link = update_helper.GetLinkArm64ec();
#else
    link = update_helper.GetLink();
#endif
    contents_zh_cn = update_helper.GetContentsZhCn();
    contents_en = update_helper.GetContentsEn();
    contents_zh_tw = update_helper.GetContentsZhTw();
    if (version.empty() || link.empty())
    {
        if (message)
        {
            CString info = CCommon::LoadText(IDS_CHECK_UPDATE_ERROR);
            info += _T("\r\nrow_data=");
            info += std::to_wstring(update_helper.IsRowData()).c_str();

            AfxMessageBox(info, MB_OK | MB_ICONWARNING);
        }
        return;
    }
    if (version > VERSION)      //如果服务器上的版本大于本地版本
    {
        CString info;
        //根据语言设置选择对应语言版本的更新内容
        wstring language_tag = m_str_table.GetLanguageInfo().bcp_47;
        wstring contents_lan;
        if (language_tag == L"zh-CN")
            contents_lan = contents_zh_cn;
        else if (language_tag == L"zh-TW")
            contents_lan = contents_zh_tw;
        else
            contents_lan = contents_en;
        if (contents_lan.empty())
            info.Format(CCommon::LoadText(IDS_UPDATE_AVLIABLE), version.c_str());
        else
            info.Format(CCommon::LoadText(IDS_UPDATE_AVLIABLE2), version.c_str(), contents_lan.c_str());

        if (AfxMessageBox(info, MB_YESNO | MB_ICONQUESTION) == IDYES)
        {
            ShellExecute(NULL, _T("open"), link.c_str(), NULL, NULL, SW_SHOW);      //转到下载链接
        }
    }
    else
    {
        if (message)
            AfxMessageBox(CCommon::LoadText(IDS_ALREADY_UPDATED), MB_OK | MB_ICONINFORMATION);
    }
}

void CPulseBarApp::CheckUpdateInThread(bool message)
{
    AfxBeginThread(CheckUpdateThreadFunc, (LPVOID)message);
}

UINT CPulseBarApp::CheckUpdateThreadFunc(LPVOID lpParam)
{
    CCommon::SetThreadLanguage(theApp.m_general_data.language.language_id);     //设置线程语言
#ifndef _DEBUG      //DEBUG下不在启动时检查更新
    theApp.CheckUpdate(lpParam);        //检查更新
#endif
    //检测插件更新
    if (!theApp.m_plugins.GetPlugins().empty())
    {
        theApp.m_plugin_update.CheckForUpdate();
    }
    return 0;
}

UINT CPulseBarApp::InitOpenHardwareMonitorLibThreadFunc(LPVOID lpParam)
{
#ifndef WITHOUT_TEMPERATURE
    CSingleLock sync(&theApp.m_minitor_lib_critical, TRUE);
    theApp.m_pMonitor = OpenHardwareMonitorApi::CreateInstance();
    //状态只在指针真正可用时前进到 READY，否则保持 NONE（采样线程锁外就能跳过）
    if (theApp.m_pMonitor == nullptr)
        theApp.m_monitor_state = MONITOR_NONE;
    else
        theApp.m_monitor_state = MONITOR_READY;
    if (theApp.m_pMonitor == nullptr)
    {
        //这里运行在后台线程且持有 m_minitor_lib_critical，不能弹模态框：
        //模态框会阻塞主线程（主线程进设置页时要等同一把锁），而 .NET 异常文本可能为空。
        //与采样线程的硬件错误处理保持一致，改为写日志。
        auto monitor_error_message{ OpenHardwareMonitorApi::GetErrorMessage() };
        if (monitor_error_message.empty())
            CCommon::WriteLog(CCommon::LoadText(IDS_HARDWARE_INFO_ACQUIRE_FAILED_ERROR), theApp.m_log_path.c_str());
        else
            CCommon::WriteLog(monitor_error_message.c_str(), theApp.m_log_path.c_str());
    }
    else
    {
        //设置硬件监控的启用状态
        //（本线程已持有 m_minitor_lib_critical，CCriticalSection 是递归锁，可重入）
        theApp.UpdateOpenHardwareMonitorEnableState();

        //温度项名称只有在 GetHardwareInfo() 里才会被填充，CreateInstance() 不会填。
        //所以必须先扫一次硬件，再写快照；否则冷启动后立刻打开设置页时，
        //CPU/硬盘温度下拉框会是空的（这是比“打开慢”更严重的功能回归）。
        //扫描放在这个后台线程里，UI 线程只读快照，仍然不会等锁。
        //注意：错误文本必须在 __try 之前构造好——在 __except 里构造 CString 会触发
        //C2712（要求对象展开的函数中不能使用 __try）。与采样线程的写法保持一致。
        CString error_info = CCommon::LoadText(IDS_HARDWARE_INFO_ACQUIRE_FAILED_ERROR);
        auto getHardwareInfo = [&]() {
            __try
            {
                theApp.m_pMonitor->GetHardwareInfo();
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                CCommon::WriteLog(error_info, theApp.m_log_path.c_str());
            }
        };
        getHardwareInfo();

        auto monitor_error_message{ OpenHardwareMonitorApi::GetErrorMessage() };
        if (!monitor_error_message.empty())
            CCommon::WriteLog(monitor_error_message.c_str(), theApp.m_log_path.c_str());

        //扫描完把温度项名称写进快照，设置页从此不必再去抢这把锁
        theApp.RefreshTemperatureNameSnapshotLocked();
    }
#endif
    return 0;
}

#ifndef WITHOUT_TEMPERATURE
void CPulseBarApp::RefreshTemperatureNameSnapshotLocked()
{
    //调用方必须已持有 m_minitor_lib_critical（m_pMonitor 的所有访问都在该锁内）
    std::vector<std::wstring> cpu_names;
    std::vector<std::wstring> hdd_names;
    if (m_pMonitor != nullptr)
    {
        const auto& cpu_items = m_pMonitor->AllCpuTemperature();
        cpu_names.reserve(cpu_items.size());
        for (const auto& item : cpu_items)
            cpu_names.push_back(item.first);
        const auto& hdd_items = m_pMonitor->AllHDDTemperature();
        hdd_names.reserve(hdd_items.size());
        for (const auto& item : hdd_items)
            hdd_names.push_back(item.first);
    }
    CSingleLock names_sync(&m_temperature_names_critical, TRUE);
    //硬件项集合没变化时不做拷贝，避免采样线程每秒复制一遍字符串
    if (m_cpu_temperature_names == cpu_names && m_hdd_temperature_names == hdd_names)
        return;
    m_cpu_temperature_names.swap(cpu_names);
    m_hdd_temperature_names.swap(hdd_names);
}
#endif

bool  CPulseBarApp::SetAutoRun(bool auto_run, bool task_scheduler)
{
    if (task_scheduler)
    {
        //使用任务计划的方式设置开机自启动
        return SetAutoRunByTaskScheduler(auto_run);
    }
    else
    {
        //使用添加注册表项的方式实现开机自启动
        return SetAutoRunByRegistry(auto_run);
    }
}

bool CPulseBarApp::GetAutoRun(wstring* auto_run_path, bool task_scheduler)
{
    if (auto_run_path != nullptr)
        auto_run_path->clear();

    if (task_scheduler)
    {
        //使用任务计划的方式实现开机自启动
        return is_auto_start_task_active_for_this_user(auto_run_path);
    }
    else
    {
        //使用添加注册表项的方式实现开机自启动
        CRegKey key;
        if (key.Open(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run")) != ERROR_SUCCESS)
        {
            //打开注册表“Software\\Microsoft\\Windows\\CurrentVersion\\Run”失败，则返回false
            return false;
        }
        wchar_t buff[256]{};
        ULONG size{ 256 };
        bool found = key.QueryStringValue(APP_NAME, buff, &size) == ERROR_SUCCESS;
        if (!found)
        {
            //兼容旧版本写入的自启动项：读取旧名称，并迁移到新名称
            size = 256;
            if (key.QueryStringValue(LEGACY_APP_NAME, buff, &size) == ERROR_SUCCESS)
            {
                key.SetStringValue(APP_NAME, buff);
                key.DeleteValue(LEGACY_APP_NAME);
                found = true;
            }
        }
        if (found)
        {
            //旧版可能把路径连引号一起写进注册表，判断是否自启动前必须先去引号，
            //并且空值不能调用 front()/back()。
            wstring registered_path{ buff };
            if (!registered_path.empty() && registered_path.front() == L'\"')
                registered_path.erase(0, 1);
            if (!registered_path.empty() && registered_path.back() == L'\"')
                registered_path.pop_back();
            if (auto_run_path != nullptr)
                *auto_run_path = registered_path;
            return (m_module_path_reg == registered_path); //如果注册表的值是当前程序的路径，就返回true，否则返回false
        }
        else
        {
            return false;       //没有找到“TrafficMonitor”键，返回false
        }
    }
}

bool CPulseBarApp::SetAutoRunByRegistry(bool auto_run)
{
    CRegKey key;
    //打开注册表项
    if (key.Open(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run")) != ERROR_SUCCESS)
    {
        AfxMessageBox(CCommon::LoadText(IDS_AUTORUN_FAILED_NO_KEY), MB_OK | MB_ICONWARNING);
        return false;
    }
    if (auto_run)       //写入注册表项
    {
        //通过注册表设置开机自启动项时删除计划任务中的自启动项
        SetAutoRunByTaskScheduler(false);

        if (key.SetStringValue(APP_NAME, m_module_path_reg.c_str()) != ERROR_SUCCESS)
        {
            CString log;
            log.Format(L"开机自动运行：写入注册表 Run 项失败，程序路径=%s", m_module_path_reg.c_str());
            CCommon::WriteLog(log.GetString(), m_log_path.c_str());
            AfxMessageBox(CCommon::LoadText(IDS_AUTORUN_FAILED_NO_ACCESS), MB_OK | MB_ICONWARNING);
            return false;
        }
    }
    else        //删除注册表项
    {
        //删除前先检查注册表项是否存在，如果不存在，则直接返回
        wchar_t buff[256];
        ULONG size{ 256 };
        const bool exists = key.QueryStringValue(APP_NAME, buff, &size) == ERROR_SUCCESS;
        const bool legacy_exists = key.QueryStringValue(LEGACY_APP_NAME, buff, &size) == ERROR_SUCCESS;
        if (!exists && !legacy_exists)
            return false;
        if (legacy_exists && key.DeleteValue(LEGACY_APP_NAME) != ERROR_SUCCESS)
        {
            AfxMessageBox(CCommon::LoadText(IDS_AUTORUN_DELETE_FAILED), MB_OK | MB_ICONWARNING);
            return false;
        }
        if (exists && key.DeleteValue(APP_NAME) != ERROR_SUCCESS)
        {
            AfxMessageBox(CCommon::LoadText(IDS_AUTORUN_DELETE_FAILED), MB_OK | MB_ICONWARNING);
            return false;
        }
    }
    return true;
}

bool CPulseBarApp::SetAutoRunByTaskScheduler(bool auto_run)
{
    bool succeed = delete_auto_start_task_for_this_user();     //先删除开机自启动
    if (!succeed)
    {
        //删除失败时不能继续：否则旧任务残留，新任务可能与它冲突
        CCommon::WriteLog(L"开机自动运行：删除已有的计划任务失败，未继续创建新任务。", m_log_path.c_str());
        return false;
    }
    if (auto_run)
    {
        //通过计划任务设置开机自启动项时删除注册表中的自启动项
        SetAutoRunByRegistry(false);

        succeed = create_auto_start_task_for_this_user(true);

        //记录本次设置的结果，便于在“设置貌似成功但开机并未启动”时排查
        CString log;
        log.Format(L"开机自动运行：已设置为“任务计划”方式，结果=%s，程序路径=%s",
            succeed ? L"成功" : L"失败", m_module_path.c_str());
        CCommon::WriteLog(log.GetString(), m_log_path.c_str());
    }
    else
    {
        CCommon::WriteLog(L"开机自动运行：已取消“任务计划”方式。", m_log_path.c_str());
    }
    return succeed;
}

CString CPulseBarApp::GetSystemInfoString()
{
    CString info;
    info += _T("System Info:\r\n");

    CString strTmp;
    strTmp.Format(_T("Windows Version: %d.%d build %d"), m_win_version.GetMajorVersion(),
        m_win_version.GetMinorVersion(), m_win_version.GetBuildNumber());
    info += strTmp;

    if (m_win_version.IsWine())
        info += _T(" (Wine)");

    info += _T("\r\n");

    strTmp.Format(_T("DPI: %d"), m_dpi);
    info += strTmp;
    info += _T("\r\n");

    info += _T("Version: ");
    info += VERSION;
    info += _T(" ");
#ifdef _M_X64
    info += _T("x64");
#elif defined _M_ARM64EC
    info += _T("Arm64EC");
#else
    info += _T("x86");
#endif

#ifdef WITHOUT_TEMPERATURE
    info += _T(" (Lite)");
#endif

    info += _T("\r\nLast compiled date: ");
    info += CCommon::GetLastCompileTime();

    return info;
}


namespace
{
    bool MenuContainsCommand(CMenu* menu, UINT command_id)
    {
        if (menu == nullptr)
            return false;

        for (int index = 0; index < menu->GetMenuItemCount(); ++index)
        {
            if (menu->GetMenuItemID(index) == command_id)
                return true;

            CMenu* sub_menu = menu->GetSubMenu(index);
            if (sub_menu != nullptr && MenuContainsCommand(sub_menu, command_id))
                return true;
        }
        return false;
    }

    CMenu* FindMenuContainingCommand(CMenu* menu, UINT command_id)
    {
        if (menu == nullptr)
            return nullptr;

        for (int index = 0; index < menu->GetMenuItemCount(); ++index)
        {
            if (menu->GetMenuItemID(index) == command_id)
                return menu;
        }

        for (int index = 0; index < menu->GetMenuItemCount(); ++index)
        {
            CMenu* sub_menu = menu->GetSubMenu(index);
            CMenu* owner = FindMenuContainingCommand(sub_menu, command_id);
            if (owner != nullptr)
                return owner;
        }
        return nullptr;
    }

    bool FindSubmenuItem(CMenu* menu, UINT command_id, CMenu*& parent_menu, int& position)
    {
        if (menu == nullptr)
            return false;

        for (int index = 0; index < menu->GetMenuItemCount(); ++index)
        {
            CMenu* sub_menu = menu->GetSubMenu(index);
            if (sub_menu != nullptr && MenuContainsCommand(sub_menu, command_id))
            {
                parent_menu = menu;
                position = index;
                return true;
            }

            if (sub_menu != nullptr && FindSubmenuItem(sub_menu, command_id, parent_menu, position))
                return true;
        }
        return false;
    }
}

void CPulseBarApp::InitMenuResourse()
{
    //载入菜单（主窗口已移除，仅保留任务栏窗口右键菜单）
    CCommon::LoadMenuResource(m_taskbar_menu, IDR_TASK_BAR_MENU);
    CCommon::LoadMenuResource(m_taskbar_menu_plugin, IDR_TASK_BAR_MENU);

    //为插件菜单添加额外项目
    m_taskbar_menu_plugin_sub_menu.CreatePopupMenu();
    m_taskbar_menu_plugin_sub_menu.AppendMenu(MF_STRING | MF_ENABLED, ID_PLUGIN_OPTIONS_TASKBAR, CCommon::LoadText(IDS_PLUGIN_OPTIONS, _T("...")));
    m_taskbar_menu_plugin_sub_menu.AppendMenu(MF_STRING | MF_ENABLED, ID_PLUGIN_DETAIL_TASKBAR, CCommon::LoadText(IDS_PLUGIN_INFO, _T("...")));
    CMenuIcon::AddIconToMenuItem(m_taskbar_menu_plugin_sub_menu.GetSafeHmenu(), ID_PLUGIN_OPTIONS_TASKBAR, FALSE, GetMenuIcon(IDI_SETTINGS));
    CMenuIcon::AddIconToMenuItem(m_taskbar_menu_plugin_sub_menu.GetSafeHmenu(), ID_PLUGIN_DETAIL_TASKBAR, FALSE, GetMenuIcon(IDI_INFO));
    CMenu* taskbar_menu_plugin = FindMenuContainingCommand(&m_taskbar_menu_plugin, ID_DISPLAY_SETTINGS);
    if (taskbar_menu_plugin != nullptr)
    {
        //插件命令子菜单放在“插件管理”之后，而不是排在“退出”之后。
        int insert_pos = taskbar_menu_plugin->GetMenuItemCount();
        for (int i = 0; i < taskbar_menu_plugin->GetMenuItemCount(); ++i)
        {
            if (taskbar_menu_plugin->GetMenuItemID(i) == ID_APP_EXIT)
            {
                insert_pos = i;
                break;
            }
        }
        taskbar_menu_plugin->InsertMenu(insert_pos++, MF_BYPOSITION | MF_SEPARATOR);
        taskbar_menu_plugin->InsertMenu(insert_pos, MF_BYPOSITION | MF_POPUP | MF_STRING,
            (UINT_PTR)m_taskbar_menu_plugin_sub_menu.m_hMenu, _T("<plugin name>"));
    }

    //托盘菜单直接在“插件管理”之后插入每个成功插件的名称 popup，
    //不再增加一层泛化的“插件”中间菜单。
    UpdateTrayPluginMenu();

    //为菜单项添加图标。所有子菜单均按命令 ID 递归定位，避免依赖资源中的固定位置。
    auto addIconForCommand = [&](CMenu* menu, UINT command_id, UINT icon_id)
    {
        CMenu* command_menu = FindMenuContainingCommand(menu, command_id);
        if (command_menu != nullptr)
            CMenuIcon::AddIconToMenuItem(command_menu->GetSafeHmenu(), command_id, FALSE, GetMenuIcon(icon_id));
    };

    auto addIconsForTaskbarMenu = [&](const CMenu& menu)
    {
        CMenu* menu_root = FindMenuContainingCommand(const_cast<CMenu*>(&menu), ID_DISPLAY_SETTINGS);
        if (menu_root == nullptr)
            return;

        CMenu* parent_menu{};
        int submenu_position{};
        if (FindSubmenuItem(menu_root, ID_SELETE_CONNECTION, parent_menu, submenu_position))
            CMenuIcon::AddIconToMenuItem(parent_menu->GetSafeHmenu(), submenu_position, TRUE, GetMenuIcon(IDI_CONNECTION));
        if (FindSubmenuItem(menu_root, ID_OPEN_TASK_MANAGER, parent_menu, submenu_position))
            CMenuIcon::AddIconToMenuItem(parent_menu->GetSafeHmenu(), submenu_position, TRUE, GetMenuIcon(IDI_FUNCTION));
        if (FindSubmenuItem(menu_root, ID_HELP, parent_menu, submenu_position))
            CMenuIcon::AddIconToMenuItem(parent_menu->GetSafeHmenu(), submenu_position, TRUE, GetMenuIcon(IDI_HELP));

        //勾选型菜单项不加图标，避免与对勾冲突。
        addIconForCommand(menu_root, ID_NETWORK_INFO, IDI_INFO);
        addIconForCommand(menu_root, ID_TRAFFIC_HISTORY, IDI_STATISTICS);
        addIconForCommand(menu_root, ID_OPEN_TASK_MANAGER, IDI_TASK_MANAGER);
        addIconForCommand(menu_root, ID_DISPLAY_SETTINGS, IDI_ITEM);
        addIconForCommand(menu_root, ID_SHOW_SPEED_GRAPH_TIP, IDI_MOUSE);
        addIconForCommand(menu_root, ID_PLUGIN_MANAGE, IDI_PLUGINS);
        addIconForCommand(menu_root, ID_OPTIONS2, IDI_SETTINGS);
        addIconForCommand(menu_root, ID_SHOW_TASK_BAR_WND, IDI_TASKBAR_WINDOW);
        addIconForCommand(menu_root, ID_HELP, IDI_HELP);
        addIconForCommand(menu_root, ID_APP_ABOUT, IDR_MAINFRAME);
        addIconForCommand(menu_root, ID_APP_EXIT, IDI_EXIT);
    };
    addIconsForTaskbarMenu(m_taskbar_menu);
    addIconsForTaskbarMenu(m_taskbar_menu_plugin);
}


HICON CPulseBarApp::GetMenuIcon(UINT id)
{
    auto iter = m_menu_icons.find(id);
    if (iter != m_menu_icons.end())
    {
        return iter->second;
    }
    else
    {
        HICON hIcon = CCommon::LoadIconResource(id, DPI(16));
        m_menu_icons[id] = hIcon;
        return hIcon;
    }
}

int CPulseBarApp::NormalizeNotifyIconSelection(int selected)
{
    return selected >= 0 && selected < MAX_NOTIFY_ICON_STYLE ? selected : NOTIFY_ICON_BLUE;
}

int CPulseBarApp::SelectNotifyIconForTheme(int selected, bool light_mode)
{
    selected = NormalizeNotifyIconSelection(selected);
    if (selected == NOTIFY_ICON_BLUE)
        return selected;
    return light_mode ? NOTIFY_ICON_DARK : NOTIFY_ICON_LIGHT;
}

int CPulseBarApp::ResolveNotifyIconForDisplay() const
{
    const int selected = NormalizeNotifyIconSelection(m_cfg_data.m_notify_icon_selected);
    if (selected != NOTIFY_ICON_AUTO)
        return selected;
    //“自动”：浅色任务栏用深色图标，深色任务栏用浅色图标；Win10 之前任务栏固定深色。
    if (m_win_version.GetMajorVersion() < 10)
        return NOTIFY_ICON_DARK;
    return CWindowsSettingHelper::IsWindows10LightTheme() ? NOTIFY_ICON_DARK : NOTIFY_ICON_LIGHT;
}

void CPulseBarApp::LoadNotifyIcons()
{
    DestroyNotifyIcons();
    const UINT resources[] = { IDR_MAINFRAME, IDI_PULSEBAR_TRAY_LIGHT, IDI_PULSEBAR_TRAY_DARK };
    for (int index = 0; index < MAX_NOTIFY_ICON; ++index)
    {
        m_notify_icons[index] = static_cast<HICON>(::LoadImage(AfxGetInstanceHandle(),
            MAKEINTRESOURCE(resources[index]), IMAGE_ICON, DPI(16), DPI(16), LR_DEFAULTCOLOR));
    }
}

void CPulseBarApp::DestroyNotifyIcons()
{
    for (HICON& icon : m_notify_icons)
    {
        if (icon != nullptr)
            ::DestroyIcon(icon);
        icon = nullptr;
    }
}

HICON CPulseBarApp::GetNotifyIcon(int selected)
{
    if (selected == NOTIFY_ICON_AUTO)
        selected = ResolveNotifyIconForDisplay();
    //这里索引的是图标资源数组，取值必须落在 0..2；样式层面的规范化见
    //NormalizeNotifyIconSelection（0..3，含“自动”）。
    if (selected < 0 || selected >= MAX_NOTIFY_ICON)
        selected = NOTIFY_ICON_BLUE;
    HICON icon = m_notify_icons[selected];
    if (icon == nullptr)
        icon = m_notify_icons[NOTIFY_ICON_BLUE];
    if (icon == nullptr)
        icon = LoadIcon(IDR_MAINFRAME);    //共享资源句柄，不写入需自行释放的图标数组。
    return icon;
}

// 唯一的一个 CPulseBarApp 对象

CPulseBarApp theApp;


// CPulseBarApp 初始化

BOOL CPulseBarApp::InitInstance()
{
    //替换掉对话框程序的默认类名
    WNDCLASS wc;
    ::GetClassInfo(AfxGetInstanceHandle(), _T("#32770"), &wc);       //MFC默认的所有对话框的类名为#32770
    wc.lpszClassName = APP_CLASS_NAME;      //将对话框的类名修改为新类名
    AfxRegisterClass(&wc);

    //设置配置文件的路径
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    m_module_path = path;
    if (m_module_path.find(L' ') != wstring::npos)
    {
        //如果路径中有空格，则在程序路径前后添加双引号
        m_module_path_reg = L'\"';
        m_module_path_reg += m_module_path;
        m_module_path_reg += L'\"';
    }
    else
    {
        m_module_path_reg = m_module_path;
    }
    m_module_dir = CCommon::GetModuleDir();
    m_system_dir = CCommon::GetSystemDir();
    m_appdata_dir = CCommon::GetAppDataConfigDir(false);

    LoadGlobalConfig();

#ifdef _DEBUG
    m_config_dir = L".\\";
#else
    if (m_general_data.portable_mode)
        m_config_dir = m_module_dir;
    else
        m_config_dir = CCommon::GetAppDataConfigDir();
#endif
    //AppData里面的程序配置文件路径
    m_config_path = m_config_dir + L"config.ini";
    m_history_traffic_path = m_config_dir + L"history_traffic.dat";
    m_log_path = m_config_dir + L"error.log";

    //#ifndef _DEBUG
    //  //原来的、程序所在目录下的配置文件的路径
    //  wstring config_path_old = m_module_dir + L"config.ini";
    //  wstring history_traffic_path_old = m_module_dir + L"history_traffic.dat";
    //  wstring log_path_old = m_module_dir + L"error.log";
    //  //如果程序所在目录下含有配置文件，则将其移动到AppData对应的目录下面
    //  CCommon::MoveAFile(config_path_old.c_str(), m_config_path.c_str());
    //  CCommon::MoveAFile(history_traffic_path_old.c_str(), m_history_traffic_path.c_str());
    //  CCommon::MoveAFile(log_path_old.c_str(), m_log_path.c_str());
    //#endif // !_DEBUG

    LoadLanguageConfig();

    //初始化界面语言
    CCommon::SetThreadLanguage(m_general_data.language.language_id);

    //初始化字符串资源
    m_str_table.Init();

    //检查是否已有实例正在运行
    CIniHelper ini{ m_config_path };
    LPCTSTR mutex_name{};
    LPCTSTR legacy_mutex_name{};
#ifdef _DEBUG
    mutex_name = _T("PulseBar-e8Ahk24HP6JC8hDy");
    legacy_mutex_name = _T("TrafficMonitor-e8Ahk24HP6JC8hDy");
#else
    mutex_name = _T("PulseBar-1419J3XLKL1w8OZc");
    legacy_mutex_name = _T("TrafficMonitor-1419J3XLKL1w8OZc");
#endif
    //先处理改名前版本持有的互斥量，避免 PulseBar 与旧 TrafficMonitor 并行运行。
    HANDLE hLegacyMutex = ::OpenMutex(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, legacy_mutex_name);
    if (hLegacyMutex != nullptr)
    {
        HWND h_legacy = ::FindWindow(APP_CLASS_NAME, nullptr);
        if (h_legacy != nullptr)
            ::PostMessage(h_legacy, PB_QUIT_REQUEST_MESSAGE, 0, 0);

        const DWORD legacy_wait = ::WaitForSingleObject(hLegacyMutex, 5000);
        if (legacy_wait == WAIT_OBJECT_0 || legacy_wait == WAIT_ABANDONED)
        {
            ::ReleaseMutex(hLegacyMutex);
            ::CloseHandle(hLegacyMutex);
        }
        else
        {
            ::CloseHandle(hLegacyMutex);
            return FALSE;
        }
    }
    else if (::GetLastError() != ERROR_FILE_NOT_FOUND)
    {
        return FALSE;
    }

    //已有实例在运行时，先请求它退出，再接手单实例互斥量。
    //这样双击新版本即可完成替换，不需要用户手动退出旧实例。
    HANDLE hMutex = nullptr;
    for (int attempt = 0; attempt < 50; ++attempt)
    {
        hMutex = ::CreateMutex(NULL, TRUE, mutex_name);
        if (hMutex == nullptr)
            break;
        if (::GetLastError() != ERROR_ALREADY_EXISTS)
            break;              //成功成为唯一实例
        ::CloseHandle(hMutex);
        hMutex = nullptr;
        HWND h_existing = ::FindWindow(APP_CLASS_NAME, nullptr);
        if (h_existing != nullptr)
            ::PostMessage(h_existing, PB_QUIT_REQUEST_MESSAGE, 0, 0);
        ::Sleep(100);           //等待旧实例保存配置并退出
    }
    if (hMutex == nullptr)
        return FALSE;           //旧实例仍未退出：本次启动静默结束

    //初始化GDI+
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, NULL);

    //初始时使用屏幕DPI
    DPIFromWindow(nullptr);

    //载入插件
    LoadPluginDisabledSettings();
    m_plugins.LoadPlugins();

    //从ini文件载入设置
    LoadConfig();
    PublishMonitorSettings(); //No background sampling/hardware initialization before this publication.

    m_taskbar_default_style.LoadConfig();

    //SaveConfig();

    // 如果一个运行在 Windows XP 上的应用程序清单指定要
    // 使用 ComCtl32.dll 版本 6 或更高版本来启用可视化方式，
    //则需要 InitCommonControlsEx()。  否则，将无法创建窗口。
    INITCOMMONCONTROLSEX InitCtrls;
    InitCtrls.dwSize = sizeof(InitCtrls);
    // 将它设置为包括所有要在应用程序中使用的
    // 公共控件类。
    InitCtrls.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&InitCtrls);

    CWinApp::InitInstance();


    AfxEnableControlContainer();

    // 创建 shell 管理器，以防对话框包含
    // 任何 shell 树视图控件或 shell 列表视图控件。
    CShellManager* pShellManager = new CShellManager;

    // 激活“Windows Native”视觉管理器，以便在 MFC 控件中启用主题
    CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

    // 标准初始化
    // 如果未使用这些功能并希望减小
    // 最终可执行文件的大小，则应移除下列
    // 不需要的特定初始化例程
    // 更改用于存储设置的注册表项
    // TODO: 应适当修改该字符串，
    // 例如修改为公司或组织名
    //SetRegistryKey(_T("应用程序向导生成的本地应用程序"));        //暂不使用注册表保存数据

    //启动时检查更新
    if (m_general_data.check_update_when_start)
    {
        CheckUpdateInThread(false);
    }

#ifndef WITHOUT_TEMPERATURE
    //检测是否安装.net framework 4.5
    if (!CWindowsSettingHelper::IsDotNetFramework4Point5Installed())
    {
        if (theApp.m_show_dot_net_notinstalled_tip)
        {
            if (AfxMessageBox(CCommon::LoadText(IDS_DOTNET_NOT_INSTALLED_TIP), MB_OKCANCEL | MB_ICONWARNING) == IDCANCEL)       //点击“取消”不再提示
            {
                theApp.m_show_dot_net_notinstalled_tip = false;
                SaveConfig();
            }
        }
    }
    else
    {
        //如果没有开启任何一项的硬件监控，则不初始化OpenHardwareMonitor
        if (theApp.m_general_data.IsHardwareEnable(HI_CPU) || theApp.m_general_data.IsHardwareEnable(HI_GPU)
            || theApp.m_general_data.IsHardwareEnable(HI_HDD) || theApp.m_general_data.IsHardwareEnable(HI_MBD))
        {
            //启动初始化OpenHardwareMonitor的线程。由于OpenHardwareMonitor初始化需要一定的时间，为了防止启动时程序卡顿，将其放到后台线程中处理
            InitOpenHardwareLibInThread();
        }
    }
#endif

    //执行测试代码
#ifdef _DEBUG
    CTest::Test();
#endif

    SendSettingsToPlugin();

    CPulseBarDlg dlg;
    m_pMainWnd = &dlg;
    if (dlg.Create(IDD_TRAFFICMONITOR_DIALOG))
    {
        //隐藏宿主仍需消息循环，不能使用会在空闲时显示窗口的 DoModal。
        dlg.RunModalLoop(MLF_NOIDLEMSG | MLF_NOKICKIDLE);
        if (::IsWindow(dlg.GetSafeHwnd()))
            dlg.DestroyWindow();
    }
    else
    {
        TRACE(traceAppMsg, 0, "警告: 应用宿主创建失败，程序将退出。\n");
    }
    m_pMainWnd = nullptr;

    // 删除上面创建的 shell 管理器。
    if (pShellManager != NULL)
    {
        delete pShellManager;
    }

#ifndef _AFXDLL
    ControlBarCleanUp();
#endif

    // 由于对话框已关闭，所以将返回 FALSE 以便退出应用程序，
    //  而不是启动应用程序的消息泵。
    return FALSE;
}

void CPulseBarApp::InitOpenHardwareLibInThread()
{
#ifndef WITHOUT_TEMPERATURE
    //避免重复初始化：冷启动时初始化线程已持锁在跑，用户此刻改设置并点“确定”
    //会再次走到这里。若此时再起一个线程，两个线程会在同一把锁上排队，
    //第二个仍会重新 CreateInstance()，白等一次 .NET 加载（数秒）。
    //这里用 compare_exchange 抢占状态：只有 NONE 才能推进到 INITIALIZING。
    int expected = MONITOR_NONE;
    if (!m_monitor_state.compare_exchange_strong(expected, MONITOR_INITIALIZING))
        return;
    AfxBeginThread(InitOpenHardwareMonitorLibThreadFunc, NULL);
#endif
}


void CPulseBarApp::PublishMonitorSettings()
{
    // Only the UI thread reads m_general_data and publishes its committed sampling fields.
    MonitorSettings settings;
    settings.monitor_time_span = m_general_data.monitor_time_span;
    settings.cpu_usage_acquire_method = m_general_data.cpu_usage_acquire_method;
    settings.hardware_monitor_item = m_general_data.hardware_monitor_item;
    settings.hard_disk_name = m_general_data.hard_disk_name;
    settings.cpu_core_name = m_general_data.cpu_core_name;
    m_monitor_settings.Publish(std::move(settings));
}

std::shared_ptr<const MonitorSettings> CPulseBarApp::GetMonitorSettingsSnapshot() const
{
    return m_monitor_settings.Read();
}

void CPulseBarApp::UpdateOpenHardwareMonitorEnableState()
{
#ifndef WITHOUT_TEMPERATURE
    // Read after acquiring the hardware lock so a waiting initialization/Apply uses
    // the latest committed version. Read() releases its own lock before any API call;
    // publication never takes the hardware lock, so this order cannot form a cycle.
    CSingleLock sync(&theApp.m_minitor_lib_critical, TRUE);
    const auto settings = GetMonitorSettingsSnapshot();
    if (m_pMonitor != nullptr)
    {
        m_pMonitor->SetCpuEnable(settings->IsHardwareEnable(HI_CPU));
        m_pMonitor->SetGpuEnable(settings->IsHardwareEnable(HI_GPU));
        m_pMonitor->SetHddEnable(settings->IsHardwareEnable(HI_HDD));
        m_pMonitor->SetMainboardEnable(settings->IsHardwareEnable(HI_MBD));
    }
#endif
}

//void CPulseBarApp::UpdateTaskbarWndMenu()
//{
//    //获取“显示设置”子菜单
//    CMenu* pMenu = m_taskbar_menu.GetSubMenu(0)->GetSubMenu(5);
//    ASSERT(pMenu != nullptr);
//    if (pMenu != nullptr)
//    {
//        //将ID_SHOW_MEMORY_USAGE后面的所有菜单项删除
//        if (pMenu->GetMenuItemCount() > 4)
//        {
//            int start_pos = CCommon::GetMenuItemPosition(pMenu, ID_SHOW_MEMORY_USAGE) + 1;
//            while (pMenu->GetMenuItemCount() > start_pos)
//            {
//                pMenu->DeleteMenu(start_pos, MF_BYPOSITION);
//            }
//        }
//
//        //添加温度相关菜单项
//#ifndef WITHOUT_TEMPERATURE
//        if (m_general_data.IsHardwareEnable(HI_GPU))
//            pMenu->AppendMenu(MF_STRING | MF_ENABLED, ID_SHOW_GPU, CCommon::LoadText(IDS_GPU_USAGE));
//        if (m_general_data.IsHardwareEnable(HI_CPU))
//            pMenu->AppendMenu(MF_STRING | MF_ENABLED, ID_SHOW_CPU_TEMPERATURE, CCommon::LoadText(IDS_CPU_TEMPERATURE));
//        if (m_general_data.IsHardwareEnable(HI_GPU))
//            pMenu->AppendMenu(MF_STRING | MF_ENABLED, ID_SHOW_GPU_TEMPERATURE, CCommon::LoadText(IDS_GPU_TEMPERATURE));
//        if (m_general_data.IsHardwareEnable(HI_HDD))
//            pMenu->AppendMenu(MF_STRING | MF_ENABLED, ID_SHOW_HDD_TEMPERATURE, CCommon::LoadText(IDS_HDD_TEMPERATURE));
//        if (m_general_data.IsHardwareEnable(HI_MBD))
//            pMenu->AppendMenu(MF_STRING | MF_ENABLED, ID_SHOW_MAIN_BOARD_TEMPERATURE, CCommon::LoadText(IDS_MAINBOARD_TEMPERATURE));
//        if (m_general_data.IsHardwareEnable(HI_HDD))
//            pMenu->AppendMenu(MF_STRING | MF_ENABLED, ID_SHOW_HDD, CCommon::LoadText(IDS_HDD_USAGE));
//#endif
//
//        pMenu->AppendMenu(MF_STRING | MF_ENABLED, ID_SHOW_TOTAL_SPEED, CCommon::LoadText(IDS_TOTAL_NET_SPEED));
//
//        //添加插件菜单项
//        if (!m_plugins.GetPluginItems().empty())
//            pMenu->AppendMenu(MF_SEPARATOR);
//        int plugin_index = 0;
//        for (const auto& plugin_item : m_plugins.GetPluginItems())
//        {
//            pMenu->AppendMenu(MF_STRING | MF_ENABLED, ID_SHOW_PLUGIN_ITEM_START + plugin_index, plugin_item->GetItemName());
//            plugin_index++;
//        }
//    }
//}

bool CPulseBarApp::IsForceShowNotifyIcon()
{
    //主窗口已移除：任务栏窗口是唯一的常驻界面，通知区图标必须始终可用
    return !m_cfg_data.m_show_task_bar_wnd;
}

std::wstring CPulseBarApp::GetPlauginTooltipInfo() const
{
    std::wstring tip_info;
    for (const auto& item : m_plugins.GetPlugins())
    {
        if (item.plugin != nullptr && item.plugin->GetAPIVersion() >= 2)
        {
            //GetTooltipInfo() 按 ABI 可以返回空指针（表示“没有额外提示信息”），
            //直接用它构造 std::wstring 就是 wcslen(nullptr)，会让任务栏刷新线程崩溃。
            const wchar_t* tooltip = item.plugin->GetTooltipInfo();
            if (tooltip == nullptr)
                continue;
            std::wstring plugin_tooltip = tooltip;
            CCommon::StringNormalize(plugin_tooltip);
            if (!plugin_tooltip.empty())
            {
                tip_info += L"\r\n";
                tip_info += plugin_tooltip.c_str();
            }
        }
    }
    return tip_info;
}

bool CPulseBarApp::IsTaksbarItemDisplayed(CommonDisplayItem item) const
{
    if (item.IsPlugin())
    {
        if (item.PluginItem() != nullptr && item.PluginItem()->GetItemId() != nullptr)
            return m_taskbar_data.plugin_display_item.Contains(item.PluginItem()->GetItemId());
    }
    else
    {
        return m_taskbar_data.display_item.Contains(item.ItemType());
    }
    return false;
}

void CPulseBarApp::SendSettingsToPlugin()
{
    for (const auto& plugin_info : m_plugins.GetPlugins())
    {
        if (plugin_info.plugin != nullptr && plugin_info.plugin->GetAPIVersion() >= 2)
        {
            plugin_info.plugin->OnExtenedInfo(ITMPlugin::EI_NAIN_WND_NET_SPEED_SHORT_MODE, std::to_wstring(m_main_wnd_data.speed_short_mode).c_str());
            plugin_info.plugin->OnExtenedInfo(ITMPlugin::EI_MAIN_WND_SPERATE_WITH_SPACE, std::to_wstring(m_main_wnd_data.separate_value_unit_with_space).c_str());
            plugin_info.plugin->OnExtenedInfo(ITMPlugin::EI_MAIN_WND_UNIT_BYTE, std::to_wstring(m_main_wnd_data.unit_byte).c_str());
            plugin_info.plugin->OnExtenedInfo(ITMPlugin::EI_MAIN_WND_UNIT_SELECT, std::to_wstring(static_cast<int>(m_main_wnd_data.speed_unit)).c_str());
            plugin_info.plugin->OnExtenedInfo(ITMPlugin::EI_MAIN_WND_NOT_SHOW_UNIT, std::to_wstring(m_main_wnd_data.hide_unit).c_str());
            plugin_info.plugin->OnExtenedInfo(ITMPlugin::EI_MAIN_WND_NOT_SHOW_PERCENT, std::to_wstring(m_main_wnd_data.hide_percent).c_str());

            plugin_info.plugin->OnExtenedInfo(ITMPlugin::EI_TASKBAR_WND_NET_SPEED_SHORT_MODE, std::to_wstring(m_taskbar_data.speed_short_mode).c_str());
            plugin_info.plugin->OnExtenedInfo(ITMPlugin::EI_TASKBAR_WND_SPERATE_WITH_SPACE, std::to_wstring(m_taskbar_data.separate_value_unit_with_space).c_str());
            plugin_info.plugin->OnExtenedInfo(ITMPlugin::EI_TASKBAR_WND_VALUE_RIGHT_ALIGN, std::to_wstring(m_taskbar_data.value_right_align).c_str());
            plugin_info.plugin->OnExtenedInfo(ITMPlugin::EI_TASKBAR_WND_NET_SPEED_WIDTH, std::to_wstring(m_taskbar_data.digits_number).c_str());
            plugin_info.plugin->OnExtenedInfo(ITMPlugin::EI_TASKBAR_WND_UNIT_BYTE, std::to_wstring(m_taskbar_data.unit_byte).c_str());
            plugin_info.plugin->OnExtenedInfo(ITMPlugin::EI_TASKBAR_WND_UNIT_SELECT, std::to_wstring(static_cast<int>(m_taskbar_data.speed_unit)).c_str());
            plugin_info.plugin->OnExtenedInfo(ITMPlugin::EI_TASKBAR_WND_NOT_SHOW_UNIT, std::to_wstring(m_taskbar_data.hide_unit).c_str());
            plugin_info.plugin->OnExtenedInfo(ITMPlugin::EI_TASKBAR_WND_NOT_SHOW_PERCENT, std::to_wstring(m_taskbar_data.hide_percent).c_str());
        }
    }
}

void CPulseBarApp::UpdatePluginMenu(CMenu* pMenu, ITMPlugin* plugin, int plugin_cmd_start_index)
{
    if (pMenu != nullptr)
    {
        //删除菜单已经存在的插件命令
        while (pMenu->GetMenuItemCount() > plugin_cmd_start_index)
        {
            if (!pMenu->DeleteMenu(pMenu->GetMenuItemCount() - 1, MF_BYPOSITION))
                break;
        }
        //添加插件命令
        if (plugin != nullptr && plugin->GetAPIVersion() >= 5)
        {
            int plugin_cmd_count = plugin->GetCommandCount();
            //插件命令 ID 使用一段固定区间，数量超出时必须截断，
            //否则会与其它菜单命令 ID 冲突。
            //区间是 [ID_PLUGIN_COMMAND_START, ID_PLUGIN_COMMAND_MAX] 的闭区间，
            //宽度为 MAX-START+1（257），少加 1 会白白丢掉最后一个可用 ID。
            const int plugin_cmd_limit = ID_PLUGIN_COMMAND_MAX - ID_PLUGIN_COMMAND_START + 1;
            if (plugin_cmd_count > plugin_cmd_limit)
                plugin_cmd_count = plugin_cmd_limit;
            if (plugin_cmd_count < 0)
                plugin_cmd_count = 0;
            //添加分隔符
            if (plugin_cmd_count > 0)
                pMenu->AppendMenu(MF_SEPARATOR);
            for (int i = 0; i < plugin_cmd_count; i++)
            {
                const wchar_t* cmd_name = plugin->GetCommandName(i);
                if (cmd_name != nullptr)
                {
                    pMenu->AppendMenu(MF_STRING | MF_ENABLED, ID_PLUGIN_COMMAND_START + i, cmd_name);
                    //添加命令图标
                    HICON cmd_icon = (HICON)plugin->GetCommandIcon(i);
                    CMenuIcon::AddIconToMenuItem(pMenu->GetSafeHmenu(), ID_PLUGIN_COMMAND_START + i, FALSE, cmd_icon);
                }
            }
        }
    }
}

void CPulseBarApp::UpdateTrayPluginMenu()
{
    if (m_tray_plugin_menu_active || m_tray_plugin_menu_building)
        return;
    CFlagLocker building(m_tray_plugin_menu_building);
    CMenu* parent = FindMenuContainingCommand(&m_taskbar_menu, ID_PLUGIN_MANAGE);
    m_tray_plugin_actions.clear();
    if (parent == nullptr)
    {
        m_tray_plugin_popup_handles.clear();
        m_tray_plugin_parent_menu = nullptr;
        return;
    }
    if (m_tray_plugin_parent_menu == parent->GetSafeHmenu())
    {
        for (int position=parent->GetMenuItemCount()-1; position>=0; --position)
        {
            CMenu* child = parent->GetSubMenu(position);
            if (child != nullptr && std::find(m_tray_plugin_popup_handles.begin(),
                m_tray_plugin_popup_handles.end(), child->GetSafeHmenu()) != m_tray_plugin_popup_handles.end())
                parent->DeleteMenu(position, MF_BYPOSITION);
        }
    }
    m_tray_plugin_popup_handles.clear();
    m_tray_plugin_parent_menu = parent->GetSafeHmenu();
    int insertion=-1;
    for (int position=0; position<parent->GetMenuItemCount(); ++position)
        if (parent->GetMenuItemID(position)==ID_PLUGIN_MANAGE) { insertion=position+1; break; }
    if (insertion < 0)
        return;

    const auto& plugins = m_plugins.GetPlugins();
    const unsigned int capacity = ID_PLUGIN_TRAY_MAX-ID_PLUGIN_TRAY_START+1;
    unsigned int remaining_bases{};
    for (const auto& info : plugins)
        if (info.plugin != nullptr && info.state==CPluginManager::PluginState::PS_SUCCEED)
            remaining_bases += 3;
    remaining_bases = min(remaining_bases, capacity);
    const auto safe_call = [&](auto call, auto fallback) {
        try { return call(); }
        catch (CException* exception) { exception->Delete(); }
        catch (...) {}
        CCommon::WriteLog("Plugin menu callback failed; using a safe fallback.", m_log_path.c_str());
        return fallback;
    };
    for (const auto& info : plugins)
    {
        if (info.plugin == nullptr || info.state != CPluginManager::PluginState::PS_SUCCEED)
            continue;
        if (remaining_bases < 3)
            break;
        remaining_bases -= 3;
        const std::wstring file_name = CFilePathHelper(info.file_path).GetFileName();
        const auto plugin_name = safe_call([&]() {
            const wchar_t* name=info.plugin->GetInfo(ITMPlugin::TMI_NAME);
            return name != nullptr && name[0] ? std::wstring(name) : file_name;
        }, file_name);
        CMenu menu;
        if (!menu.CreatePopupMenu())
            continue;
        std::vector<std::pair<UINT, TrayPluginAction>> actions;
        const auto append = [&](TrayPluginActionKind kind, int command, const std::wstring& label, HICON icon) {
            TrayPluginAction action{info.file_path,info.plugin,kind,command,label};
            const std::wstring key=std::to_wstring(info.file_path.size())+L":"+info.file_path+L"|"
                +std::to_wstring(reinterpret_cast<UINT_PTR>(info.plugin))+L"|"
                +std::to_wstring(static_cast<int>(kind))+L"|"+std::to_wstring(command)+L"|"+label;
            auto binding=m_tray_plugin_command_ids.find(key);
            UINT id{};
            if (binding != m_tray_plugin_command_ids.end())
                id=binding->second;
            else
            {
                const unsigned int reserve=kind==TrayPluginActionKind::PLUGIN_COMMAND ? remaining_bases : 0;
                if (m_tray_plugin_command_ids.size()+reserve >= capacity)
                    return UINT{};
                id=ID_PLUGIN_TRAY_START+static_cast<UINT>(m_tray_plugin_command_ids.size());
                m_tray_plugin_command_ids.emplace(key,id);
            }
            if (!menu.AppendMenu(MF_STRING | MF_ENABLED,id,label.c_str()))
                return UINT{};
            actions.emplace_back(id,std::move(action));
            if (icon != nullptr)
                CMenuIcon::AddIconToMenuItem(menu.GetSafeHmenu(),id,FALSE,icon);
            return id;
        };
        const UINT detail=append(TrayPluginActionKind::DETAIL,-1,
            m_str_table.LoadMenuText(L"TXT_PLUGIN_DETAIL").c_str(),GetMenuIcon(IDI_INFO));
        const UINT options=append(TrayPluginActionKind::OPTIONS,-1,
            m_str_table.LoadMenuText(L"TXT_PLUGIN_OPTIONS").c_str(),GetMenuIcon(IDI_SETTINGS));
        const UINT disabled=append(TrayPluginActionKind::DISABLE,-1,
            m_str_table.LoadMenuText(L"TXT_PLUGIN_DISABLE").c_str(),nullptr);
        if (!detail || !options || !disabled)
            continue;
        menu.CheckMenuItem(disabled,MF_BYCOMMAND | (m_cfg_data.plugin_disabled.Contains(file_name) ? MF_CHECKED : MF_UNCHECKED));
        const int api=safe_call([&]() { return info.plugin->GetAPIVersion(); },0);
        const int count=api>=5 ? max(0,min(safe_call([&]() { return info.plugin->GetCommandCount(); },0),257)) : 0;
        bool separated=false;
        for (int command=0; command<count; ++command)
        {
            //插件可复用 GetCommandName 的 scratch buffer；调用下一 API 前复制文本。
            const std::wstring label=safe_call([&]() {
                const wchar_t* name=info.plugin->GetCommandName(command);
                return name != nullptr ? std::wstring(name) : std::wstring{};
            },std::wstring{});
            if (label.empty())
                continue;
            if (!separated) { menu.AppendMenu(MF_SEPARATOR); separated=true; }
            HICON icon=safe_call([&]() { return static_cast<HICON>(info.plugin->GetCommandIcon(command)); },static_cast<HICON>(nullptr));
            const UINT id=append(TrayPluginActionKind::PLUGIN_COMMAND,command,label,icon);
            if (id==0)
                break;
            const int checked=safe_call([&]() { return info.plugin->IsCommandChecked(command); },0);
            menu.CheckMenuItem(id,MF_BYCOMMAND | (checked ? MF_CHECKED : MF_UNCHECKED));
        }
        HICON icon=api>=5 ? safe_call([&]() { return static_cast<HICON>(info.plugin->GetPluginIcon()); },static_cast<HICON>(nullptr)) : nullptr;
        const HMENU popup=menu.GetSafeHmenu();
        if (!parent->InsertMenu(insertion,MF_BYPOSITION | MF_POPUP | MF_STRING,
            reinterpret_cast<UINT_PTR>(popup),plugin_name.c_str()))
            continue;
        menu.Detach();
        m_tray_plugin_popup_handles.push_back(popup);
        for (auto& action : actions)
            m_tray_plugin_actions.emplace(action.first,std::move(action.second));
        if (icon != nullptr)
            CMenuIcon::AddIconToMenuItem(parent->GetSafeHmenu(),insertion,TRUE,icon);
        ++insertion;
    }
}


HWND CPulseBarApp::GetDialogOwner(HWND fallback) const
{
    const auto usable = [](HWND window) {
        return ::IsWindow(window) && ::IsWindowVisible(window) && ::IsWindowEnabled(window);
    };
    const HWND foreground=::GetAncestor(::GetForegroundWindow(),GA_ROOT);
    for (const auto& item : CBaseDialog::AllUniqueHandels())
        if (item.second==foreground && usable(item.second))
            return foreground;
    const HWND last_active=::GetLastActivePopup(fallback);
    if (last_active != fallback && usable(last_active))
        return last_active;
    for (const auto& item : CBaseDialog::AllUniqueHandels())
        if (usable(item.second))
            return item.second;
    return fallback;
}

void CPulseBarApp::CheckWindows11Taskbar()
{
    // 在“Shell_TrayWnd”的子窗口找到类名为“Windows.UI.Composition.DesktopWindowContentBridge”的窗口则认为是Windows11的任务栏
    if (m_win_version.IsWindows11OrLater())
    {
        HWND hTaskbar = ::FindWindow(L"Shell_TrayWnd", NULL);
        m_is_windows11_taskbar = (::FindWindowExW(hTaskbar, 0, L"Windows.UI.Composition.DesktopWindowContentBridge", NULL) != NULL);
    }
    else
    {
        m_is_windows11_taskbar = false;
    }
}

bool CPulseBarApp::DPIFromRect(const RECT& rect, UINT* out_dpi_x, UINT* out_dpi_y)
{
    HMONITOR h_current_monitor = ::MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST);
    HRESULT hr = m_dll_functions.GetDpiForMonitor(h_current_monitor, MDT_EFFECTIVE_DPI, out_dpi_x, out_dpi_y);
    return hr == S_OK;
}

unsigned int CPulseBarApp::GetThemeColor() const
{
    return m_theme_color;
}

void CPulseBarApp::SetThemeColor(COLORREF color)
{
    m_theme_color = color;
}

void CPulseBarApp::OnHelp()
{
    // TODO: 在此添加命令处理程序代码
    CString help_url;
    if (m_str_table.IsSimplifiedChinese())
        help_url = _T("https://github.com/xiaopeng66/PulseBar/wiki");
    else
        help_url = _T("https://github.com/xiaopeng66/PulseBar/wiki/Home_en");
    ShellExecute(NULL, _T("open"), help_url, NULL, NULL, SW_SHOW);
}


void CPulseBarApp::OnFrequentyAskedQuestions()
{
    // TODO: 在此添加命令处理程序代码
    CString url_domain;
    if (static_cast<CUpdateHelper::UpdateSource>(m_general_data.update_source) == CUpdateHelper::UpdateSource::GiteeSource)
        url_domain = _T("gitee.com");
    else
        url_domain = _T("github.com");
    CString file_name;
    if (m_str_table.IsSimplifiedChinese())
        file_name = _T("Help.md");
    else
        file_name = _T("Help_en-us.md");
    CString url;
    url.Format(_T("https://%s/xiaopeng66/PulseBar/blob/master/%s"), url_domain.GetString(), file_name.GetString());
    ShellExecute(NULL, _T("open"), url, NULL, NULL, SW_SHOW);
}


void CPulseBarApp::OnUpdateLog()
{
    // TODO: 在此添加命令处理程序代码
    CString url_domain;
    if (static_cast<CUpdateHelper::UpdateSource>(m_general_data.update_source) == CUpdateHelper::UpdateSource::GiteeSource)
        url_domain = _T("gitee.com");
    else
        url_domain = _T("github.com");
    wstring language_tag = m_str_table.GetLanguageInfo().bcp_47;
    CString file_name;
    if (language_tag == L"zh-CN")
        file_name = _T("update_log.md");
    else if (language_tag == L"zh-TW")
        file_name = _T("update_log_zh-tw.md");
    else
        file_name = _T("update_log_en-us.md");
    CString url;
    url.Format(_T("https://%s/xiaopeng66/PulseBar/blob/master/UpdateLog/%s"), url_domain.GetString(), file_name.GetString());
    ShellExecute(NULL, _T("open"), url, NULL, NULL, SW_SHOW);
}


int CPulseBarApp::ExitInstance()
{
    // 释放GDI+
    Gdiplus::GdiplusShutdown(m_gdiplusToken);

    return CWinApp::ExitInstance();
}

int CPulseBarApp::GetAPIVersion()
{
    return 1;
}

const wchar_t* CPulseBarApp::GetVersion()
{
    return VERSION;
}

double CPulseBarApp::GetMonitorValue(MonitorItem item)
{
    switch (item)
    {
    case MI_UP: return m_out_speed;
    case MI_DOWN: return m_in_speed;
    case MI_CPU: return m_cpu_usage;
    case MI_MEMORY: return m_memory_usage;
    case MI_GPU_USAGE: return m_gpu_usage;
    case MI_CPU_TEMP: return m_cpu_temperature;
    case MI_GPU_TEMP: return m_gpu_temperature;
    case MI_HDD_TEMP: return m_hdd_temperature;
    case MI_MAIN_BOARD_TEMP: return m_main_board_temperature;
    case MI_HDD_USAGE: return m_hdd_usage;
    case MI_CPU_FREQ: return m_cpu_freq;
    case MI_TODAY_UP_TRAFFIC: return m_today_up_traffic;
    case MI_TODAY_DOWN_TRAFFIC: return m_today_down_traffic;
    }
    return 0.0;
}

const wchar_t* CPulseBarApp::GetMonitorValueString(MonitorItem item, int is_main_window)
{
    static CString str_value;
    if (item == MI_TODAY_UP_TRAFFIC)
    {
        str_value = CCommon::KBytesToString(theApp.m_today_up_traffic / 1024u);
    }
    else if (item == MI_TODAY_DOWN_TRAFFIC)
    {
        str_value = CCommon::KBytesToString(theApp.m_today_down_traffic / 1024u);
    }
    else
    {
        DisplayItem display_item{};
        switch (item)
        {
        case MI_UP: display_item = DisplayItem::TDI_UP; break;
        case MI_DOWN: display_item = DisplayItem::TDI_DOWN; break;
        case MI_CPU: display_item = DisplayItem::TDI_CPU; break;
        case MI_MEMORY: display_item = DisplayItem::TDI_MEMORY; break;
        case MI_GPU_USAGE: display_item = DisplayItem::TDI_GPU_USAGE; break;
        case MI_CPU_TEMP: display_item = DisplayItem::TDI_CPU_TEMP; break;
        case MI_GPU_TEMP: display_item = DisplayItem::TDI_GPU_TEMP; break;
        case MI_HDD_TEMP: display_item = DisplayItem::TDI_HDD_TEMP; break;
        case MI_MAIN_BOARD_TEMP: display_item = DisplayItem::TDI_MAIN_BOARD_TEMP; break;
        case MI_HDD_USAGE: display_item = DisplayItem::TDI_HDD_USAGE; break;
        case MI_CPU_FREQ: display_item = DisplayItem::TDI_CPU_FREQ; break;
        }
        str_value = CommonDisplayItem(display_item).GetItemValueText(is_main_window);
    }
    return str_value.GetString();
}

void CPulseBarApp::ShowNotifyMessage(const wchar_t* strMsg)
{
    CPulseBarDlg* pMainWnd = dynamic_cast<CPulseBarDlg*>(m_pMainWnd);
    if (pMainWnd != nullptr)
    {
        pMainWnd->ShowNotifyTip(CCommon::LoadText(IDS_TRAFFICMONITOR_PLUGIN_NITIFICATION), strMsg);
    }
}

unsigned short CPulseBarApp::GetLanguageId() const
{
    return m_general_data.language.language_id;
}

const wchar_t* CPulseBarApp::GetPluginConfigDir() const
{
    static std::wstring config_dir;
    config_dir = m_config_dir;
    config_dir += L"plugins\\";
    return config_dir.c_str();
}

int CPulseBarApp::GetDPI(DPIType type) const
{
    CPulseBarDlg* pMainWnd = CPulseBarDlg::Instance();
    switch (type)
    {
    case DPI_MAIN_WND:
        return m_dpi;
    case DPI_TASKBAR:
        if (pMainWnd != nullptr)
        {
            if (pMainWnd->IsTaskbarWndValid())
                return pMainWnd->GetTaskbarWindow()->GetDPI();
        }
        return m_dpi;
    }
    return 0;
}

void* CPulseBarApp::GetMainWindowHwnd()
{
    if (m_pMainWnd == nullptr)
        return nullptr;
    return m_pMainWnd->GetSafeHwnd();
}

void* CPulseBarApp::GetTaskbarWindowHwnd()
{
    CPulseBarDlg* pMainWnd = CPulseBarDlg::Instance();
    if (pMainWnd != nullptr && pMainWnd->IsTaskbarWndValid())
        return pMainWnd->GetTaskbarWindow()->GetSafeHwnd();
    return nullptr;
}

const wchar_t* CPulseBarApp::GetStringRes(const wchar_t* key, const wchar_t* section)
{
    return m_str_table.LoadText(key, section).c_str();
}
