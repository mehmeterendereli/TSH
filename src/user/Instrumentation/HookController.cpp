#include "user/HookController.hpp"

namespace tsh::user
{
    namespace
    {
        bool WriteBytes(HANDLE process, std::uintptr_t address, const std::vector<std::byte>& bytes)
        {
            if (bytes.empty())
            {
                return true;
            }

            SIZE_T bytesWritten = 0;
            return ::WriteProcessMemory(
                       process,
                       reinterpret_cast<LPVOID>(address),
                       bytes.data(),
                       bytes.size(),
                       &bytesWritten) &&
                   bytesWritten == bytes.size();
        }

        bool ReadBytes(HANDLE process, std::uintptr_t address, std::vector<std::byte>& bytes)
        {
            if (bytes.empty())
            {
                return true;
            }
            SIZE_T bytesRead = 0;
            return ::ReadProcessMemory(
                       process,
                       reinterpret_cast<LPCVOID>(address),
                       bytes.data(),
                       bytes.size(),
                       &bytesRead) &&
                   bytesRead == bytes.size();
        }
    } // namespace

    bool HookController::ApplyPatch(HANDLE process, PatchInstruction& instruction) const
    {
        if (!process || instruction.address == 0 || instruction.replacementBytes.empty())
        {
            return false;
        }

        instruction.originalBytes.resize(instruction.replacementBytes.size());
        if (!ReadBytes(process, instruction.address, instruction.originalBytes))
        {
            instruction.originalBytes.clear();
            return false;
        }

        return WriteBytes(process, instruction.address, instruction.replacementBytes);
    }

    bool HookController::RevertPatch(HANDLE process, const PatchInstruction& instruction) const
    {
        if (!process || instruction.address == 0 || instruction.originalBytes.empty())
        {
            return false;
        }

        return WriteBytes(process, instruction.address, instruction.originalBytes);
    }
} // namespace tsh::user

