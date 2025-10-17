#include "user/DriverChannel.hpp"

#include "shared/DriverIoctl.h"

#include <array>
#include <vector>


namespace tsh::user
{
    namespace
    {
        HANDLE DuplicateHandleValue(HANDLE source)
        {
            return (source == INVALID_HANDLE_VALUE) ? INVALID_HANDLE_VALUE : source;
        }
    } // namespace

    DriverChannel::~DriverChannel()
    {
        Close();
    }

    DriverChannel::DriverChannel(DriverChannel&& other) noexcept
        : m_deviceHandle(DuplicateHandleValue(other.m_deviceHandle))
    {
        other.m_deviceHandle = INVALID_HANDLE_VALUE;
    }

    DriverChannel& DriverChannel::operator=(DriverChannel&& other) noexcept
    {
        if (this != &other)
        {
            Close();
            m_deviceHandle = DuplicateHandleValue(other.m_deviceHandle);
            other.m_deviceHandle = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

    bool DriverChannel::Open()
    {
        if (IsOpen())
        {
            return true;
        }

        m_deviceHandle = ::CreateFileW(
            tsh::protocol::DeviceName().data(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

        return IsOpen();
    }

    void DriverChannel::Close()
    {
        if (IsOpen())
        {
            ::CloseHandle(m_deviceHandle);
            m_deviceHandle = INVALID_HANDLE_VALUE;
        }
    }

    bool DriverChannel::Ping(std::uint32_t token, std::uint32_t& response) const
    {
        if (!IsOpen())
        {
            return false;
        }

        tsh::protocol::PingRequest request{ token };
        auto pingRequest = request.ToCStruct();
        TSH_PING_RESPONSE pingResponse{};

        DWORD bytesReturned = 0;
        BOOL result = ::DeviceIoControl(
            m_deviceHandle,
            IOCTL_TSH_PING,
            &pingRequest,
            sizeof(pingRequest),
            &pingResponse,
            sizeof(pingResponse),
            &bytesReturned,
            nullptr);

        if (!result || bytesReturned != sizeof(pingResponse))
        {
            return false;
        }

        response = tsh::protocol::PingResponse(pingResponse).value;
        return true;
    }

    bool DriverChannel::QueryProcessRegions(std::uint32_t processId, std::vector<tsh::protocol::MemoryRegion>& regions) const
    {
        if (!IsOpen())
        {
            return false;
        }

        const tsh::protocol::RegionQueryRequest request{ processId, 0 };
        auto query = request.ToCStruct();

        std::array<TSH_MEMORY_REGION, 256> regionBuffer{};

        DWORD bytesReturned = 0;
        BOOL result = ::DeviceIoControl(
            m_deviceHandle,
            IOCTL_TSH_QUERY_REGIONS,
            &query,
            sizeof(query),
            regionBuffer.data(),
            static_cast<DWORD>(regionBuffer.size() * sizeof(TSH_MEMORY_REGION)),
            &bytesReturned,
            nullptr);

        if (!result)
        {
            return false;
        }

        const std::size_t regionCount = bytesReturned / sizeof(TSH_MEMORY_REGION);
        regions.clear();
        regions.reserve(regionCount);

        for (std::size_t i = 0; i < regionCount; ++i)
        {
            regions.emplace_back(regionBuffer[i]);
        }

        return true;
    }
} // namespace tsh::user
