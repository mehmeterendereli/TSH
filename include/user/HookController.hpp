#pragma once

#include <windows.h>

#include <cstdint>
#include <vector>

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

        bool ApplyPatch(HANDLE process, PatchInstruction& instruction) const;
        bool RevertPatch(HANDLE process, const PatchInstruction& instruction) const;
    };
} // namespace tsh::user
