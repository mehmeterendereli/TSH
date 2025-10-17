#pragma once

#include <windows.h>

#include <cstdint>
#include <vector>

namespace tsh::user
{
    struct PointerPathNode
    {
        std::uintptr_t address{};
        std::intptr_t  offset{};
    };

    using PointerPath = std::vector<PointerPathNode>;

    class PointerResolver
    {
    public:
        PointerResolver() = default;

        PointerPath ResolvePointerChain(HANDLE process, std::uintptr_t baseAddress, const std::vector<std::intptr_t>& offsets) const;
    };
} // namespace tsh::user

