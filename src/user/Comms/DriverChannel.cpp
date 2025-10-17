#include "user/DriverChannel.hpp"

#include "shared/DriverIoctl.h"

#include <cstring>
#include <vector>
#include <windows.h>
#include <errhandlingapi.h>


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

    bool DriverChannel::ReadMemory(std::uint32_t processId, std::uintptr_t address, std::span<std::byte> buffer) const
    {
        if (!IsOpen() || buffer.empty())
        {
            return false;
        }

        tsh::protocol::ReadMemoryRequest request{};
        request.processId = processId;
        request.address = address;
        request.size = static_cast<std::uint32_t>(buffer.size());
        auto native = request.ToCStruct();

        DWORD bytesReturned = 0;
        BOOL result = ::DeviceIoControl(
            m_deviceHandle,
            IOCTL_TSH_READ_MEMORY,
            &native,
            sizeof(native),
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            &bytesReturned,
            nullptr);

        return result && bytesReturned == buffer.size();
    }

    bool DriverChannel::PointerTrace(std::uint32_t processId, std::uintptr_t baseAddress, const std::vector<std::intptr_t>& offsets, std::vector<tsh::protocol::PointerTraceNode>& nodes) const
    {
        if (!IsOpen() || offsets.empty())
        {
            return false;
        }

        const std::size_t inputSize = sizeof(TSH_POINTER_TRACE_REQUEST) + offsets.size() * sizeof(LONGLONG);
        std::vector<std::byte> input(inputSize);

        auto* request = reinterpret_cast<TSH_POINTER_TRACE_REQUEST*>(input.data());
        request->ProcessId = processId;
        request->MaxDepth = static_cast<std::uint32_t>(offsets.size());
        request->BaseAddress = static_cast<std::uint64_t>(baseAddress);

        auto* offsetBuffer = reinterpret_cast<LONGLONG*>(request + 1);
        for (std::size_t index = 0; index < offsets.size(); ++index)
        {
            offsetBuffer[index] = static_cast<LONGLONG>(offsets[index]);
        }

        std::vector<TSH_POINTER_TRACE_NODE> output(offsets.size());

        DWORD bytesReturned = 0;
        BOOL result = ::DeviceIoControl(
            m_deviceHandle,
            IOCTL_TSH_POINTER_TRACE,
            input.data(),
            static_cast<DWORD>(input.size()),
            output.data(),
            static_cast<DWORD>(output.size() * sizeof(TSH_POINTER_TRACE_NODE)),
            &bytesReturned,
            nullptr);

        if (!result || bytesReturned == 0)
        {
            return false;
        }

        const std::size_t nodeCount = bytesReturned / sizeof(TSH_POINTER_TRACE_NODE);
        nodes.clear();
        nodes.reserve(nodeCount);
        for (std::size_t index = 0; index < nodeCount; ++index)
        {
            nodes.emplace_back(output[index]);
        }

        return true;
    }

    bool DriverChannel::MonitorSnapshot(std::uint32_t processId, const std::vector<tsh::protocol::MonitorEntry>& entries, std::vector<tsh::protocol::MonitorSample>& samples) const
    {
        if (!IsOpen() || entries.empty())
        {
            return false;
        }

        const std::size_t inputSize = sizeof(TSH_MONITOR_REQUEST) + entries.size() * sizeof(TSH_MONITOR_ENTRY);
        std::vector<std::byte> input(inputSize);

        auto* header = reinterpret_cast<TSH_MONITOR_REQUEST*>(input.data());
        header->ProcessId = processId;
        header->EntryCount = static_cast<std::uint32_t>(entries.size());
        header->PollingIntervalMs = 0;
        header->Flags = 0;

        auto* entryBuffer = reinterpret_cast<TSH_MONITOR_ENTRY*>(header + 1);
        for (std::size_t index = 0; index < entries.size(); ++index)
        {
            entryBuffer[index] = entries[index].ToCStruct();
        }

        std::vector<TSH_MONITOR_SAMPLE> output(entries.size());

        DWORD bytesReturned = 0;
        BOOL result = ::DeviceIoControl(
            m_deviceHandle,
            IOCTL_TSH_MONITOR_CONTROL,
            input.data(),
            static_cast<DWORD>(input.size()),
            output.data(),
            static_cast<DWORD>(output.size() * sizeof(TSH_MONITOR_SAMPLE)),
            &bytesReturned,
            nullptr);

        if (!result)
        {
            return false;
        }

        const std::size_t sampleCount = bytesReturned / sizeof(TSH_MONITOR_SAMPLE);
        samples.clear();
        samples.reserve(sampleCount);
        for (std::size_t index = 0; index < sampleCount; ++index)
        {
            samples.emplace_back(output[index]);
        }

        return true;
    }

    bool DriverChannel::ApplyPatch(std::uint32_t processId, std::uintptr_t address, std::span<const std::byte> data, std::uint32_t flags, tsh::protocol::PatchResponse& response) const
    {
        if (!IsOpen() || data.empty())
        {
            return false;
        }

        const std::size_t inputSize = sizeof(TSH_PATCH_REQUEST) + data.size();
        std::vector<std::byte> input(inputSize);

        auto* request = reinterpret_cast<TSH_PATCH_REQUEST*>(input.data());
        request->ProcessId = processId;
        request->Flags = flags;
        request->TargetAddress = static_cast<std::uint64_t>(address);
        request->PayloadSize = static_cast<std::uint32_t>(data.size());
        request->Reserved = 0;

        std::memcpy(request + 1, data.data(), data.size());

        TSH_PATCH_RESPONSE patchResponse{};
        DWORD bytesReturned = 0;
        BOOL result = ::DeviceIoControl(
            m_deviceHandle,
            IOCTL_TSH_PATCH_REQUEST,
            input.data(),
            static_cast<DWORD>(input.size()),
            &patchResponse,
            sizeof(patchResponse),
            &bytesReturned,
            nullptr);

        if (!result)
        {
            return false;
        }

        if (bytesReturned == sizeof(patchResponse))
        {
            response = tsh::protocol::PatchResponse(patchResponse);
        }
        else
        {
            response.bytesWritten = static_cast<std::uint32_t>(data.size());
            response.status = ERROR_SUCCESS;
        }

        return true;
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

        constexpr std::size_t kInitialCapacity = 128;
        constexpr std::size_t kMaximumCapacity = 16 * 1024;

        std::vector<TSH_MEMORY_REGION> regionBuffer(kInitialCapacity);

        while (true)
        {
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
                const DWORD error = ::GetLastError();
                if ((error == ERROR_MORE_DATA || error == ERROR_INSUFFICIENT_BUFFER) &&
                    regionBuffer.size() < kMaximumCapacity)
                {
                    regionBuffer.resize(regionBuffer.size() * 2);
                    continue;
                }

                return false;
            }

            const std::size_t regionCount = bytesReturned / sizeof(TSH_MEMORY_REGION);

            regions.clear();
            regions.reserve(regionCount);

            for (std::size_t index = 0; index < regionCount; ++index)
            {
                regions.emplace_back(regionBuffer[index]);
            }

            return true;
        }
    }
} // namespace tsh::user
