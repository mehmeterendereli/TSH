#pragma once

#include <windows.h>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "shared/Protocol.hpp"

namespace tsh::user
{
    class DriverChannel
    {
    public:
        DriverChannel() = default;
        ~DriverChannel();

        DriverChannel(const DriverChannel&) = delete;
        DriverChannel& operator=(const DriverChannel&) = delete;

        DriverChannel(DriverChannel&& other) noexcept;
        DriverChannel& operator=(DriverChannel&& other) noexcept;

        bool Open();
        void Close();
        bool IsOpen() const noexcept { return m_deviceHandle != INVALID_HANDLE_VALUE; }

        bool Ping(std::uint32_t token, std::uint32_t& response) const;
        bool QueryProcessRegions(std::uint32_t processId, std::vector<tsh::protocol::MemoryRegion>& regions) const;
        bool ReadMemory(std::uint32_t processId, std::uintptr_t address, std::span<std::byte> buffer) const;
        bool PointerTrace(std::uint32_t processId, std::uintptr_t baseAddress, const std::vector<std::intptr_t>& offsets, std::vector<tsh::protocol::PointerTraceNode>& nodes) const;
        bool MonitorSnapshot(std::uint32_t processId, const std::vector<tsh::protocol::MonitorEntry>& entries, std::vector<tsh::protocol::MonitorSample>& samples) const;
        bool ApplyPatch(std::uint32_t processId, std::uintptr_t address, std::span<const std::byte> data, std::uint32_t flags, tsh::protocol::PatchResponse& response) const;

    private:
        HANDLE m_deviceHandle{ INVALID_HANDLE_VALUE };
    };
} // namespace tsh::user
