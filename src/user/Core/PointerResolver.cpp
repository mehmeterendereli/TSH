#include "user/PointerResolver.hpp"

#include <span>

namespace tsh::user
{
    PointerPath PointerResolver::ResolvePointerChain(const MemoryAccessor& accessor, std::uintptr_t baseAddress, const std::vector<std::intptr_t>& offsets) const
    {
        PointerPath path;
        if (!accessor.IsBound() || baseAddress == 0)
        {
            return path;
        }

        std::uintptr_t currentAddress = baseAddress;
        for (std::intptr_t offset : offsets)
        {
            std::uintptr_t pointerValue = 0;
            auto buffer = std::span<std::byte>(reinterpret_cast<std::byte*>(&pointerValue), sizeof(pointerValue));
            if (!accessor.Read(currentAddress, buffer))
            {
                break;
            }

            currentAddress = pointerValue + static_cast<std::uintptr_t>(offset);
            path.push_back(PointerPathNode{ pointerValue, offset });
        }

        return path;
    }
} // namespace tsh::user
