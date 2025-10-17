#pragma once

#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

namespace tsh::user
{
    struct ProcessInfo
    {
        std::uint32_t processId{};
        std::wstring  name;
        HANDLE        handle{ nullptr };
    };

    class ProcessManager
    {
    public:
        ProcessManager() = default;
        ~ProcessManager();

        ProcessManager(const ProcessManager&) = delete;
        ProcessManager& operator=(const ProcessManager&) = delete;

        ProcessManager(ProcessManager&& other) noexcept;
        ProcessManager& operator=(ProcessManager&& other) noexcept;

        std::vector<ProcessInfo> EnumerateProcesses() const;

        bool Attach(std::uint32_t processId, DWORD desiredAccess = PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION);
        void Detach();

        [[nodiscard]] bool IsAttached() const noexcept { return m_processHandle != nullptr; }
        [[nodiscard]] HANDLE ProcessHandle() const noexcept { return m_processHandle; }
        [[nodiscard]] std::uint32_t AttachedProcessId() const noexcept { return m_attachedPid; }

    private:
        static std::vector<ProcessInfo> EnumerateWithToolhelp();

        void Reset();

        HANDLE        m_processHandle{ nullptr };
        std::uint32_t m_attachedPid{ 0 };
    };
} // namespace tsh::user

