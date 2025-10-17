#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "user/PatternScanner.hpp"

namespace tsh::user
{
    class ScanSession
    {
    public:
        ScanSession() = default;

        void Begin(const std::vector<ScanRegion>& regions);
        void UpdateResults(std::vector<ScanHit> hits);

        [[nodiscard]] const std::vector<ScanRegion>& Regions() const noexcept { return m_regions; }
        [[nodiscard]] const std::vector<ScanHit>& Results() const noexcept { return m_hits; }

    private:
        std::vector<ScanRegion> m_regions;
        std::vector<ScanHit>    m_hits;
    };
} // namespace tsh::user

