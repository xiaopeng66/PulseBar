
// PulseBarDlg.h : 头文件
//

#pragma once
#pragma comment (lib, "iphlpapi.lib")

#include "NetworkInfoDlg.h"
#include "afxwin.h"
#include "Common.h"
#include "TaskBarDlg.h"
#include "HistoryTrafficDlg.h"
#include "OptionsDlg.h"
#include "IconSelectDlg.h"
#include "TraySpeedIcon.h"
#include "DrawCommon.h"
#include "IniHelper.h"
#include "AdapterCommon.h"
#include "AboutDlg.h"
#include "PdhHardwareQuery/CPUUsage.h"
#include "PdhHardwareQuery/CpuFreq.h"
#include "PdhHardwareQuery/GpuUsage.h"
#include "PdhHardwareQuery/DiskSpeed.h"
#include "PdhHardwareQuery/DiskUsage.h"
#include "HistoryTrafficFile.h"
#include <atomic>

// 应用宿主（不可见）
class CPulseBarDlg : public CDialog
{
    // 构造
public:
    CPulseBarDlg(CWnd* pParent = NULL);   // 标准构造函数
    ~CPulseBarDlg();
    CTaskBarDlg* GetTaskbarWindow() const;

    static CPulseBarDlg* Instance();

    // 对话框数据
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_TRAFFICMONITOR_DIALOG };
#endif

    CPdhDiskUsage& GetPdhDiskUsageHelper() { return m_disk_usage_helper; }
    bool IsGetDiskUsageByPdh() const { return m_get_disk_usage_by_pdh; }

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持


// 实现
protected:
    HICON m_hIcon;
    NOTIFYICONDATA m_ntIcon{};    //通知区域图标
    CString m_last_notify_tip;  //优化:上次托盘 tooltip 文本,未变化时跳过 Shell_NotifyIcon
    CTaskBarDlg* m_tBarDlg{};     //任务栏窗口的指针

    CCriticalSection m_connection_lock;
    vector<NetWorkConection> m_connections; //保存获取到的要显示到“选择网卡”菜单项中的所有网络连接
    MIB_IFTABLE* m_pIfTable{ nullptr };
    DWORD m_dwSize{};	//m_pIfTable的大小
    int m_connection_selected{ 0 }; //要显示流量的连接的序号
    unsigned __int64 m_in_bytes{};        //当前已接收的字节数
    unsigned __int64 m_out_bytes{};   //当前已发送的字节数
    unsigned __int64 m_last_in_bytes{}; //上次已接收的字节数
    unsigned __int64 m_last_out_bytes{};    //上次已发送的字节数

    CCPUUsage m_cpu_usage_helper;
    CPdhCpuFreq m_cpu_freq_helper;
    CPdhGPUUsage m_gpu_usage_helper;
    CGPUMemoryMonitor m_gpu_mem_helper;                     //专用GPU内存查询(NVML+DXGI)
    CDiskSpeedMonitor m_disk_speed_helper;                  //磁盘读写速度查询(IOCTL,不依赖PDH)
    CPdhDiskUsage m_disk_usage_helper;

    bool m_get_disk_usage_by_pdh{};

    bool m_first_start{ true };     //初始时为true，在定时器第一次启动后置为flase

    CSize m_screen_size;        //屏幕的大小（包含任务栏）

    int m_restart_cnt{ -1 };    //重新初始化次数
    unsigned int m_timer_cnt{};     //定时器触发次数（自程序启动以来的秒数）
    unsigned int m_taskbar_timer_cnt{0}; //适用于TaskBarDlg的定时器触发次数（自程序启动以来的秒数）
    unsigned int m_monitor_time_cnt{};
    int m_zero_speed_cnt{}; //如果检测不到网速，该变量就会自加
    int m_resume_check_times{};
    int m_insert_to_taskbar_cnt{};  //用来统计尝试嵌入任务栏的次数
    int m_cannot_insert_to_task_bar_warning{ true };   //指示是否会在无法嵌入任务栏时弹出提示框

    static unsigned int m_WM_TASKBARCREATED;    //任务栏重启消息

    SYSTEMTIME m_start_time;    //程序启动时的时间
    CCriticalSection m_history_lock;
    CHistoryTrafficFile m_history_traffic{ theApp.m_history_traffic_path }; //储存历史流量

    std::atomic_bool m_connection_change_flag{ false };
    std::atomic_bool m_connection_refresh_pending{ false };
    std::atomic_bool m_close_pending{ false };
    static constexpr UINT_PTR MONITOR_EXIT_TIMER = 1243;

    HDC m_desktop_dc;

    string m_connection_name_preferd{ theApp.m_cfg_data.m_connection_name };          //保存用户手动选择的网络连接名称

    void DoMonitorAcquisition();    //获取一次监控信息
    static UINT MonitorThreadCallback(LPVOID dwUser);   //获取监控信息的线程函数
    CEvent m_monitorWakeEvent{ FALSE, FALSE };  //采样请求事件(自动重置),优化:替代 bool 标志 + Sleep(10) 轮询
    CEvent m_threadExitEvent;       //用于通知主线程工作线程已退出(现为线程退出信号源)
    CWinThread* m_monitorThread{ nullptr };    //由宿主持有并在 join 后释放
public:
    void ExitMonitorThread();       //停止监控线程

protected:
    void GetScreenSize();           //获取屏幕的大小（任务栏自动背景色需要）

    void AutoSelect();
    //void UpdateConnections();
    //自动选择连接
    bool UpdateIfTable(bool* resized = nullptr);
    void IniConnection();   //初始化连接
    void RequestConnectionRefresh();

    MIB_IFROW GetConnectIfTable(int connection_index);    //获取当前选择的网络连接的MIB_IFROW对象。connection_index为m_connections中的索引
    NetWorkConection GetConnection(int connection_index); //获取当前选择的网络连接的NetWorkConection对象。connection_index为m_connections中的索引

    void IniConnectionMenu(CMenu* pMenu);   //初始化“选择网络连接”菜单
    void IniTaskBarConnectionMenu();        //初始化任务栏窗口中的“选择网络连接”子菜单
    void InsertDynamicMenuItems();          //B3/B4:插入动态开关菜单项(仅在初始化时调用,带防重)        //初始化任务栏窗口的“选择网络连接”菜单
    void SetConnectionMenuState(CMenu* pMenu);      //设置“选择网络连接”菜单中选中的项目

    void CloseTaskBarWnd(); //关闭任务栏窗口
    void OpenTaskBarWnd();  //打开任务栏窗口

    void AddNotifyIcon();       //添加通知区图标
    void DeleteNotifyIcon();
public:
    void ShowNotifyTip(const wchar_t* title, const wchar_t* message);       //显示通知区提示
protected:
    HICON GetCurrentNotifyIcon();
    void UpdateNotifyIconTip(bool force = false);     //更新通知区图标及鼠标提示
    CTraySpeedIcon m_tray_speed_icon;   //通知区网速图标(B3:图标显示实时网速)

    void SaveHistoryTraffic();        // 增量保存，只更新第一行和今天的记录
    void SaveHistoryTrafficFull();    // 完整保存，用于程序退出时确保所有数据都保存
    void LoadHistoryTraffic();
    void BackupHistoryTrafficFile();

    void _OnOptions(int tab, CWnd* pParent);   //打开“选项”对话框的处理，tab：打开时切换的标签

    void ApplySettings(COptionsDlg& optionsDlg);

public:
    bool IsTaskbarWndValid() const;
protected:

public:
    //void ApplySettings();
    bool IsTemperatureNeeded() const;       //判断是否需要显示温度信息

protected:
    // 生成的消息映射函数
    virtual BOOL OnInitDialog();
    DECLARE_MESSAGE_MAP()
public:
    //  afx_msg LRESULT OnNcHitTest(CPoint point);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnNetworkInfo();
    afx_msg void OnClose();
    afx_msg LRESULT OnQuitRequest(WPARAM, LPARAM);
    virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
    virtual BOOL PreTranslateMessage(MSG* pMsg);
protected:
    afx_msg LRESULT OnNotifyIcon(WPARAM wParam, LPARAM lParam);
public:
    afx_msg void OnShowNotifyIcon();
    afx_msg void OnNotifyIconShowSpeed();   //B3:切换通知区图标显示网速
    afx_msg void OnShowSpeedGraphTip();     //B4:切换悬停显示网速曲线
    void ApplyTaskbarTheme();               //主题切换后统一应用
    afx_msg void OnTbThemeSelect(UINT theme_id);
    afx_msg void OnTbThemeBgImage();
    afx_msg void OnTbThemeSelectImage();
    afx_msg void OnTbThemeOpacitySub();
    afx_msg void OnTbThemeOpacityAdd();
    afx_msg void OnDestroy();
    afx_msg void OnShowTaskBarWnd();
    afx_msg void OnAppAbout();
    afx_msg void OnShowCpuMemory2();
    afx_msg LRESULT OnTaskBarCreated(WPARAM wParam, LPARAM lParam);
    //afx_msg void OnSetFont();
    afx_msg void OnTrafficHistory();
    afx_msg void OnOptions();
    afx_msg void OnOptions2();
protected:
    afx_msg LRESULT OnExitmenuloop(WPARAM wParam, LPARAM lParam);
public:
    afx_msg void OnChangeNotifyIcon();
    afx_msg void OnCheckUpdate();
protected:
    afx_msg LRESULT OnTaskbarMenuPopedUp(WPARAM wParam, LPARAM lParam);
public:
    afx_msg BOOL OnQueryEndSession();
protected:
    afx_msg LRESULT OnDpichanged(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnTaskbarWndClosed(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnMonitorInfoUpdated(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnDisplaychange(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnReopenTaksbarWnd(WPARAM wParam, LPARAM lParam);
public:
    afx_msg void OnPluginManage();
    afx_msg void OnOpenTaskManager();
protected:
    afx_msg LRESULT OnSettingsApplied(WPARAM wParam, LPARAM lParam);
public:
    afx_msg void OnDisplaySettings();
    afx_msg void OnRefreshConnectionList();
protected:
    afx_msg LRESULT OnTabletQuerysystemgesturestatus(WPARAM wParam, LPARAM lParam);
public:
    afx_msg void OnPluginOptionsTaksbar();
    afx_msg void OnPluginDetailTaksbar();
    afx_msg UINT OnPowerBroadcast(UINT nPowerEvent, LPARAM nEventData);
    afx_msg void OnColorizationColorChanged(DWORD dwColorizationColor, BOOL bOpacity);
};
