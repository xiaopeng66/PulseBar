#pragma once
#include <vector>
#include "PdhQuery.h"

// GPU利用率监控类(PDH;本机 PDH 若损坏则不可用)
class CPdhGPUUsage : public CPdhQuery
{
public:
    CPdhGPUUsage();
    ~CPdhGPUUsage();

    // 获取GPU利用率（0-100）
    bool GetGpuUsage(/*out*/ int& usage);
};

// 专用GPU内存监控类
// 数据源:NVML(NVIDIA 驱动自带 nvml.dll,与任务管理器一致的准确值)。
// NVML 不可用或取不到已用量时返回失败,不伪造数据。不依赖 PDH
// (部分系统如 Win11 26200 内部版的 PDH 计数器名称表损坏,全部返回 BAD_COUNTERNAME)。
class CGPUMemoryMonitor
{
public:
    CGPUMemoryMonitor();
    ~CGPUMemoryMonitor();

    bool GetDedicatedGpuMemory(/*out*/ int& usage_pct,
                               unsigned long long& used_b,
                               unsigned long long& total_b);

    //NVIDIA GPU 状态:一次设备循环取回利用率(0-100)/温度(℃)/功耗(W),多卡取最大值。
    //NVML 不可用时三个出参均为 -1 并返回 false。作为 PDH 损坏时的数据源
    bool GetNvidiaGpuStatus(/*out*/ int& util, /*out*/ int& temp_c, /*out*/ int& power_w);

private:
    bool InitNvml();
    void ShutdownNvml();

    HMODULE m_nvml = nullptr;
    bool m_nvml_inited = false;
    bool m_nvml_failed = false;     //NVML 初始化失败后不再重试
};
