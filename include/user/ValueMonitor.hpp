#pragma once

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <chrono>
#include <functional>
#include <thread>
#include <vector>

namespace tsh::user
{
    struct MonitorEntry
    {
        std::uintptr_t address{};
        std::size_t    size{};
    };

    using MonitorCallback = std::function<void(const MonitorEntry&, const std::vector<std::byte>&)>;

    class ValueMonitor
    {
    public:
        ValueMonitor() = default;
        ~ValueMonitor();

        ValueMonitor(const ValueMonitor&) = delete;
        ValueMonitor& operator=(const ValueMonitor&) = delete;

        void Configure(HANDLE process, std::vector<MonitorEntry> entries, MonitorCallback callback, std::chrono::milliseconds interval);
        void Start();
        void Stop();

        [[nodiscard]] bool IsActive() const noexcept { return m_running.load(std::memory_order_acquire); }

    private:
        void MonitorLoop();

        HANDLE                        m_process{ nullptr };
        std::vector<MonitorEntry>     m_entries;
        MonitorCallback               m_callback;
        std::chrono::milliseconds     m_interval{ 500 };
        std::atomic<bool>             m_running{ false };
        std::thread                   m_worker;
    };
} // namespace tsh::user
