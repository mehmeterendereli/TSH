#pragma once

#include <windows.h>

#include <cstdint>
#include <span>
#include <vector>

#include "user/DriverChannel.hpp"

namespace tsh::user
{
    class MemoryAccessor
    {
    public:
        MemoryAccessor() = default;

        void Bind(DriverChannel* channel, HANDLE processHandle, std::uint32_t processId, bool preferDriver);
        void Reset();

        [[nodiscard]] bool IsBound() const noexcept { return m_processHandle != nullptr || (m_channel != nullptr && m_processId != 0); }
        [[nodiscard]] std::uint32_t ProcessId() const noexcept { return m_processId; }
        [[nodiscard]] HANDLE ProcessHandle() const noexcept { return m_processHandle; }

        bool Read(std::uintptr_t address, std::span<std::byte> buffer) const;
        bool Read(std::uintptr_t address, std::span<char> buffer) const;
        bool Write(std::uintptr_t address, std::span<const std::byte> data, std::uint32_t flags = 0) const;

    private:
        template <typename TBuffer>
        bool ReadInternal(std::uintptr_t address, TBuffer buffer) const;

        DriverChannel* m_channel{ nullptr };
        HANDLE         m_processHandle{ nullptr };
        std::uint32_t  m_processId{ 0 };
        bool           m_preferDriver{ false };
    };
} // namespace tsh::user

