#pragma once

#include <string>

namespace tsh::user
{
    class ResearchLoader
    {
    public:
        static bool EnsureDriverLoaded();

        static bool TryLoadDriver_NormalSigned();
        static bool TryLoadDriver_TestSigning();
        static bool TryLoadDriver_ExternalHelper();

    private:
        static std::wstring ResolveDriverPath();
        static bool EnsureServiceStarted(const std::wstring& serviceName, const std::wstring& displayName, const std::wstring& driverPath, const char* strategyTag);
        static void LogServiceError(const char* strategyTag, const std::wstring& phase, DWORD errorCode);
    };
} // namespace tsh::user

