#include "user/ResultRefiner.hpp"

#include <algorithm>

namespace tsh::user
{
    std::vector<ScanHit> ResultRefiner::Filter(const std::vector<ScanHit>& hits, const Predicate& predicate) const
    {
        if (!predicate)
        {
            return hits;
        }

        std::vector<ScanHit> filtered;
        filtered.reserve(hits.size());

        for (const auto& hit : hits)
        {
            if (predicate(hit))
            {
                filtered.push_back(hit);
            }
        }

        return filtered;
    }

    std::vector<ScanHit> ResultRefiner::Limit(const std::vector<ScanHit>& hits, std::size_t maxCount) const
    {
        if (maxCount == 0 || hits.size() <= maxCount)
        {
            return hits;
        }

        return std::vector<ScanHit>(hits.begin(), hits.begin() + static_cast<std::ptrdiff_t>(maxCount));
    }
} // namespace tsh::user

