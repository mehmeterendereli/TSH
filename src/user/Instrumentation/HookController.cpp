#include "user/HookController.hpp"

#include <span>

namespace tsh::user
{
    bool HookController::ApplyPatch(MemoryAccessor& accessor, PatchInstruction& instruction, std::uint32_t flags) const
    {
        if (!accessor.IsBound() || instruction.address == 0 || instruction.replacementBytes.empty())
        {
            return false;
        }

        instruction.originalBytes.resize(instruction.replacementBytes.size());
        auto readSpan = std::span<std::byte>(instruction.originalBytes.data(), instruction.originalBytes.size());
        if (!accessor.Read(instruction.address, readSpan))
        {
            instruction.originalBytes.clear();
            return false;
        }

        return accessor.Write(instruction.address, instruction.replacementBytes, flags);
    }

    bool HookController::RevertPatch(MemoryAccessor& accessor, const PatchInstruction& instruction, std::uint32_t flags) const
    {
        if (!accessor.IsBound() || instruction.address == 0 || instruction.originalBytes.empty())
        {
            return false;
        }

        return accessor.Write(instruction.address, instruction.originalBytes, flags);
    }
} // namespace tsh::user
