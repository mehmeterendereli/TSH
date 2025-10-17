#pragma once

#include <cstdint>
#include <vector>

#include "user/MemoryAccessor.hpp"

namespace tsh::user
{
    struct PatchInstruction
    {
        std::uintptr_t address{};
        std::vector<std::byte> originalBytes;
        std::vector<std::byte> replacementBytes;
    };

    class HookController
    {
    public:
        HookController() = default;

        bool ApplyPatch(MemoryAccessor& accessor, PatchInstruction& instruction, std::uint32_t flags = 0) const;
        bool RevertPatch(MemoryAccessor& accessor, const PatchInstruction& instruction, std::uint32_t flags = 0) const;
    };
} // namespace tsh::user
