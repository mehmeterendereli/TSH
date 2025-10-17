
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "shared/Protocol.h"

namespace tsh::protocol
{
    constexpr std::uint32_t kProtocolVersion = TSH_PROTOCOL_VERSION;

    struct PingRequest
    {
        std::uint32_t token{};

        [[nodiscard]] TSH_PING_REQUEST ToCStruct() const noexcept
        {
            return TSH_PING_REQUEST{ token };
        }
    };

    struct PingResponse
    {
        std::uint32_t value{};

        explicit PingResponse(const TSH_PING_RESPONSE& response) noexcept
            : value(response.Value)
        {
        }
    };

    struct RegionQueryRequest
    {
        std::uint32_t processId{};
        std::uint32_t flags{};

        [[nodiscard]] TSH_REGION_QUERY_REQUEST ToCStruct() const noexcept
        {
            return TSH_REGION_QUERY_REQUEST{ processId, flags };
        }
    };

    struct MemoryRegion
    {
        std::uintptr_t baseAddress{};
        std::size_t    size{};
        std::uint32_t  protection{};
        std::uint32_t  state{};
        std::uint32_t  type{};

        MemoryRegion() = default;

        explicit MemoryRegion(const TSH_MEMORY_REGION& region) noexcept
            : baseAddress(static_cast<std::uintptr_t>(region.BaseAddress))
            , size(static_cast<std::size_t>(region.Size))
            , protection(region.Protection)
            , state(region.State)
            , type(region.Type)
        {
        }
    };

    inline std::wstring_view KernelDevicePath()
    {
        return TSH_DEVICE_NAME;
    }

    inline std::wstring_view DeviceName()
    {
        return L"\\\\.\\TSHDiagnostic";
    }
} // namespace tsh::protocol
