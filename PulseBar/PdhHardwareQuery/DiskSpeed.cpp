#include "stdafx.h"
#include "DiskSpeed.h"
#include <winioctl.h>

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
    m_prev_read.clear();
    m_prev_write.clear();
    m_prev_valid.clear();
    m_has_prev = false;
}

void CDiskSpeedMonitor::EnsureHandles()
{
    if (!m_handles.empty())
        return;
    for (int i = 0; i < 16; i++)
    {
        wchar_t path[64];
        swprintf_s(path, L"\\\\.\\PhysicalDrive%d", i);
        HANDLE h = CreateFileW(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE)
            break;
        DISK_PERFORMANCE dp{};
        DWORD ret = 0;
        if (DeviceIoControl(h, IOCTL_DISK_PERFORMANCE, nullptr, 0, &dp, sizeof(dp), &ret, nullptr))
        {
            m_handles.push_back(h);
        }
        else
        {
            CloseHandle(h);
            break;
        }
    }
    //差分缓存与句柄一一对应,必须同步分配(否则 operator[] 越界)
    m_prev_read.assign(m_handles.size(), 0);
    m_prev_write.assign(m_handles.size(), 0);
    m_prev_valid.assign(m_handles.size(), false);
}

bool CDiskSpeedMonitor::GetDiskSpeed(unsigned long long& read_bps, unsigned long long& write_bps)
{
    read_bps = 0;
    write_bps = 0;
    if (m_failed)
        return false;
    EnsureHandles();
    if (m_handles.empty())
    {
        //IOCTL 发不出(句柄没开成)或全部失败累计,连续 3 次放弃
        if (++m_fail_count >= 3)
            m_failed = true;
        return false;
    }

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
        DWORD ret = 0;
        ok[i] = DeviceIoControl(m_handles[i], IOCTL_DISK_PERFORMANCE, nullptr, 0,
                                &cur[i], sizeof(DISK_PERFORMANCE), &ret, nullptr) != 0;
        if (ok[i])
            ok_count++;
    }
    if (ok_count == 0)
    {
        if (++m_fail_count >= 3)
            m_failed = true;
        return false;
    }
    m_fail_count = 0;

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
