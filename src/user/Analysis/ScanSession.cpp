#include "user/ScanSession.hpp"

namespace tsh::user
{
    void ScanSession::Begin(const std::vector<ScanRegion>& regions)
    {
        m_regions = regions;
        m_hits.clear();
    }

    void ScanSession::UpdateResults(std::vector<ScanHit> hits)
    {
        m_hits = std::move(hits);
    }
} // namespace tsh::user

