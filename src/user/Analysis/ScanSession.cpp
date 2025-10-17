#include "user/ScanSession.hpp"

namespace tsh::user
{
    void ScanSession::Begin(const std::vector<ScanRegion>& regions, ScanDataType type, std::vector<std::byte> pattern)
    {
        m_regions = regions;
        m_type = type;
        m_pattern = std::move(pattern);
        m_hits.clear();
        m_active = true;
    }

    void ScanSession::UpdateResults(std::vector<ScanHit> hits)
    {
        m_hits = std::move(hits);
    }

    void ScanSession::Reset()
    {
        m_active = false;
        m_pattern.clear();
        m_hits.clear();
        m_regions.clear();
    }
} // namespace tsh::user
