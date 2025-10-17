#include "user/ValueMonitor.hpp"

#include <vector>

namespace tsh::user
{
    ValueMonitor::~ValueMonitor()
    {
        Stop();
    }

    void ValueMonitor::Configure(const MemoryAccessor* accessor, std::vector<MonitorEntry> entries, MonitorCallback callback, std::chrono::milliseconds interval)
    {
        Stop();

        m_accessor = accessor;
        m_entries = std::move(entries);
        m_callback = std::move(callback);
        m_interval = interval;
    }

    void ValueMonitor::Start()
    {
        if (IsActive() || m_accessor == nullptr || !m_accessor->IsBound() || !m_callback || m_entries.empty())
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
                if (entry.size == 0)
                {
                    continue;
                }

                if (m_accessor != nullptr && m_accessor->Read(entry.address, std::span<std::byte>(buffer.data(), buffer.size())))
                {
                    m_callback(entry, buffer);
                }
            }

            std::this_thread::sleep_for(m_interval);
        }
    }
} // namespace tsh::user
