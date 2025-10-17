#include "user/PatternScanner.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

namespace tsh::user
{
    namespace
    {
        constexpr std::size_t kDefaultChunkSize = 64 * 1024;

        bool MemoryReadExact(const MemoryAccessor& accessor, std::uintptr_t address, std::span<std::byte> buffer)
        {
            return accessor.Read(address, buffer);
        }

        void AppendMatchesForWindow(
            std::uintptr_t baseAddress,
            std::size_t baseOffset,
            std::span<const std::byte> window,
            std::span<const std::byte> pattern,
            std::vector<ScanHit>& hits)
        {
            const std::size_t patternSize = pattern.size();
            if (patternSize == 0 || window.size() < patternSize)
            {
                return;
            }

            for (std::size_t index = 0; index + patternSize <= window.size(); ++index)
            {
                if (std::memcmp(window.data() + index, pattern.data(), patternSize) == 0)
                {
                    hits.push_back(ScanHit{ baseAddress + baseOffset + index });
                }
            }
        }
    } // namespace

    std::vector<ScanHit> PatternScanner::ExecuteInitialScan(
        const MemoryAccessor& accessor,
        ScanDataType type,
        std::span<const std::byte> pattern,
        const std::vector<ScanRegion>& regions) const
    {
        std::vector<ScanHit> hits;

        if (!accessor.IsBound() || pattern.empty())
        {
            return hits;
        }

        const std::size_t patternSize = pattern.size();
        const std::size_t overlapSize = (patternSize > 1) ? patternSize - 1 : 0;
        std::vector<std::byte> chunkBuffer(kDefaultChunkSize);
        std::vector<std::byte> windowBuffer;
        windowBuffer.reserve(kDefaultChunkSize + overlapSize);
        std::vector<std::byte> tailBuffer(overlapSize);
        std::size_t tailLength = 0;

        for (const auto& region : regions)
        {
            if (region.size < patternSize)
            {
                continue;
            }

            tailLength = 0;
            std::uintptr_t regionAddress = region.baseAddress;
            std::size_t regionOffset = 0;

            while (regionOffset < region.size)
            {
                const std::size_t toRead = std::min(kDefaultChunkSize, region.size - regionOffset);
                if (!accessor.Read(regionAddress + regionOffset, std::span<std::byte>(chunkBuffer.data(), toRead)))
                {
                    regionOffset += toRead;
                    tailLength = 0;
                    continue;
                }

                const std::size_t validBytes = toRead;
                windowBuffer.clear();

                const std::size_t prefix = std::min(tailLength, regionOffset);

                if (prefix > 0)
                {
                    windowBuffer.insert(windowBuffer.end(), tailBuffer.begin(), tailBuffer.begin() + prefix);
                }

                windowBuffer.insert(windowBuffer.end(), chunkBuffer.begin(), chunkBuffer.begin() + validBytes);

                AppendMatchesForWindow(regionAddress, regionOffset - prefix, windowBuffer, pattern, hits);

                if (overlapSize > 0)
                {
                    if (windowBuffer.size() >= overlapSize)
                    {
                        const auto start = windowBuffer.end() - overlapSize;
                        std::copy(start, windowBuffer.end(), tailBuffer.begin());
                        tailLength = overlapSize;
                    }
                    else
                    {
                        std::copy(windowBuffer.begin(), windowBuffer.end(), tailBuffer.begin());
                        tailLength = windowBuffer.size();
                    }
                }

                regionOffset += validBytes;
            }
        }

        (void)type;
        return hits;
    }

    std::vector<ScanHit> PatternScanner::RefineHits(
        const MemoryAccessor& accessor,
        ScanDataType type,
        std::span<const std::byte> pattern,
        const std::vector<ScanHit>& existingHits) const
    {
        std::vector<ScanHit> refined;

        if (!accessor.IsBound() || pattern.empty())
        {
            return refined;
        }

        std::vector<std::byte> buffer(pattern.size());

        for (const auto& hit : existingHits)
        {
            if (MemoryReadExact(accessor, hit.address, std::span<std::byte>(buffer.data(), buffer.size())) &&
                std::memcmp(buffer.data(), pattern.data(), pattern.size()) == 0)
            {
                refined.push_back(hit);
            }
        }

        (void)type;
        return refined;
    }
} // namespace tsh::user
