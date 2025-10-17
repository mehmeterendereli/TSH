#include "user/PatternScanner.hpp"

#include <cstring>
#include <memory>
#include <vector>

namespace tsh::user
{
    namespace
    {
        bool MemoryRead(HANDLE process, std::uintptr_t address, void* buffer, std::size_t size)
        {
            SIZE_T bytesRead = 0;
            return ::ReadProcessMemory(process, reinterpret_cast<LPCVOID>(address), buffer, size, &bytesRead) && bytesRead == size;
        }
    } // namespace

    std::vector<ScanHit> PatternScanner::ExecuteInitialScan(HANDLE process, ScanDataType type, const void* value, std::size_t valueSize, const std::vector<ScanRegion>& regions) const
    {
        std::vector<ScanHit> hits;

        if (!process || !value || valueSize == 0)
        {
            return hits;
        }

        std::vector<std::byte> compareBuffer(valueSize);

        for (const auto& region : regions)
        {
            if (region.size < valueSize)
            {
                continue;
            }

            const std::uintptr_t start = region.baseAddress;
            const std::uintptr_t end = start + region.size - valueSize;

            for (std::uintptr_t address = start; address <= end; ++address)
            {
                if (!MemoryRead(process, address, compareBuffer.data(), valueSize))
                {
                    continue;
                }

                if (std::memcmp(compareBuffer.data(), value, valueSize) == 0)
                {
                    hits.push_back(ScanHit{ address });
                }
            }
        }

        (void)type; // placeholder until type-specific comparers are implemented.
        return hits;
    }

    std::vector<ScanHit> PatternScanner::RefineHits(HANDLE process, ScanDataType type, const void* value, std::size_t valueSize, const std::vector<ScanHit>& existingHits) const
    {
        std::vector<ScanHit> refined;

        if (!process || !value || valueSize == 0)
        {
            return refined;
        }

        std::vector<std::byte> compareBuffer(valueSize);

        for (const auto& hit : existingHits)
        {
            if (MemoryRead(process, hit.address, compareBuffer.data(), valueSize) &&
                std::memcmp(compareBuffer.data(), value, valueSize) == 0)
            {
                refined.push_back(hit);
            }
        }

        (void)type;
        return refined;
    }
} // namespace tsh::user
