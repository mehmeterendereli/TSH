#pragma once

#include <windows.h>
#include <cstdint>
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

    private:
        HANDLE m_deviceHandle{ INVALID_HANDLE_VALUE };
    };
} // namespace tsh::user
