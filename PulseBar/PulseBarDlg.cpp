

// PulseBarDlg.cpp : 实现文件

//



#include "stdafx.h"

#include "PulseBar.h"

#include "PulseBarDlg.h"

#include "afxdialogex.h"

#include "Test.h"

#include "PluginManagerDlg.h"

#include "SetItemOrderDlg.h"

#include "WindowsSettingHelper.h"

#include "PluginInfoDlg.h"

#include "FilePathHelper.h"

#include "WIC.h"

#include "SupportedRenderEnums.h"

#include "ClassicalTaskbarDlg.h"

#include "Win11TaskbarDlg.h"

#include "WineTaskbarDlg.h"

#include "TaskbarHelper.h"



#ifdef _DEBUG

#define new DEBUG_NEW

#endif







// CPulseBarDlg 对话框



//静态成员初始化

unsigned int CPulseBarDlg::m_WM_TASKBARCREATED{ ::RegisterWindowMessage(_T("TaskbarCreated")) };  //注册任务栏建立的消息

namespace
{
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

    void CheckMenuItemByCommand(CMenu* menu, UINT command_id, UINT flags)
    {
        CMenu* owner = FindMenuContainingCommand(menu, command_id);
        if (owner != nullptr)
            owner->CheckMenuItem(command_id, MF_BYCOMMAND | flags);
    }

    void EnableMenuItemByCommand(CMenu* menu, UINT command_id, UINT flags)
    {
        CMenu* owner = FindMenuContainingCommand(menu, command_id);
        if (owner != nullptr)
            owner->EnableMenuItem(command_id, MF_BYCOMMAND | flags);
    }
}


CPulseBarDlg::CPulseBarDlg(CWnd* pParent /*=NULL*/)

    : CDialog(IDD_TRAFFICMONITOR_DIALOG, pParent)

{

    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

    m_desktop_dc = ::GetDC(NULL);

}



CPulseBarDlg::~CPulseBarDlg()

{

    ExitMonitorThread();
    free(m_pIfTable);



    if (m_tBarDlg != nullptr)

    {

        delete m_tBarDlg;

        m_tBarDlg = nullptr;

    }



    ::ReleaseDC(NULL, m_desktop_dc);



    theApp.DestroyNotifyIcons();

}



CTaskBarDlg* CPulseBarDlg::GetTaskbarWindow() const

{

    if (IsTaskbarWndValid())

        return m_tBarDlg;

    else

        return nullptr;

}



CPulseBarDlg* CPulseBarDlg::Instance()

{

    return dynamic_cast<CPulseBarDlg*>(theApp.m_pMainWnd);

}



void CPulseBarDlg::DoDataExchange(CDataExchange* pDX)

{

    CDialog::DoDataExchange(pDX);

}



BEGIN_MESSAGE_MAP(CPulseBarDlg, CDialog)
    ON_WM_TIMER()
    ON_COMMAND(ID_NETWORK_INFO, &CPulseBarDlg::OnNetworkInfo)
    ON_WM_CLOSE()
    ON_MESSAGE(PB_QUIT_REQUEST_MESSAGE, &CPulseBarDlg::OnQuitRequest)
    ON_MESSAGE(PB_ALREADY_RUNNING_MESSAGE, &CPulseBarDlg::OnAlreadyRunning)
    ON_MESSAGE(WM_RESTART_FOR_PLUGIN, &CPulseBarDlg::OnRestartForPlugin)
    ON_MESSAGE(MY_WM_NOTIFYICON, &CPulseBarDlg::OnNotifyIcon)
    ON_COMMAND(ID_SHOW_NOTIFY_ICON, &CPulseBarDlg::OnShowNotifyIcon)
    ON_COMMAND(ID_NOTIFY_ICON_SHOW_SPEED, &CPulseBarDlg::OnNotifyIconShowSpeed)
    ON_COMMAND(ID_SHOW_SPEED_GRAPH_TIP, &CPulseBarDlg::OnShowSpeedGraphTip)
    ON_COMMAND_RANGE(ID_TB_THEME_CLASSIC, ID_TB_THEME_CUSTOM, &CPulseBarDlg::OnTbThemeSelect)
    ON_COMMAND(ID_TB_THEME_BG_IMAGE, &CPulseBarDlg::OnTbThemeBgImage)
    ON_COMMAND(ID_TB_THEME_SELECT_IMAGE, &CPulseBarDlg::OnTbThemeSelectImage)
    ON_COMMAND(ID_TB_THEME_OPACITY_SUB, &CPulseBarDlg::OnTbThemeOpacitySub)
    ON_COMMAND(ID_TB_THEME_OPACITY_ADD, &CPulseBarDlg::OnTbThemeOpacityAdd)
    ON_WM_DESTROY()
    ON_COMMAND(ID_SHOW_TASK_BAR_WND, &CPulseBarDlg::OnShowTaskBarWnd)
    ON_COMMAND(ID_APP_ABOUT, &CPulseBarDlg::OnAppAbout)
    ON_COMMAND(ID_SHOW_CPU_MEMORY2, &CPulseBarDlg::OnShowCpuMemory2)
    ON_REGISTERED_MESSAGE(m_WM_TASKBARCREATED, &CPulseBarDlg::OnTaskBarCreated)
    ON_COMMAND(ID_TRAFFIC_HISTORY, &CPulseBarDlg::OnTrafficHistory)
    ON_COMMAND(ID_OPTIONS, &CPulseBarDlg::OnOptions)
    ON_COMMAND(ID_OPTIONS2, &CPulseBarDlg::OnOptions2)
    ON_MESSAGE(WM_EXITMENULOOP, &CPulseBarDlg::OnExitmenuloop)
    ON_COMMAND(ID_CHANGE_NOTIFY_ICON, &CPulseBarDlg::OnChangeNotifyIcon)
    ON_COMMAND(ID_CHECK_UPDATE, &CPulseBarDlg::OnCheckUpdate)
    ON_MESSAGE(WM_TASKBAR_MENU_POPED_UP, &CPulseBarDlg::OnTaskbarMenuPopedUp)
    ON_WM_QUERYENDSESSION()
    ON_MESSAGE(WM_DPICHANGED, &CPulseBarDlg::OnDpichanged)
    ON_MESSAGE(WM_TASKBAR_WND_CLOSED, &CPulseBarDlg::OnTaskbarWndClosed)
    ON_MESSAGE(WM_MONITOR_INFO_UPDATED, &CPulseBarDlg::OnMonitorInfoUpdated)
    ON_MESSAGE(WM_DISPLAYCHANGE, &CPulseBarDlg::OnDisplaychange)
    ON_COMMAND(ID_PLUGIN_MANAGE, &CPulseBarDlg::OnPluginManage)
    ON_MESSAGE(WM_REOPEN_TASKBAR_WND, &CPulseBarDlg::OnReopenTaksbarWnd)
    ON_COMMAND(ID_OPEN_TASK_MANAGER, &CPulseBarDlg::OnOpenTaskManager)
    ON_MESSAGE(WM_SETTINGS_APPLIED, &CPulseBarDlg::OnSettingsApplied)
    ON_COMMAND(ID_DISPLAY_SETTINGS, &CPulseBarDlg::OnDisplaySettings)
    ON_COMMAND(ID_REFRESH_CONNECTION_LIST, &CPulseBarDlg::OnRefreshConnectionList)
    ON_MESSAGE(WM_TABLET_QUERYSYSTEMGESTURESTATUS, &CPulseBarDlg::OnTabletQuerysystemgesturestatus)
    ON_COMMAND(ID_PLUGIN_OPTIONS_TASKBAR, &CPulseBarDlg::OnPluginOptionsTaksbar)
    ON_COMMAND(ID_PLUGIN_DETAIL_TASKBAR, &CPulseBarDlg::OnPluginDetailTaksbar)
    //托盘插件动态命令通过独立 registry 路由，不经过任务栏点击状态。
    ON_COMMAND_RANGE(ID_PLUGIN_TRAY_START, ID_PLUGIN_TRAY_MAX, &CPulseBarDlg::OnPluginTrayCommand)
    ON_WM_POWERBROADCAST()
    ON_WM_DWMCOLORIZATIONCOLORCHANGED()

END_MESSAGE_MAP()





void CPulseBarDlg::GetScreenSize()

{

    //主窗口已移除：只保留任务栏自动背景色所需的屏幕尺寸

    m_screen_size.cx = GetSystemMetrics(SM_CXSCREEN);

    m_screen_size.cy = GetSystemMetrics(SM_CYSCREEN);

}





void CPulseBarDlg::AutoSelect()

{

    CSingleLock connection_lock(&m_connection_lock, TRUE);
    unsigned __int64 max_in_out_bytes{};

    unsigned __int64 in_out_bytes;

    //m_connection_selected = m_connections[0].index;

    m_connection_selected = 0;

    //自动选择连接时，查找已发送和已接收字节数之和最多的那个连接，并将其设置为当前查看的连接

    for (size_t i{}; i < m_connections.size(); i++)

    {

        auto table = GetConnectIfTable(i);

        if (table.dwOperStatus == IF_OPER_STATUS_OPERATIONAL)     //只选择网络状态为正常的连接

        {

            in_out_bytes = table.dwInOctets + table.dwOutOctets;

            if (in_out_bytes > max_in_out_bytes)

            {

                max_in_out_bytes = in_out_bytes;

                m_connection_selected = i;

            }

        }

    }

    theApp.m_cfg_data.m_connection_name = GetConnection(m_connection_selected).description_2;

    m_connection_change_flag = true;

}



bool CPulseBarDlg::UpdateIfTable(bool* resized)

{

    const auto is_valid_table = [](const MIB_IFTABLE* table, DWORD capacity)
    {
        if (table == nullptr || capacity < FIELD_OFFSET(MIB_IFTABLE, table))
            return false;
        const size_t available_rows =
            (capacity - FIELD_OFFSET(MIB_IFTABLE, table)) / sizeof(MIB_IFROW);
        return table->dwNumEntries <= available_rows;
    };

    if (resized != nullptr)
        *resized = false;

    DWORD required_size = m_dwSize;
    if (m_pIfTable != nullptr && required_size >= sizeof(MIB_IFTABLE))
    {
        DWORD actual_size = required_size;
        const DWORD result = GetIfTable(m_pIfTable, &actual_size, FALSE);
        if (result == NO_ERROR)
        {
            if (!is_valid_table(m_pIfTable, required_size))
            {
                free(m_pIfTable);
                m_pIfTable = nullptr;
                m_dwSize = 0;
                return false;
            }
            return true;
        }
        if (result != ERROR_INSUFFICIENT_BUFFER || actual_size <= required_size)
            return false;
        required_size = actual_size;
        free(m_pIfTable);
        m_pIfTable = nullptr;
        m_dwSize = 0;
    }
    else
    {
        required_size = sizeof(MIB_IFTABLE);
    }

    for (int attempt = 0; attempt < 4; ++attempt)
    {
        MIB_IFTABLE* new_table = static_cast<MIB_IFTABLE*>(malloc(required_size));
        if (new_table == nullptr)
            return false;

        DWORD actual_size = required_size;
        const DWORD result = GetIfTable(new_table, &actual_size, FALSE);
        if (result == NO_ERROR)
        {
            if (!is_valid_table(new_table, required_size))
            {
                free(new_table);
                return false;
            }
            free(m_pIfTable);
            m_pIfTable = new_table;
            m_dwSize = required_size;
            if (resized != nullptr)
                *resized = true;
            return true;
        }

        free(new_table);
        if (result != ERROR_INSUFFICIENT_BUFFER || actual_size <= required_size)
            return false;
        required_size = actual_size;
    }

    return false;
}


void CPulseBarDlg::IniConnection()

{

    CSingleLock connection_lock(&m_connection_lock, TRUE);
    if (!UpdateIfTable())
        return;



    //获取当前所有的连接，并保存到m_connections容器中

    if (!theApp.m_general_data.show_all_interface)

    {

        m_connections.clear();

        vector<NetWorkConection> connections;

        CAdapterCommon::GetAdapterInfo(connections);

        for (const auto& item : connections)

        {

            if (!theApp.m_general_data.connections_hide.Contains(CCommon::StrToUnicode(item.description.c_str())))

                m_connections.push_back(item);

        }

        CAdapterCommon::GetIfTableInfo(m_connections, m_pIfTable);

    }

    else

    {

        CAdapterCommon::GetAllIfTableInfo(m_connections, m_pIfTable);

    }



    //如果在设置了“显示所有网络连接”时设置了“选择全部”，则改为“自动选择”

    if (theApp.m_general_data.show_all_interface && theApp.m_cfg_data.m_select_all)

    {

        theApp.m_cfg_data.m_select_all = false;

        theApp.m_cfg_data.m_auto_select = true;

    }



    //写入调试日志

    if (theApp.m_debug_log)

    {

        CString log_str;

        log_str += _T("正在初始化网络连接...\n");

        log_str += _T("连接列表：\n");

        for (size_t i{}; i < m_connections.size(); i++)

        {

            log_str += m_connections[i].description.c_str();

            log_str += _T(", ");

            log_str += CCommon::IntToString(m_connections[i].index);

            log_str += _T("\n");

        }

        log_str += _T("IfTable:\n");

        for (size_t i{}; i < m_pIfTable->dwNumEntries; i++)

        {

            log_str += CCommon::IntToString(i);

            log_str += _T(" ");

            log_str += (const char*)m_pIfTable->table[i].bDescr;

            log_str += _T("\n");

        }

        CCommon::WriteLog(log_str, (theApp.m_config_dir + L".\\connections.log").c_str());

    }



    //if (m_connection_selected < 0 || m_connection_selected >= m_connections.size() || m_auto_select)

    //  AutoSelect();

    //选择网络连接

    if (theApp.m_cfg_data.m_auto_select)    //自动选择

    {

        if (m_restart_cnt != -1)    //当m_restart_cnt不等于-1时，即不是第一次初始化时，需要延时5秒再重新初始化连接

        {

            KillTimer(DELAY_TIMER);

            SetTimer(DELAY_TIMER, 5000, NULL);

        }

        else

        {

            AutoSelect();

        }

    }

    else        //查找网络名为上次选择的连接

    {

        m_connection_selected = 0;

        for (size_t i{}; i < m_connections.size(); i++)

        {

            if (m_connections[i].description_2 == m_connection_name_preferd)

                m_connection_selected = i;

        }

    }

    if (m_connection_selected < 0 || m_connection_selected >= m_connections.size())

        m_connection_selected = 0;

    theApp.m_cfg_data.m_connection_name = GetConnection(m_connection_selected).description_2;



    //根据已获取到的连接在菜单中添加相应项目

    IniTaskBarConnectionMenu();     //初始化任务栏窗口中的“选择网络连接”子菜单项



    InsertDynamicMenuItems();       //B3/B4:插入动态开关菜单项(带防重)



    m_restart_cnt++;    //记录初始化次数

    //映射刷新不代表用户切换采样范围；按 LUID 保留正常接口的连续基线。

}



MIB_IFROW CPulseBarDlg::GetConnectIfTable(int connection_index)

{
    CSingleLock connection_lock(&m_connection_lock, TRUE);

    if (connection_index >= 0 && connection_index < static_cast<int>(m_connections.size())
        && m_pIfTable != nullptr)
    {
        const auto interface_index = m_connections[connection_index].interface_index;
        for (DWORD row = 0; row < m_pIfTable->dwNumEntries; ++row)
        {
            if (interface_index != 0 && m_pIfTable->table[row].dwIndex == interface_index)
                return m_pIfTable->table[row];
        }
    }

    return MIB_IFROW();

}



NetWorkConection CPulseBarDlg::GetConnection(int connection_index)

{
    CSingleLock connection_lock(&m_connection_lock, TRUE);

    if (connection_index >= 0 && connection_index < static_cast<int>(m_connections.size()))

        return m_connections[connection_index];

    else

        return NetWorkConection();

}



void CPulseBarDlg::IniConnectionMenu(CMenu* pMenu)

{

    CSingleLock connection_lock(&m_connection_lock, TRUE);
    ASSERT(pMenu != nullptr);

    if (pMenu != nullptr)

    {

        const int select_all_pos = [&]()
        {
            for (int index = 0; index < pMenu->GetMenuItemCount(); ++index)
            {
                if (pMenu->GetMenuItemID(index) == ID_SELECT_ALL_CONNECTION)
                    return index;
            }
            return -1;
        }();
        if (select_all_pos < 0)
            return;

        //连接详情是资源中的固定项目。动态网卡项目和刷新命令插入到它之前，
        //清理时只删除动态区域，避免误删固定的“连接详情”。
        int detail_pos = -1;
        for (int index = select_all_pos + 1; index < pMenu->GetMenuItemCount(); ++index)
        {
            if (pMenu->GetMenuItemID(index) == ID_NETWORK_INFO)
            {
                detail_pos = index;
                break;
            }
        }

        int dynamic_start = select_all_pos + 1;
        if (detail_pos >= 0)
        {
            for (int index = select_all_pos + 1; index < detail_pos; ++index)
            {
                if (pMenu->GetMenuItemID(index) == 0)
                {
                    dynamic_start = index + 1;
                    break;
                }
            }

            //删除上次生成的网卡项目、分隔线和刷新命令，保留资源中的固定分隔线。
            while (detail_pos > dynamic_start)
            {
                pMenu->DeleteMenu(--detail_pos, MF_BYPOSITION);
            }
        }
        else
        {
            //兼容没有固定“连接详情”的旧菜单资源，只清理动态命令范围，
            //但保留资源中的分隔线，并把新项目插到分隔线之后。
            int separator_pos = -1;
            for (int index = select_all_pos + 1; index < pMenu->GetMenuItemCount(); ++index)
            {
                if (pMenu->GetMenuItemID(index) == 0)
                {
                    separator_pos = index;
                    break;
                }
            }
            for (int index = pMenu->GetMenuItemCount() - 1; index > select_all_pos; --index)
            {
                UINT command_id = pMenu->GetMenuItemID(index);
                if ((command_id > ID_SELECT_ALL_CONNECTION && command_id <= ID_SELETE_CONNECTION_MAX)
                    || command_id == ID_REFRESH_CONNECTION_LIST)
                    pMenu->DeleteMenu(index, MF_BYPOSITION);
            }
            dynamic_start = separator_pos >= 0 ? separator_pos + 1 : select_all_pos + 1;
        }

        CString connection_descr;

        int insert_pos = dynamic_start;
        for (size_t i{}; i < m_connections.size(); i++)

        {

            connection_descr = CCommon::StrToUnicode(m_connections[i].description.c_str()).c_str();

            pMenu->InsertMenu(insert_pos++, MF_BYPOSITION | MF_STRING | MF_ENABLED,
                ID_SELECT_ALL_CONNECTION + i + 1, connection_descr);

        }



        //添加“刷新网络列表”命令

        pMenu->InsertMenu(insert_pos++, MF_BYPOSITION | MF_SEPARATOR);

        pMenu->InsertMenu(insert_pos, MF_BYPOSITION | MF_STRING | MF_ENABLED,
            ID_REFRESH_CONNECTION_LIST, CCommon::LoadText(IDS_REFRESH_CONNECTION_LIST));

    }

}



void CPulseBarDlg::IniTaskBarConnectionMenu()

{

    //按命令 ID 递归定位“选择网络连接”子菜单，避免依赖资源中的固定位置。
    CMenu* taskbar_menu = FindMenuContainingCommand(&theApp.m_taskbar_menu, ID_DISPLAY_SETTINGS);
    CMenu* plugin_taskbar_menu = FindMenuContainingCommand(&theApp.m_taskbar_menu_plugin, ID_DISPLAY_SETTINGS);
    IniConnectionMenu(FindMenuContainingCommand(taskbar_menu, ID_SELETE_CONNECTION));
    IniConnectionMenu(FindMenuContainingCommand(plugin_taskbar_menu, ID_SELETE_CONNECTION));

}



//B3/B4 的开关菜单项已直接放入任务栏菜单资源，

//弹出菜单的处理函数只负责更新勾选状态,不做结构性修改。

void CPulseBarDlg::InsertDynamicMenuItems()

{

    CMenu* taskbar_menus[2] =
    {
        FindMenuContainingCommand(&theApp.m_taskbar_menu, ID_DISPLAY_SETTINGS),
        FindMenuContainingCommand(&theApp.m_taskbar_menu_plugin, ID_DISPLAY_SETTINGS)
    };

    for (CMenu* pMenu : taskbar_menus)

    {

        if (pMenu != nullptr)
            CheckMenuItemByCommand(pMenu, ID_SHOW_SPEED_GRAPH_TIP,
                theApp.m_taskbar_data.show_speed_graph_tip ? MF_CHECKED : MF_UNCHECKED);

    }

}



void CPulseBarDlg::SetConnectionMenuState(CMenu* pMenu)
{
    if (pMenu == nullptr)
        return;

    CSingleLock connection_lock(&m_connection_lock, TRUE);
    auto find_position = [pMenu](UINT command_id)
    {
        for (int index = 0; index < pMenu->GetMenuItemCount(); ++index)
        {
            if (pMenu->GetMenuItemID(index) == command_id)
                return index;
        }
        return -1;
    };

    const int auto_pos = find_position(ID_SELETE_CONNECTION);
    const int select_all_pos = find_position(ID_SELECT_ALL_CONNECTION);
    if (auto_pos < 0 || select_all_pos < 0)
        return;

    const int first_pos = min(auto_pos, select_all_pos);
    int last_pos = max(auto_pos, select_all_pos);
    for (int index = 0; index < pMenu->GetMenuItemCount(); ++index)
    {
        const UINT command_id = pMenu->GetMenuItemID(index);
        if (command_id > ID_SELECT_ALL_CONNECTION &&
            command_id <= ID_SELECT_ALL_CONNECTION + m_connections.size())
        {
            last_pos = max(last_pos, index);
        }
    }

    int selected_pos = auto_pos;
    UINT selected_command = ID_SELETE_CONNECTION;
    if (theApp.m_cfg_data.m_select_all)
    {
        selected_pos = select_all_pos;
        selected_command = ID_SELECT_ALL_CONNECTION;
    }
    else if (!theApp.m_cfg_data.m_auto_select)
    {
        selected_command = ID_SELECT_ALL_CONNECTION + m_connection_selected + 1;
        selected_pos = find_position(selected_command);
        //动态项位于资源分隔线之后；找不到时回退到“自动选择”，绝不能选到分隔线。
        if (selected_pos < 0)
        {
            selected_pos = auto_pos;
            selected_command = ID_SELETE_CONNECTION;
        }
    }

    pMenu->CheckMenuRadioItem(first_pos, last_pos, selected_pos,
        MF_BYPOSITION | MF_CHECKED);
    if (theApp.m_cfg_data.m_select_all)
        pMenu->SetDefaultItem(static_cast<UINT>(-1), TRUE);
    else
        pMenu->SetDefaultItem(selected_command, FALSE);
}




void CPulseBarDlg::CloseTaskBarWnd()

{

    //S6: 先强制隐藏悬停面板(含解除钉住),防止置顶面板残留

    CSpeedGraphTip::Instance().ForceHide();

    if (m_tBarDlg != nullptr)

    {

        if (IsTaskbarWndValid())

            m_tBarDlg->OnCancel();

        delete m_tBarDlg;

        m_tBarDlg = nullptr;

        theApp.m_taskbar_data.update_layered_window_error_code = 0;

    }

}



void CPulseBarDlg::OpenTaskBarWnd()

{

    if (m_tBarDlg != nullptr)

    {

        if (IsTaskbarWndValid())

            return;

        delete m_tBarDlg;

        m_tBarDlg = nullptr;

    }

    theApp.m_taskbar_data.update_layered_window_error_code = 0;

    // 强制初始化theApp.m_is_windows11_taskbar的值

    theApp.CheckWindows11Taskbar();

    if (theApp.m_win_version.IsWine())

        m_tBarDlg = new CWineTaskbarDlg();

    else if (theApp.IsWindows11Taskbar())

        m_tBarDlg = new CWin11TaskbarDlg();

    else

        m_tBarDlg = new CClassicalTaskbarDlg();



    CSupportedRenderEnums supported_render_enums{};

    CTaskBarDlg::DisableRenderFeatureIfNecessary(supported_render_enums);

    auto render_type = supported_render_enums.GetAutoFitEnum();

    // WS_EX_LAYERED 和 WS_EX_NOREDIRECTIONBITMAP 可以共存，见微软示例代码

    // https://github.com/microsoft/Windows-classic-samples/blob/7cbd99ac1d2b4a0beffbaba29ea63d024ceff700/Samples/DynamicDPI/cpp/SampleDesktopWindow.cpp#L179

    // 但是WS_EX_NOREDIRECTIONBITMAP似乎会导致UpdateLayeredWindowIndirect失败

    BOOL created = FALSE;
    switch (render_type)

    {

        using namespace DrawCommonHelper;

    case RenderType::D2D1_WITH_DCOMPOSITION:

        created = m_tBarDlg->Create(IDD_TASK_BAR_DIALOG_NOREDIRECTIONBITMAP, this);

        break;

    // 包括RenderType::D2D1在内的其他值

    default:

        created = m_tBarDlg->Create(IDD_TASK_BAR_DIALOG, this);

        break;

    }

    if (!created || !::IsWindow(m_tBarDlg->GetSafeHwnd()) || !m_tBarDlg->IsReadyToShow())

    {

        if (::IsWindow(m_tBarDlg->GetSafeHwnd()))
            m_tBarDlg->DestroyWindow();
        delete m_tBarDlg;
        m_tBarDlg = nullptr;
        return;

    }

    m_tBarDlg->ShowWindow(SW_SHOW);

    //m_tBarDlg->ShowInfo();

    //IniTaskBarConnectionMenu();

}



void CPulseBarDlg::AddNotifyIcon()

{

    if (theApp.m_cfg_data.m_show_task_bar_wnd)

        CloseTaskBarWnd();

    //添加前按当前模式取图，隐藏期间修改样式也不会复用旧句柄。
    KillTimer(DELETE_NOTIFY_ICON_TIMER);
    m_ntIcon.hIcon = GetCurrentNotifyIcon();
    m_ntIcon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    if (!::Shell_NotifyIcon(NIM_ADD, &m_ntIcon))
        m_last_notify_tip.Empty();   //添加失败不能把尚未发布的新图标当作已更新。

    if (theApp.m_cfg_data.m_show_task_bar_wnd)

        OpenTaskBarWnd();

}



void CPulseBarDlg::DeleteNotifyIcon()

{

    if (theApp.m_cfg_data.m_show_task_bar_wnd)

        CloseTaskBarWnd();

    //删除通知栏图标

    ::Shell_NotifyIcon(NIM_DELETE, &m_ntIcon);

    if (theApp.m_cfg_data.m_show_task_bar_wnd)

        OpenTaskBarWnd();

}



void CPulseBarDlg::ShowNotifyTip(const wchar_t* title, const wchar_t* message)

{

    //要显示通知区提示，必须先将通知区图标显示出来

    if (!theApp.m_general_data.show_notify_icon)

    {

        //添加通知栏图标

        AddNotifyIcon();

    }

    //显示通知提示

    m_ntIcon.uFlags |= NIF_INFO;

    //wcscpy_s(m_ntIcon.szInfo, message ? message : _T(""));

    //wcscpy_s(m_ntIcon.szInfoTitle, title ? title : _T(""));

    CCommon::WStringCopy(m_ntIcon.szInfo, 256, message);

    CCommon::WStringCopy(m_ntIcon.szInfoTitle, 64, title);

    ::Shell_NotifyIcon(NIM_MODIFY, &m_ntIcon);

    m_ntIcon.uFlags &= ~NIF_INFO;



    //如果不显示通知区域图标，则在弹出通知的一段时间后删除通知区图标

    if (!theApp.m_general_data.show_notify_icon)

    {

        //延迟一定时间后删除通知区图标

        KillTimer(DELETE_NOTIFY_ICON_TIMER);

        SetTimer(DELETE_NOTIFY_ICON_TIMER, 8000, NULL);

    }

}



HICON CPulseBarDlg::GetCurrentNotifyIcon()
{
    if (theApp.m_general_data.notify_icon_show_speed)
    {
        HICON icon = m_tray_speed_icon.GetSpeedIcon(theApp.m_out_speed, theApp.m_in_speed, theApp.DPI(16));
        if (icon != nullptr)
            return icon;
    }
    else
        m_tray_speed_icon.DestroyIconInternal();
    return theApp.GetNotifyIcon(theApp.m_cfg_data.m_notify_icon_selected);
}

void CPulseBarDlg::UpdateNotifyIconTip(bool force)
{
    HICON icon = GetCurrentNotifyIcon();
    const bool icon_changed = icon != m_ntIcon.hIcon;
    m_ntIcon.hIcon = icon;
    m_ntIcon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;



    CString strTip;         //鼠标指向图标时显示的提示

#ifdef _DEBUG

    strTip = CCommon::LoadText(IDS_TRAFFICMONITOR, _T(" (Debug)"));

#else

    strTip = CCommon::LoadText(IDS_TRAFFICMONITOR);

#endif



    CString in_speed = CCommon::DataSizeToString(theApp.m_in_speed);

    CString out_speed = CCommon::DataSizeToString(theApp.m_out_speed);



    strTip += CCommon::StringFormat(_T("\r\n<%1%>: <%2%>/s"), { CCommon::LoadText(IDS_UPLOAD), out_speed });

    strTip += CCommon::StringFormat(_T("\r\n<%1%>: <%2%>/s"), { CCommon::LoadText(IDS_DOWNLOAD), in_speed });

    strTip += CCommon::StringFormat(_T("\r\nCPU: <%1%> %"), { theApp.m_cpu_usage });

    strTip += CCommon::StringFormat(_T("\r\n<%1%>: <%2%> %"), { CCommon::LoadText(IDS_MEMORY), theApp.m_memory_usage });

    if (IsTemperatureNeeded())

    {

        if (theApp.m_general_data.IsHardwareEnable(HI_GPU) && theApp.m_gpu_usage >= 0)

            strTip += CCommon::StringFormat(_T("\r\n<%1%>: <%2%> %"), { CCommon::LoadText(IDS_GPU_USAGE), theApp.m_gpu_usage });

        if (theApp.m_general_data.IsHardwareEnable(HI_CPU) && theApp.m_cpu_temperature > 0)

            strTip += CCommon::StringFormat(_T("\r\n<%1%>: <%2%> °C"), { CCommon::LoadText(IDS_CPU_TEMPERATURE), static_cast<int>(theApp.m_cpu_temperature) });

        if (theApp.m_general_data.IsHardwareEnable(HI_GPU) && theApp.m_gpu_temperature > 0)

            strTip += CCommon::StringFormat(_T("\r\n<%1%>: <%2%> °C"), { CCommon::LoadText(IDS_GPU_TEMPERATURE), static_cast<int>(theApp.m_gpu_temperature) });

        if (theApp.m_general_data.IsHardwareEnable(HI_HDD) && theApp.m_hdd_temperature > 0)

            strTip += CCommon::StringFormat(_T("\r\n<%1%>: <%2%> °C"), { CCommon::LoadText(IDS_HDD_TEMPERATURE), static_cast<int>(theApp.m_hdd_temperature) });

        if (theApp.m_general_data.IsHardwareEnable(HI_MBD) && theApp.m_main_board_temperature > 0)

            strTip += CCommon::StringFormat(_T("\r\n<%1%>: <%2%> °C"), { CCommon::LoadText(IDS_MAINBOARD_TEMPERATURE), static_cast<int>(theApp.m_main_board_temperature) });

        if (theApp.m_general_data.IsHardwareEnable(HI_HDD) && theApp.m_hdd_usage >= 0)

            strTip += CCommon::StringFormat(_T("\r\n<%1%>: <%2%> %"), { CCommon::LoadText(IDS_HDD_USAGE), theApp.m_hdd_usage });

    }



    CCommon::WStringCopy(m_ntIcon.szTip, 128, strTip);

    //图标和提示独立比较；模式或主题变化不能被相同的文字跳过。
    if (force || icon_changed || strTip != m_last_notify_tip)
    {
        if (::Shell_NotifyIcon(NIM_MODIFY, &m_ntIcon))
            m_last_notify_tip = strTip;
        else
            m_last_notify_tip.Empty();
    }



}



bool CPulseBarDlg::SaveHistoryTraffic()
{
    CSingleLock history_lock(&m_history_lock, TRUE);
    return m_history_traffic.SaveTodayOnly();
}



bool CPulseBarDlg::SaveHistoryTrafficFull()
{
    CSingleLock history_lock(&m_history_lock, TRUE);
    return m_history_traffic.Save();
}



void CPulseBarDlg::LoadHistoryTraffic()

{

    const bool main_loaded = m_history_traffic.Load();
    CHistoryTrafficFile backup_file(theApp.m_history_traffic_path + L".bak");

    // 按日期合并有效备份，条数相同也可能缺少不同日期。
    const size_t size_before = m_history_traffic.Size();
    bool backup_missing{};
    const bool backup_loaded = backup_file.Load(&backup_missing) && !backup_missing;
    if (backup_loaded)
    {
        if (main_loaded)
            m_history_traffic.Merge(backup_file, true);
        else
        {
            // 只有有效备份才能解除加载失败的写保护。
            m_history_traffic = backup_file;
            m_history_traffic.SetFilePath(theApp.m_history_traffic_path);
        }
    }
    if (!backup_missing && !backup_loaded)
    {
        m_history_traffic.PreserveBackup();
        CCommon::WriteLog(L"History backup could not be loaded; backup replacement is disabled to preserve the original file.", theApp.m_log_path.c_str());
    }
    if (!main_loaded && !backup_loaded)
        CCommon::WriteLog(L"History could not be loaded; automatic saving is disabled to preserve the original file.", theApp.m_log_path.c_str());
    const size_t size_after = m_history_traffic.Size();
    if (size_after > size_before)
    {
        CString log_info = CCommon::LoadTextFormat(IDS_HISTORY_TRAFFIC_LOST_ERROR_LOG, { size_before, size_after - size_before });
        CCommon::WriteLog(log_info, theApp.m_log_path.c_str());
    }



    theApp.m_today_up_traffic = m_history_traffic.GetTodayUpTraffic();

    theApp.m_today_down_traffic = m_history_traffic.GetTodayDownTraffic();

}



bool CPulseBarDlg::BackupHistoryTrafficFile()
{
    CSingleLock history_lock(&m_history_lock, TRUE);
    return m_history_traffic.SaveBackup();
}



void CPulseBarDlg::_OnOptions(int tab, CWnd* pParent)

{

    COptionsDlg optionsDlg(tab, pParent);



    //将选项设置数据传递给选项设置对话框

    if (COptionsDlg::GetUniqueHandel(OPTION_DLG_NAME) == NULL)     //确保此时选项设置对话框已经关闭

    {

        optionsDlg.m_tab2_dlg.m_data = theApp.m_taskbar_data;

        optionsDlg.m_tab3_dlg.m_data = theApp.m_general_data;

    }



    if (optionsDlg.DoModal() == IDOK)

    {

        ApplySettings(optionsDlg);

    }

}



void CPulseBarDlg::ApplySettings(COptionsDlg& optionsDlg)

{

    optionsDlg.m_tab2_dlg.SaveColorSettingToDefaultStyle();
    bool is_notify_icon_style_changed = (optionsDlg.m_notify_icon_selected != theApp.m_cfg_data.m_notify_icon_selected);
    bool is_notify_icon_mode_changed = (optionsDlg.m_tab3_dlg.m_data.notify_icon_show_speed != theApp.m_general_data.notify_icon_show_speed);
    bool is_hardware_monitor_item_changed = (optionsDlg.m_tab3_dlg.m_data.hardware_monitor_item != theApp.m_general_data.hardware_monitor_item);

    bool is_show_notify_icon_changed = (optionsDlg.m_tab3_dlg.m_data.show_notify_icon != theApp.m_general_data.show_notify_icon);

    bool is_connections_hide_changed = (optionsDlg.m_tab3_dlg.m_data.connections_hide.data() != theApp.m_general_data.connections_hide.data());

    bool d2d_turned_on = (theApp.m_taskbar_data.disable_d2d && !optionsDlg.m_tab2_dlg.m_data.disable_d2d);

    //需要重新关闭再打开任务栏窗口的情况

    bool taskbar_changed = (theApp.m_taskbar_data.show_taskbar_wnd_in_secondary_display != optionsDlg.m_tab2_dlg.m_data.show_taskbar_wnd_in_secondary_display

        || theApp.m_taskbar_data.secondary_display_index != optionsDlg.m_tab2_dlg.m_data.secondary_display_index

        || theApp.m_taskbar_data.disable_d2d != optionsDlg.m_tab2_dlg.m_data.disable_d2d

        || theApp.m_taskbar_data.IsTaskbarTransparent() != optionsDlg.m_tab2_dlg.m_data.IsTaskbarTransparent()

        || theApp.m_taskbar_data.auto_set_background_color != optionsDlg.m_tab2_dlg.m_data.auto_set_background_color
        || theApp.m_taskbar_data.tb_theme != optionsDlg.m_tab2_dlg.m_data.tb_theme
        || theApp.m_taskbar_data.tb_use_background_image != optionsDlg.m_tab2_dlg.m_data.tb_use_background_image
        || theApp.m_taskbar_data.tb_background_image != optionsDlg.m_tab2_dlg.m_data.tb_background_image
        || theApp.m_taskbar_data.custom_value_bold != optionsDlg.m_tab2_dlg.m_data.custom_value_bold

        );

    theApp.m_taskbar_data = optionsDlg.m_tab2_dlg.m_data;

    //配置目录切换：先把现有配置、历史和插件迁移到新位置，失败则保持原目录
    const bool target_portable_mode = optionsDlg.m_tab3_dlg.m_data.portable_mode;
    if (target_portable_mode != theApp.m_general_data.portable_mode)
    {
        CString migrate_message;
        const bool migrated = theApp.MigrateConfigDir(target_portable_mode, migrate_message);
        if (!migrated)
        {
            optionsDlg.m_tab3_dlg.m_data.portable_mode = theApp.m_general_data.portable_mode;   //回滚，不改设置
        }
        else
        {
            //历史文件对象内部缓存了旧路径，必须同步，否则退出时仍写旧目录
            m_history_traffic.SetFilePath(theApp.m_history_traffic_path);
        }
        if (!migrate_message.IsEmpty())
            AfxMessageBox(migrate_message, (migrated ? MB_ICONINFORMATION : MB_ICONWARNING) | MB_OK);
    }

    theApp.m_general_data = optionsDlg.m_tab3_dlg.m_data;
    theApp.PublishMonitorSettings(); //Publish before plugins, timers or hardware can observe this commit.
    theApp.m_cfg_data.m_notify_icon_selected = theApp.NormalizeNotifyIconSelection(optionsDlg.m_notify_icon_selected);
    if (is_notify_icon_style_changed || is_notify_icon_mode_changed)
        UpdateNotifyIconTip(true);

    theApp.SendSettingsToPlugin();



    CGeneralSettingsDlg::CheckTaskbarDisplayItem();



    //打开了D2D渲染后自动开启“背景透明”并关闭“根据任务栏颜色自动设置背景色”

    if (d2d_turned_on)

    {

        theApp.m_taskbar_data.SetTaskabrTransparent(true);

        theApp.m_taskbar_data.auto_set_background_color = false;

    }



    //CTaskBarDlg::SaveConfig();

    if (IsTaskbarWndValid())
    {
        if (taskbar_changed || m_tBarDlg->NeedsWindowRecreation())
        {
            CloseTaskBarWnd();
            OpenTaskBarWnd();
        }
        else
        {
            m_tBarDlg->ApplySettings();
            m_tBarDlg->WidthChanged();
        }
        //Create/渲染器初始化失败时 m_tBarDlg 已清空，不能再解引用。
        if (IsTaskbarWndValid())
            m_tBarDlg->ApplyWindowTransparentColor();
    }

    if (optionsDlg.m_tab3_dlg.IsAutoRunModified())

    {

        if (!theApp.SetAutoRun(theApp.m_general_data.auto_run, theApp.m_general_data.auto_run_by_task_scheduler))

        {

            //失败原因已写入 error.log，这里把日志位置一并告知用户，便于排查
            CString warn = CCommon::LoadText(IDS_SET_AUTO_RUN_FAILED_WARNING);
            warn += _T("\r\n");
            warn += CCommon::LoadText(IDS_PATH, _T("Log: "));
            warn += theApp.m_log_path.c_str();
            MessageBox(warn, NULL, MB_ICONWARNING | MB_OK);
        }
        else
        {
            //成功：告知用户当前生效的是哪一种方式，以及任务计划时额外显示任务名/路径
            CString msg;
            if (theApp.m_general_data.auto_run && theApp.m_general_data.auto_run_by_task_scheduler)
            {
                //任务计划名格式固定为 "Autorun for <USERNAME>"，与 auto_start_helper.cpp 保持一致
                WCHAR username[256]{};
                GetEnvironmentVariable(L"USERNAME", username, 256);
                CString taskName;
                taskName.Format(L"Autorun for %s", username);
                msg = CCommon::LoadTextFormat(IDS_SET_AUTO_RUN_SUCCESS,
                    { taskName.GetString(), theApp.m_module_path.c_str() });
            }
            else
            {
                msg = CCommon::LoadText(IDS_SET_AUTO_RUN_SUCCESS);
            }
            MessageBox(msg, NULL, MB_ICONINFORMATION | MB_OK);
        }

    }



    if (optionsDlg.m_tab3_dlg.IsShowAllInterfaceModified() || is_connections_hide_changed)

        IniConnection();



    if (optionsDlg.m_tab3_dlg.IsMonitorTimeSpanModified())      //如果监控时间间隔改变了，则重设定时器

    {

        KillTimer(MONITOR_TIMER);

        SetTimer(MONITOR_TIMER, theApp.m_general_data.monitor_time_span, NULL);

    }



#ifndef WITHOUT_TEMPERATURE

    if (is_hardware_monitor_item_changed)

    {

        //如果关闭了硬件监控，则析构硬件监控类

        if (theApp.m_general_data.hardware_monitor_item == 0)

        {

            CSingleLock sync(&theApp.m_minitor_lib_critical, TRUE);

            theApp.m_pMonitor.reset();
            //必须在锁内把状态退回 NONE：采样线程先读状态再进锁，
            //若指针已空而状态仍是 READY，它会白等一次锁（无害）；
            //反过来（状态先变 NONE 而指针未释放）则会让采样线程漏读。
            theApp.m_monitor_state = CPulseBarApp::MONITOR_NONE;

        }

        else

        {

            //判空放在锁内：初始化线程在持锁时给 m_pMonitor 赋值，
            //锁外读 shared_ptr 会与那次赋值竞争（形式上就是数据竞争）。
            //但也不能无条件抢锁：冷启动时初始化线程正跨 .NET CreateInstance() 持锁数秒，
            //用户在此时点“确定”会白等一整段 .NET 加载。所以先用原子状态分流：
            //仍在初始化 → 交给那个线程（它会按最新的已发布采样快照设置启用项），
            //已就绪 → 才进锁取指针；NONE → 直接起初始化线程。
            if (theApp.IsHardwareMonitorInitializing())

            {

                //初始化线程收尾时会调用 UpdateOpenHardwareMonitorEnableState()，
                //它在取得硬件锁后读取最新发布快照，因此这里不需要再做任何事。

            }

            else

            {

                bool monitor_ready = false;

                {

                    CSingleLock sync(&theApp.m_minitor_lib_critical, TRUE);

                    monitor_ready = (theApp.m_pMonitor != nullptr);

                }

                if (monitor_ready)

                {

                    theApp.UpdateOpenHardwareMonitorEnableState();

                }

                else if (IsTemperatureNeeded())

                {

                    theApp.InitOpenHardwareLibInThread();

                }

            }

        }

        //更新任务栏窗口右键菜单

        //theApp.UpdateTaskbarWndMenu();

    }

#endif



    if (is_show_notify_icon_changed)

    {

        if (theApp.IsForceShowNotifyIcon())

            theApp.m_general_data.show_notify_icon = true;

        if (theApp.m_general_data.show_notify_icon)

            AddNotifyIcon();

        else

            DeleteNotifyIcon();

    }



    optionsDlg.m_tab2_dlg.OnSettingsApplied();
    theApp.SaveConfig();

    theApp.SaveGlobalConfig();

}



bool CPulseBarDlg::IsTaskbarWndValid() const

{

    return m_tBarDlg != nullptr && ::IsWindow(m_tBarDlg->GetSafeHwnd());

}



bool CPulseBarDlg::IsTemperatureNeeded() const
{
    return IsTemperatureNeeded(*theApp.GetMonitorSettingsSnapshot());
}

bool CPulseBarDlg::IsTemperatureNeeded(const MonitorSettings& settings)
{
    return settings.IsHardwareEnable(HI_CPU) || settings.IsHardwareEnable(HI_GPU)
        || settings.IsHardwareEnable(HI_HDD) || settings.IsHardwareEnable(HI_MBD);
}


// CPulseBarDlg 消息处理程序



BOOL CPulseBarDlg::OnInitDialog()

{

    CDialog::OnInitDialog();



    // 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动

    //  执行此操作

    SetIcon(m_hIcon, TRUE);         // 设置大图标

    SetIcon(m_hIcon, FALSE);        // 设置小图标



    // TODO: 在此添加额外的初始化代码

    SetWindowText(APP_NAME);

    //设置隐藏任务栏图标

    ModifyStyleEx(WS_EX_APPWINDOW, WS_EX_TOOLWINDOW);



    theApp.DPIFromWindow(this);

    //主窗口已移除：仅保留屏幕尺寸（任务栏自动背景色需要）

    GetScreenSize();



    //初始化菜单

    theApp.InitMenuResourse();



    IniConnection();    //初始化连接

    //任务栏隐藏时，托盘是唯一可打开设置的入口。
    if (theApp.IsForceShowNotifyIcon())
        theApp.m_general_data.show_notify_icon = true;



    //载入通知区图标
    theApp.LoadNotifyIcons();



    //设置通知区域图标

    m_ntIcon.cbSize = sizeof(NOTIFYICONDATA);   //该结构体变量的大小

    theApp.m_cfg_data.m_notify_icon_selected = theApp.NormalizeNotifyIconSelection(theApp.m_cfg_data.m_notify_icon_selected);
    m_ntIcon.hIcon = GetCurrentNotifyIcon();

    m_ntIcon.hWnd = this->m_hWnd;               //接收托盘图标通知消息的窗口句柄

    CString atip;           //鼠标指向图标时显示的提示

#ifdef _DEBUG

    atip = CCommon::LoadText(IDS_TRAFFICMONITOR, _T(" (Debug)"));

#else

    atip = CCommon::LoadText(IDS_TRAFFICMONITOR);

#endif

    //wcscpy_s(m_ntIcon.szTip, 128, strTip);

    CCommon::WStringCopy(m_ntIcon.szTip, 128, atip.GetString());

    m_ntIcon.uCallbackMessage = MY_WM_NOTIFYICON;   //应用程序定义的消息ID号

    m_ntIcon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP; //图标的属性：设置成员uCallbackMessage、hIcon、szTip有效

    if (theApp.m_general_data.show_notify_icon)

        ::Shell_NotifyIcon(NIM_ADD, &m_ntIcon); //在系统通知区域增加这个图标



    //载入流量历史记录

    LoadHistoryTraffic();



    //设置1000毫秒触发的定时器

    SetTimer(MAIN_TIMER, 1000, NULL);



    SetTimer(MONITOR_TIMER, theApp.m_general_data.monitor_time_span, NULL);

    CWinThread* pMonitorThread = AfxBeginThread(MonitorThreadCallback, this, THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);

    if (pMonitorThread)
    {
        pMonitorThread->m_bAutoDelete = FALSE;
        m_monitorThread = pMonitorThread;
        pMonitorThread->ResumeThread();
    }



    //获取启动时的时间

    GetLocalTime(&m_start_time);



    SetTimer(TASKBAR_TIMER, 100, NULL);



    return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE

}





//当用户拖动最小化窗口时系统调用此函数取得光标

//显示。

//计算指定秒数的时间内Monitor定时器会触发的次数

static int GetMonitorTimerCount(int second, const MonitorSettings& settings)

{

    int count = second * 1000 / settings.monitor_time_span;

    if (count <= 0) count = 1;

    return count;

}





void CPulseBarDlg::DoMonitorAcquisition()

{
    //Pin one version before taking any other lock or calling external code.
    const auto settings = theApp.GetMonitorSettingsSnapshot();
    //Fallback names are round-local; saving settings preserves the UI's explicit choice.
    std::wstring hard_disk_name = settings->hard_disk_name;

    CSingleLock connection_lock(&m_connection_lock, TRUE);
    //获取网络连接速度；表和连接列表必须作为同一快照访问。

    unsigned __int64 cur_in_speed{}, cur_out_speed{};
    std::map<unsigned long long, NetworkCounterSample> current_samples;
    if (m_connection_change_flag)
        m_network_baselines.clear();

    bool interface_table_valid = false;
    bool table_resized = false;
    {
        interface_table_valid = UpdateIfTable(&table_resized);
        if (interface_table_valid && table_resized)
        {
            RequestConnectionRefresh();
            interface_table_valid = false;
        }
        if (!interface_table_valid)
            RequestConnectionRefresh();
        m_in_bytes = 0;
        m_out_bytes = 0;

        if (interface_table_valid)
        {
            const auto acquire = [&](int connection_index)
            {
                if (connection_index < 0 || connection_index >= static_cast<int>(m_connections.size()))
                    return;
                auto& connection = m_connections[connection_index];
                NetworkCounterSample sample;
                if (!CAdapterCommon::GetInterfaceCounters(connection.interface_index, connection.interface_luid, sample))
                {
                    RequestConnectionRefresh();
                    return; //No sample: its old baseline is dropped below.
                }
                if (connection.interface_luid == 0)
                    connection.interface_luid = sample.interface_luid;
                if (!current_samples.emplace(sample.interface_luid, sample).second)
                    return; //Filtered aliases must not count the same interface twice.
                m_in_bytes += sample.in_bytes;
                m_out_bytes += sample.out_bytes;
                const auto previous = m_network_baselines.find(sample.interface_luid);
                if (previous != m_network_baselines.end())
                {
                    //Reset each direction independently before summing interfaces.
                    if (sample.in_bytes >= previous->second.in_bytes)
                        cur_in_speed += sample.in_bytes - previous->second.in_bytes;
                    if (sample.out_bytes >= previous->second.out_bytes)
                        cur_out_speed += sample.out_bytes - previous->second.out_bytes;
                }
            };
            if (!theApp.m_cfg_data.m_select_all)
                acquire(m_connection_selected);
            else
                for (size_t i = 0; i < m_connections.size(); ++i)
                    acquire(static_cast<int>(i));
        }
    }
    //Only consecutive valid observations share a baseline. Disappearing/failed
    //interfaces start afresh on recovery; successful peers retain their deltas.
    m_network_baselines.swap(current_samples);

    ////如果大于1GB/s，说明可能产生了异常，网速无效

    //if (cur_in_speed > 1073741824)

    //  cur_in_speed = 0;

    //if (cur_out_speed > 1073741824)

    //  cur_out_speed = 0;



    //计算两次获取网速的时间间隔

    static ULONGLONG last_net_speed_time = 0;

    ULONGLONG net_speed_time = ::GetTickCount64();

    int time_span = settings->monitor_time_span;

    if (last_net_speed_time != 0)

        time_span = static_cast<int>(min(max(net_speed_time - last_net_speed_time, 1ULL), 60000ULL));

    last_net_speed_time = net_speed_time;



    //将当前监控时间间隔的流量转换成每秒时间间隔内的流量

    theApp.m_in_speed = static_cast<unsigned __int64>(cur_in_speed * 1000 / time_span);

    theApp.m_out_speed = static_cast<unsigned __int64>(cur_out_speed * 1000 / time_span);



    m_connection_change_flag = false;    //清除连接发生变化的标志



    //处于自动选择状态时，如果连续30秒没有网速，则可能自动选择的网络不对，此时执行一次自动选择

    if (theApp.m_cfg_data.m_auto_select)

    {

        if (cur_in_speed == 0 && cur_out_speed == 0)

            m_zero_speed_cnt++;

        else

            m_zero_speed_cnt = 0;

        if (m_zero_speed_cnt >= GetMonitorTimerCount(30, *settings))

        {

            PostMessage(WM_COMMAND, ID_SELETE_CONNECTION);

            m_zero_speed_cnt = 0;

        }

    }



    connection_lock.Unlock();
    CSingleLock history_lock(&m_history_lock, TRUE);

    //检测当前日期是否改变，如果已改变，就向历史流量列表插入一个新的日期

    SYSTEMTIME current_time;

    GetLocalTime(&current_time);

    static int last_check_day = -1;  //用于检测日期变化，重置保存状态

    if (m_history_traffic.GetTodayTraffic().day != current_time.wDay)

    {

        m_history_traffic.OnDateChanged();

        theApp.m_today_up_traffic = 0;

        theApp.m_today_down_traffic = 0;

        last_check_day = -1;  //重置日期标记，下次检查时会重新初始化保存状态

    }



    //统计今天已使用的流量

    theApp.m_today_up_traffic += cur_out_speed;

    theApp.m_today_down_traffic += cur_in_speed;

    m_history_traffic.SetTodayTrafficKBytes(theApp.m_today_up_traffic / 1024u,
        theApp.m_today_down_traffic / 1024u);

    //每隔30秒保存一次流量历史记录

    if (m_monitor_time_cnt % GetMonitorTimerCount(30, *settings) == GetMonitorTimerCount(30, *settings) - 1)

    {

        static unsigned __int64 last_today_kbytes = 0;

        static bool last_today_kbytes_initialized = false;

        unsigned __int64 current_kbytes = m_history_traffic.GetTodayTraffic().kBytes();



        //如果日期改变了，重置初始化状态

        if (last_check_day != current_time.wDay)

        {

            last_today_kbytes_initialized = false;

            last_check_day = current_time.wDay;

        }



        //首次检查时初始化，不保存

        if (!last_today_kbytes_initialized)

        {

            last_today_kbytes = current_kbytes;

            last_today_kbytes_initialized = true;

        }

        else

        {

            //只有当30秒内流量变化超过10MB时才保存历史流量记录，防止磁盘写入过于频繁

            unsigned __int64 change_kbytes = current_kbytes - last_today_kbytes;

            if (change_kbytes >= 10240u) // 10MB = 10240KB

            {

                if (SaveHistoryTraffic())
                    last_today_kbytes = current_kbytes;

            }

        }

    }






    history_lock.Unlock();

    if (m_monitor_time_cnt % GetMonitorTimerCount(3, *settings) == GetMonitorTimerCount(3, *settings) - 1)

    {
        CSingleLock connection_state_lock(&m_connection_lock, TRUE);

        //重新获取当前连接数量

        static DWORD last_interface_num = -1;

        DWORD interface_num{};

        const DWORD interface_status = GetNumberOfInterfaces(&interface_num);

        if (interface_status == NO_ERROR && last_interface_num == static_cast<DWORD>(-1))
            last_interface_num = interface_num;

        if (interface_status == NO_ERROR && interface_num != last_interface_num)    //如果连接数发生变化，则重新初始化连接

        {

            if (theApp.m_debug_log)

            {

                CString info = CCommon::LoadText(IDS_CONNECTION_NUM_CHANGED);

                info.Replace(_T("<%before%>"), CCommon::IntToString(last_interface_num));

                info.Replace(_T("<%after%>"), CCommon::IntToString(interface_num));

                info.Replace(_T("<%cnt%>"), CCommon::IntToString(m_restart_cnt + 1));

                CCommon::WriteLog(info, theApp.m_log_path.c_str());

            }

            RequestConnectionRefresh();

            last_interface_num = interface_num;

        }



        string descr;
        descr = (const char*)GetConnectIfTable(m_connection_selected).bDescr;
        const string selected_connection_name = theApp.m_cfg_data.m_connection_name;

        if (descr != selected_connection_name)

        {

            //写入额外的调试信息

            if (theApp.m_debug_log)

            {

                CString log_str;

                log_str = _T("连接名称不匹配：\r\n");

                log_str += _T("IfTable description: ");

                log_str += descr.c_str();

                log_str += _T("\r\nm_connection_name: ");

                log_str += theApp.m_cfg_data.m_connection_name.c_str();

                CCommon::WriteLog(log_str, (theApp.m_config_dir + L".\\connections.log").c_str());

            }



            RequestConnectionRefresh();

            CString info = CCommon::LoadText(IDS_CONNECTION_NOT_MATCH);

            info.Replace(_T("<%cnt%>"), CCommon::IntToString(m_restart_cnt));

            CCommon::WriteLog(info, theApp.m_log_path.c_str());

        }

    }



    bool lite_version = false;

#ifdef WITHOUT_TEMPERATURE

    lite_version = true;

#endif



    bool cpu_freq_acquired = false;

    bool gpu_usage_acquired = false;

    m_get_disk_usage_by_pdh = false;



    //获取CPU使用率

    theApp.m_cpu_usage = m_cpu_usage_helper.GetCpuUsage(settings->cpu_usage_acquire_method == GeneralSettingData::CA_CPU_TIME);



    //获取CPU频率

    //if (lite_version || is_arm64ec || !settings->IsHardwareEnable(HI_CPU))

    //{

    if (m_cpu_freq_helper.GetCpuFreq(theApp.m_cpu_freq))

        cpu_freq_acquired = true;

    //}



    //获取GPU利用率

    //S1: NVML 状态一次取回(利用率/温度/功耗);功耗在函数末尾落值,温度在 OHM 之后兜底

    int nvml_util_cache = -1, nvml_temp_cache = -1, nvml_power_cache = -1;

    bool nvml_ok = m_gpu_mem_helper.GetNvidiaGpuStatus(nvml_util_cache, nvml_temp_cache, nvml_power_cache);

    if (lite_version /*|| is_arm64ec*/ || !settings->IsHardwareEnable(HI_GPU))

    {

        if (m_gpu_usage_helper.GetGpuUsage(theApp.m_gpu_usage))

            gpu_usage_acquired = true;

        else

            theApp.m_gpu_usage = -1;

        //本机 PDH 计数器名称表损坏时 GPU 利用率取不到值,回退 NVML(NVIDIA)

        if (theApp.m_gpu_usage < 0)

            theApp.m_gpu_usage = nvml_ok ? nvml_util_cache : -1;

    }



    //获取专用GPU内存(任务管理器"专用 GPU 内存"口径)

    unsigned long long gpu_mem_used_b = 0, gpu_mem_total_b = 0;

    if (m_gpu_mem_helper.GetDedicatedGpuMemory(theApp.m_dedicated_gpu_memory_pct, gpu_mem_used_b, gpu_mem_total_b)

        && theApp.m_dedicated_gpu_memory_pct >= 0)

    {

        theApp.m_dedicated_gpu_memory_used_mb = gpu_mem_used_b >> 20;

        theApp.m_dedicated_gpu_memory_total_mb = gpu_mem_total_b >> 20;

    }

    else

    {

        theApp.m_dedicated_gpu_memory_pct = -1;

    }



    //获取硬盘利用率

    if (lite_version /*|| is_arm64ec*/ || !settings->IsHardwareEnable(HI_HDD))

    {

        int disk_index = m_disk_usage_helper.FindDiskIndex(hard_disk_name);

        //没有找到要监控的硬盘时默认使用总体利用率

        if (disk_index < 0)

        {

            disk_index = m_disk_usage_helper.FindDiskIndex(L"_Total");

            if (disk_index >= 0)

            {

                hard_disk_name = L"_Total";

            }

            //仍然没有找到使用第1块硬盘

            else

            {

                const auto& disk_names = m_disk_usage_helper.GetDiskNames();

                if (!disk_names.empty())

                {

                    disk_index = 0;

                    hard_disk_name = disk_names.front();

                }

            }

        }

        if (m_disk_usage_helper.GetDiskUsage(disk_index, theApp.m_hdd_usage))

            m_get_disk_usage_by_pdh = true;

        else

            theApp.m_hdd_usage = -1;

    }

    //S2: 磁盘读写速度(IOCTL_DISK_PERFORMANCE 差分,不依赖 PDH)。首次调用仅建基线

    unsigned long long disk_read_bps = 0, disk_write_bps = 0;

    if (m_disk_speed_helper.GetDiskSpeed(disk_read_bps, disk_write_bps))

    {

        theApp.m_disk_speed_available = true;

        theApp.m_disk_read_speed = disk_read_bps;

        theApp.m_disk_write_speed = disk_write_bps;

    }

    else

    {

        theApp.m_disk_speed_available = false;

    }

    //获取内存利用率

    MEMORYSTATUSEX statex{};

    statex.dwLength = sizeof(statex);

    if (!GlobalMemoryStatusEx(&statex))
        statex = {};

    theApp.m_memory_usage = statex.dwMemoryLoad;

    theApp.m_used_memory = static_cast<int>((statex.ullTotalPhys - statex.ullAvailPhys) / 1024);

    theApp.m_total_memory = static_cast<int>(statex.ullTotalPhys / 1024);



#ifndef WITHOUT_TEMPERATURE

    //获取温度

    if (IsTemperatureNeeded(*settings) && theApp.IsHardwareMonitorReady())

    {

        CSingleLock sync(&theApp.m_minitor_lib_critical, TRUE);

        //锁外只读原子状态（避免冷启动时卡在初始化线程的锁上），
        //指针判空仍必须在锁内：UI 线程关闭硬件监控时会持锁 reset() 它，
        //锁外判空与持锁使用之间留有窗口，会把已释放的指针交回给这里。
        if (theApp.m_pMonitor != nullptr)

        {
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

            {

                CCommon::WriteLog(monitor_error_message.c_str(), theApp.m_log_path.c_str());

            }

            //theApp.m_cpu_temperature = theApp.m_pMonitor->CpuTemperature();

            theApp.m_gpu_temperature = theApp.m_pMonitor->GpuTemperature();

            //theApp.m_hdd_temperature = theApp.m_pMonitor->HDDTemperature();

            theApp.m_main_board_temperature = theApp.m_pMonitor->MainboardTemperature();

            if (!gpu_usage_acquired)

                theApp.m_gpu_usage = theApp.m_pMonitor->GpuUsage();

            if (!cpu_freq_acquired)

                theApp.m_cpu_freq = theApp.m_pMonitor->CpuFreq();

            //获取CPU温度

            if (!theApp.m_pMonitor->AllCpuTemperature().empty())

            {

                if (settings->cpu_core_name == CCommon::LoadText(IDS_AVREAGE_TEMPERATURE).GetString())  //如果选择了平均温度

                {

                    theApp.m_cpu_temperature = theApp.m_pMonitor->CpuTemperature();

                }

                else

                {

                    auto iter = theApp.m_pMonitor->AllCpuTemperature().find(settings->cpu_core_name);

                    if (iter == theApp.m_pMonitor->AllCpuTemperature().end())

                    {

                        iter = theApp.m_pMonitor->AllCpuTemperature().begin();

                        //Use this fallback value only; keep the user's selected core unchanged.

                    }

                    theApp.m_cpu_temperature = iter->second;

                }

            }

            else

            {

                theApp.m_cpu_temperature = -1;

            }

            //获取硬盘温度

            if (!theApp.m_pMonitor->AllHDDTemperature().empty())

            {

                auto iter = theApp.m_pMonitor->AllHDDTemperature().find(hard_disk_name);

                if (iter == theApp.m_pMonitor->AllHDDTemperature().end())

                {

                    iter = theApp.m_pMonitor->AllHDDTemperature().begin();

                    hard_disk_name = iter->first;

                }

                theApp.m_hdd_temperature = iter->second;

            }

            else

            {

                theApp.m_hdd_temperature = -1;

            }

            //获取硬盘利用率

            if (!m_get_disk_usage_by_pdh)

            {

                if (!theApp.m_pMonitor->AllHDDUsage().empty())

                {

                    auto iter = theApp.m_pMonitor->AllHDDUsage().find(hard_disk_name);

                    if (iter == theApp.m_pMonitor->AllHDDUsage().end())

                    {

                        iter = theApp.m_pMonitor->AllHDDUsage().begin();

                        hard_disk_name = iter->first;

                    }

                    theApp.m_hdd_usage = iter->second;

                }

                else

                {

                    theApp.m_hdd_usage = -1;

                }

            }

            //顺手刷新温度项名称快照（仍持有 m_minitor_lib_critical），
            //设置页读快照即可，不必等采样锁；集合没变时内部直接返回，几乎无开销
            theApp.RefreshTemperatureNameSnapshotLocked();

        }

    }

#endif



    if (m_close_pending)
        return;

    //通知插件获取数据，以及向插件传递监控数据

    for (const auto& plugin_info : theApp.m_plugins.GetPlugins())

    {

        if (plugin_info.plugin != nullptr)

        {

            try
            {
                plugin_info.plugin->DataRequired();
            }
            catch (...)
            {
                LogPluginCallbackFailure(L"ITMPlugin::DataRequired");
            }

            ITMPlugin::MonitorInfo monitor_info;

            monitor_info.up_speed = theApp.m_out_speed;

            monitor_info.down_speed = theApp.m_in_speed;

            monitor_info.cpu_usage = theApp.m_cpu_usage;

            monitor_info.memory_usage = theApp.m_memory_usage;

            monitor_info.gpu_usage = theApp.m_gpu_usage;

            monitor_info.hdd_usage = theApp.m_hdd_usage;

            monitor_info.cpu_temperature = theApp.m_cpu_temperature;

            monitor_info.gpu_temperature = theApp.m_gpu_temperature;

            monitor_info.hdd_temperature = theApp.m_hdd_temperature;

            monitor_info.cpu_freq = theApp.m_cpu_freq;

            monitor_info.main_board_temperature = theApp.m_main_board_temperature;

            try
            {
                plugin_info.plugin->OnMonitorInfo(monitor_info);
            }
            catch (...)
            {
                LogPluginCallbackFailure(L"ITMPlugin::OnMonitorInfo");
            }

        }

    }



    //S1: NVML GPU 功耗恒由 NVML 落值;GPU 温度在 OHM 之后兜底(OHM 无传感器时会用 -1 覆盖);
    //NVML 作为 GPU 利用率的最后回退(OHM 回退之后)

    theApp.m_gpu_power_w = nvml_ok ? nvml_power_cache : -1;

    if (nvml_ok && theApp.m_gpu_usage < 0)

        theApp.m_gpu_usage = nvml_util_cache;

    if (nvml_ok && nvml_temp_cache > 0 && theApp.m_gpu_temperature <= 0)

        theApp.m_gpu_temperature = static_cast<float>(nvml_temp_cache);

    m_monitor_time_cnt++;



    //异步通知 UI，避免采样线程依赖 UI 消息泵完成同步调用。

    PostMessage(WM_MONITOR_INFO_UPDATED);

}



UINT CPulseBarDlg::MonitorThreadCallback(LPVOID dwUser)

{

    CPulseBarDlg* pThis = (CPulseBarDlg*)dwUser;

    HANDLE waitHandles[2] = { pThis->m_threadExitEvent.m_hObject, pThis->m_monitorWakeEvent.m_hObject };

    while (true)

    {

        //事件驱动:等待"退出"或"新采样请求"信号,不再 Sleep(10) 轮询

        DWORD result = ::WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);

        if (result == WAIT_OBJECT_0)        //退出事件

        {

            return 0;

        }

        if (result == WAIT_OBJECT_0 + 1)    //采样请求事件

        {

            //获取一次监控数据

            pThis->DoMonitorAcquisition();

        }

    }



    return 0;

}





void CPulseBarDlg::ExitMonitorThread()

{

    m_threadExitEvent.SetEvent();

    if (m_monitorThread != nullptr)
    {
        CWinThread* monitor_thread = m_monitorThread;
        ::WaitForSingleObject(monitor_thread->m_hThread, INFINITE);
        m_monitorThread = nullptr;
        delete monitor_thread;
    }

}





void CPulseBarDlg::OnTimer(UINT_PTR nIDEvent)

{

    // TODO: 在此添加消息处理程序代码和/或调用默认值

    if (nIDEvent == MONITOR_EXIT_TIMER)
    {
        if (m_monitorThread == nullptr || ::WaitForSingleObject(m_monitorThread->m_hThread, 0) == WAIT_OBJECT_0)
        {
            KillTimer(MONITOR_EXIT_TIMER);
            PostMessage(WM_CLOSE);
        }
        return;
    }

    if (m_close_pending)
        return;

    if (nIDEvent == INIT_CONNECT_TIMER)
    {
        IniConnection();
        ++m_resume_check_times;
        CString info = CCommon::LoadTextFormat(IDS_RESTORE_FROM_SLEEP_LOG, { m_restart_cnt });
        CCommon::WriteLog(info, theApp.m_log_path.c_str());
        CSingleLock connection_lock(&m_connection_lock, TRUE);
        if (!m_connections.empty() || m_resume_check_times >= 20)
        {
            KillTimer(INIT_CONNECT_TIMER);
            m_resume_check_times = 0;
        }
        return;
    }

    if (nIDEvent == MONITOR_TIMER)

    {

        //通知线程获取监控数据(事件驱动唤醒)

        m_monitorWakeEvent.SetEvent();

    }



    if (nIDEvent == MAIN_TIMER)

    {

        m_timer_cnt++;

        if (m_first_start)      //这个if语句在程序启动后1秒执行

        {

            //打开任务栏窗口

            if (theApp.m_cfg_data.m_show_task_bar_wnd && m_tBarDlg == nullptr)

                OpenTaskBarWnd();



            m_first_start = false;

        }



        if (m_timer_cnt % 2 == 1)       //每隔2秒钟获取一次屏幕尺寸（任务栏自动背景色需要）

        {

            GetScreenSize();

        }



        //每10秒检测一次任务栏嵌入是否失败(仅在任务栏窗口启用且存在时)
        if (theApp.m_cfg_data.m_show_task_bar_wnd && !theApp.m_win_version.IsWine() && IsTaskbarWndValid() && m_timer_cnt % 10 == 1)

        {

            if (m_tBarDlg->GetCannotInsertToTaskBar() && m_insert_to_taskbar_cnt < MAX_INSERT_TO_TASKBAR_CNT)

            {

                CloseTaskBarWnd();

                OpenTaskBarWnd();

                m_insert_to_taskbar_cnt++;

                if (m_tBarDlg->GetCannotInsertToTaskBar() && m_insert_to_taskbar_cnt >= WARN_INSERT_TO_TASKBAR_CNT)

                {

                    //写入错误日志

                    CString info = CCommon::LoadText(IDS_CONNOT_INSERT_TO_TASKBAR_ERROR_LOG);

                    info.Replace(_T("<%cnt%>"), CCommon::IntToString(m_insert_to_taskbar_cnt));

                    info.Replace(_T("<%error_code%>"), CCommon::IntToString(m_tBarDlg->GetErrorCode()));

                    CCommon::WriteLog(info, theApp.m_log_path.c_str());

                    if (m_cannot_insert_to_task_bar_warning)      //确保提示信息只弹出一次

                    {

                        //弹出错误信息

                        m_cannot_insert_to_task_bar_warning = false;

                        MessageBox(CCommon::LoadText(IDS_CONNOT_INSERT_TO_TASKBAR, CCommon::IntToString(m_tBarDlg->GetErrorCode())), NULL, MB_ICONWARNING);

                    }

                }

            }

            if (!m_tBarDlg->GetCannotInsertToTaskBar())

            {

                m_insert_to_taskbar_cnt = 0;

            }

        }

        //通知不依赖任务栏窗口是否启用，关闭任务栏后仍检查阈值。
        auto checkNotifyTip = [&](GeneralSettingData::NotifyTipSettings setting_data, int value, int& last_value, int& notify_time, LPCTSTR tip_str)
        {
            if (setting_data.enable)
            {
                if (last_value < setting_data.tip_value && value >= setting_data.tip_value && (m_timer_cnt - notify_time > static_cast<unsigned int>(theApp.m_notify_interval)))
                {
                    ShowNotifyTip(CCommon::LoadText(APP_NAME _T(" "), IDS_NOTIFY, _T("")), tip_str);
                    notify_time = m_timer_cnt;
                }
                last_value = value;
            }
        };

        //内存使用率及硬件温度超出提示，保留跨阈值触发与通知间隔。
        CString info;
        info.Format(CCommon::LoadText(IDS_MEMORY_UDAGE_EXCEED, _T(" %d%%!")), theApp.m_memory_usage);
        static int last_memory_usage;
        static int memory_usage_notify_time{ -theApp.m_notify_interval };
        checkNotifyTip(theApp.m_general_data.memory_usage_tip, theApp.m_memory_usage, last_memory_usage, memory_usage_notify_time, info.GetString());

        info.Format(CCommon::LoadText(IDS_CPU_TEMPERATURE_EXCEED, _T(" %d°C!")), static_cast<int>(theApp.m_cpu_temperature));
        static int last_cpu_temp;
        static int cpu_temp_notify_time{ -theApp.m_notify_interval };
        checkNotifyTip(theApp.m_general_data.cpu_temp_tip, theApp.m_cpu_temperature, last_cpu_temp, cpu_temp_notify_time, info.GetString());

        info.Format(CCommon::LoadText(IDS_GPU_TEMPERATURE_EXCEED, _T(" %d°C!")), static_cast<int>(theApp.m_gpu_temperature));
        static int last_gpu_temp;
        static int gpu_temp_notify_time{ -theApp.m_notify_interval };
        checkNotifyTip(theApp.m_general_data.gpu_temp_tip, theApp.m_gpu_temperature, last_gpu_temp, gpu_temp_notify_time, info.GetString());

        info.Format(CCommon::LoadText(IDS_HDD_TEMPERATURE_EXCEED, _T(" %d°C!")), static_cast<int>(theApp.m_hdd_temperature));
        static int last_hdd_temp;
        static int hdd_temp_notify_time{ -theApp.m_notify_interval };
        checkNotifyTip(theApp.m_general_data.hdd_temp_tip, theApp.m_hdd_temperature, last_hdd_temp, hdd_temp_notify_time, info.GetString());

        info.Format(CCommon::LoadText(IDS_MBD_TEMPERATURE_EXCEED, _T(" %d°C!")), static_cast<int>(theApp.m_main_board_temperature));
        static int last_main_board_temp;
        static int main_board_temp_notify_time{ -theApp.m_notify_interval };
        checkNotifyTip(theApp.m_general_data.mainboard_temp_tip, theApp.m_main_board_temperature, last_main_board_temp, main_board_temp_notify_time, info.GetString());

        //今日流量跨过用户设置的 MB/GB 阈值时通知。
        if (theApp.m_general_data.traffic_tip_enable)
        {
            static __int64 last_today_traffic;
            __int64 traffic_tip_value;
            if (theApp.m_general_data.traffic_tip_unit == 0)
                traffic_tip_value = static_cast<__int64>(theApp.m_general_data.traffic_tip_value) * 1024 * 1024;
            else
                traffic_tip_value = static_cast<__int64>(theApp.m_general_data.traffic_tip_value) * 1024 * 1024 * 1024;

            __int64 today_traffic = theApp.m_today_up_traffic + theApp.m_today_down_traffic;
            if (last_today_traffic < traffic_tip_value && today_traffic >= traffic_tip_value)
            {
                CString info = CCommon::LoadText(IDS_TODAY_TRAFFIC_EXCEED, CCommon::DataSizeToString(today_traffic));
                ShowNotifyTip(CCommon::LoadText(APP_NAME _T(" "), IDS_NOTIFY, _T("")), info.GetString());
            }
            last_today_traffic = today_traffic;
        }

        //系统主题检测沿用 WindowsSettingHelper 的缓存，不影响监控采样。
        CWindowsSettingHelper::CheckWindows10LightTheme();
        bool light_mode = CWindowsSettingHelper::IsWindows10LightTheme();
        if (theApp.m_last_light_mode != light_mode)
        {
            theApp.m_last_light_mode = light_mode;
            bool refresh_taskbar_dlg{ false };
            if (theApp.m_taskbar_data.auto_adapt_light_theme)
            {
                int style_index = light_mode ? theApp.m_taskbar_data.light_default_style : theApp.m_taskbar_data.dark_default_style;
                theApp.m_taskbar_default_style.ApplyDefaultStyle(style_index, theApp.m_taskbar_data);
                theApp.SaveConfig();
                refresh_taskbar_dlg = true;
            }
            bool is_taskbar_transparent{ theApp.m_taskbar_data.IsTaskbarTransparent() };
            if (!is_taskbar_transparent)
            {
                theApp.m_taskbar_data.SetTaskabrTransparent(false);
                refresh_taskbar_dlg = true;
            }
            if (refresh_taskbar_dlg && IsTaskbarWndValid())
            {
                m_tBarDlg->ApplyWindowTransparentColor();
                //颜色不参与数据重绘签名，主题切换时显式刷新。
                m_tBarDlg->Invalidate(FALSE);
            }

            //通知区图标“自动”样式继续随系统深浅色切换（显示时按需解析，无需改写配置）。
            if (theApp.m_cfg_data.m_notify_icon_selected == NOTIFY_ICON_AUTO)
                UpdateNotifyIconTip(true);
        }

        //沿用任务栏边缘取色逻辑；任务栏所在显示器全屏时不读取桌面像素。
        if (theApp.m_taskbar_data.auto_set_background_color && theApp.m_win_version.IsWindows8OrLater()
            && IsTaskbarWndValid() && theApp.m_taskbar_data.transparent_color != 0
            && theApp.m_taskbar_data.disable_d2d
            && !CCommon::IsForegroundFullscreen(::MonitorFromWindow(m_tBarDlg->GetSafeHwnd(), MONITOR_DEFAULTTONEAREST)))
        {
            CRect rect;
            ::GetWindowRect(m_tBarDlg->GetSafeHwnd(), rect);
            int pointx{ rect.left - 1 };
            if (theApp.m_taskbar_data.tbar_wnd_on_left && m_tBarDlg->IsTasksbarOnTopOrBottom())
                pointx = rect.right + 1;
            int pointy = rect.bottom;
            if (pointx < 0) pointx = 0;
            if (pointx >= m_screen_size.cx) pointx = m_screen_size.cx - 1;
            if (pointy < 0) pointy = 0;
            if (pointy >= m_screen_size.cy) pointy = m_screen_size.cy - 1;
            COLORREF color = ::GetPixel(m_desktop_dc, pointx, pointy);
            if (!CCommon::IsColorSimilar(color, theApp.m_taskbar_data.back_color) && color != 0)
            {
                bool is_taskbar_transparent{ theApp.m_taskbar_data.IsTaskbarTransparent() };
                theApp.m_taskbar_data.back_color = color;
                theApp.m_taskbar_data.SetTaskabrTransparent(is_taskbar_transparent);
                if (is_taskbar_transparent)
                    m_tBarDlg->ApplyWindowTransparentColor();
                m_tBarDlg->Invalidate(FALSE);
            }
        }

        UpdateNotifyIconTip();





    }



    if (nIDEvent == DELAY_TIMER)

    {

        AutoSelect();

        KillTimer(DELAY_TIMER);

    }



    if (nIDEvent == TASKBAR_TIMER)

    {

        ++m_taskbar_timer_cnt;

        if (m_taskbar_timer_cnt % 5 == 0 && theApp.m_cfg_data.m_show_task_bar_wnd && theApp.m_taskbar_data.show_taskbar_wnd_in_secondary_display)

        {

        static int last_taskbar_num = 0;

        int taskbar_num = CTaskbarHelper::GetSecondaryTaskbarNum();

        //如果副显示器的任务栏数量发生变化，则重启任务栏窗口

        if (last_taskbar_num != taskbar_num)

        {

            last_taskbar_num = taskbar_num;

            //延迟一段时间后重启任务栏窗口

            KillTimer(RESTART_TASKBAR_TIMER);

            SetTimer(RESTART_TASKBAR_TIMER, 500, [](HWND, UINT, UINT_PTR, DWORD) {

                theApp.m_pMainWnd->SendMessage(WM_REOPEN_TASKBAR_WND);

                ::KillTimer(theApp.m_pMainWnd->GetSafeHwnd(), RESTART_TASKBAR_TIMER);

            });

        }

        }



        if (IsTaskbarWndValid())

        {

            //启动时就隐藏主窗体的情况下，无法收到dpichange消息，故需要手动检查

            //每次100ms*10执行一次屏幕DPI检查，并且尽可能少的检查操作系统版本

            if (m_taskbar_timer_cnt % 10 == 0 && theApp.m_win_version.IsWindows8Point1OrLater())

            {

                CTaskBarDlg::CheckWindowMonitorDPIAndHandle(*m_tBarDlg, [p_TaskBarDlg = m_tBarDlg](UINT new_dpi_x, UINT new_dpi_y)

                                                            {

                                                                // auto s_dpi = std::to_string(new_dpi_x);

                                                                // s_dpi += '\n';

                                                                // TRACE(s_dpi.c_str());

                                                                //考虑到任务栏窗口可能和主窗口不在同一个屏幕上，dpi可能不同

                                                                //设置DPI并刷新窗口

                                                                p_TaskBarDlg->SetDPI(new_dpi_x);

                                                                p_TaskBarDlg->SetTextFont();

                                                                p_TaskBarDlg->CalculateWindowSize(); });

            }



            m_tBarDlg->AdjustWindowPos();

            //优化:数据变化才重绘,替代每100ms无条件Invalidate

            m_tBarDlg->InvalidateIfNeeded();

        }

    }



    if (nIDEvent == DELETE_NOTIFY_ICON_TIMER)
    {
        KillTimer(DELETE_NOTIFY_ICON_TIMER);
        if (!theApp.m_general_data.show_notify_icon)
            DeleteNotifyIcon();
    }



    CDialog::OnTimer(nIDEvent);

}





void CPulseBarDlg::OnNetworkInfo()

{

    // TODO: 在此添加命令处理程序代码

    //弹出“连接详情”对话框

    vector<NetWorkConection> connections;
    vector<MIB_IFROW> if_rows;
    int connection_selected{};
    {
        CSingleLock connection_lock(&m_connection_lock, TRUE);
        connections = m_connections;
        connection_selected = m_connection_selected;
        if (m_pIfTable != nullptr && m_pIfTable->dwNumEntries > 0)
            if_rows.assign(m_pIfTable->table, m_pIfTable->table + m_pIfTable->dwNumEntries);
    }
    CNetworkInfoDlg aDlg(connections, if_rows, connection_selected);

    ////向CNetworkInfoDlg类传递自启动以来已发送和接收的字节数

    //aDlg.m_in_bytes = m_pIfTable->table[m_connections[m_connection_selected].index].dwInOctets - m_connections[m_connection_selected].in_bytes;

    //aDlg.m_out_bytes = m_pIfTable->table[m_connections[m_connection_selected].index].dwOutOctets - m_connections[m_connection_selected].out_bytes;

    aDlg.m_start_time = m_start_time;

    aDlg.DoModal();

    //SetAlwaysOnTop(); //由于在“连接详情”对话框内设置了取消窗口置顶，所有在对话框关闭后，重新设置窗口置顶

    if (m_tBarDlg != nullptr)

        m_tBarDlg->m_tool_tips.SetWindowPos(&wndTopMost, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);  //重新设置任务栏窗口的提示信息置顶

}





void CPulseBarDlg::OnClose()

{

    if (m_monitorThread != nullptr && ::WaitForSingleObject(m_monitorThread->m_hThread, 0) == WAIT_TIMEOUT)
    {
        if (!m_close_pending)
        {
            m_close_pending = true;
            KillTimer(MONITOR_TIMER);
            KillTimer(INIT_CONNECT_TIMER);
            KillTimer(DELAY_TIMER);
            KillTimer(MAIN_TIMER);
            KillTimer(TASKBAR_TIMER);
            m_threadExitEvent.SetEvent();
            SetTimer(MONITOR_EXIT_TIMER, 50, nullptr);
        }
        return;
    }

    m_close_pending = false;
    KillTimer(MONITOR_EXIT_TIMER);
    ExitMonitorThread();

    // TODO: 在此添加消息处理程序代码和/或调用默认值

    theApp.m_cannot_save_config_warning = true;

    theApp.m_cannot_save_global_config_warning = true;

    theApp.SaveConfig();    //退出前保存设置到ini文件

    theApp.SaveGlobalConfig();

    if (!SaveHistoryTrafficFull() || !BackupHistoryTrafficFile())
        CCommon::WriteLog(L"History save/backup failed; previous committed data was preserved.", theApp.m_log_path.c_str());



    //系统关闭必须走统一释放路径，不能留下指向已销毁 HWND 的 m_tBarDlg。
    CloseTaskBarWnd();



    //确保在退出前关闭所有窗口

    const auto dialogs = CBaseDialog::AllUniqueHandels();
    for (const auto& item : dialogs)

    {

        ::SendMessage(item.second, WM_COMMAND, IDCANCEL, 0);

    }



    CDialog::OnClose();

}

LRESULT CPulseBarDlg::OnAlreadyRunning(WPARAM, LPARAM)
{
    //重复启动：只提示，不改变任何状态
    ShowNotifyTip(APP_NAME, CCommon::LoadText(IDS_ALREADY_RUNNING_TIP));
    return 0;
}

LRESULT CPulseBarDlg::OnQuitRequest(WPARAM, LPARAM)
{
    //新版本启动时请求退出：按正常关闭流程保存配置并结束进程
    PostMessage(WM_CLOSE);
    return 0;
}

LRESULT CPulseBarDlg::OnRestartForPlugin(WPARAM, LPARAM)
{
    //安装插件后重启：先关掉所有打开的对话框，再启动新实例并让本实例正常退出。
    //延迟重启器放在独立进程里，避免本实例退出过程中新实例被单实例逻辑挡掉。
    for (const auto& item : CBaseDialog::AllUniqueHandels())
    {
        if (::IsWindow(item.second))
            ::PostMessage(item.second, WM_COMMAND, IDCANCEL, 0);
    }

    CString command;
    command.Format(L"/c timeout /t 2 >nul & start \"\" \"%s\"", theApp.m_module_path.c_str());
    ::ShellExecuteW(nullptr, L"open", L"cmd.exe", command, nullptr, SW_HIDE);

    PostMessage(WM_CLOSE);
    return 0;
}





BOOL CPulseBarDlg::OnCommand(WPARAM wParam, LPARAM lParam)

{

    // TODO: 在此添加专用代码和/或调用基类

    UINT uMsg = LOWORD(wParam);
    CSingleLock connection_lock(&m_connection_lock, TRUE);

    if (uMsg == ID_SELECT_ALL_CONNECTION)

    {

        theApp.m_cfg_data.m_select_all = true;

        theApp.m_cfg_data.m_auto_select = false;

        m_connection_change_flag = true;

    }

    //选择了“选择网络连接”子菜单中项目时的处理

    if (uMsg == ID_SELETE_CONNECTION)   //选择了“自动选择”菜单项

    {

        AutoSelect();

        theApp.m_cfg_data.m_auto_select = true;

        theApp.m_cfg_data.m_select_all = false;

        theApp.SaveConfig();

        m_connection_change_flag = true;

    }

    if (uMsg > ID_SELECT_ALL_CONNECTION && uMsg <= ID_SELECT_ALL_CONNECTION + m_connections.size()) //选择了一个网络连接

    {

        m_connection_selected = uMsg - ID_SELECT_ALL_CONNECTION - 1;

        theApp.m_cfg_data.m_connection_name = GetConnection(m_connection_selected).description_2;

        m_connection_name_preferd = theApp.m_cfg_data.m_connection_name;

        theApp.m_cfg_data.m_auto_select = false;

        theApp.m_cfg_data.m_select_all = false;

        theApp.SaveConfig();

        m_connection_change_flag = true;

    }

    connection_lock.Unlock();

#ifdef DEBUG

    if (uMsg == ID_CMD_TEST)

    {

        CTest::TestCommand();

        //ShowNotifyTip(CCommon::LoadText(APP_NAME _T(" "), IDS_NOTIFY, _T("")), _T("测试通知"));



    }

#endif // DEBUG



    return CDialog::OnCommand(wParam, lParam);

    }





BOOL CPulseBarDlg::PreTranslateMessage(MSG* pMsg)

{

    //主窗口已移除：这里只保留屏蔽回车/ESC 退出（防止隐藏宿主被意外关闭）

    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_ESCAPE) return TRUE;

    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN) return TRUE;

    return CDialog::PreTranslateMessage(pMsg);

}





afx_msg LRESULT CPulseBarDlg::OnNotifyIcon(WPARAM wParam, LPARAM lParam)
{
    if (lParam!=WM_LBUTTONDOWN && lParam!=WM_RBUTTONUP && lParam!=WM_LBUTTONDBLCLK)
        return 0;
    const HWND owner=theApp.GetDialogOwner(GetSafeHwnd());
    const bool dialog_exist=owner!=GetSafeHwnd();
    if (lParam==WM_LBUTTONDOWN || lParam==WM_LBUTTONDBLCLK)
    {
        if (dialog_exist)
            ::SetForegroundWindow(owner);
        else
            _OnOptions(0,this);
        return 0;
    }
    CSpeedGraphTip::Instance().ForceHide();
    if (IsTaskbarWndValid())
    {
        m_tBarDlg->HideSpeedGraphTip();
        m_tBarDlg->m_tool_tips.Pop();
    }
    theApp.UpdateTrayPluginMenu(); //进入菜单循环前才允许改变结构。
    OnTaskbarMenuPopedUp(0,0);
    CMenu* menu=theApp.m_taskbar_menu.GetSubMenu(0);
    if (menu==nullptr)
        return 0;
    ::SetForegroundWindow(owner);
    CPoint point;
    GetCursorPos(&point);
    menu->SetDefaultItem(-1);
    CTaskBarDlg::ApplyMenuCheckStates(menu);
    UINT command{};
    {
        CFlagLocker tracking(theApp.m_tray_plugin_menu_active);
        command=menu->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD,
            point.x,point.y,CWnd::FromHandle(owner));
    }
    if (command==0)
        return 0;
    if (command==ID_APP_EXIT && dialog_exist)
    {
        ::SendMessage(owner,WM_CLOSE,0,0);
        ::PostMessage(GetSafeHwnd(),WM_COMMAND,command,0);
    }
    else
        ::SendMessage(GetSafeHwnd(),WM_COMMAND,command,0);
    return 0;
}





void CPulseBarDlg::OnShowNotifyIcon()

{

    // TODO: 在此添加命令处理程序代码

    if (theApp.m_general_data.show_notify_icon)

    {

        DeleteNotifyIcon();

        theApp.m_general_data.show_notify_icon = false;

    }

    else

    {

        AddNotifyIcon();

        theApp.m_general_data.show_notify_icon = true;

    }

    theApp.SaveConfig();

}



//B4:切换"悬停显示网速曲线"

void CPulseBarDlg::OnShowSpeedGraphTip()

{

    theApp.m_taskbar_data.show_speed_graph_tip = !theApp.m_taskbar_data.show_speed_graph_tip;

    if (!theApp.m_taskbar_data.show_speed_graph_tip && IsTaskbarWndValid())

        m_tBarDlg->HideSpeedGraphTip();

    theApp.SaveConfig();

}



//==================== 任务栏窗口主题(经典/圆角面板/渐变色条/背景图) ====================

void CPulseBarDlg::ApplyTaskbarTheme()
{
    theApp.SaveConfig();
    if (!IsTaskbarWndValid())
        return;
    if (m_tBarDlg->NeedsWindowRecreation())
    {
        CloseTaskBarWnd();
        OpenTaskBarWnd();
    }
    else
        m_tBarDlg->ApplySettings();
    if (IsTaskbarWndValid())
    {
        m_tBarDlg->ApplyWindowTransparentColor();
        m_tBarDlg->Invalidate(FALSE);
    }
}



void CPulseBarDlg::OnTbThemeSelect(UINT theme_id)

{

    //命令 ID 转换为主题编号(0~8,-1=自定义)，否则主题表查找与 radio 勾选都会失效

    int theme = static_cast<int>(theme_id) - ID_TB_THEME_CLASSIC;

    if (theme == (ID_TB_THEME_CUSTOM - ID_TB_THEME_CLASSIC))

        theApp.m_taskbar_data.tb_theme = -1;

    else if (theme >= 0 && theme <= (ID_TB_THEME_PILL_BLACK_GOLD - ID_TB_THEME_CLASSIC))

        theApp.m_taskbar_data.tb_theme = theme;

    else

        return;

    theApp.m_taskbar_data.tb_use_background_image = false;

    ApplyTaskbarTheme();

}



void CPulseBarDlg::OnTbThemeBgImage()

{

    theApp.m_taskbar_data.tb_use_background_image = !theApp.m_taskbar_data.tb_use_background_image;

    ApplyTaskbarTheme();

}



void CPulseBarDlg::OnTbThemeSelectImage()

{

    CFileDialog dlg(TRUE, nullptr, theApp.m_taskbar_data.tb_background_image.c_str(),

        OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,

        L"图片文件|*.png;*.jpg;*.jpeg;*.bmp|所有文件|*.*||", this);

    if (dlg.DoModal() == IDOK)

    {

        theApp.m_taskbar_data.tb_background_image = dlg.GetPathName().GetString();

        theApp.m_taskbar_data.tb_use_background_image = true;

        ApplyTaskbarTheme();

    }

}



void CPulseBarDlg::OnTbThemeOpacitySub()

{

    theApp.m_taskbar_data.tb_panel_opacity = max(theApp.m_taskbar_data.tb_panel_opacity - 10, 10);

    ApplyTaskbarTheme();

}



void CPulseBarDlg::OnTbThemeOpacityAdd()

{

    theApp.m_taskbar_data.tb_panel_opacity = min(theApp.m_taskbar_data.tb_panel_opacity + 10, 100);

    ApplyTaskbarTheme();

}



//B4:切换"通知区图标显示网速"

void CPulseBarDlg::OnNotifyIconShowSpeed()
{
    theApp.m_general_data.notify_icon_show_speed = !theApp.m_general_data.notify_icon_show_speed;
    UpdateNotifyIconTip(true);
    theApp.SaveConfig();
}





void CPulseBarDlg::OnDestroy()

{

    ExitMonitorThread();

    CDialog::OnDestroy();



    //程序退出时删除通知栏图标

    ::Shell_NotifyIcon(NIM_DELETE, &m_ntIcon);




}





//任务栏窗口切换显示CPU和内存利用率时的处理

void CPulseBarDlg::OnShowCpuMemory2()

{

    // TODO: 在此添加命令处理程序代码

    if (IsTaskbarWndValid())

    {

        bool show_cpu_memory = (theApp.m_taskbar_data.display_item.Contains(TDI_CPU) || theApp.m_taskbar_data.display_item.Contains(TDI_MEMORY));

        if (show_cpu_memory)

        {

            theApp.m_taskbar_data.display_item.Remove(TDI_CPU);

            theApp.m_taskbar_data.display_item.Remove(TDI_MEMORY);

        }

        else

        {

            theApp.m_taskbar_data.display_item.Add(TDI_CPU);

            theApp.m_taskbar_data.display_item.Add(TDI_MEMORY);

        }

        //theApp.m_cfg_data.m_tbar_show_cpu_memory = !theApp.m_cfg_data.m_tbar_show_cpu_memory;

        //切换显示CPU和内存利用率时，删除任务栏窗口，再重新显示

        //CloseTaskBarWnd();

        //OpenTaskBarWnd();

        m_tBarDlg->WidthChanged();

    }

}





void CPulseBarDlg::OnShowTaskBarWnd()

{

    // TODO: 在此添加命令处理程序代码

    if (m_tBarDlg != nullptr)

    {

        CloseTaskBarWnd();

    }

    if (!theApp.m_cfg_data.m_show_task_bar_wnd)

    {

        theApp.m_cfg_data.m_show_task_bar_wnd = true;

        OpenTaskBarWnd();

    }

    else

    {

        theApp.m_cfg_data.m_show_task_bar_wnd = false;

        //关闭任务栏窗口后，如果没有显示通知区图标，且没有显示主窗口或设置了鼠标穿透，则将通知区图标显示出来

        if (!theApp.m_general_data.show_notify_icon && theApp.IsForceShowNotifyIcon())

        {

            AddNotifyIcon();

            theApp.m_general_data.show_notify_icon = true;

        }

    }

    theApp.SaveConfig();

}





void CPulseBarDlg::OnAppAbout()

{

    // TODO: 在此添加命令处理程序代码

    //弹出“关于”对话框

    CAboutDlg aDlg;

    aDlg.DoModal();

}





//当资源管理器重启时会触发此消息

LRESULT CPulseBarDlg::OnTaskBarCreated(WPARAM wParam, LPARAM lParam)
{
    const bool taskbar_open = m_tBarDlg != nullptr;
    if (taskbar_open)
        CloseTaskBarWnd();
    if (theApp.m_general_data.show_notify_icon)
    {
        m_ntIcon.hIcon = GetCurrentNotifyIcon();
        m_ntIcon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        ::Shell_NotifyIcon(NIM_DELETE, &m_ntIcon);
        ::Shell_NotifyIcon(NIM_ADD, &m_ntIcon);
    }
    if (taskbar_open)
        OpenTaskBarWnd();
    return LRESULT();
}





void CPulseBarDlg::OnTrafficHistory()

{

    // TODO: 在此添加命令处理程序代码

    deque<HistoryTraffic> history_snapshot;
    {
        CSingleLock history_lock(&m_history_lock, TRUE);
        history_snapshot = m_history_traffic.GetTraffics();
    }
    CHistoryTrafficDlg historyDlg(history_snapshot);
    historyDlg.DoModal();

}





void CPulseBarDlg::OnOptions()

{

    _OnOptions(0, this);

}





//通过任务栏窗口的右键菜单打开“选项”对话框

void CPulseBarDlg::OnOptions2()

{

    //设置窗口归属稳定宿主，应用设置时重建任务栏不会连带关闭它。
    _OnOptions(0, this);

}





afx_msg LRESULT CPulseBarDlg::OnExitmenuloop(WPARAM wParam, LPARAM lParam)

{

    return 0;

}





void CPulseBarDlg::OnChangeNotifyIcon()

{

    // TODO: 在此添加命令处理程序代码

    CIconSelectDlg dlg(theApp.m_cfg_data.m_notify_icon_selected);

    if (dlg.DoModal() == IDOK)

    {

        theApp.m_cfg_data.m_notify_icon_selected = dlg.GetIconSelected();

        UpdateNotifyIconTip(true);

        theApp.SaveConfig();

    }

}





void CPulseBarDlg::OnCheckUpdate()

{

    // TODO: 在此添加命令处理程序代码

    theApp.CheckUpdateInThread(true);

}





afx_msg LRESULT CPulseBarDlg::OnTaskbarMenuPopedUp(WPARAM wParam, LPARAM lParam)
{
    CMenu* menus[] = { &theApp.m_taskbar_menu, &theApp.m_taskbar_menu_plugin };
    for (CMenu* menu : menus)
    {
        CMenu* connections = FindMenuContainingCommand(menu, ID_SELETE_CONNECTION);
        if (connections != nullptr)
            SetConnectionMenuState(connections);
        CheckMenuItemByCommand(menu, ID_SHOW_SPEED_GRAPH_TIP,
            theApp.m_taskbar_data.show_speed_graph_tip ? MF_CHECKED : MF_UNCHECKED);
        EnableMenuItemByCommand(menu, ID_SELECT_ALL_CONNECTION,
            theApp.m_general_data.show_all_interface ? MF_GRAYED : MF_ENABLED);
        EnableMenuItemByCommand(menu, ID_CHECK_UPDATE,
            theApp.IsCheckingForUpdate() ? MF_GRAYED : MF_ENABLED);
        // 「显示任务栏面板」固定文案，状态用勾选表示（不再靠切换文字）
        CheckMenuItemByCommand(menu, ID_SHOW_TASK_BAR_WND,
            theApp.m_cfg_data.m_show_task_bar_wnd ? MF_CHECKED : MF_UNCHECKED);
    }
    return 0;
}


BOOL CPulseBarDlg::OnQueryEndSession()

{

    if (!CDialog::OnQueryEndSession())

        return FALSE;



    // TODO:  在此添加专用的查询结束会话代码

    theApp.SaveConfig();

    theApp.SaveGlobalConfig();

    CSingleLock history_lock(&m_history_lock, TRUE);
    const bool history_saved = SaveHistoryTrafficFull() && BackupHistoryTrafficFile();
    history_lock.Unlock();
    if (!history_saved)
        CCommon::WriteLog(L"History save/backup failed; previous committed data was preserved.", theApp.m_log_path.c_str());



    if (theApp.m_debug_log)

    {

        CCommon::WriteLog(_T("PulseBar进程已被终止，设置已保存。"), (theApp.m_config_dir + L".\\debug.log").c_str());

    }



    return TRUE;

}





afx_msg LRESULT CPulseBarDlg::OnDpichanged(WPARAM wParam, LPARAM lParam)
{
    //主窗口已移除：主窗口不再接收 DPI 变化。任务栏窗口的 DPI 由 TASKBAR_TIMER 里的轮询处理。
    return 0;
}

afx_msg LRESULT CPulseBarDlg::OnTaskbarWndClosed(WPARAM wParam, LPARAM lParam)

{
    if (m_tBarDlg == nullptr || wParam != m_tBarDlg->GetInstanceToken())
        return 0;


    if (m_tBarDlg != nullptr && !::IsWindow(m_tBarDlg->GetSafeHwnd()))
    {
        delete m_tBarDlg;
        m_tBarDlg = nullptr;
        theApp.m_taskbar_data.update_layered_window_error_code = 0;
    }

    theApp.m_cfg_data.m_show_task_bar_wnd = false;

    //关闭任务栏窗口后，如果没有显示通知区图标，且没有显示主窗口或设置了鼠标穿透，则将通知区图标显示出来

    if (!theApp.m_general_data.show_notify_icon && theApp.IsForceShowNotifyIcon())

    {

        AddNotifyIcon();

        theApp.m_general_data.show_notify_icon = true;

    }

    return 0;

}







afx_msg LRESULT CPulseBarDlg::OnMonitorInfoUpdated(WPARAM wParam, LPARAM lParam)
{
    //主窗口已移除：只需要刷新任务栏窗口的鼠标提示
    if (IsTaskbarWndValid())
        m_tBarDlg->UpdateToolTips();
    return 0;
}

LRESULT CPulseBarDlg::OnDisplaychange(WPARAM wParam, LPARAM lParam)
{
    //主窗口已移除：屏幕尺寸变化时只更新缓存值（任务栏窗口自己会处理位置）
    GetScreenSize();
    return 0;
}

void CPulseBarDlg::OnPluginManage()

{

    // TODO: 在此添加命令处理程序代码

    CPluginManagerDlg dlg;

    dlg.DoModal();

}



LRESULT CPulseBarDlg::OnReopenTaksbarWnd(WPARAM wParam, LPARAM lParam)

{
    if (wParam != 0 && (!IsTaskbarWndValid() || reinterpret_cast<HWND>(wParam) != m_tBarDlg->GetSafeHwnd()))
        return 0;


    CloseTaskBarWnd();

    OpenTaskBarWnd();

    return 0;

}





void CPulseBarDlg::OnOpenTaskManager()

{

    ShellExecuteW(NULL, _T("open"), (theApp.m_system_dir + L"\\Taskmgr.exe").c_str(), NULL, NULL, SW_NORMAL);       //打开任务管理器

}





afx_msg LRESULT CPulseBarDlg::OnSettingsApplied(WPARAM wParam, LPARAM lParam)

{

    COptionsDlg* pOptionsDlg = (COptionsDlg*)wParam;

    if (pOptionsDlg != nullptr)

    {

        ApplySettings(*pOptionsDlg);

    }

    return 0;

}





void CPulseBarDlg::OnDisplaySettings()
{
    CSetItemOrderDlg dlg(this);
    dlg.SetAppearance(theApp.m_taskbar_data.disp_str, theApp.m_taskbar_data.text_colors);
    dlg.SetItemOrder(theApp.m_taskbar_data.item_order.GetItemOrderConst());
    dlg.SetDisplayItem(theApp.m_taskbar_data.display_item);
    dlg.SetPluginDisplayItem(theApp.m_taskbar_data.plugin_display_item);
    if (dlg.DoModal() == IDOK)
    {
        theApp.m_taskbar_data.item_order.SetOrder(dlg.GetItemOrder());
        theApp.m_taskbar_data.display_item = dlg.GetDisplayItem();
        theApp.m_taskbar_data.plugin_display_item = dlg.GetPluginDisplayItem();
        theApp.m_taskbar_data.disp_str = dlg.GetDisplayTexts();
        theApp.m_taskbar_data.text_colors = dlg.GetColors();
        if (dlg.ColorsModified())
            theApp.m_taskbar_data.specify_each_item_color = true;
        if (IsTaskbarWndValid())
        {
            m_tBarDlg->ApplySettings();
            m_tBarDlg->WidthChanged();
        }
        theApp.SaveConfig();
    }
}


void CPulseBarDlg::RequestConnectionRefresh()
{
    if (!m_connection_refresh_pending.exchange(true))
    {
        if (!PostMessage(WM_COMMAND, ID_REFRESH_CONNECTION_LIST))
            m_connection_refresh_pending = false;
    }
}

void CPulseBarDlg::OnRefreshConnectionList()
{
    m_connection_refresh_pending = false;
    if (!m_close_pending)
        IniConnection();
}





afx_msg LRESULT CPulseBarDlg::OnTabletQuerysystemgesturestatus(WPARAM wParam, LPARAM lParam)

{

    return 0;

}





//托盘插件命令只接受当前重建产生的动态 registry 项，避免把 ID 当作插件
//vector 下标，也不读取任务栏窗口的点击残留状态。
void CPulseBarDlg::OnPluginTrayCommand(UINT nID)
{
    const auto found=theApp.m_tray_plugin_actions.find(nID);
    if (found==theApp.m_tray_plugin_actions.end())
        return;
    const TrayPluginAction action=found->second; //回调可重入菜单刷新，不能保留 map 内引用。
    ITMPlugin* plugin{};
    int plugin_index=-1;
    const auto& plugins=theApp.m_plugins.GetPlugins();
    for (size_t index=0; index<plugins.size(); ++index)
        if (plugins[index].plugin==action.plugin && plugins[index].file_path==action.file_path
            && plugins[index].state==CPluginManager::PluginState::PS_SUCCEED)
        { plugin=action.plugin; plugin_index=static_cast<int>(index); break; }
    if (plugin==nullptr)
        return;
    const HWND owner=theApp.GetDialogOwner(GetSafeHwnd());
    try
    {
        switch (action.kind)
        {
        case TrayPluginActionKind::DETAIL:
        {
            CPluginInfoDlg dialog(plugin_index,CWnd::FromHandle(owner));
            dialog.DoModal();
            return;
        }
        case TrayPluginActionKind::OPTIONS:
        {
            const auto result=plugin->ShowOptionsDialog(static_cast<void*>(owner));
            if (result==ITMPlugin::OR_OPTION_CHANGED && IsTaskbarWndValid())
                m_tBarDlg->WidthChanged();
            if (result==ITMPlugin::OR_OPTION_NOT_PROVIDED)
                MessageBox(CCommon::LoadText(IDS_PLUGIN_NO_OPTIONS_INFO),nullptr,MB_ICONINFORMATION | MB_OK);
            return;
        }
        case TrayPluginActionKind::DISABLE:
        {
            const auto name=CFilePathHelper(action.file_path).GetFileName();
            const bool disabled=theApp.m_cfg_data.plugin_disabled.Contains(name);
            theApp.m_cfg_data.plugin_disabled.SetStrContained(name,!disabled);
            MessageBox(CCommon::LoadText(IDS_RESTART_TO_APPLY_CHANGE_INFO),nullptr,MB_ICONINFORMATION | MB_OK);
            return;
        }
        case TrayPluginActionKind::PLUGIN_COMMAND:
        {
            const int index=action.plugin_command_index;
            if (plugin->GetAPIVersion()<5 || index<0 || index>=plugin->GetCommandCount())
                return;
            const wchar_t* label=plugin->GetCommandName(index);
            if (label==nullptr || action.command_label!=label)
                return; //插件在 popup 后改变了命令语义，不能运行旧索引。
            plugin->OnPluginCommand(index,static_cast<void*>(owner),nullptr);
            return;
        }
        default:
            return;
        }
    }
    catch (CException* exception) { exception->Delete(); }
    catch (...) {}
    CCommon::WriteLog("Plugin tray action failed; callback exception contained.",theApp.m_log_path.c_str());
}


void CPulseBarDlg::OnPluginOptionsTaksbar()

{

    //判断任务栏窗口中点击的项目是否是插件项目

    if (IsTaskbarWndValid() && m_tBarDlg->GetClickedItem().IsPlugin())

    {

        //找到对应的插件

        ITMPlugin* plugin = theApp.m_plugins.GetPluginByItem(m_tBarDlg->GetClickedItem().PluginItem());

        if (plugin != nullptr)

        {

            //显示插件的选项设置

            auto rtn = plugin->ShowOptionsDialog(m_tBarDlg->GetSafeHwnd());

            if (rtn == ITMPlugin::OR_OPTION_CHANGED)    //选项设置有更改，重新打开任务栏窗口

            {

                //CloseTaskBarWnd();

                //OpenTaskBarWnd();

                m_tBarDlg->WidthChanged();

            }

            if (rtn == ITMPlugin::OR_OPTION_NOT_PROVIDED)

                MessageBox(CCommon::LoadText(IDS_PLUGIN_NO_OPTIONS_INFO), nullptr, MB_ICONINFORMATION | MB_OK);

        }

    }

}





void CPulseBarDlg::OnPluginDetailTaksbar()

{

    if (IsTaskbarWndValid() && m_tBarDlg->GetClickedItem().IsPlugin())

    {

        //找到对应的插件

        ITMPlugin* plugin = theApp.m_plugins.GetPluginByItem(m_tBarDlg->GetClickedItem().PluginItem());

        if (plugin != nullptr)

        {

            int index = theApp.m_plugins.GetPluginIndex(plugin);

            CPluginInfoDlg dlg(index);

            dlg.DoModal();

        }

    }

}





UINT CPulseBarDlg::OnPowerBroadcast(UINT nPowerEvent, LPARAM nEventData)
{
    if (nPowerEvent == PBT_APMRESUMESUSPEND && !m_close_pending)
    {
        m_resume_check_times = 0;
        KillTimer(INIT_CONNECT_TIMER);
        SetTimer(INIT_CONNECT_TIMER, 10000, nullptr);
    }
    return CDialog::OnPowerBroadcast(nPowerEvent, nEventData);
}





void CPulseBarDlg::OnColorizationColorChanged(DWORD dwColorizationColor, BOOL bOpacity)

{

    // 此功能要求 Windows Vista 或更高版本。

    // _WIN32_WINNT 符号必须 >= 0x0600。

    // TODO: 在此添加消息处理程序代码和/或调用默认值



    static DWORD last_color;

    if (last_color != dwColorizationColor)

    {

        last_color = dwColorizationColor;

        BYTE red = (dwColorizationColor >> 16) & 0xFF;

        BYTE green = (dwColorizationColor >> 8) & 0xFF;

        BYTE blue = dwColorizationColor & 0xFF;

        COLORREF theme_color = RGB(red, green, blue);

        theApp.SetThemeColor(theme_color);

    }

    CDialog::OnColorizationColorChanged(dwColorizationColor, bOpacity);

}
