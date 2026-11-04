#include "stdafx.h"
#include "TaskbarHelper.h"
#include <algorithm>

// 存储显示器信息
struct MonitorInfo
{
    HMONITOR hMonitor;
    CRect rect;
};

// 存储任务栏信息
struct TaskbarInfo
{
    HWND hwnd;
    CRect rect;
};

std::vector<MonitorInfo> monitors;
std::vector<TaskbarInfo> taskbars;

// 枚举显示器的回调函数
static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
    MonitorInfo info;
    info.hMonitor = hMonitor;
    info.rect = *lprcMonitor;
    if (!info.rect.IsRectEmpty())
        monitors.push_back(info);
    else
        ASSERT(FALSE);
    return TRUE;
}

// 枚举窗口的回调函数
static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    TCHAR className[256];
    GetClassName(hwnd, className, 256);

    // 检查是否是副显示器的任务栏
    if (_tcscmp(className, _T("Shell_SecondaryTrayWnd")) == 0)
    {
        TaskbarInfo info;
        info.hwnd = hwnd;
        GetWindowRect(hwnd, &info.rect);
        taskbars.push_back(info);
    }

    return TRUE;
}

//任务栏归属的显示器序号：取与任务栏矩形重叠面积最大的那一台，全都不重叠时返回 -1。
//不用“左上角落在某台显示器内”判断——任务栏可能跨在显示器边界上，
//那样两台任务栏会被判成同一台显示器，排序结果不稳定。
static int MonitorIndexOf(const CRect& rect)
{
    int best = -1;
    long long best_area = 0;
    for (size_t i = 0; i < monitors.size(); ++i)
    {
        CRect overlap;
        if (overlap.IntersectRect(rect, monitors[i].rect) == FALSE)
            continue;
        const long long area = static_cast<long long>(overlap.Width()) * overlap.Height();
        if (area > best_area)
        {
            best_area = area;
            best = static_cast<int>(i);
        }
    }
    return best;
}

// 比较函数：按显示器的顺序排序，同一显示器内再按任务栏位置排序
static bool CompareTaskbarByMonitorOrder(const TaskbarInfo& a, const TaskbarInfo& b)
{
    //这里必须是严格弱序（compare(a,a)==false、compare(a,b) 与 compare(b,a) 互斥、
    //且可传递），否则 std::sort 的行为是未定义的，可能越界读写。
    //原实现用「在某个显示器内就返回 true」的逐显示器判断，既不对称也不可传递。
    //改为「显示器序号 → left → top → 句柄」四级键，任意两个不同任务栏都能分出先后，
    //键完全相同的元素按等价处理。
    const int a_index = MonitorIndexOf(a.rect);
    const int b_index = MonitorIndexOf(b.rect);
    const int a_key = a_index < 0 ? INT_MAX : a_index;     //找不到归属的排最后
    const int b_key = b_index < 0 ? INT_MAX : b_index;
    if (a_key != b_key)
        return a_key < b_key;
    if (a.rect.left != b.rect.left)
        return a.rect.left < b.rect.left;
    if (a.rect.top != b.rect.top)
        return a.rect.top < b.rect.top;
    //指针直接比大小在标准 C++ 里是未定义的，转成整数再比
    return reinterpret_cast<INT_PTR>(a.hwnd) < reinterpret_cast<INT_PTR>(b.hwnd);
}

void CTaskbarHelper::GetAllSecondaryDisplayTaskbar(std::vector<HWND>& secondary_taskbars)
{
    monitors.clear();
    taskbars.clear();
    secondary_taskbars.clear();

    // 获取所有显示器信息
    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, 0);

    // 获取所有任务栏句柄
    EnumWindows(EnumWindowsProc, 0);

    // 按显示器顺序对任务栏排序
    std::sort(taskbars.begin(), taskbars.end(), CompareTaskbarByMonitorOrder);

    //保存任务栏句柄
    for (const auto& taskbar : taskbars)
    {
        secondary_taskbars.push_back(taskbar.hwnd);
    }
}

int CTaskbarHelper::GetDisplayNum()
{
    monitors.clear();
    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, 0);
    return static_cast<int>(monitors.size());
}

int CTaskbarHelper::GetSecondaryTaskbarNum()
{
    taskbars.clear();
    EnumWindows(EnumWindowsProc, 0);
    return static_cast<int>(taskbars.size());
}
