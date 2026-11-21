#pragma once
#include <windows.h>
#include <vector>

//S2: 磁盘读写速度监控(IOCTL_DISK_PERFORMANCE 差分)
//数据源 \\.\PhysicalDriveN 物理盘总和,不依赖 PDH(本机 PDH 计数器名称表损坏)。
//0 访问权打开物理盘不需要管理员权限。首次调用仅建立基线返回 false,
//自第二次调用起返回上一间隔的平均速度;失败设备会关闭并重试,每 5 秒扫描新设备。
class CDiskSpeedMonitor
{
public:
    CDiskSpeedMonitor();
    ~CDiskSpeedMonitor();
    CDiskSpeedMonitor(const CDiskSpeedMonitor&) = delete;
    CDiskSpeedMonitor& operator=(const CDiskSpeedMonitor&) = delete;

    //读取所有物理盘的读写速度(B/s);不可用返回 false
    bool GetDiskSpeed(/*out*/ unsigned long long& read_bps, /*out*/ unsigned long long& write_bps);

private:
    void EnsureHandles();
    void CloseHandles();

    std::vector<HANDLE> m_handles;
    std::vector<int> m_disk_numbers;
    std::vector<long long> m_prev_read;     //上次 BytesRead.QuadPart
    std::vector<long long> m_prev_write;    //上次 BytesWritten.QuadPart
    std::vector<bool> m_prev_valid;
    ULONGLONG m_last_tick{};
    ULONGLONG m_last_scan_tick{};
    bool m_has_prev{ false };
    bool m_rescan_requested{ true };
};
