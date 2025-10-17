
#include "user/MemoryAccessor.hpp"

#include <span>

namespace tsh::user
{
    void MemoryAccessor::Bind(DriverChannel* channel, HANDLE processHandle, std::uint32_t processId, bool preferDriver)
    {
        m_channel = channel;
        m_processHandle = processHandle;
        m_processId = processId;
        m_preferDriver = preferDriver;
    }

    void MemoryAccessor::Reset()
    {
        m_channel = nullptr;
        m_processHandle = nullptr;
        m_processId = 0;
        m_preferDriver = false;
    }

    bool MemoryAccessor::Read(std::uintptr_t address, std::span<std::byte> buffer) const
    {
        return ReadInternal(address, buffer);
    }

    bool MemoryAccessor::Read(std::uintptr_t address, std::span<char> buffer) const
    {
        return ReadInternal(address, buffer);
    }

    bool MemoryAccessor::Write(std::uintptr_t address, std::span<const std::byte> data, std::uint32_t flags) const
    {
        if (data.empty())
        {
            return false;
        }

        if (m_preferDriver && m_channel && m_channel->IsOpen() && m_processId != 0)
        {
            tsh::protocol::PatchResponse response{};
            if (m_channel->ApplyPatch(m_processId, address, data, flags, response))
            {
                return response.status == ERROR_SUCCESS;
            }
        }

        if (m_processHandle)
        {
            SIZE_T bytesWritten = 0;
            return ::WriteProcessMemory(
                       m_processHandle,
                       reinterpret_cast<LPVOID>(address),
                       data.data(),
                       data.size(),
                       &bytesWritten) &&
                   bytesWritten == data.size();
        }

        return false;
    }

    template <typename TBuffer>
    bool MemoryAccessor::ReadInternal(std::uintptr_t address, TBuffer buffer) const
    {
        if (buffer.empty())
        {
            return false;
        }

        if (m_preferDriver && m_channel && m_channel->IsOpen() && m_processId != 0)
        {
            auto byteSpan = std::as_writable_bytes(buffer);
            if (m_channel->ReadMemory(m_processId, address, byteSpan))
            {
                return true;
            }
        }

        if (m_processHandle)
        {
            SIZE_T bytesRead = 0;
            return ::ReadProcessMemory(
                       m_processHandle,
                       reinterpret_cast<LPCVOID>(address),
                       buffer.data(),
                       buffer.size(),
                       &bytesRead) &&
                   bytesRead == buffer.size();
        }

        return false;
    }

    template bool MemoryAccessor::ReadInternal<std::span<std::byte>>(std::uintptr_t, std::span<std::byte>) const;
    template bool MemoryAccessor::ReadInternal<std::span<char>>(std::uintptr_t, std::span<char>) const;
} // namespace tsh::user
