#include "user/ValueMonitor.hpp"

#include <vector>

namespace tsh::user
{
    ValueMonitor::~ValueMonitor()
    {
        Stop();
    }

    void ValueMonitor::Configure(HANDLE process, std::vector<MonitorEntry> entries, MonitorCallback callback, std::chrono::milliseconds interval)
    {
        Stop();

        m_process = process;
        m_entries = std::move(entries);
        m_callback = std::move(callback);
        m_interval = interval;
    }

    void ValueMonitor::Start()
    {
        if (IsActive() || m_process == nullptr || !m_callback || m_entries.empty())
        {
            return;
        }

        m_running.store(true, std::memory_order_release);
        m_worker = std::thread(&ValueMonitor::MonitorLoop, this);
    }

    void ValueMonitor::Stop()
    {
        if (IsActive())
        {
            m_running.store(false, std::memory_order_release);
            if (m_worker.joinable())
            {
                m_worker.join();
            }
        }
    }

    void ValueMonitor::MonitorLoop()
    {
        std::vector<std::byte> buffer;

        while (m_running.load(std::memory_order_acquire))
        {
            for (const auto& entry : m_entries)
            {
                buffer.resize(entry.size);

                SIZE_T bytesRead = 0;
                if (::ReadProcessMemory(m_process, reinterpret_cast<LPCVOID>(entry.address), buffer.data(), buffer.size(), &bytesRead) &&
                    bytesRead == buffer.size())
                {
                    m_callback(entry, buffer);
                }
            }

            ::Sleep(static_cast<DWORD>(m_interval.count()));
        }
    }
} // namespace tsh::user

