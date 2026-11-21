#include "stdafx.h"
#include "DiskSpeed.h"
#include <winioctl.h>
#include <algorithm>

CDiskSpeedMonitor::CDiskSpeedMonitor() = default;

CDiskSpeedMonitor::~CDiskSpeedMonitor()
{
    CloseHandles();
}

void CDiskSpeedMonitor::CloseHandles()
{
    for (HANDLE h : m_handles)
    {
        if (h != INVALID_HANDLE_VALUE)
            CloseHandle(h);
    }
    m_handles.clear();
    m_disk_numbers.clear();
    m_prev_read.clear();
    m_prev_write.clear();
    m_prev_valid.clear();
    m_has_prev = false;
    m_rescan_requested = true;
}

void CDiskSpeedMonitor::EnsureHandles()
{
    const ULONGLONG now = GetTickCount64();
    if (!m_rescan_requested && now - m_last_scan_tick < 5000)
        return;
    m_last_scan_tick = now;
    m_rescan_requested = false;
    for (int i = 0; i < 16; i++)
    {
        if (std::find(m_disk_numbers.begin(), m_disk_numbers.end(), i) != m_disk_numbers.end())
            continue;
        wchar_t path[64];
        swprintf_s(path, L"\\\\.\\PhysicalDrive%d", i);
        HANDLE h = CreateFileW(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE)
            continue;
        DISK_PERFORMANCE dp{};
        DWORD ret = 0;
        if (DeviceIoControl(h, IOCTL_DISK_PERFORMANCE, nullptr, 0, &dp, sizeof(dp), &ret, nullptr))
        {
            m_handles.push_back(h);
            m_disk_numbers.push_back(i);
            m_prev_read.push_back(0);
            m_prev_write.push_back(0);
            m_prev_valid.push_back(false);
        }
        else
        {
            CloseHandle(h);
        }
    }
}

bool CDiskSpeedMonitor::GetDiskSpeed(unsigned long long& read_bps, unsigned long long& write_bps)
{
    read_bps = 0;
    write_bps = 0;
    EnsureHandles();
    if (std::none_of(m_handles.begin(), m_handles.end(), [](HANDLE h) { return h != INVALID_HANDLE_VALUE; }))
        return false;

    size_t count = m_handles.size();
    //防御:差分缓存尺寸必须与句柄一致(EnsureHandles 已分配,这里兜底)
    if (m_prev_read.size() != count || m_prev_write.size() != count || m_prev_valid.size() != count)
    {
        m_prev_read.assign(count, 0);
        m_prev_write.assign(count, 0);
        m_prev_valid.assign(count, false);
        m_has_prev = false;
    }
    std::vector<DISK_PERFORMANCE> cur(count);
    std::vector<bool> ok(count, false);
    size_t ok_count = 0;
    for (size_t i = 0; i < count; i++)
    {
        if (m_handles[i] == INVALID_HANDLE_VALUE)
            continue;
        DWORD ret = 0;
        ok[i] = DeviceIoControl(m_handles[i], IOCTL_DISK_PERFORMANCE, nullptr, 0,
                                &cur[i], sizeof(DISK_PERFORMANCE), &ret, nullptr) != 0;
        if (ok[i])
            ok_count++;
        else if (m_handles[i] != INVALID_HANDLE_VALUE)
        {
            CloseHandle(m_handles[i]);
            m_handles[i] = INVALID_HANDLE_VALUE;
            m_disk_numbers[i] = -1;
            m_prev_valid[i] = false;
            m_rescan_requested = true;
        }
    }
    if (ok_count == 0)
        return false;

    ULONGLONG now = GetTickCount64();
    bool have_rate = false;
    if (m_has_prev && m_last_tick > 0)
    {
        ULONGLONG elapsed = now - m_last_tick;
        if (elapsed > 0)
        {
            unsigned long long read_bytes = 0, write_bytes = 0;
            for (size_t i = 0; i < count; i++)
            {
                //仅对连续两次都成功的盘做差分,负差(计数器重置)跳过
                if (!ok[i] || !m_prev_valid[i])
                    continue;
                long long dr = cur[i].BytesRead.QuadPart - m_prev_read[i];
                long long dw = cur[i].BytesWritten.QuadPart - m_prev_write[i];
                if (dr > 0)
                    read_bytes += (unsigned long long)dr;
                if (dw > 0)
                    write_bytes += (unsigned long long)dw;
            }
            read_bps = read_bytes * 1000 / (unsigned long long)elapsed;
            write_bps = write_bytes * 1000 / (unsigned long long)elapsed;
            have_rate = true;
        }
    }

    for (size_t i = 0; i < count; i++)
    {
        if (ok[i])
        {
            m_prev_read[i] = cur[i].BytesRead.QuadPart;
            m_prev_write[i] = cur[i].BytesWritten.QuadPart;
            m_prev_valid[i] = true;
        }
        else
        {
            m_prev_valid[i] = false;
        }
    }
    m_last_tick = now;
    m_has_prev = true;
    return have_rate;
}
