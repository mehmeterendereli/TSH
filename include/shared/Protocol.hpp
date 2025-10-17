
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

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

    struct ReadMemoryRequest
    {
        std::uint32_t processId{};
        std::uint32_t flags{};
        std::uintptr_t address{};
        std::uint32_t size{};

        [[nodiscard]] TSH_READ_MEMORY_REQUEST ToCStruct() const noexcept
        {
            return TSH_READ_MEMORY_REQUEST{
                processId,
                flags,
                static_cast<std::uint64_t>(address),
                size,
                0u };
        }
    };

    struct PointerTraceNode
    {
        std::uintptr_t address{};
        std::uintptr_t value{};

        PointerTraceNode() = default;

        explicit PointerTraceNode(const TSH_POINTER_TRACE_NODE& node) noexcept
            : address(static_cast<std::uintptr_t>(node.Address))
            , value(static_cast<std::uintptr_t>(node.Value))
        {
        }
    };

    struct MonitorEntry
    {
        std::uintptr_t address{};
        std::uint32_t  size{};

        [[nodiscard]] TSH_MONITOR_ENTRY ToCStruct() const noexcept
        {
            return TSH_MONITOR_ENTRY{
                static_cast<std::uint64_t>(address),
                size,
                0u };
        }
    };

    constexpr std::size_t kMonitorSampleMaxBytes = TSH_MONITOR_SAMPLE_MAX_BYTES;

    struct MonitorSample
    {
        std::uintptr_t address{};
        std::uint32_t  requestedSize{};
        std::uint32_t  capturedSize{};
        std::uint32_t  status{};
        std::vector<std::byte> data;

        MonitorSample() = default;

        explicit MonitorSample(const TSH_MONITOR_SAMPLE& sample)
            : address(static_cast<std::uintptr_t>(sample.Address))
            , requestedSize(sample.RequestedSize)
            , capturedSize(sample.CapturedSize)
            , status(sample.Status)
        {
            const auto count = std::min<std::uint32_t>(sample.CapturedSize, TSH_MONITOR_SAMPLE_MAX_BYTES);
            data.assign(sample.Data, sample.Data + count);
        }
    };

    struct PatchResponse
    {
        std::uint32_t bytesWritten{};
        std::uint32_t status{};

        PatchResponse() = default;

        explicit PatchResponse(const TSH_PATCH_RESPONSE& response) noexcept
            : bytesWritten(response.BytesWritten)
            , status(response.Status)
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
