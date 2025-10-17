#include "user/PointerResolver.hpp"

namespace tsh::user
{
    PointerPath PointerResolver::ResolvePointerChain(HANDLE process, std::uintptr_t baseAddress, const std::vector<std::intptr_t>& offsets) const
    {
        PointerPath path;
        if (!process || baseAddress == 0)
        {
            return path;
        }

        std::uintptr_t currentAddress = baseAddress;

        for (std::intptr_t offset : offsets)
        {
            std::uintptr_t pointerValue = 0;
            SIZE_T bytesRead = 0;
            if (!::ReadProcessMemory(process, reinterpret_cast<LPCVOID>(currentAddress), &pointerValue, sizeof(pointerValue), &bytesRead) ||
                bytesRead != sizeof(pointerValue))
            {
                break;
            }

            currentAddress = pointerValue + static_cast<std::uintptr_t>(offset);
            path.push_back(PointerPathNode{ pointerValue, offset });
        }

        return path;
    }
} // namespace tsh::user

