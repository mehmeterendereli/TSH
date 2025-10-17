#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "user/PatternScanner.hpp"

namespace tsh::user
{
    class ResultRefiner
    {
    public:
        using Predicate = std::function<bool(const ScanHit&)>;

        ResultRefiner() = default;

        std::vector<ScanHit> Filter(const std::vector<ScanHit>& hits, const Predicate& predicate) const;
        std::vector<ScanHit> Limit(const std::vector<ScanHit>& hits, std::size_t maxCount) const;
    };
} // namespace tsh::user

