// GeneralSettingsDlg.cpp : implementation file
//

#include "stdafx.h"
#include "PulseBar.h"
#include "PulseBarDlg.h"
#include "GeneralSettingsDlg.h"

#ifndef WITHOUT_TEMPERATURE
//轮询等待后台线程把温度项名称写进快照（启动后立刻打开设置页时用）
#define TIMER_TEMP_NAMES 1
#endif

static const TemplateControl k_general_template_controls[] = {
    { 8, 5, 314, 56, true, 0 },  // IDC_STATIC
    { 18, 20, 292, 12, true, 0 },  // IDC_SHOW_ALL_CONNECTION_CHECK
    { 18, 34, 150, 18, true, 0 },  // IDC_SELECT_CONNECTIONS_BUTTON
    { 8, 60, 314, 78, true, 0 },  // IDC_HARDWARE_MONITOR_STATIC
    { 18, 75, 66, 12, true, 0 },  // IDC_CPU_CHECK
    { 90, 75, 66, 12, true, 0 },  // IDC_GPU_CHECK
    { 164, 75, 66, 12, true, 0 },  // IDC_HDD_CHECK
    { 236, 75, 76, 12, true, 0 },  // IDC_MBD_CHECK
    { 18, 96, 142, 10, true, 0 },  // IDC_SELECT_HDD_STATIC
    { 164, 93, 146, 70, true, 0 },  // IDC_SELECT_HARD_DISK_COMBO
    { 18, 116, 142, 10, true, 0 },  // IDC_SELECT_CPU_STATIC
    { 164, 113, 146, 70, true, 0 },  // IDC_SELECT_CPU_COMBO
    { 8, 143, 314, 78, true, 0 },  // IDC_STATIC
    { 18, 158, 292, 10, true, 0 },  // IDC_STATIC
    { 18, 173, 142, 12, true, 0 },  // IDC_USE_CPU_TIME_RADIO
    { 164, 173, 146, 12, true, 0 },  // IDC_USE_PDH_RADIO
    { 18, 196, 90, 10, true, 0 },  // IDC_MONITOR_INTERVAL_STATIC
    { 112, 194, 54, 14, true, 0 },  // IDC_MONITOR_SPAN_EDIT
    { 174, 196, 66, 10, true, 0 },  // IDC_MILLISECONDS_STATIC
    { 246, 192, 64, 18, true, 0 },  // IDC_RESTORE_DEFAULT_TIME_SPAN_BUTTON
    { 8, 405, 314, 114, true, 1 },  // IDC_STATIC
    { 18, 420, 292, 12, true, 1 },  // IDC_AUTO_RUN_CHECK
    { 18, 441, 90, 10, true, 1 },  // IDC_STATIC
    { 112, 439, 74, 12, true, 1 },  // IDC_AUTO_RUN_METHOD_REGESTRY_RADIO
    { 206, 437, 104, 18, true, 1 },  // IDC_RESET_AUTO_RUN_BUTTON
    { 18, 458, 292, 12, true, 1 },  // IDC_AUTO_RUN_METHOD_TASK_SCHEDULE_RADIO
    { 18, 478, 176, 12, true, 1 },  // IDC_SHOW_NOTIFY_ICON_CHECK
    { 18, 499, 176, 12, true, 1 },  // IDC_NOTIFY_ICON_SHOW_SPEED_CHECK
    { 206, 494, 104, 18, true, 1 },  // IDC_SETTINGS_NOTIFY_ICON_BUTTON
    { 8, 528, 314, 87, true, 1 },  // IDC_STATIC
    { 18, 543, 292, 12, true, 1 },  // IDC_SAVE_TO_APPDATA_RADIO
    { 18, 562, 292, 12, true, 1 },  // IDC_SAVE_TO_PROGRAM_DIR_RADIO
    { 18, 584, 192, 20, true, 1 },  // IDC_OPEN_CONFIG_PATH_BUTTON
    { 218, 584, 92, 20, true, 1 },  // IDC_PLUGIN_MANAGE_BUTTON
    { 8, 805, 314, 143, true, 2 },  // IDC_STATIC
    { 18, 820, 172, 12, true, 2 },  // IDC_TODAY_TRAFFIC_TIP_CHECK
    { 204, 818, 54, 14, true, 2 },  // IDC_TODAY_TRAFFIC_TIP_EDIT
    { 266, 817, 44, 65, true, 2 },  // IDC_TODAY_TRAFFIC_TIP_COMBO
    { 18, 820, 1, 1, false, 2 },  // IDC_TODAY_TRAFFIC_BACK_STATIC
    { 18, 841, 172, 12, true, 2 },  // IDC_MEMORY_USAGE_TIP_CHECK
    { 204, 838, 54, 14, true, 2 },  // IDC_MEMORY_USAGE_TIP_EDIT
    { 266, 841, 44, 10, true, 2 },  // IDC_MEMORY_USAGE_BACK_STATIC
    { 18, 861, 172, 12, true, 2 },  // IDC_CPU_TEMP_TIP_CHECK
    { 204, 859, 54, 14, true, 2 },  // IDC_CPU_TEMP_TIP_EDIT
    { 266, 861, 44, 10, true, 2 },  // IDC_CPU_TEMP_STATIC
    { 18, 882, 172, 12, true, 2 },  // IDC_GPU_TEMP_TIP_CHECK
    { 204, 879, 54, 14, true, 2 },  // IDC_GPU_TEMP_TIP_EDIT
    { 266, 882, 44, 10, true, 2 },  // IDC_GPU_TEMP_STATIC
    { 18, 902, 172, 12, true, 2 },  // IDC_HDD_TEMP_TIP_CHECK
    { 204, 900, 54, 14, true, 2 },  // IDC_HDD_TIP_EDIT
    { 266, 902, 44, 10, true, 2 },  // IDC_HDD_STATIC
    { 18, 922, 172, 12, true, 2 },  // IDC_MBD_TEMP_TIP_CHECK
    { 204, 920, 54, 14, true, 2 },  // IDC_MBD_TEMP_TIP_EDIT
    { 266, 922, 44, 10, true, 2 },  // IDC_MBD_TEMP_STATIC
    { 8, 955, 314, 83, true, 2 },  // IDC_STATIC
    { 18, 972, 90, 10, true, 2 },  // IDC_LANGUAGE_STATIC
    { 112, 968, 198, 80, true, 2 },  // IDC_LANGUAGE_COMBO
    { 18, 992, 178, 12, true, 2 },  // IDC_CHECK_UPDATE_CHECK
    { 206, 988, 104, 18, true, 2 },  // IDC_CHECK_NOW_BUTTON
    { 18, 1016, 90, 10, true, 2 },  // IDC_UPDATE_SORUCE_STATIC
    { 112, 1014, 90, 12, true, 2 },  // IDC_GITHUB_RADIO
    { 214, 1014, 96, 12, true, 2 },  // IDC_GITEE_RADIO
};
#include "PluginManagerDlg.h"
#include "SelectConnectionsDlg.h"
#include "IconSelectDlg.h"


// CGeneralSettingsDlg dialog

static const int MONITOR_SPAN_STEP = 100;

IMPLEMENT_DYNAMIC(CGeneralSettingsDlg, CTabDlg)

CGeneralSettingsDlg::CGeneralSettingsDlg(CWnd* pParent /*=NULL*/)
    : CTabDlg(IDD_GENERAL_SETTINGS_DIALOG, pParent)
{

}

CGeneralSettingsDlg::~CGeneralSettingsDlg()
{
}

void CGeneralSettingsDlg::CheckTaskbarDisplayItem()
{
    //如果选项设置中关闭了某个硬件监控，则不显示对应的温度监控相关项目
    if (!theApp.m_general_data.IsHardwareEnable(HI_CPU))
    {
        theApp.m_taskbar_data.display_item.Remove(TDI_CPU_TEMP);
    }
    if (!theApp.m_general_data.IsHardwareEnable(HI_GPU))
    {
        theApp.m_taskbar_data.display_item.Remove(TDI_GPU_TEMP);
    }
    if (!theApp.m_general_data.IsHardwareEnable(HI_HDD))
    {
        theApp.m_taskbar_data.display_item.Remove(TDI_HDD_TEMP);
    }
    if (!theApp.m_general_data.IsHardwareEnable(HI_MBD))
        theApp.m_taskbar_data.display_item.Remove(TDI_MAIN_BOARD_TEMP);
}

void CGeneralSettingsDlg::SetControlMouseWheelEnable(bool enable)
{
    m_traffic_tip_combo.SetMouseWheelEnable(enable);
    m_language_combo.SetMouseWheelEnable(enable);
    m_traffic_tip_edit.SetMouseWheelEnable(enable);
    m_memory_tip_edit.SetMouseWheelEnable(enable);
    m_monitor_span_edit.SetMouseWheelEnable(enable);
    m_cpu_temp_tip_edit.SetMouseWheelEnable(enable);
    m_gpu_temp_tip_edit.SetMouseWheelEnable(enable);
    m_hdd_temp_tip_edit.SetMouseWheelEnable(enable);
    m_mbd_temp_tip_edit.SetMouseWheelEnable(enable);
    m_hard_disk_combo.SetMouseWheelEnable(enable);
    m_select_cpu_combo.SetMouseWheelEnable(enable);
}

void CGeneralSettingsDlg::OnSettingsApplied()
{
    //当设置被应用时，重置xxxx_ori的值
    m_monitor_time_span_ori = m_data.monitor_time_span;
    m_update_source_ori = m_data.update_source;
    m_auto_run_modified = false;
    m_show_all_interface_modified = false;
}

bool CGeneralSettingsDlg::InitializeControls()
{
    return false;
}

bool CGeneralSettingsDlg::ShowHardwareMonitorWarning()
{
    //如果已经有硬件监控项目被勾选了，则不再弹出提示
    if (m_data.hardware_monitor_item != 0)
        return true;

    if (SHMessageBoxCheck(m_hWnd, CCommon::LoadText(IDS_HARDWARE_MONITOR_WARNING), APP_NAME, MB_OKCANCEL | MB_ICONWARNING, IDOK, _T("{B8A281A7-76DF-4F0F-BF6A-1A394EF8BAD5}")) == IDOK)
        return true;

    return false;
}

void CGeneralSettingsDlg::AddOrUpdateAutoRunTooltip(bool add)
{
    if (m_data.auto_run)
    {
        CString str_tool_tip;
        if (m_data.auto_run_by_task_scheduler)
            str_tool_tip = CCommon::LoadText(IDS_AUTO_RUN_METHOD_TASK_SCHEDULE);
        else
            str_tool_tip = CCommon::LoadText(IDS_AUTO_RUN_METHOD_REGESTRY);
        if (!m_auto_run_path.empty())
        {
            str_tool_tip += _T("\r\n");
            str_tool_tip += CCommon::LoadText(IDS_PATH, _T(": "));
            str_tool_tip += m_auto_run_path.c_str();
        }
        if (add)
            m_toolTip.AddTool(GetDlgItem(IDC_AUTO_RUN_CHECK), str_tool_tip);
        else
            m_toolTip.UpdateTipText(str_tool_tip, GetDlgItem(IDC_AUTO_RUN_CHECK));
    }
    else
    {
        if (add)
            m_toolTip.AddTool(GetDlgItem(IDC_AUTO_RUN_CHECK), CString());
        else
            m_toolTip.UpdateTipText(CString(), GetDlgItem(IDC_AUTO_RUN_CHECK));
    }
}

bool CGeneralSettingsDlg::IsMonitorTimeSpanModified() const
{
    return m_data.monitor_time_span != m_monitor_time_span_ori;
}

void CGeneralSettingsDlg::DoDataExchange(CDataExchange* pDX)
{
    CTabDlg::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_TODAY_TRAFFIC_TIP_EDIT, m_traffic_tip_edit);
    DDX_Control(pDX, IDC_TODAY_TRAFFIC_TIP_COMBO, m_traffic_tip_combo);
    DDX_Control(pDX, IDC_MEMORY_USAGE_TIP_EDIT, m_memory_tip_edit);
    DDX_Control(pDX, IDC_LANGUAGE_COMBO, m_language_combo);
    DDX_Control(pDX, IDC_MONITOR_SPAN_EDIT, m_monitor_span_edit);
    DDX_Control(pDX, IDC_CPU_TEMP_TIP_EDIT, m_cpu_temp_tip_edit);
    DDX_Control(pDX, IDC_GPU_TEMP_TIP_EDIT, m_gpu_temp_tip_edit);
    DDX_Control(pDX, IDC_HDD_TIP_EDIT, m_hdd_temp_tip_edit);
    DDX_Control(pDX, IDC_MBD_TEMP_TIP_EDIT, m_mbd_temp_tip_edit);
    DDX_Control(pDX, IDC_SELECT_HARD_DISK_COMBO, m_hard_disk_combo);
    DDX_Control(pDX, IDC_SELECT_CPU_COMBO, m_select_cpu_combo);
    DDX_Control(pDX, IDC_PLUGIN_MANAGE_BUTTON, m_plugin_manager_btn);
    DDX_Control(pDX, IDC_SELECT_CONNECTIONS_BUTTON, m_select_connection_btn);
}

void CGeneralSettingsDlg::SetControlEnable()
{
    m_traffic_tip_edit.EnableWindow(m_data.traffic_tip_enable);
    m_traffic_tip_combo.EnableWindow(m_data.traffic_tip_enable);
    m_memory_tip_edit.EnableWindow(m_data.memory_usage_tip.enable);
    m_cpu_temp_tip_edit.EnableWindow(m_data.cpu_temp_tip.enable);
    m_gpu_temp_tip_edit.EnableWindow(m_data.gpu_temp_tip.enable);
    m_hdd_temp_tip_edit.EnableWindow(m_data.hdd_temp_tip.enable);
    m_mbd_temp_tip_edit.EnableWindow(m_data.mainboard_temp_tip.enable);

    //m_hard_disk_combo.EnableWindow(m_data.IsHardwareEnable(HI_HDD));
    m_select_cpu_combo.EnableWindow(m_data.IsHardwareEnable(HI_CPU));

    EnableDlgCtrl(IDC_SELECT_CONNECTIONS_BUTTON, !m_data.show_all_interface);

    EnableDlgCtrl(IDC_RESET_AUTO_RUN_BUTTON, m_data.auto_run);
    //方式单选按钮始终可用：选中一种方式即表示要开启开机自动运行
    EnableDlgCtrl(IDC_AUTO_RUN_METHOD_REGESTRY_RADIO, TRUE);
    EnableDlgCtrl(IDC_AUTO_RUN_METHOD_TASK_SCHEDULE_RADIO, TRUE);
}


BEGIN_MESSAGE_MAP(CGeneralSettingsDlg, CTabDlg)
    ON_BN_CLICKED(IDC_CHECK_NOW_BUTTON, &CGeneralSettingsDlg::OnBnClickedCheckNowButton)
    ON_BN_CLICKED(IDC_CHECK_UPDATE_CHECK, &CGeneralSettingsDlg::OnBnClickedCheckUpdateCheck)
    ON_BN_CLICKED(IDC_AUTO_RUN_CHECK, &CGeneralSettingsDlg::OnBnClickedAutoRunCheck)
    ON_BN_CLICKED(IDC_TODAY_TRAFFIC_TIP_CHECK, &CGeneralSettingsDlg::OnBnClickedTodayTrafficTipCheck)
    ON_BN_CLICKED(IDC_MEMORY_USAGE_TIP_CHECK, &CGeneralSettingsDlg::OnBnClickedMemoryUsageTipCheck)
    ON_BN_CLICKED(IDC_OPEN_CONFIG_PATH_BUTTON, &CGeneralSettingsDlg::OnBnClickedOpenConfigPathButton)
    ON_BN_CLICKED(IDC_SHOW_ALL_CONNECTION_CHECK, &CGeneralSettingsDlg::OnBnClickedShowAllConnectionCheck)
    ON_BN_CLICKED(IDC_USE_CPU_TIME_RADIO, &CGeneralSettingsDlg::OnBnClickedUseCpuTimeRadio)
    ON_BN_CLICKED(IDC_USE_PDH_RADIO, &CGeneralSettingsDlg::OnBnClickedUsePdhRadio)
    ON_EN_KILLFOCUS(IDC_MONITOR_SPAN_EDIT, &CGeneralSettingsDlg::OnEnKillfocusMonitorSpanEdit)
    ON_BN_CLICKED(IDC_CPU_TEMP_TIP_CHECK, &CGeneralSettingsDlg::OnBnClickedCpuTempTipCheck)
    ON_BN_CLICKED(IDC_GPU_TEMP_TIP_CHECK, &CGeneralSettingsDlg::OnBnClickedGpuTempTipCheck)
    ON_BN_CLICKED(IDC_HDD_TEMP_TIP_CHECK, &CGeneralSettingsDlg::OnBnClickedHddTempTipCheck)
    ON_BN_CLICKED(IDC_MBD_TEMP_TIP_CHECK, &CGeneralSettingsDlg::OnBnClickedMbdTempTipCheck)
    ON_BN_CLICKED(IDC_GITHUB_RADIO, &CGeneralSettingsDlg::OnBnClickedGithubRadio)
    ON_BN_CLICKED(IDC_GITEE_RADIO, &CGeneralSettingsDlg::OnBnClickedGiteeRadio)
    ON_BN_CLICKED(IDC_RESTORE_DEFAULT_TIME_SPAN_BUTTON, &CGeneralSettingsDlg::OnBnClickedRestoreDefaultTimeSpanButton)
    ON_CBN_SELCHANGE(IDC_SELECT_HARD_DISK_COMBO, &CGeneralSettingsDlg::OnCbnSelchangeSelectHardDiskCombo)
    ON_BN_CLICKED(IDC_CPU_CHECK, &CGeneralSettingsDlg::OnBnClickedCpuCheck)
    ON_BN_CLICKED(IDC_GPU_CHECK, &CGeneralSettingsDlg::OnBnClickedGpuCheck)
    ON_BN_CLICKED(IDC_HDD_CHECK, &CGeneralSettingsDlg::OnBnClickedHddCheck)
    ON_BN_CLICKED(IDC_MBD_CHECK, &CGeneralSettingsDlg::OnBnClickedMbdCheck)
    ON_CBN_SELCHANGE(IDC_SELECT_CPU_COMBO, &CGeneralSettingsDlg::OnCbnSelchangeSelectCpuCombo)
    ON_BN_CLICKED(IDC_PLUGIN_MANAGE_BUTTON, &CGeneralSettingsDlg::OnBnClickedPluginManageButton)
    ON_BN_CLICKED(IDC_SHOW_NOTIFY_ICON_CHECK, &CGeneralSettingsDlg::OnBnClickedShowNotifyIconCheck)
    ON_BN_CLICKED(IDC_NOTIFY_ICON_SHOW_SPEED_CHECK, &CGeneralSettingsDlg::OnBnClickedNotifyIconShowSpeedCheck)
    ON_BN_CLICKED(IDC_SELECT_CONNECTIONS_BUTTON, &CGeneralSettingsDlg::OnBnClickedSelectConnectionsButton)
    ON_BN_CLICKED(IDC_RESET_AUTO_RUN_BUTTON, &CGeneralSettingsDlg::OnBnClickedResetAutoRunButton)
    ON_EN_CHANGE(IDC_MONITOR_SPAN_EDIT, &CGeneralSettingsDlg::OnEnChangeMonitorSpanEdit)
    ON_MESSAGE(WM_SPIN_EDIT_POS_CHANGED, &CGeneralSettingsDlg::OnSpinEditPosChanged)
    ON_BN_CLICKED(IDC_AUTO_RUN_METHOD_REGESTRY_RADIO, &CGeneralSettingsDlg::OnBnClickedAutoRunMethodRegestryRadio)
    ON_BN_CLICKED(IDC_AUTO_RUN_METHOD_TASK_SCHEDULE_RADIO, &CGeneralSettingsDlg::OnBnClickedAutoRunMethodTaskScheduleRadio)
    ON_BN_CLICKED(IDC_SETTINGS_NOTIFY_ICON_BUTTON, &CGeneralSettingsDlg::OnNotifyIconAppearance)
#ifndef WITHOUT_TEMPERATURE
    ON_WM_TIMER()
#endif
END_MESSAGE_MAP()


// CGeneralSettingsDlg 消息处理程序


BOOL CGeneralSettingsDlg::OnInitDialog()
{
    CTabDlg::OnInitDialog();

    // TODO:  在此添加额外的初始化

    ((CButton*)GetDlgItem(IDC_CHECK_UPDATE_CHECK))->SetCheck(m_data.check_update_when_start);
    if (theApp.IsForceShowNotifyIcon())
    {
        m_data.show_notify_icon = true;
        EnableDlgCtrl(IDC_SHOW_NOTIFY_ICON_CHECK, FALSE);
    }
    CheckDlgButton(IDC_SHOW_NOTIFY_ICON_CHECK, m_data.show_notify_icon);
    CheckDlgButton(IDC_NOTIFY_ICON_SHOW_SPEED_CHECK, m_data.notify_icon_show_speed);
    EnableDlgCtrl(IDC_NOTIFY_ICON_SHOW_SPEED_CHECK, m_data.show_notify_icon);

    if (m_data.update_source == 0)
        CheckDlgButton(IDC_GITHUB_RADIO, TRUE);
    else
        CheckDlgButton(IDC_GITEE_RADIO, TRUE);

    //检查开始菜单的“启动”目录下有没有程序的快捷方式，如果有则设置开机自启动，然后删除快捷方式
    wstring start_up_path = CCommon::GetStartUpPath();
    bool shortcut_exist = CCommon::FileExist((start_up_path + L"\\TrafficMonitor.lnk").c_str());
    m_data.auto_run = false;
#ifdef WITHOUT_TEMPERATURE
    m_data.auto_run_by_task_scheduler = false;
#else
    m_data.auto_run_by_task_scheduler = true;
#endif

    if (shortcut_exist)
    {
        //旧版本用“启动”目录下的快捷方式实现自启动，这里迁移成注册表方式。
        //必须先确认注册表写入成功再删快捷方式：写入失败时删掉快捷方式会让自启动彻底丢失。
        if (theApp.SetAutoRun(true, false))
        {
            m_data.auto_run = true;
            CheckDlgButton(IDC_AUTO_RUN_METHOD_REGESTRY_RADIO, TRUE);
            DeleteFile((start_up_path + L"\\TrafficMonitor.lnk").c_str());
        }
    }
    //检查开机自动运行的设置是通过注册表还是任务计划程序实现的，并设置相应的选项
    else
    {
        bool auto_run_by_registry = theApp.GetAutoRun(&m_auto_run_path, false);
        if (auto_run_by_registry)
        {
            CheckDlgButton(IDC_AUTO_RUN_METHOD_REGESTRY_RADIO, TRUE);
            m_data.auto_run = true;
            m_data.auto_run_by_task_scheduler = false;
        }
        else
        {
            bool auto_run_by_task_scheduler = theApp.GetAutoRun(&m_auto_run_path, true);
            if (auto_run_by_task_scheduler)
            {
                CheckDlgButton(IDC_AUTO_RUN_METHOD_TASK_SCHEDULE_RADIO, TRUE);
                m_data.auto_run = true;
                m_data.auto_run_by_task_scheduler = true;
            }
        }
    }
    //只有在开机自动运行已开启时才勾选方式单选按钮，否则会出现
    //“方式已被选中，但设置并未生效”的误导状态
    if (m_data.auto_run)
    {
        if (m_data.auto_run_by_task_scheduler)
            CheckDlgButton(IDC_AUTO_RUN_METHOD_TASK_SCHEDULE_RADIO, TRUE);
        else
            CheckDlgButton(IDC_AUTO_RUN_METHOD_REGESTRY_RADIO, TRUE);
    }


    ((CButton*)GetDlgItem(IDC_SAVE_TO_APPDATA_RADIO))->SetCheck(!m_data.portable_mode);
    ((CButton*)GetDlgItem(IDC_SAVE_TO_PROGRAM_DIR_RADIO))->SetCheck(m_data.portable_mode);
    GetDlgItem(IDC_SAVE_TO_PROGRAM_DIR_RADIO)->EnableWindow(theApp.m_module_dir_writable);

    ((CButton*)GetDlgItem(IDC_AUTO_RUN_CHECK))->SetCheck(m_data.auto_run);

    ((CButton*)GetDlgItem(IDC_TODAY_TRAFFIC_TIP_CHECK))->SetCheck(m_data.traffic_tip_enable);
    m_traffic_tip_edit.SetRange(1, 32767);
    m_traffic_tip_edit.SetValue(m_data.traffic_tip_value);
    m_traffic_tip_combo.AddString(_T("MB"));
    m_traffic_tip_combo.AddString(_T("GB"));
    //载入时已钳制到 0/1，这里再夹一次，任何来源的越界值都不会让下拉框空白
    m_traffic_tip_combo.SetCurSel(max(0, min(m_data.traffic_tip_unit, m_traffic_tip_combo.GetCount() - 1)));
    CheckDlgButton(IDC_MEMORY_USAGE_TIP_CHECK, m_data.memory_usage_tip.enable);
    m_memory_tip_edit.SetRange(1, 100);
    m_memory_tip_edit.SetValue(m_data.memory_usage_tip.tip_value);

    CheckDlgButton(IDC_CPU_TEMP_TIP_CHECK, m_data.cpu_temp_tip.enable);
    m_cpu_temp_tip_edit.SetRange(1, 120);
    m_cpu_temp_tip_edit.SetValue(m_data.cpu_temp_tip.tip_value);

    CheckDlgButton(IDC_GPU_TEMP_TIP_CHECK, m_data.gpu_temp_tip.enable);
    m_gpu_temp_tip_edit.SetRange(1, 120);
    m_gpu_temp_tip_edit.SetValue(m_data.gpu_temp_tip.tip_value);

    CheckDlgButton(IDC_HDD_TEMP_TIP_CHECK, m_data.hdd_temp_tip.enable);
    m_hdd_temp_tip_edit.SetRange(1, 120);
    m_hdd_temp_tip_edit.SetValue(m_data.hdd_temp_tip.tip_value);

    CheckDlgButton(IDC_MBD_TEMP_TIP_CHECK, m_data.mainboard_temp_tip.enable);
    m_mbd_temp_tip_edit.SetRange(1, 120);
    m_mbd_temp_tip_edit.SetValue(m_data.mainboard_temp_tip.tip_value);

    SetControlEnable();

    m_language_combo.AddString(CCommon::LoadText(IDS_FOLLOWING_SYSTEM));
    int current_language_index{ -1 };       //当前语言在所有语言列表中的序号
    for (size_t i = 0; i < theApp.m_str_table.GetLanguageList().size(); i++)
    {
        const LanguageInfo& language_info = theApp.m_str_table.GetLanguageList()[i];
        m_language_combo.AddString(language_info.display_name.c_str());
        if (language_info == m_data.language)
            current_language_index = static_cast<int>(i);
    }
    m_language_combo.SetCurSel(current_language_index + 1);     //由于ComboBox第一项是“跟随系统”，因此ComboBox的序号需要加1

    ((CButton*)GetDlgItem(IDC_SHOW_ALL_CONNECTION_CHECK))->SetCheck(m_data.show_all_interface);

    m_toolTip.Create(this);
    m_toolTip.SetMaxTipWidth(theApp.DPI(300));
    m_toolTip.AddTool(GetDlgItem(IDC_SHOW_ALL_CONNECTION_CHECK), CCommon::LoadText(IDS_SHOW_ALL_INFO_TIP));
    m_toolTip.AddTool(GetDlgItem(IDC_SAVE_TO_APPDATA_RADIO), theApp.m_appdata_dir.c_str());
    m_toolTip.AddTool(GetDlgItem(IDC_SAVE_TO_PROGRAM_DIR_RADIO), theApp.m_module_dir.c_str());
    AddOrUpdateAutoRunTooltip(true);

    if (m_data.cpu_usage_acquire_method == GeneralSettingData::CA_CPU_TIME)
    {
        CheckDlgButton(IDC_USE_CPU_TIME_RADIO, TRUE);
    }
    else if (m_data.cpu_usage_acquire_method == GeneralSettingData::CA_PDH)
    {
        CheckDlgButton(IDC_USE_PDH_RADIO, TRUE);
    }

    m_monitor_span_edit.SetRange(MONITOR_TIME_SPAN_MIN, MONITOR_TIME_SPAN_MAX, MONITOR_SPAN_STEP);
    m_monitor_span_edit.SetValue(m_data.monitor_time_span);

    m_monitor_time_span_ori = m_data.monitor_time_span;
    m_update_source_ori = m_data.update_source;

    CPulseBarDlg* pMainWnd = CPulseBarDlg::Instance();
    if (pMainWnd != nullptr && pMainWnd->IsGetDiskUsageByPdh())
    {
        const auto& disk_names = pMainWnd->GetPdhDiskUsageHelper().GetDiskNames();
        for (const auto& hdd_name : disk_names)
            m_hard_disk_combo.AddString(hdd_name);
        int cur_index = m_hard_disk_combo.FindString(-1, m_data.hard_disk_name.c_str());
        //找不到已保存的硬盘名时 SetCurSel(-1) 会让下拉框显示空白；
        //改为选中第一项并同步保存的名称，避免“界面显示 A、配置里存 B”
        if (cur_index < 0 && m_hard_disk_combo.GetCount() > 0)
        {
            cur_index = 0;
            CString first_disk;
            m_hard_disk_combo.GetLBText(0, first_disk);
            m_data.hard_disk_name = first_disk.GetString();
        }
        if (cur_index >= 0)
            m_hard_disk_combo.SetCurSel(cur_index);
    }
#ifndef WITHOUT_TEMPERATURE
    //初始化硬件监控Check box
    CheckDlgButton(IDC_CPU_CHECK, m_data.IsHardwareEnable(HI_CPU));
    CheckDlgButton(IDC_GPU_CHECK, m_data.IsHardwareEnable(HI_GPU));
    CheckDlgButton(IDC_HDD_CHECK, m_data.IsHardwareEnable(HI_HDD));
    CheckDlgButton(IDC_MBD_CHECK, m_data.IsHardwareEnable(HI_MBD));

    //不判断 m_pMonitor 是否已创建：后台初始化线程可能还没跑完，此时它仍是空的。
    //只要本机启用了任一硬件监控项（即需要温度），就按快照填充；快照暂时为空时
    //FillTemperatureNameCombos 会登记轮询，等后台扫描完成后再补齐。
    if (m_data.IsHardwareEnable(HI_CPU) || m_data.IsHardwareEnable(HI_GPU)
        || m_data.IsHardwareEnable(HI_HDD) || m_data.IsHardwareEnable(HI_MBD))
    {
        FillTemperatureNameCombos();
    }
#endif

    //不含温度监控的版本，禁用温度相关的控件
#ifdef WITHOUT_TEMPERATURE
    EnableDlgCtrl(IDC_CPU_TEMP_TIP_CHECK, false);
    EnableDlgCtrl(IDC_CPU_TEMP_TIP_EDIT, false);
    EnableDlgCtrl(IDC_GPU_TEMP_TIP_CHECK, false);
    EnableDlgCtrl(IDC_GPU_TEMP_TIP_EDIT, false);
    EnableDlgCtrl(IDC_HDD_TEMP_TIP_CHECK, false);
    EnableDlgCtrl(IDC_HDD_TIP_EDIT, false);
    EnableDlgCtrl(IDC_MBD_TEMP_TIP_CHECK, false);
    EnableDlgCtrl(IDC_MBD_TEMP_TIP_EDIT, false);
    EnableDlgCtrl(IDC_CPU_CHECK, false);
    EnableDlgCtrl(IDC_GPU_CHECK, false);
    EnableDlgCtrl(IDC_HDD_CHECK, false);
    EnableDlgCtrl(IDC_MBD_CHECK, false);
    //EnableDlgCtrl(IDC_SELECT_HARD_DISK_COMBO, false);
    EnableDlgCtrl(IDC_SELECT_CPU_COMBO, false);
    EnableDlgCtrl(IDC_CPU_TEMP_STATIC, false);
    EnableDlgCtrl(IDC_GPU_TEMP_STATIC, false);
    EnableDlgCtrl(IDC_HDD_STATIC, false);
    EnableDlgCtrl(IDC_MBD_TEMP_STATIC, false);
    //EnableDlgCtrl(IDC_SELECT_HDD_STATIC, false);
    EnableDlgCtrl(IDC_SELECT_CPU_STATIC, false);
    EnableDlgCtrl(IDC_HARDWARE_MONITOR_STATIC, false);
#endif

    m_plugin_manager_btn.SetIcon(theApp.GetMenuIcon(IDI_PLUGINS));
    m_select_connection_btn.SetIcon(theApp.GetMenuIcon(IDI_CONNECTION));
    //按资源模板表建立基准位置，不依赖运行时坐标推算分区（否则控件移动后会被永久归错页）。
    int index = 0;
    const int count = static_cast<int>(sizeof(k_general_template_controls) / sizeof(k_general_template_controls[0]));
    for (CWnd* ctrl = GetWindow(GW_CHILD); ctrl != nullptr && index < count; ctrl = ctrl->GetWindow(GW_HWNDNEXT), ++index)
    {
        const TemplateControl& tc = k_general_template_controls[index];
        SectionControl item;
        item.hwnd = ctrl->GetSafeHwnd();
        item.section = tc.section;
        CRect rect(tc.x, tc.y, tc.x + tc.cx, tc.y + tc.cy);
        MapDialogRect(rect);
        CRect section_offset(0, 0, 0, 400 * tc.section);
        MapDialogRect(section_offset);
        rect.OffsetRect(0, -section_offset.bottom);
        item.rect = rect;
        item.visible = tc.visible;
        m_section_controls.push_back(item);
    }

    return TRUE;  // return TRUE unless you set the focus to a control
                  // 异常: OCX 属性页应返回 FALSE
}

#ifndef WITHOUT_TEMPERATURE
void CGeneralSettingsDlg::FillTemperatureNameCombos()
{
    //读温度项名称快照，不在这里等 m_minitor_lib_critical：
    //该锁被启动初始化线程（跨 CreateInstance + 首次扫描硬件的 .NET 调用，冷启动可达数秒）
    //和采样线程（跨 GetHardwareInfo + 全部温度读取）持有，
    //在 UI 线程上抢这把锁正是设置页打开慢的主要原因。
    std::vector<std::wstring> hdd_names;
    std::vector<std::wstring> cpu_names;
    {
        CSingleLock names_sync(&theApp.m_temperature_names_critical, TRUE);
        hdd_names = theApp.m_hdd_temperature_names;
        cpu_names = theApp.m_cpu_temperature_names;
    }

    //快照为空说明后台线程还没扫完硬件：先按空列表填充，并登记短时轮询等待补齐。
    //不在这里直接抢 m_minitor_lib_critical 兜底——那正是要消除的卡顿来源。
    if (cpu_names.empty() && hdd_names.empty())
    {
        if (!m_temp_names_pending)
        {
            m_temp_names_pending = true;
            m_temp_names_retry = 0;
            SetTimer(TIMER_TEMP_NAMES, 300, nullptr);
        }
    }
    else
    {
        m_temp_names_pending = false;
        KillTimer(TIMER_TEMP_NAMES);
    }

    CPulseBarDlg* pMainWnd = CPulseBarDlg::Instance();
    //初始化选择硬盘下拉列表
    if (pMainWnd != nullptr && !pMainWnd->IsGetDiskUsageByPdh())
    {
        //后台扫描完成后会重复填充，先抑制绘制避免 ResetContent 短暂露出空白，
        //再按保存的名称恢复选中项，避免用户看到空的下拉框。
        m_hard_disk_combo.SetRedraw(FALSE);
        m_hard_disk_combo.ResetContent();
        for (const auto& hdd_name : hdd_names)
            m_hard_disk_combo.AddString(hdd_name.c_str());
        int cur_index = m_hard_disk_combo.FindString(-1, m_data.hard_disk_name.c_str());
        //同 PDH 分支：找不到就选中第一项并同步名称，避免下拉框空白
        if (cur_index < 0 && m_hard_disk_combo.GetCount() > 0)
        {
            cur_index = 0;
            CString first_disk;
            m_hard_disk_combo.GetLBText(0, first_disk);
            m_data.hard_disk_name = first_disk.GetString();
        }
        m_hard_disk_combo.SetRedraw(TRUE);
        if (cur_index >= 0)
            m_hard_disk_combo.SetCurSel(cur_index);
    }
    //初始化选择CPU下拉列表
    m_select_cpu_combo.SetRedraw(FALSE);
    m_select_cpu_combo.ResetContent();
    m_select_cpu_combo.AddString(CCommon::LoadText(IDS_AVREAGE_TEMPERATURE));
    for (const auto& cpu_name : cpu_names)
        m_select_cpu_combo.AddString(cpu_name.c_str());
    int cur_index = m_select_cpu_combo.FindString(-1, m_data.cpu_core_name.c_str());
    if (cur_index < 0)
        cur_index = 0;
    m_select_cpu_combo.SetRedraw(TRUE);
    m_select_cpu_combo.SetCurSel(cur_index);
}

void CGeneralSettingsDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == TIMER_TEMP_NAMES)
    {
        //轮询等后台线程把温度项名称写进快照。有上限，避免扫描失败时一直空转；
        //次数用完后保持当前（空）列表，用户仍可正常操作其它设置项。
        if (++m_temp_names_retry > 40)
        {
            m_temp_names_pending = false;
            KillTimer(TIMER_TEMP_NAMES);
            return;
        }
        //下拉列表正展开时不要重建：ResetContent 会把用户正在看的列表抽掉。
        //等下一次轮询（用户收起列表后）再补。
        if (m_select_cpu_combo.GetDroppedState() || m_hard_disk_combo.GetDroppedState())
            return;
        FillTemperatureNameCombos();
        return;
    }
    CTabDlg::OnTimer(nIDEvent);
}
#endif


void CGeneralSettingsDlg::OnBnClickedCheckNowButton()
{
    // TODO: 在此添加控件通知处理程序代码
    theApp.CheckUpdateInThread(true);
}


void CGeneralSettingsDlg::OnBnClickedCheckUpdateCheck()
{
    // TODO: 在此添加控件通知处理程序代码
    m_data.check_update_when_start = (IsDlgButtonChecked(IDC_CHECK_UPDATE_CHECK) != 0);
}


void CGeneralSettingsDlg::OnBnClickedAutoRunCheck()
{
    // TODO: 在此添加控件通知处理程序代码
    m_data.auto_run = (IsDlgButtonChecked(IDC_AUTO_RUN_CHECK) != 0);
    m_auto_run_modified = true;
    SetControlEnable();
}


void CGeneralSettingsDlg::CollectSettings()
{
    // TODO: 在此添加专用代码和/或调用基类
    //获取消息提示的设置
    m_data.traffic_tip_value = m_traffic_tip_edit.GetValue();
    if (m_data.traffic_tip_value < 1) m_data.traffic_tip_value = 1;
    if (m_data.traffic_tip_value > 32767) m_data.traffic_tip_value = 32767;
    //下拉框没有选中项时 GetCurSel 返回 -1，会把 -1 写进配置；此时保留原值
    if (m_traffic_tip_combo.GetCurSel() >= 0)
        m_data.traffic_tip_unit = m_traffic_tip_combo.GetCurSel();

    auto checkTipValue = [](int& value)
    {
        if (value < 1) value = 1;
        if (value > 100) value = 100;
    };
    auto checkTempTipValue = [](int& value)
    {
        if (value < 1) value = 1;
        if (value > 120) value = 120;
    };
    m_data.memory_usage_tip.tip_value = m_memory_tip_edit.GetValue();
    checkTipValue(m_data.memory_usage_tip.tip_value);

    m_data.cpu_temp_tip.tip_value = m_cpu_temp_tip_edit.GetValue();
    checkTempTipValue(m_data.cpu_temp_tip.tip_value);

    m_data.gpu_temp_tip.tip_value = m_gpu_temp_tip_edit.GetValue();
    checkTempTipValue(m_data.gpu_temp_tip.tip_value);

    m_data.hdd_temp_tip.tip_value = m_hdd_temp_tip_edit.GetValue();
    checkTempTipValue(m_data.hdd_temp_tip.tip_value);

    m_data.mainboard_temp_tip.tip_value = m_mbd_temp_tip_edit.GetValue();
    checkTempTipValue(m_data.mainboard_temp_tip.tip_value);

    //获取语言的设置
    m_data.language = LanguageInfo();
    if (m_language_combo.GetCurSel() > 0)
    {
        //选择的不是“跟随系统”
        int current_language_index = m_language_combo.GetCurSel() - 1;
        if (current_language_index >= 0 && current_language_index < static_cast<int>(theApp.m_str_table.GetLanguageList().size()))
        {
            m_data.language = theApp.m_str_table.GetLanguageList()[current_language_index];
        }
    }
    if (m_data.language != theApp.m_general_data.language)
    {
        MessageBox(CCommon::LoadText(IDS_LANGUAGE_CHANGE_INFO), NULL, MB_ICONINFORMATION | MB_OK);
    }
    m_show_all_interface_modified = (m_data.show_all_interface != theApp.m_general_data.show_all_interface);

    //获取数据文件保存位置的设置
    //迁移与提示统一在 CPulseBarDlg::ApplySettings 里执行，这里只读取选择
    m_data.portable_mode = (((CButton*)GetDlgItem(IDC_SAVE_TO_PROGRAM_DIR_RADIO))->GetCheck() != 0);

    m_data.monitor_time_span = max(MONITOR_TIME_SPAN_MIN, min(m_monitor_span_edit.GetValue(), MONITOR_TIME_SPAN_MAX));
    m_monitor_span_edit.SetValue(m_data.monitor_time_span);
}

void CGeneralSettingsDlg::OnOK()
{
    CollectSettings();
    CTabDlg::OnOK();
}


void CGeneralSettingsDlg::OnBnClickedTodayTrafficTipCheck()
{
    // TODO: 在此添加控件通知处理程序代码
    m_data.traffic_tip_enable = (IsDlgButtonChecked(IDC_TODAY_TRAFFIC_TIP_CHECK) != 0);
    SetControlEnable();
}


void CGeneralSettingsDlg::OnBnClickedMemoryUsageTipCheck()
{
    // TODO: 在此添加控件通知处理程序代码
    m_data.memory_usage_tip.enable = (IsDlgButtonChecked(IDC_MEMORY_USAGE_TIP_CHECK) != 0);
    SetControlEnable();
}


void CGeneralSettingsDlg::OnBnClickedOpenConfigPathButton()
{
    // TODO: 在此添加控件通知处理程序代码
    ShellExecute(NULL, _T("explore"), theApp.m_config_dir.c_str(), NULL, NULL, SW_SHOWNORMAL);
}


void CGeneralSettingsDlg::OnBnClickedShowAllConnectionCheck()
{
    // TODO: 在此添加控件通知处理程序代码
    m_data.show_all_interface = (IsDlgButtonChecked(IDC_SHOW_ALL_CONNECTION_CHECK) != 0);
    SetControlEnable();
}


BOOL CGeneralSettingsDlg::PreTranslateMessage(MSG* pMsg)
{
    // TODO: 在此添加专用代码和/或调用基类
    if (pMsg->message == WM_MOUSEMOVE)
        m_toolTip.RelayEvent(pMsg);

    return CTabDlg::PreTranslateMessage(pMsg);
}

void CGeneralSettingsDlg::OnBnClickedUseCpuTimeRadio()
{
    m_data.cpu_usage_acquire_method = GeneralSettingData::CA_CPU_TIME;
}


void CGeneralSettingsDlg::OnBnClickedUsePdhRadio()
{
    m_data.cpu_usage_acquire_method = GeneralSettingData::CA_PDH;
}

afx_msg LRESULT CGeneralSettingsDlg::OnSpinEditPosChanged(WPARAM wParam, LPARAM lParam)
{
    CSpinButtonCtrl* pSpin = (CSpinButtonCtrl*)wParam;
    if (pSpin == nullptr)
        return 0;
    CWnd* pEdit = pSpin->GetBuddy();
    if (pEdit == &m_monitor_span_edit)       //当用户点击了“监控时间间隔”的微调按钮时
    {
        LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(lParam);
        if (pNMUpDown->iDelta == -1)
        {
            // 用户按下了spin控件的向下箭头
            int value = m_monitor_span_edit.GetValue();
            value -= MONITOR_SPAN_STEP;
            value /= MONITOR_SPAN_STEP;
            value *= MONITOR_SPAN_STEP;
            m_monitor_span_edit.SetValue(value);
        }
        else if (pNMUpDown->iDelta == 1)
        {
            // 用户按下了spin控件的向上箭头
            int value = m_monitor_span_edit.GetValue();
            value += MONITOR_SPAN_STEP;
            value /= MONITOR_SPAN_STEP;
            value *= MONITOR_SPAN_STEP;
            m_monitor_span_edit.SetValue(value);
        }
        pNMUpDown->iDelta = 0;
    }
    return 0;
}



void CGeneralSettingsDlg::OnEnKillfocusMonitorSpanEdit()
{
    // TODO: 在此添加控件通知处理程序代码

    //这里限制监控时间间隔只能输入100的倍数
    CString str;
    GetDlgItemText(IDC_MONITOR_SPAN_EDIT, str);
    str.Replace(_T(","), _T(""));
    int value = _ttoi(str.GetString());
    if (value < MONITOR_TIME_SPAN_MIN || value > MONITOR_TIME_SPAN_MAX)
    {
        value = 1000;
    }
    else
    {
        value /= MONITOR_SPAN_STEP;
        value *= MONITOR_SPAN_STEP;
    }
    m_monitor_span_edit.SetValue(value);
}


void CGeneralSettingsDlg::OnBnClickedCpuTempTipCheck()
{
    // TODO: 在此添加控件通知处理程序代码
    m_data.cpu_temp_tip.enable = (IsDlgButtonChecked(IDC_CPU_TEMP_TIP_CHECK) != 0);
    SetControlEnable();
}


void CGeneralSettingsDlg::OnBnClickedGpuTempTipCheck()
{
    // TODO: 在此添加控件通知处理程序代码
    m_data.gpu_temp_tip.enable = (IsDlgButtonChecked(IDC_GPU_TEMP_TIP_CHECK) != 0);
    SetControlEnable();
}


void CGeneralSettingsDlg::OnBnClickedHddTempTipCheck()
{
    // TODO: 在此添加控件通知处理程序代码
    m_data.hdd_temp_tip.enable = (IsDlgButtonChecked(IDC_HDD_TEMP_TIP_CHECK) != 0);
    SetControlEnable();
}


void CGeneralSettingsDlg::OnBnClickedMbdTempTipCheck()
{
    // TODO: 在此添加控件通知处理程序代码
    m_data.mainboard_temp_tip.enable = (IsDlgButtonChecked(IDC_MBD_TEMP_TIP_CHECK) != 0);
    SetControlEnable();
}


void CGeneralSettingsDlg::OnBnClickedGithubRadio()
{
    // TODO: 在此添加控件通知处理程序代码
    m_data.update_source = 0;
    theApp.m_general_data.update_source = 0;        //点击“更新源”的单选按钮时，同时更改theApp中的设置，以确保点击“立即检查”按钮时使用选择的更新源更新
}


void CGeneralSettingsDlg::OnBnClickedGiteeRadio()
{
    // TODO: 在此添加控件通知处理程序代码
    m_data.update_source = 1;
    theApp.m_general_data.update_source = 1;
}


void CGeneralSettingsDlg::OnCancel()
{
    RestoreUpdateSource();
    CTabDlg::OnCancel();
}


void CGeneralSettingsDlg::OnBnClickedRestoreDefaultTimeSpanButton()
{
    // TODO: 在此添加控件通知处理程序代码
    m_monitor_span_edit.SetValue(1000);
}


void CGeneralSettingsDlg::OnCbnSelchangeSelectHardDiskCombo()
{
    // TODO: 在此添加控件通知处理程序代码
    //没有选中项时 GetWindowText 取到空串，会把已保存的硬盘名清空
    if (m_hard_disk_combo.GetCurSel() < 0)
        return;
    CString hard_disk_name;
    m_hard_disk_combo.GetWindowText(hard_disk_name);
    m_data.hard_disk_name = hard_disk_name.GetString();
}


void CGeneralSettingsDlg::OnBnClickedCpuCheck()
{
    // TODO: 在此添加控件通知处理程序代码
    bool checked = IsDlgButtonChecked(IDC_CPU_CHECK) != 0;
    if (checked && !ShowHardwareMonitorWarning())
    {
        checked = false;
        CheckDlgButton(IDC_CPU_CHECK, FALSE);
    }
    m_data.SetHardwareEnable(HI_CPU, checked);
}


void CGeneralSettingsDlg::OnBnClickedGpuCheck()
{
    // TODO: 在此添加控件通知处理程序代码
    bool checked = IsDlgButtonChecked(IDC_GPU_CHECK) != 0;
    if (checked && !ShowHardwareMonitorWarning())
    {
        checked = false;
        CheckDlgButton(IDC_GPU_CHECK, FALSE);
    }
    m_data.SetHardwareEnable(HI_GPU, checked);
}


void CGeneralSettingsDlg::OnBnClickedHddCheck()
{
    // TODO: 在此添加控件通知处理程序代码
    bool checked = IsDlgButtonChecked(IDC_HDD_CHECK) != 0;
    if (checked && !ShowHardwareMonitorWarning())
    {
        checked = false;
        CheckDlgButton(IDC_HDD_CHECK, FALSE);
    }
    m_data.SetHardwareEnable(HI_HDD, checked);
}


void CGeneralSettingsDlg::OnBnClickedMbdCheck()
{
    // TODO: 在此添加控件通知处理程序代码
    bool checked = IsDlgButtonChecked(IDC_MBD_CHECK) != 0;
    if (checked && !ShowHardwareMonitorWarning())
    {
        checked = false;
        CheckDlgButton(IDC_MBD_CHECK, FALSE);
    }
    m_data.SetHardwareEnable(HI_MBD, checked);

}


void CGeneralSettingsDlg::OnCbnSelchangeSelectCpuCombo()
{
    // TODO: 在此添加控件通知处理程序代码
    //没有选中项时保留原值，避免把空串写进 cpu_core_name
    if (m_select_cpu_combo.GetCurSel() < 0)
        return;
    CString cpu_core_name;
    m_select_cpu_combo.GetWindowText(cpu_core_name);
    m_data.cpu_core_name = cpu_core_name.GetString();
}


void CGeneralSettingsDlg::OnBnClickedPluginManageButton()
{
    // TODO: 在此添加控件通知处理程序代码
    CPluginManagerDlg dlg;
    dlg.DoModal();
}


void CGeneralSettingsDlg::OnBnClickedShowNotifyIconCheck()
{
    // TODO: 在此添加控件通知处理程序代码
    m_data.show_notify_icon = (IsDlgButtonChecked(IDC_SHOW_NOTIFY_ICON_CHECK) != 0);
    //隐藏通知区图标时联动禁用"图标显示网速"
    EnableDlgCtrl(IDC_NOTIFY_ICON_SHOW_SPEED_CHECK, m_data.show_notify_icon);
}


void CGeneralSettingsDlg::OnBnClickedNotifyIconShowSpeedCheck()
{
    m_data.notify_icon_show_speed = (IsDlgButtonChecked(IDC_NOTIFY_ICON_SHOW_SPEED_CHECK) != 0);
}


void CGeneralSettingsDlg::OnBnClickedSelectConnectionsButton()
{
    // TODO: 在此添加控件通知处理程序代码
    CSelectConnectionsDlg dlg(m_data.connections_hide);
    if (dlg.DoModal() == IDOK)
    {
        m_data.connections_hide = dlg.GetData();
    }
}


void CGeneralSettingsDlg::OnBnClickedResetAutoRunButton()
{
    //“重置”是对实时系统状态的操作（立即重建自启动项），不是草稿改动：
    //成功后必须同步 m_data.auto_run 与 m_auto_run_modified，
    //否则取消无法还原、而确定时又会因为标志位未置位而跳过写入。
    bool auto_run = theApp.SetAutoRun(true, m_data.auto_run_by_task_scheduler);    //重新设置开机自动运行
    if (!auto_run)
    {
        MessageBox(CCommon::LoadText(IDS_SET_AUTO_RUN_FAILED_WARNING), NULL, MB_ICONWARNING | MB_OK);
        return;
    }
    //获取开机自动运行的路径
    auto_run = theApp.GetAutoRun(&m_auto_run_path, m_data.auto_run_by_task_scheduler);
    m_data.auto_run = auto_run;
    m_auto_run_modified = true;
    //重新勾选“开机自动运行”复选框
    CheckDlgButton(IDC_AUTO_RUN_CHECK, auto_run);
    //更新鼠标提示
    AddOrUpdateAutoRunTooltip(false);
}


void CGeneralSettingsDlg::OnEnChangeMonitorSpanEdit()
{
    m_data.monitor_time_span = m_monitor_span_edit.GetValue();
}

void CGeneralSettingsDlg::OnBnClickedAutoRunMethodRegestryRadio()
{
    SetAutoRunEnabledByMethod(false);
}

void CGeneralSettingsDlg::OnBnClickedAutoRunMethodTaskScheduleRadio()
{
    SetAutoRunEnabledByMethod(true);
}

//选择开机自动运行的方式时，如果“开机自动运行”复选框尚未勾选，则自动勾选上，
//避免出现“选中了方式，但点击应用后没有任何效果”的情况
void CGeneralSettingsDlg::SetAutoRunEnabledByMethod(bool auto_run_by_task_scheduler)
{
    m_data.auto_run_by_task_scheduler = auto_run_by_task_scheduler;
    if (!m_data.auto_run)
    {
        m_data.auto_run = true;
        CheckDlgButton(IDC_AUTO_RUN_CHECK, TRUE);
        //原来保存的路径可能已失效（如程序被移动），清除以避免提示信息显示错误路径
        m_auto_run_path.clear();
        AddOrUpdateAutoRunTooltip(false);
    }
    m_auto_run_modified = true;
    SetControlEnable();
}

void CGeneralSettingsDlg::ShowSection(int section, int height)
{
    //每次调用都按模板矩形重新锚定控件，避免相对位移累积。
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
    for (const auto& item : m_section_controls)
    {
        int item_height = item.rect.Height();
        {
            wchar_t cls[32]{};
            ::GetClassName(item.hwnd, cls, 32);
            if (_wcsicmp(cls, L"Edit") == 0)
                item_height = EditControlHeight();
        }
        ::SetWindowPos(item.hwnd, nullptr, item.rect.left, item.rect.top,
            item.rect.Width(), item_height, SWP_NOZORDER | SWP_NOACTIVATE);
        ::ShowWindow(item.hwnd, item.section == section && item.visible ? SW_SHOW : SW_HIDE);
    }
    SetRedraw(TRUE);
    RepaintPageSwitch();
    const int heights[] = { 268, 292, 270 };
    CRect content(0, 0, 0, heights[max(0, min(section, 2))]);
    MapDialogRect(content);
    SetScrollbarInfo(height, content.bottom);
    RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

void CGeneralSettingsDlg::RestoreUpdateSource()
{
    theApp.m_general_data.update_source = m_update_source_ori;
}

void CGeneralSettingsDlg::OnNotifyIconAppearance()
{
    CIconSelectDlg dlg(m_notify_icon_selected, this);
    if (dlg.DoModal() == IDOK)
    {
        m_notify_icon_selected = dlg.GetIconSelected();
    }
}
