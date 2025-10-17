#include "user/ProcessManager.hpp"

#include <TlHelp32.h>

#include <algorithm>

namespace tsh::user
{
    namespace
    {
        HANDLE DuplicateHandleValue(HANDLE handle)
        {
            return (handle != nullptr && handle != INVALID_HANDLE_VALUE) ? handle : nullptr;
        }
    } // namespace

    ProcessManager::~ProcessManager()
    {
        Detach();
    }

    ProcessManager::ProcessManager(ProcessManager&& other) noexcept
        : m_processHandle(DuplicateHandleValue(other.m_processHandle))
        , m_attachedPid(other.m_attachedPid)
    {
        other.m_processHandle = nullptr;
        other.m_attachedPid = 0;
    }

    ProcessManager& ProcessManager::operator=(ProcessManager&& other) noexcept
    {
        if (this != &other)
        {
            Detach();
            m_processHandle = DuplicateHandleValue(other.m_processHandle);
            m_attachedPid = other.m_attachedPid;

            other.m_processHandle = nullptr;
            other.m_attachedPid = 0;
        }
        return *this;
    }

    std::vector<ProcessInfo> ProcessManager::EnumerateProcesses() const
    {
        return EnumerateWithToolhelp();
    }

    bool ProcessManager::Attach(std::uint32_t processId, DWORD desiredAccess)
    {
        if (processId == 0)
        {
            return false;
        }

        HANDLE processHandle = ::OpenProcess(desiredAccess, FALSE, processId);
        if (!processHandle)
        {
            return false;
        }

        Detach();
        m_processHandle = processHandle;
        m_attachedPid = processId;
        return true;
    }

    void ProcessManager::Detach()
    {
        if (m_processHandle)
        {
            ::CloseHandle(m_processHandle);
            m_processHandle = nullptr;
        }
        m_attachedPid = 0;
    }

    std::vector<ProcessInfo> ProcessManager::EnumerateWithToolhelp()
    {
        std::vector<ProcessInfo> processes;

        HANDLE snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
        {
            return processes;
        }

        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);

        if (::Process32FirstW(snapshot, &entry))
        {
            do
            {
                ProcessInfo info{};
                info.processId = entry.th32ProcessID;
                info.name.assign(entry.szExeFile);
                processes.emplace_back(std::move(info));
            } while (::Process32NextW(snapshot, &entry));
        }

        ::CloseHandle(snapshot);

        std::sort(processes.begin(), processes.end(), [](const ProcessInfo& lhs, const ProcessInfo& rhs) {
            return lhs.processId < rhs.processId;
        });

        return processes;
    }

    void ProcessManager::Reset()
    {
        Detach();
    }
} // namespace tsh::user

