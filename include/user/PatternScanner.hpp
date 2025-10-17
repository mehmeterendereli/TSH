#pragma once

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tsh::user
{
    enum class ScanDataType
    {
        Int32,
        UInt32,
        Float,
        AsciiString,
        Utf16String,
        BytePattern
    };

    struct ScanRegion
    {
        std::uintptr_t baseAddress{};
        std::size_t    size{};
    };

    struct ScanHit
    {
        std::uintptr_t address{};
    };

    class PatternScanner
    {
    public:
        PatternScanner() = default;

        std::vector<ScanHit> ExecuteInitialScan(HANDLE process, ScanDataType type, std::span<const std::byte> pattern, const std::vector<ScanRegion>& regions) const;
        std::vector<ScanHit> RefineHits(HANDLE process, ScanDataType type, std::span<const std::byte> pattern, const std::vector<ScanHit>& existingHits) const;
    };
} // namespace tsh::user
