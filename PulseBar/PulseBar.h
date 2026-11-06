
// PulseBar.h : PROJECT_NAME 应用程序的主头文件
//

#pragma once

#ifndef __AFXWIN_H__
#error "在包含此文件之前包含“stdafx.h”以生成 PCH 文件"
#endif

#include "resource.h"       // 主符号
#include "Common.h"
#include "WinVersionHelper.h"
#include "SimpleXML.h"
#include "TaskbarDefaultStyle.h"
#include <map>
#include <vector>
#include <atomic>
#include "OpenHardwareMonitor/OpenHardwareMonitorApi.h"
#include "PluginManager.h"
#include "Nullable.hpp"
#include "TaskBarDlgDrawCommon.h"
#include "DllFunctions.h"
#include "StrTable.h"
#include "PluginUpdateHelper.h"
#include "PluginInterface.h"
#include "MonitorSettings.h"

// CPulseBarApp:
// 有关此类的实现，请参阅 PulseBar.cpp
//


//用于“新版本启动时让旧实例退出”的注册消息
constexpr UINT PB_QUIT_REQUEST_MESSAGE = WM_APP + 500;   //新版本请求旧实例退出

enum class TrayPluginActionKind
{
    DETAIL,
    OPTIONS,
    DISABLE,
    PLUGIN_COMMAND,
};

struct TrayPluginAction
{
    std::wstring file_path;       //稳定的插件身份，不依赖插件 vector 下标
    ITMPlugin* plugin{};
    TrayPluginActionKind kind{};
    int plugin_command_index{ -1 };
    std::wstring command_label;
};

class CPulseBarApp : public CWinApp, public ITrafficMonitor
{
public:
    //各种路径
    static CPulseBarApp* self;
    wstring m_module_dir;       //程序exe文件的目录
    wstring m_appdata_dir;
    wstring m_module_path;      //程序exe文件的路径
    wstring m_module_path_reg;  //用于作为写入注册表开机自项的exe文件的路径（如果路径中有空格，加上引号）
    wstring m_config_path;
    wstring m_history_traffic_path;
    wstring m_log_path;
    wstring m_system_dir;
    wstring m_config_dir;

    //以下数据定义为App类中的公共成员，以便于在主对话框和任务栏窗口中都能访问
    unsigned __int64 m_in_speed{};      //下载速度
    unsigned __int64 m_out_speed{};     //上传速度
    int m_cpu_usage{ -1 };      //CPU利用率
    int m_memory_usage{ -1 };   //内存利用率
    int m_used_memory{};    //可用物理内存（单位为KB）
    int m_total_memory{};   //物理内存总量（单位为KB）
    float m_cpu_temperature{ -1 };  //CPU温度
    float m_cpu_freq{ -1 };  //CPU 频率
    float m_gpu_temperature{ -1 };  //显卡温度
    float m_hdd_temperature{ -1 };  //硬盘温度
    float m_main_board_temperature{ -1 };    //主板温度
    int m_gpu_usage{ -1 };      //显卡利用率
    int m_hdd_usage{ -1 };      //硬盘利用率
    int m_gpu_power_w{ -1 };    //GPU功耗(瓦;NVML;-1不可用)
    bool m_disk_speed_available{ false };   //磁盘读写速度是否可用(IOCTL)
    unsigned __int64 m_disk_read_speed{};   //磁盘读取速度(B/s,物理盘总和)
    unsigned __int64 m_disk_write_speed{};  //磁盘写入速度(B/s,物理盘总和)
    int m_dedicated_gpu_memory_pct{ -1 };   //专用GPU内存占用百分比(-1不可用)
    unsigned long long m_dedicated_gpu_memory_used_mb{};    //专用GPU内存已用(MB)
    unsigned long long m_dedicated_gpu_memory_total_mb{};   //专用GPU内存总量(MB)

    unsigned __int64 m_today_up_traffic{};  //今天已使用的上传流量
    unsigned __int64 m_today_down_traffic{};    //今天已使用的下载流量

    bool m_cannot_save_config_warning{ true };  //指示是否会在无法保存设置时弹出提示框
    bool m_cannot_save_global_config_warning{ true };   //指示是否会在无法保存设置时弹出提示框

    bool m_module_dir_writable{ true };         //指示程序所在目录是否可写

    //选项设置数据
    MainWndSettingData m_main_wnd_data;
    TaskBarSettingData m_taskbar_data;
    GeneralSettingData m_general_data; //UI-owned; workers use GetMonitorSettingsSnapshot().
    //其他设置数据
    MainConfigData m_cfg_data;
    int m_notify_interval;      //弹出通知消息的时间间隔
    bool m_debug_log{};
    bool m_taksbar_transparent_color_enable{};
    bool m_last_light_mode{};
    bool m_show_mouse_panetrate_tip{};  //是否显示开启“鼠标穿透”时的提示消息。
    bool m_show_dot_net_notinstalled_tip{};

    //bool m_is_windows10_fall_creator;
    CWinVersionHelper m_win_version;        //当前Windows的版本

    HICON m_notify_icons[MAX_NOTIFY_ICON]{};

    CTaskbarDefaultStyle m_taskbar_default_style;
    CPluginManager m_plugins;
    CDllFunctions m_dll_functions;
    CStrTable m_str_table;
    CPluginUpdateHelper m_plugin_update;

    //主窗口已移除：原 m_main_menu / m_main_menu_plugin / m_main_menu_plugin_sub_menu 一并删除
    CMenu m_taskbar_menu;       //任务栏窗口右键菜单
    CMenu m_taskbar_menu_plugin;    //右击任务栏窗口插件区域的右键菜单
    CMenu m_taskbar_menu_plugin_sub_menu;

    //托盘插件菜单的动态状态。父菜单接管 popup 后只保留句柄用于按句柄清理，
    //不再让 CMenu wrapper 持有同一个 HMENU，避免重复 DestroyMenu。
    std::map<UINT, TrayPluginAction> m_tray_plugin_actions;
    std::vector<HMENU> m_tray_plugin_popup_handles;
    HMENU m_tray_plugin_parent_menu{};
    std::map<std::wstring, UINT> m_tray_plugin_command_ids; //进程内不把旧 ID 重新绑定到其他动作
    bool m_tray_plugin_menu_active{};
    bool m_tray_plugin_menu_building{};

#ifndef WITHOUT_TEMPERATURE
    //硬件监控实例的构造状态。采样线程在锁外读它做快速跳过判断，
    //避免冷启动时卡在初始化线程跨 .NET CreateInstance() 持锁的那几秒。
    enum MonitorState { MONITOR_NONE = 0, MONITOR_INITIALIZING = 1, MONITOR_READY = 2 };
    //OpenHardwareMonitor 接口的指针
    std::shared_ptr<OpenHardwareMonitorApi::IOpenHardwareMonitor> m_pMonitor{};
    //m_pMonitor 的构造状态，供锁外快速判断用。
    //采样线程每秒调用一次；如果每次都去抢 m_minitor_lib_critical，
    //冷启动时初始化线程正跨 .NET CreateInstance() 持锁数秒，任务栏会整段停止刷新。
    //所以先读这个原子状态：不是 READY 就直接跳过，是 READY 才进锁内取指针。
    //它只是“提示”，真正的判空仍在锁内完成（锁外读 shared_ptr 是数据竞争）。
    //三态同时用来避免重复初始化：初始化进行中再点“确定”不会再起第二个初始化线程。
    std::atomic<int> m_monitor_state{ MONITOR_NONE };
#endif // !WITHOUT_TEMPERATURE

    CCriticalSection m_minitor_lib_critical;        //用于访问OpenHardwareMonitor进行线程同步的临界区对象
#ifndef WITHOUT_TEMPERATURE
    //硬件监控项名称的只读快照。采样线程在持有 m_minitor_lib_critical 时更新它，
    //设置页只读这份快照，不用在 UI 线程上等初始化/采样锁，打开设置不再卡顿。
    //快照锁的获取顺序固定为 m_minitor_lib_critical -> m_temperature_names_critical，
    //而 UI 线程只拿后者，不会形成环。
    CCriticalSection m_temperature_names_critical;
    std::vector<std::wstring> m_cpu_temperature_names;   //CPU/核心温度项名称（不含“平均温度”）
    std::vector<std::wstring> m_hdd_temperature_names;   //硬盘温度项名称
#endif
    //CCriticalSection m_lftable_critical;            //用于访问LfTable2进行线程同步的临界区对象
    CLazyConstructable<class CTaskBarDlgDrawCommonSupport> m_d2d_taskbar_draw_common_support{}; // 当使用D2D渲染时自动初始化的全局依赖

public:
#ifndef WITHOUT_TEMPERATURE
    bool IsHardwareMonitorReady() const { return m_monitor_state.load() == MONITOR_READY; }
    //初始化线程正在跑（可能正跨 .NET CreateInstance() 持锁）。调用方据此避免等锁。
    bool IsHardwareMonitorInitializing() const { return m_monitor_state.load() == MONITOR_INITIALIZING; }
#endif

    CPulseBarApp();

    void LoadLanguageConfig();
    void LoadConfig();
    void SaveConfig();
    void LoadPluginDisabledSettings();

    void LoadGlobalConfig();
    void SaveGlobalConfig();

    int DPI(int pixel);
    void DPI(CRect& rect);
    void DPIFromWindow(CWnd* pWnd);
    int GetDpi() const { return m_dpi; }
    void SetDPI(int dpi) { m_dpi = dpi; }

    void CheckUpdate(bool message);     //检查更新，如果message为true，则在检查时弹出提示信息
    void CheckUpdateInThread(bool message); //在后台线程中检查更新
    //启动时检查更新线程函数
    static UINT CheckUpdateThreadFunc(LPVOID lpParam);
    static UINT InitOpenHardwareMonitorLibThreadFunc(LPVOID lpParam);
#ifndef WITHOUT_TEMPERATURE
    //把当前的温度项名称刷新到快照（调用方须已持有 m_minitor_lib_critical）
    void RefreshTemperatureNameSnapshotLocked();
#endif

    bool SetAutoRun(bool auto_run, bool task_scheduler);
    bool GetAutoRun(wstring* auto_run_path, bool task_scheduler);        //判断是否开机自动进行，如果是，将开机自动运行的路径写入auto_run_path

    bool SetAutoRunByRegistry(bool auto_run);       //通过注册表实现开机自启动
    bool SetAutoRunByTaskScheduler(bool auto_run);  //通过任务计划实现开机自启动

    //获取系统信息文本
    CString GetSystemInfoString();

    void InitMenuResourse();

    //获取一个图标资源，如果资源还未加载，会自动加载。
    //由于本函数中使用了CPulseBarApp::DPI函数，因此本函数必须确保在CPulseBarApp::DPIFromWindow之后调用
    HICON GetMenuIcon(UINT id);

    static int NormalizeNotifyIconSelection(int selected);
    static int SelectNotifyIconForTheme(int selected, bool light_mode);
    int ResolveNotifyIconForDisplay() const;    //把“自动”样式解析为当前任务栏下实际显示的图标样式
    void LoadNotifyIcons();
    void DestroyNotifyIcons();
    HICON GetNotifyIcon(int selected);

    bool IsCheckingForUpdate() const { return m_checking_update; }      //是否正在检查更新

    //UI-only publisher; read handles are immutable and may outlive later UI commits.
    void PublishMonitorSettings();
    std::shared_ptr<const MonitorSettings> GetMonitorSettingsSnapshot() const;
    void InitOpenHardwareLibInThread();     //开启一个后台线程初始化OpenHardwareMonitor
    void UpdateOpenHardwareMonitorEnableState();    //更新硬件监控的启用/禁用状态

    //void UpdateTaskbarWndMenu();      //更新任务栏窗口右键菜单
    bool IsForceShowNotifyIcon();       //是否需要强制显示通知区图标

    std::wstring GetPlauginTooltipInfo() const;
    bool IsTaksbarItemDisplayed(CommonDisplayItem item) const;

    void SendSettingsToPlugin();    //向所有插件发送当前的选项设置

    //更新插件子菜单
    //plugin_cmd_start_index: 插件命令在菜单中的起始位置
    static void UpdatePluginMenu(CMenu* pMenu, ITMPlugin* plugin, int plugin_cmd_start_index);
    //重建托盘插件菜单：在“插件管理”后直接添加每个成功插件的名称 popup。
    //每个 popup 的动态命令由 m_tray_plugin_actions 独立 registry 路由。
    void UpdateTrayPluginMenu();
    HWND GetDialogOwner(HWND fallback) const;

    void CheckWindows11Taskbar();
    bool IsWindows11Taskbar() const { return m_is_windows11_taskbar; }

    bool DPIFromRect(const RECT& rect, UINT* out_dpi_x, UINT* out_dpi_y);

    virtual unsigned int GetThemeColor() const override;
    void SetThemeColor(COLORREF color);

private:
    MonitorSettingsStore m_monitor_settings;
    //int m_no_multistart_warning_time{};       //用于设置在开机后多长时间内不弹出“已经有一个程序正在运行”的警告提示
    bool m_exit_when_start_by_restart_manager{ true };      //如果程序被Windows重启管理器重新启动，则退出程序
    int m_dpi{ 96 };

    bool m_checking_update{ false };        //是否正在检查更新

    std::map<UINT, HICON> m_menu_icons;      //菜单图标资源。key是图标资源的ID，vlaue是图标的句柄

    ULONG_PTR m_gdiplusToken{};

    bool m_is_windows11_taskbar{ false };  //是否为Windows11的任务栏
    COLORREF m_theme_color{};

// 重写
public:
    virtual BOOL InitInstance();

    // 实现

    DECLARE_MESSAGE_MAP()
    afx_msg void OnHelp();
    afx_msg void OnFrequentyAskedQuestions();
    afx_msg void OnUpdateLog();
    virtual int ExitInstance();

    // 通过 ITrafficMonitor 继承
private:
    virtual int GetAPIVersion() override;
    virtual const wchar_t* GetVersion() override;
    double GetMonitorValue(MonitorItem item) override;
    virtual const wchar_t* GetMonitorValueString(MonitorItem item, int is_main_window = false) override;
    virtual const wchar_t* GetStringRes(const wchar_t* key, const wchar_t* section) override;
public:
    void ShowNotifyMessage(const wchar_t* strMsg) override;
    unsigned short GetLanguageId() const override;
    const wchar_t* GetPluginConfigDir() const override;
    int GetDPI(DPIType type) const override;
    virtual void* GetMainWindowHwnd() override;
    virtual void* GetTaskbarWindowHwnd() override;
};

extern CPulseBarApp theApp;
