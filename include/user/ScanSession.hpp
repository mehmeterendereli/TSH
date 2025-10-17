#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "user/PatternScanner.hpp"

namespace tsh::user
{
    class ScanSession
    {
    public:
        ScanSession() = default;

        void Begin(const std::vector<ScanRegion>& regions, ScanDataType type, std::vector<std::byte> pattern);
        void UpdateResults(std::vector<ScanHit> hits);
        void Reset();

        [[nodiscard]] bool IsActive() const noexcept { return m_active; }
        [[nodiscard]] const std::vector<ScanRegion>& Regions() const noexcept { return m_regions; }
        [[nodiscard]] const std::vector<ScanHit>& Results() const noexcept { return m_hits; }
        [[nodiscard]] ScanDataType Type() const noexcept { return m_type; }
        [[nodiscard]] std::span<const std::byte> Pattern() const noexcept { return m_pattern; }

    private:
        bool                    m_active{ false };
        ScanDataType            m_type{ ScanDataType::Int32 };
        std::vector<std::byte>  m_pattern;
        std::vector<ScanRegion> m_regions;
        std::vector<ScanHit>    m_hits;
    };
} // namespace tsh::user

