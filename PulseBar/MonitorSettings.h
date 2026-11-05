#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

// Only settings consumed by sampling/background hardware initialization belong here.
// GeneralSettingData remains UI-owned; readers hold one immutable version per round.
struct MonitorSettings
{
    std::uint64_t version{};
    int monitor_time_span{ 1000 };
    int cpu_usage_acquire_method{}; // GeneralSettingData::CpuUsageAcquireMethod
    unsigned int hardware_monitor_item{};
    std::wstring hard_disk_name;
    std::wstring cpu_core_name;

    bool IsHardwareEnable(unsigned int item_type) const
    {
        return (hardware_monitor_item & item_type) != 0;
    }
};

class MonitorSettingsStore
{
public:
    void Publish(MonitorSettings settings)
    {
        // Copy strings/allocate before locking; release the previous version after unlocking.
        auto next = std::make_shared<MonitorSettings>(std::move(settings));
        std::shared_ptr<const MonitorSettings> previous;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            next->version = m_snapshot->version + 1;
            previous = std::move(m_snapshot);
            m_snapshot = std::move(next);
        }
    }

    std::shared_ptr<const MonitorSettings> Read() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_snapshot;
    }

private:
    // Never hold this mutex across another lock, hardware/API/plugin calls or I/O.
    mutable std::mutex m_mutex;
    std::shared_ptr<const MonitorSettings> m_snapshot{ std::make_shared<MonitorSettings>() };
};
