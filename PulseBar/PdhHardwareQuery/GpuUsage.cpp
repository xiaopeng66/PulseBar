#include "stdafx.h"
#include "GpuUsage.h"
#include "../DllFunctions.h"

#include <ShlObj.h>
#include <algorithm>
#include <cwchar>
#include <string>
#include <vector>

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Uuid.lib")

namespace
{
    constexpr DWORD kRestrictedDllSearch =
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32;

    bool IsAbsoluteLocalPath(const std::wstring& path)
    {
        return path.size() >= 3 &&
            ((path[0] >= L'A' && path[0] <= L'Z') || (path[0] >= L'a' && path[0] <= L'z')) &&
            path[1] == L':' && (path[2] == L'\\' || path[2] == L'/');
    }

    std::wstring CanonicalExistingPath(const std::wstring& path, bool directory)
    {
        if (!IsAbsoluteLocalPath(path))
            return {};

        DWORD attributes = ::GetFileAttributesW(path.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES ||
            (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 ||
            ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) != directory)
            return {};

        std::vector<wchar_t> canonical(MAX_PATH);
        DWORD length = ::GetFullPathNameW(path.c_str(),
                                          static_cast<DWORD>(canonical.size()),
                                          canonical.data(), nullptr);
        if (length == 0)
            return {};
        if (length >= canonical.size())
        {
            canonical.resize(static_cast<size_t>(length) + 1);
            length = ::GetFullPathNameW(path.c_str(),
                                        static_cast<DWORD>(canonical.size()),
                                        canonical.data(), nullptr);
            if (length == 0 || length >= canonical.size())
                return {};
        }

        std::wstring result(canonical.data(), length);
        std::replace(result.begin(), result.end(), L'/', L'\\');
        std::wstring normalized_input = path;
        std::replace(normalized_input.begin(), normalized_input.end(), L'/', L'\\');
        if (_wcsicmp(result.c_str(), normalized_input.c_str()) != 0)
            return {};
        return result;
    }

    bool IsDescendantPath(const std::wstring& path, const std::wstring& root)
    {
        if (path.size() <= root.size() ||
            _wcsnicmp(path.c_str(), root.c_str(), root.size()) != 0)
            return false;
        return path[root.size()] == L'\\';
    }

    HMODULE LoadTrustedAbsoluteDll(const std::wstring& path)
    {
        if (path.empty())
            return nullptr;

        // NVSMI has no unrestricted compatibility fallback: on older loaders,
        // an absent optional dependency must not re-enable CWD/PATH searching.
        return ::LoadLibraryExW(path.c_str(), nullptr, kRestrictedDllSearch);
    }

    std::wstring GetTrustedNvmlPath()
    {
        PWSTR program_files = nullptr;
        if (FAILED(::SHGetKnownFolderPath(FOLDERID_ProgramFiles, KF_FLAG_DEFAULT,
                                          nullptr, &program_files)))
            return {};

        std::wstring program_files_path(program_files);
        ::CoTaskMemFree(program_files);
        program_files_path = CanonicalExistingPath(program_files_path, true);
        if (program_files_path.empty())
            return {};

        std::wstring nvidia_directory = CanonicalExistingPath(
            program_files_path + L"\\NVIDIA Corporation", true);
        if (nvidia_directory.empty() || !IsDescendantPath(nvidia_directory, program_files_path))
            return {};

        std::wstring nvmsi_directory = CanonicalExistingPath(
            nvidia_directory + L"\\NVSMI", true);
        if (nvmsi_directory.empty() || !IsDescendantPath(nvmsi_directory, program_files_path))
            return {};

        std::wstring nvml_path = CanonicalExistingPath(
            nvmsi_directory + L"\\nvml.dll", false);
        if (nvml_path.empty() || !IsDescendantPath(nvml_path, program_files_path))
            return {};
        return nvml_path;
    }

    HMODULE LoadTrustedNvml()
    {
        // NVML is optional. If neither trusted location exists, callers fall
        // back to the existing unavailable-feature behavior.
        HMODULE module = LoadKnownSystemDll(L"nvml.dll");
        if (module != nullptr)
            return module;

        return LoadTrustedAbsoluteDll(GetTrustedNvmlPath());
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
// CPdhGPUUsage 实现
///////////////////////////////////////////////////////////////////////////////////////////
CPdhGPUUsage::CPdhGPUUsage()
: CPdhQuery(_T("\\GPU Engine(*)\\Utilization Percentage"))
{
}

CPdhGPUUsage::~CPdhGPUUsage()
{
}

bool CPdhGPUUsage::GetGpuUsage(int& usage)
{
    if (isInitialized)
    {
        std::vector<CounterValueItem> valueItems;
        if (QueryValues(valueItems))
        {
            if (!valueItems.empty())
            {
                //获取所有类型的利用率
                std::map<std::wstring, double> gpu_usage_map;
                for (const auto& item : valueItems)
                {
                    std::wstring item_name = item.name;
                    size_t index = item.name.rfind(L'_');
                    if (index != std::wstring::npos)
                        item_name = item.name.substr(index + 1);
                    gpu_usage_map[item_name] += item.value;
                }
                //查找所有类型中最大的值作为总利用率（同Windows任务管理器的处理）
                double max_value = 0;
                for (const auto& item : gpu_usage_map)
                {
                    if (item.second > max_value)
                        max_value = item.second;
                }

                usage = static_cast<int>(max_value + 0.5);  // 四舍五入
                usage = min(max(usage, 0), 100);        // 限制在0-100范围
                return true;
            }
        }
    }

    return false;
}

///////////////////////////////////////////////////////////////////////////////////////////
// CGPUMemoryMonitor 实现
// 数据源 NVML(NVIDIA 驱动自带 nvml.dll,准确、绕开损坏的 PDH)。
// NVML 不可用或取不到已用量时返回失败,不伪造数据。
///////////////////////////////////////////////////////////////////////////////////////////

// NVML 最小接口
typedef struct nvmlDevice_st* nvmlDevice_t;
typedef int nvmlReturn_t;
typedef int (CDECL* PFN_nvmlInit)();
typedef int (CDECL* PFN_nvmlShutdown)();
typedef int (CDECL* PFN_nvmlDeviceGetCount)(unsigned int*);
typedef int (CDECL* PFN_nvmlDeviceGetHandleByIndex)(unsigned int, nvmlDevice_t*);
typedef int (CDECL* PFN_nvmlDeviceGetMemoryInfo)(nvmlDevice_t, void*);   // struct {ull total, free, used}
typedef int (CDECL* PFN_nvmlDeviceGetUtilizationRates)(nvmlDevice_t, void*);   // struct {uint gpu, memory}
typedef int (CDECL* PFN_nvmlDeviceGetTemperature)(nvmlDevice_t, int, unsigned int*);   //sensor 0=GPU
typedef int (CDECL* PFN_nvmlDeviceGetPowerUsage)(nvmlDevice_t, unsigned int*);   //毫瓦

CGPUMemoryMonitor::CGPUMemoryMonitor()
{
}

CGPUMemoryMonitor::~CGPUMemoryMonitor()
{
    ShutdownNvml();
}

bool CGPUMemoryMonitor::InitNvml()
{
    if (m_nvml_inited)
        return true;
    if (m_nvml_failed)
        return false;
    m_nvml = LoadTrustedNvml();
    if (m_nvml == nullptr)
    {
        m_nvml_failed = true;
        return false;
    }
    auto init = (PFN_nvmlInit)GetProcAddress(m_nvml, "nvmlInit_v2");
    if (init == nullptr || init() != 0)
    {
        FreeLibrary(m_nvml);
        m_nvml = nullptr;
        m_nvml_failed = true;
        return false;
    }
    m_nvml_inited = true;
    return true;
}

void CGPUMemoryMonitor::ShutdownNvml()
{
    if (m_nvml_inited && m_nvml != nullptr)
    {
        auto shutdown = (PFN_nvmlShutdown)GetProcAddress(m_nvml, "nvmlShutdown");
        if (shutdown != nullptr)
            shutdown();
        FreeLibrary(m_nvml);
        m_nvml = nullptr;
    }
    m_nvml_inited = false;
}

bool CGPUMemoryMonitor::GetDedicatedGpuMemory(int& usage_pct, unsigned long long& used_b, unsigned long long& total_b)
{
    used_b = 0;
    total_b = 0;

    if (InitNvml())
    {
        auto getCount = (PFN_nvmlDeviceGetCount)GetProcAddress(m_nvml, "nvmlDeviceGetCount_v2");
        auto getHandle = (PFN_nvmlDeviceGetHandleByIndex)GetProcAddress(m_nvml, "nvmlDeviceGetHandleByIndex_v2");
        auto getMem = (PFN_nvmlDeviceGetMemoryInfo)GetProcAddress(m_nvml, "nvmlDeviceGetMemoryInfo");
        if (getCount != nullptr && getHandle != nullptr && getMem != nullptr)
        {
            unsigned int count = 0;
            unsigned long long used = 0, total = 0;
            if (getCount(&count) == 0)
            {
                for (unsigned int i = 0; i < count; i++)
                {
                    nvmlDevice_t dev = nullptr;
                    if (getHandle(i, &dev) == 0)
                    {
                        struct { unsigned long long total, free_, used; } mem{};
                        if (getMem(dev, &mem) == 0)
                        {
                            used += mem.used;
                            total += mem.total;
                        }
                    }
                }
            }
            if (total > 0)
            {
                int pct = (int)(used * 100 / total);
                pct = min(max(pct, 0), 100);
                usage_pct = pct;
                used_b = used;
                total_b = total;
                return true;
            }
        }
    }

    //NVML 不可用(无 NVIDIA 驱动)或读取失败时返回 false,
    //不伪造数据;已用量取不到时仅靠 DXGI 总量无意义,不再回退
    return false;
}

//NVIDIA GPU 状态:一次设备循环取回利用率/温度/功耗,多卡取最大值;NVML 不可用返回 false
bool CGPUMemoryMonitor::GetNvidiaGpuStatus(int& util, int& temp_c, int& power_w)
{
    util = -1;
    temp_c = -1;
    power_w = -1;
    if (!InitNvml())
        return false;
    auto getCount = (PFN_nvmlDeviceGetCount)GetProcAddress(m_nvml, "nvmlDeviceGetCount_v2");
    auto getHandle = (PFN_nvmlDeviceGetHandleByIndex)GetProcAddress(m_nvml, "nvmlDeviceGetHandleByIndex_v2");
    auto getUtil = (PFN_nvmlDeviceGetUtilizationRates)GetProcAddress(m_nvml, "nvmlDeviceGetUtilizationRates");
    auto getTemp = (PFN_nvmlDeviceGetTemperature)GetProcAddress(m_nvml, "nvmlDeviceGetTemperature");
    auto getPower = (PFN_nvmlDeviceGetPowerUsage)GetProcAddress(m_nvml, "nvmlDeviceGetPowerUsage");
    if (getCount == nullptr || getHandle == nullptr || getUtil == nullptr || getTemp == nullptr || getPower == nullptr)
        return false;
    unsigned int count = 0;
    if (getCount(&count) != 0)
        return false;
    for (unsigned int i = 0; i < count; i++)
    {
        nvmlDevice_t dev = nullptr;
        if (getHandle(i, &dev) != 0)
            continue;
        struct { unsigned int gpu, memory; } util_data{};
        unsigned int temp = 0, power_mw = 0;
        if (getUtil(dev, &util_data) == 0)
            util = max(util, (int)util_data.gpu);
        if (getTemp(dev, 0, &temp) == 0)
            temp_c = max(temp_c, (int)temp);
        if (getPower(dev, &power_mw) == 0)
            power_w = max(power_w, (int)((power_mw + 500) / 1000));     //mW->W 四舍五入
    }
    return true;
}

//S6: GPU 图形进程枚举已移除——面板进程区按用户要求删除后无调用方,
//本机 WDDM 驱动亦不支持(nvmlDeviceGetGraphicsRunningProcesses 全版本 NOT_SUPPORTED)
