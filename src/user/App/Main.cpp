
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string_view>
#include <cstdint>
#include <vector>

#include "user/DriverChannel.hpp"
#include "user/ProcessManager.hpp"
#include "user/PatternScanner.hpp"
#include "user/ResultRefiner.hpp"
#include "user/ScanSession.hpp"

namespace tsh::app
{
    void InitializeConsole()
    {
        if (!::AllocConsole())
        {
            return;
        }
        FILE* stdStream = nullptr;
        ::freopen_s(&stdStream, "CONOUT$", "w", stdout);
        ::freopen_s(&stdStream, "CONOUT$", "w", stderr);
    }

    namespace
    {
        void PrintStartupBanner()
        {
            constexpr std::string_view banner =
                "[TSH] Native Diagnostic Shell\n"
                "Commands pending implementation: attach, scan, refine, monitor, patch\n";
            std::fwrite(banner.data(), 1, banner.size(), stdout);
        }
    } // namespace

    int Run()
    {
        PrintStartupBanner();

        tsh::user::ProcessManager processManager;
        auto processes = processManager.EnumerateProcesses();
        std::fprintf(stdout, "[TSH] Discovered %zu running processes.\n", processes.size());

        tsh::user::PatternScanner scanner;
        (void)scanner;

        tsh::user::ScanSession session;
        session.Begin({});

        tsh::user::ResultRefiner refiner;
        auto limitedResults = refiner.Limit(session.Results(), 16);
        (void)limitedResults;

        tsh::user::DriverChannel channel;
        if (channel.Open())
        {
            std::fputs("[TSH] Driver channel established.\n", stdout);

            std::uint32_t response = 0;
            if (channel.Ping(0xDEADBEEF, response))
            {
                std::fprintf(stdout, "[TSH] Driver ping response: 0x%08X\n", response);
            }
            else
            {
                std::fputs("[TSH] Driver ping failed.\n", stdout);
            }

            if (!processes.empty())
            {
                std::vector<tsh::protocol::MemoryRegion> regions;
                if (channel.QueryProcessRegions(processes.front().processId, regions))
                {
                    std::fprintf(stdout, "[TSH] Received %zu memory regions from driver.\n", regions.size());
                }
                else
                {
                    std::fputs("[TSH] Memory region query not serviced by driver.\n", stdout);
                }
            }
        }
        else
        {
            std::fputs("[TSH] Driver channel unavailable. Running in offline mode.\n", stdout);
        }

        return 0;
    }
} // namespace tsh::app

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    tsh::app::InitializeConsole();

    try
    {
        return tsh::app::Run();
    }
    catch (const std::exception& ex)
    {
        MessageBoxA(nullptr, ex.what(), "TSH - Fatal Error", MB_ICONERROR | MB_OK);
    }
    catch (...)
    {
        MessageBoxA(nullptr, "Unknown failure.", "TSH - Fatal Error", MB_ICONERROR | MB_OK);
    }
    return EXIT_FAILURE;
}
