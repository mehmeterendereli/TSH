#include "user/ResearchLoader.hpp"

#include <windows.h>

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <system_error>

namespace tsh::user
{
    namespace
    {
        constexpr const wchar_t* kDriverServiceName = L"tsh_driver";
        constexpr const wchar_t* kDriverDisplayName = L"TSH Diagnostics";
        constexpr const wchar_t* kDriverRelativeFromExe = L"..\\driver\\Release\\tsh_driver.sys";
        constexpr const wchar_t* kDriverRelativeFromRepo = L"build\\vs\\driver\\Release\\tsh_driver.sys";

        void DescribeCodeIntegrityBlock(DWORD errorCode)
        {
            switch (errorCode)
            {
                case ERROR_INVALID_IMAGE_HASH:
#ifdef ERROR_DRIVER_BLOCKED
                case ERROR_DRIVER_BLOCKED:
#endif
#ifdef ERROR_FAILED_DRIVER_ENTRY
                case ERROR_FAILED_DRIVER_ENTRY:
#endif
#ifdef ERROR_IMAGE_MACHINE_TYPE_MISMATCH_EXE
                case ERROR_IMAGE_MACHINE_TYPE_MISMATCH_EXE:
#endif
#ifdef ERROR_SIGNATURE_OSIMAGE_MISMATCH
                case ERROR_SIGNATURE_OSIMAGE_MISMATCH:
#endif
#ifdef ERROR_BAD_EXE_FORMAT
                case ERROR_BAD_EXE_FORMAT:
#endif
                {
                    std::cerr
                        << "[TSH] Secure Boot veya kod butunlugu kisitlamasi surucuyu engelledi."
                        << " (GetLastError=" << errorCode << ", NTSTATUS=0xC0000428)\n"
                        << "[TSH] Secure Boot aktif oldugundan imzasiz surucu yuklenemedi. Test mode veya external mapper gerekiyor.\n";
                    break;
                }
                default:
                    break;
            }
        }
    } // namespace

    bool ResearchLoader::EnsureDriverLoaded()
    {
        if (TryLoadDriver_NormalSigned())
        {
            return true;
        }

        if (TryLoadDriver_TestSigning())
        {
            return true;
        }

        return TryLoadDriver_ExternalHelper();
    }

    bool ResearchLoader::TryLoadDriver_NormalSigned()
    {
        const std::wstring driverPath = ResolveDriverPath();
        if (driverPath.empty())
        {
            std::cerr << "[TSH] Surucu yolu cozumlenemedi; dosya bulunamadi.\n";
            return false;
        }

        std::cout << "[TSH] [NormalSigned] Standard SCM yuklemesi deneniyor...\n";
        return EnsureServiceStarted(kDriverServiceName, kDriverDisplayName, driverPath, "NormalSigned");
    }

    bool ResearchLoader::TryLoadDriver_TestSigning()
    {
        const std::wstring driverPath = ResolveDriverPath();
        if (driverPath.empty())
        {
            return false;
        }

        std::cout << "[TSH] [TestSigning] Test modunun kullanıcı tarafından etkinleştirildiği varsayılıyor...\n";
        const bool started = EnsureServiceStarted(kDriverServiceName, kDriverDisplayName, driverPath, "TestSigning");
        if (!started)
        {
            std::cerr << "[TSH] [TestSigning] Test moduna ragmen surucu yuklenemedi. Kod butunlugu hala etkin olabilir.\n";
        }
        return started;
    }

    bool ResearchLoader::TryLoadDriver_ExternalHelper()
    {
        std::cerr << "[TSH] [ExternalHelper] Harici yukleyici yapilandirilmedi. DSEFix/KDU gibi arastirma araclarini manuel calistirin.\n";
        return false;
    }

    std::wstring ResearchLoader::ResolveDriverPath()
    {
        wchar_t modulePath[MAX_PATH] = {};
        const DWORD len = ::GetModuleFileNameW(nullptr, modulePath, static_cast<DWORD>(std::size(modulePath)));
        std::filesystem::path basePath;
        if (len != 0 && len < std::size(modulePath))
        {
            basePath = std::filesystem::path(modulePath).parent_path();
        std::error_code ec;
        const std::filesystem::path candidate = std::filesystem::weakly_canonical(basePath / kDriverRelativeFromExe, ec);
        if (!ec && std::filesystem::exists(candidate))
        {
            return candidate.wstring();
        }
    }

    const std::filesystem::path cwd = std::filesystem::current_path();
    std::error_code ecRepo;
    const std::filesystem::path repoCandidate = std::filesystem::weakly_canonical(cwd / kDriverRelativeFromRepo, ecRepo);
    if (!ecRepo && std::filesystem::exists(repoCandidate))
    {
        return repoCandidate.wstring();
    }

    return {};
}

bool ResearchLoader::EnsureServiceStarted(const std::wstring& serviceName, const std::wstring& displayName, const std::wstring& driverPath, const char* strategyTag)
{
    const std::filesystem::path driverFile(driverPath);
    std::error_code existsError;
    if (driverPath.empty() || !std::filesystem::exists(driverFile, existsError))
    {
        std::cerr << "[TSH] [" << strategyTag << "] Surucu dosyasi bulunamadi: " << std::string(driverPath.begin(), driverPath.end()) << "\n";
        return false;
    }

        SC_HANDLE scm = ::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
        if (!scm)
        {
            LogServiceError(strategyTag, L"OpenSCManager", ::GetLastError());
            return false;
        }

        SC_HANDLE service = ::OpenServiceW(scm, serviceName.c_str(), SERVICE_START | SERVICE_QUERY_STATUS | SERVICE_STOP | DELETE);
        if (!service)
        {
            const DWORD openError = ::GetLastError();
            if (openError == ERROR_SERVICE_DOES_NOT_EXIST)
            {
                service = ::CreateServiceW(
                    scm,
                    serviceName.c_str(),
                    displayName.c_str(),
                    SERVICE_START | SERVICE_QUERY_STATUS | SERVICE_STOP | DELETE,
                    SERVICE_KERNEL_DRIVER,
                    SERVICE_DEMAND_START,
                    SERVICE_ERROR_NORMAL,
                    driverPath.c_str(),
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr);

                if (!service)
                {
                    LogServiceError(strategyTag, L"CreateService", ::GetLastError());
                    ::CloseServiceHandle(scm);
                    return false;
                }

                std::cout << "[TSH] [" << strategyTag << "] Hizmet oluşturuldu: " << std::string(serviceName.begin(), serviceName.end()) << "\n";
            }
            else
            {
                LogServiceError(strategyTag, L"OpenService", openError);
                ::CloseServiceHandle(scm);
                return false;
            }
        }

        bool started = false;
        if (!::StartServiceW(service, 0, nullptr))
        {
            const DWORD startError = ::GetLastError();
            if (startError == ERROR_SERVICE_ALREADY_RUNNING)
            {
                std::cout << "[TSH] [" << strategyTag << "] Surucu servisi zaten calisiyor.\n";
                started = true;
            }
            else
            {
                LogServiceError(strategyTag, L"StartService", startError);
                DescribeCodeIntegrityBlock(startError);
            }
        }
        else
        {
            started = true;
            std::cout << "[TSH] [" << strategyTag << "] Surucu basariyla baslatildi.\n";
        }

        ::CloseServiceHandle(service);
        ::CloseServiceHandle(scm);
        return started;
    }

    void ResearchLoader::LogServiceError(const char* strategyTag, const std::wstring& phase, DWORD errorCode)
    {
        const std::string phaseNarrow(phase.begin(), phase.end());
        std::cerr
            << "[TSH] [" << strategyTag << "] " << phaseNarrow
            << " hatası: " << errorCode << " (0x" << std::hex << errorCode << std::dec << ")\n";

        if (errorCode == ERROR_ACCESS_DENIED)
        {
            std::cerr << "[TSH] Yonetici olarak calistirmayi veya SeLoadDriverPrivilege iznini dogrulamayi unutmayin.\n";
        }
    }
} // namespace tsh::user
