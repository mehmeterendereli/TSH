#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <exception>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <sstream>
#include <string>
#include <string_view>
#include <span>
#include <vector>

#include "shared/Protocol.hpp"
#include "user/DriverChannel.hpp"
#include "user/MemoryAccessor.hpp"
#include "user/PatternScanner.hpp"
#include "user/ProcessManager.hpp"
#include "user/ResultRefiner.hpp"
#include "user/ScanSession.hpp"
#include "user/ValueMonitor.hpp"
#ifdef TSH_WITH_IMGUI
#include "user/ui/ImGuiShell.hpp"
#endif

namespace tsh::app
{
    namespace
    {
        using MonitorEntry = tsh::user::MonitorEntry;

        struct AppContext
        {
            tsh::user::ProcessManager processManager;
            tsh::user::DriverChannel  driverChannel;
            tsh::user::PatternScanner scanner;
            tsh::user::ResultRefiner  refiner;
            tsh::user::ScanSession    session;
            tsh::user::MemoryAccessor accessor;

            std::vector<tsh::protocol::PointerTraceNode> pointerChain;
            std::vector<tsh::protocol::MonitorSample>    monitorSamples;

            bool                     driverAvailable{ false };
            std::uint32_t            attachedPid{ 0 };
            std::wstring             attachedName;
        };

        void SetupConsole()
        {
            if (::AllocConsole())
            {
                FILE* stdStream = nullptr;
                ::freopen_s(&stdStream, "CONOUT$", "w", stdout);
                ::freopen_s(&stdStream, "CONOUT$", "w", stderr);
                ::freopen_s(&stdStream, "CONIN$", "r", stdin);
            }

            ::SetConsoleOutputCP(CP_UTF8);
            ::SetConsoleCP(CP_UTF8);
            std::ios::sync_with_stdio(false);
            std::cin.tie(nullptr);
        }

        void PrintBanner()
        {
            std::cout
                << "[TSH] Native Diagnostic Shell\n"
                << "Type 'help' for command reference.\n\n";
        }

        std::string Trim(const std::string& input)
        {
            const auto first = input.find_first_not_of(" \t\r\n");
            if (first == std::string::npos)
            {
                return {};
            }
            const auto last = input.find_last_not_of(" \t\r\n");
            return input.substr(first, last - first + 1);
        }

        std::string ToLower(std::string value)
        {
            for (char& ch : value)
            {
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
            return value;
        }

        std::optional<tsh::user::ScanDataType> ParseDataType(const std::string& token)
        {
            const std::string lowered = ToLower(token);
            if (lowered == "int" || lowered == "int32")
            {
                return tsh::user::ScanDataType::Int32;
            }
            if (lowered == "uint" || lowered == "uint32")
            {
                return tsh::user::ScanDataType::UInt32;
            }
            if (lowered == "float")
            {
                return tsh::user::ScanDataType::Float;
            }
            if (lowered == "ascii" || lowered == "string")
            {
                return tsh::user::ScanDataType::AsciiString;
            }
            if (lowered == "utf16" || lowered == "wide")
            {
                return tsh::user::ScanDataType::Utf16String;
            }
            if (lowered == "bytes" || lowered == "hex")
            {
                return tsh::user::ScanDataType::BytePattern;
            }
            return std::nullopt;
        }

        template <typename T>
        std::vector<std::byte> ToBytes(const T& value)
        {
            std::vector<std::byte> buffer(sizeof(T));
            std::memcpy(buffer.data(), &value, sizeof(T));
            return buffer;
        }

        std::vector<std::byte> AsciiToBytes(const std::string& value)
        {
            std::vector<std::byte> buffer(value.size());
            std::memcpy(buffer.data(), value.data(), value.size());
            return buffer;
        }

        std::vector<std::byte> Utf16ToBytes(const std::string& value)
        {
            if (value.empty())
            {
                return {};
            }

            const int required = ::MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
            if (required <= 0)
            {
                return {};
            }

            std::wstring wide(static_cast<std::size_t>(required), L'\0');
            const int written = ::MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), wide.data(), required);
            if (written <= 0)
            {
                return {};
            }

            const std::size_t byteCount = static_cast<std::size_t>(written) * sizeof(wchar_t);
            std::vector<std::byte> buffer(byteCount);
            std::memcpy(buffer.data(), wide.data(), byteCount);
            return buffer;
        }

        std::optional<std::vector<std::byte>> HexToBytes(const std::string& input)
        {
            std::vector<std::byte> bytes;
            std::istringstream stream(input);
            std::string token;

            while (stream >> token)
            {
                const std::string cleaned = Trim(token);
                if (cleaned.empty())
                {
                    continue;
                }

                unsigned int value = 0;
                std::stringstream converter;
                converter << std::hex << cleaned;
                if (!(converter >> value) || value > 0xFFu)
                {
                    return std::nullopt;
                }
                bytes.push_back(static_cast<std::byte>(value & 0xFFu));
            }

            return bytes;
        }

        std::optional<std::vector<std::byte>> BuildPattern(tsh::user::ScanDataType type, const std::string& literal)
        {
            try
            {
                switch (type)
                {
                    case tsh::user::ScanDataType::Int32:
                    {
                        const auto value = static_cast<std::int32_t>(std::stol(literal));
                        return ToBytes(value);
                    }
                    case tsh::user::ScanDataType::UInt32:
                    {
                        const auto value = static_cast<std::uint32_t>(std::stoul(literal));
                        return ToBytes(value);
                    }
                    case tsh::user::ScanDataType::Float:
                    {
                        const float value = std::stof(literal);
                        return ToBytes(value);
                    }
                    case tsh::user::ScanDataType::AsciiString:
                        return AsciiToBytes(literal);
                    case tsh::user::ScanDataType::Utf16String:
                    {
                        auto bytes = Utf16ToBytes(literal);
                        if (bytes.empty())
                        {
                            return std::nullopt;
                        }
                        return bytes;
                    }
                    case tsh::user::ScanDataType::BytePattern:
                        return HexToBytes(literal);
                }
            }
            catch (const std::exception&)
            {
                return std::nullopt;
            }

            return std::nullopt;
        }

        std::vector<tsh::user::ScanRegion> FallbackQueryRegions(const tsh::user::MemoryAccessor& accessor)
        {
            std::vector<tsh::user::ScanRegion> regions;

            HANDLE processHandle = accessor.ProcessHandle();
            if (!processHandle)
            {
                return regions;
            }

            MEMORY_BASIC_INFORMATION mbi{};
            std::byte* address = nullptr;

            while (::VirtualQueryEx(processHandle, address, &mbi, sizeof(mbi)) == sizeof(mbi))
            {
                const bool committed = (mbi.State == MEM_COMMIT);
                const bool accessible = !(mbi.Protect & PAGE_GUARD) && !(mbi.Protect & PAGE_NOACCESS);

                if (committed && accessible && mbi.RegionSize > 0)
                {
                    tsh::user::ScanRegion region{};
                    region.baseAddress = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
                    region.size = mbi.RegionSize;
                    regions.push_back(region);
                }

                auto* next = reinterpret_cast<std::byte*>(mbi.BaseAddress) + mbi.RegionSize;
                if (next <= address)
                {
                    break;
                }
                address = next;
            }

            return regions;
        }

        std::vector<tsh::user::ScanRegion> CollectRegions(AppContext& context)
        {
            std::vector<tsh::user::ScanRegion> regions;

            if (context.driverAvailable)
            {
                std::vector<tsh::protocol::MemoryRegion> driverRegions;
                if (context.driverChannel.QueryProcessRegions(context.attachedPid, driverRegions))
                {
                    regions.reserve(driverRegions.size());
                    for (const auto& entry : driverRegions)
                    {
                        if (entry.state != MEM_COMMIT || entry.size == 0)
                        {
                            continue;
                        }

                        tsh::user::ScanRegion region{};
                        region.baseAddress = entry.baseAddress;
                        region.size = entry.size;
                        regions.push_back(region);
                    }
                    return regions;
                }

                std::cout << "[warn] Driver region query failed; falling back to Win32 enumeration.\n";
            }

            regions = FallbackQueryRegions(context.accessor);
            return regions;
        }

        void PrintProcesses(const std::vector<tsh::user::ProcessInfo>& processes, std::size_t limit)
        {
            std::cout << "PID\tName\n";
            std::cout << "-------------------------------\n";

            const std::size_t displayCount = std::min(limit, processes.size());
            for (std::size_t index = 0; index < displayCount; ++index)
            {
                const auto& proc = processes[index];
                std::wcout << proc.processId << L"\t" << proc.name << L"\n";
            }
            if (processes.size() > displayCount)
            {
                std::cout << "... (" << (processes.size() - displayCount) << " more)\n";
            }
        }

        void ShowHelp()
        {
            std::cout
                << "Commands:\n"
                << "  help                 Show this message\n"
                << "  list                 Enumerate running processes\n"
                << "  attach <pid>         Attach to target process (requires permissions)\n"
                << "  regions              List cached region count for current process\n"
                << "  scan <type> <value>  Start new scan (types: int32,uint32,float,ascii,utf16,bytes)\n"
                << "  refine <value>       Filter existing hits with updated value\n"
                << "  results [count]      Display first <count> results (default 16)\n"
                << "  pointer <addr> <offsets...>  Trace pointer chain (hex or decimal inputs)\n"
                << "  monitor <addr[:size],...>    Snapshot addresses (size default 4)\n"
                << "  patch <addr> <hex-bytes>     Apply raw bytes via driver patch interface\n"
                << "  clear                Reset current scan session\n"
                << "  driver               Display driver availability status\n"
                << "  quit/exit            Leave the shell\n";
        }

        void ShowDriverStatus(const AppContext& context)
        {
            std::cout << "[driver] status: " << (context.driverAvailable ? "connected" : "unavailable") << '\n';
        }

        void ShowRegionsSummary(const AppContext& context)
        {
            if (!context.session.IsActive())
            {
                std::cout << "[info] No active scan session.\n";
                return;
            }

            std::cout << "[info] Cached regions: " << context.session.Regions().size() << '\n';
        }

        void ShowResults(const AppContext& context, std::size_t limit)
        {
            if (!context.session.IsActive())
            {
                std::cout << "[info] No active scan session.\n";
                return;
            }

            const auto& hits = context.session.Results();
            if (hits.empty())
            {
                std::cout << "[info] No hits recorded.\n";
                return;
            }

            const std::size_t count = std::min(limit, hits.size());
            const auto type = context.session.Type();
            const auto pattern = context.session.Pattern();

            std::vector<std::byte> valueBuffer(pattern.begin(), pattern.end());

            std::cout << "Index\tAddress\tValue\n";
            std::cout << "----------------------------------------\n";

            for (std::size_t index = 0; index < count; ++index)
            {
                const auto address = hits[index].address;
                std::cout << index << '\t' << "0x" << std::hex << std::uppercase << address << std::dec << '\t';

                if (context.accessor.IsBound() && !valueBuffer.empty())
                {
                    if (context.accessor.Read(address, std::span<std::byte>(valueBuffer.data(), valueBuffer.size())))
                    {
                        switch (type)
                        {
                            case tsh::user::ScanDataType::Int32:
                            {
                                std::int32_t current{};
                                std::memcpy(&current, valueBuffer.data(), sizeof(current));
                                std::cout << current;
                                break;
                            }
                            case tsh::user::ScanDataType::UInt32:
                            {
                                std::uint32_t current{};
                                std::memcpy(&current, valueBuffer.data(), sizeof(current));
                                std::cout << current;
                                break;
                            }
                            case tsh::user::ScanDataType::Float:
                            {
                                float current{};
                                std::memcpy(&current, valueBuffer.data(), sizeof(current));
                                std::cout << current;
                                break;
                            }
                            case tsh::user::ScanDataType::AsciiString:
                            {
                                std::string text(valueBuffer.size(), '\0');
                                std::memcpy(text.data(), valueBuffer.data(), valueBuffer.size());
                                std::cout << '"' << Trim(text) << '"';
                                break;
                            }
                            case tsh::user::ScanDataType::Utf16String:
                                std::cout << "<utf16 bytes>";
                                break;
                            case tsh::user::ScanDataType::BytePattern:
                                std::cout << "<bytes>";
                                break;
                        }
                    }
                    else
                    {
                        std::cout << "<unreadable>";
                    }
                }
                else
                {
                    std::cout << '-';
                }

                std::cout << '\n';
            }

            if (hits.size() > count)
            {
                std::cout << "... (" << (hits.size() - count) << " more)\n";
            }
        }

        void ResetSession(AppContext& context, bool quiet)
        {
            context.session.Reset();
            if (!quiet)
            {
                std::cout << "[info] Scan session cleared.\n";
            }
        }

        bool AttachProcess(AppContext& context, std::uint32_t pid)
        {
            if (pid == 0)
            {
                std::cout << "[warn] Invalid PID.\n";
                return false;
            }

            context.accessor.Reset();
            context.pointerChain.clear();
            context.monitorSamples.clear();

            if (!context.processManager.Attach(pid))
            {
                std::cout << "[error] Unable to attach to PID " << pid << ".\n";
                return false;
            }

            context.attachedPid = pid;
            context.attachedName.clear();

            const auto processes = context.processManager.EnumerateProcesses();
            for (const auto& proc : processes)
            {
                if (proc.processId == pid)
                {
                    context.attachedName = proc.name;
                    break;
                }
            }

            context.accessor.Bind(&context.driverChannel, context.processManager.ProcessHandle(), pid, context.driverAvailable);

            ResetSession(context, true);
            std::wcout << L"[info] Attached to PID " << pid << L" (" << (context.attachedName.empty() ? L"unknown" : context.attachedName.c_str()) << L")\n";
            return true;
        }

        void ExecuteScan(AppContext& context, tsh::user::ScanDataType type, const std::vector<std::byte>& pattern)
        {
            if (context.attachedPid == 0 || !context.accessor.IsBound())
            {
                std::cout << "[warn] Attach to a process first.\n";
                return;
            }

            if (pattern.empty())
            {
                std::cout << "[warn] Pattern cannot be empty.\n";
                return;
            }

            auto regions = CollectRegions(context);
            if (regions.empty())
            {
                std::cout << "[warn] No readable regions discovered.\n";
                return;
            }

            auto hits = context.scanner.ExecuteInitialScan(
                context.accessor,
                type,
                std::span<const std::byte>(pattern.data(), pattern.size()),
                regions);

            std::vector<std::byte> patternCopy(pattern.begin(), pattern.end());
            context.session.Begin(regions, type, std::move(patternCopy));
            context.session.UpdateResults(std::move(hits));

            std::cout << "[info] Scan complete. Hits: " << context.session.Results().size() << '\n';
        }

        std::optional<std::uintptr_t> ParseAddress(const std::string& token)
        {
            try
            {
                if (token.rfind("0x", 0) == 0 || token.rfind("0X", 0) == 0)
                {
                    return static_cast<std::uintptr_t>(std::stoull(token, nullptr, 16));
                }
                return static_cast<std::uintptr_t>(std::stoull(token, nullptr, 10));
            }
            catch (const std::exception&)
            {
                return std::nullopt;
            }
        }

        std::optional<std::intptr_t> ParseOffset(const std::string& token)
        {
            try
            {
                if (token.rfind("0x", 0) == 0 || token.rfind("0X", 0) == 0)
                {
                    return static_cast<std::intptr_t>(std::stoll(token, nullptr, 16));
                }
                return static_cast<std::intptr_t>(std::stoll(token, nullptr, 10));
            }
            catch (const std::exception&)
            {
                return std::nullopt;
            }
        }

        std::optional<std::vector<MonitorEntry>> ParseMonitorEntries(const std::string& input)
        {
            std::vector<MonitorEntry> entries;
            std::istringstream stream(input);
            std::string token;

            while (std::getline(stream, token, ','))
            {
                token = Trim(token);
                if (token.empty())
                {
                    continue;
                }

                const auto colon = token.find(':');
                const std::string addrPart = token.substr(0, colon);
                const std::string sizePart = (colon != std::string::npos) ? token.substr(colon + 1) : "";

                auto address = ParseAddress(addrPart);
                if (!address)
                {
                    return std::nullopt;
                }

                std::size_t size = 4;
                if (!sizePart.empty())
                {
                    try
                    {
                        size = static_cast<std::size_t>(std::stoul(sizePart, nullptr, 0));
                    }
                    catch (const std::exception&)
                    {
                        return std::nullopt;
                    }
                }

                entries.push_back(MonitorEntry{ *address, size });
            }

            if (entries.empty())
            {
                return std::nullopt;
            }
            return entries;
        }

        std::optional<std::vector<std::byte>> ParseHexBytes(const std::string& literal)
        {
            std::istringstream stream(literal);
            std::string token;
            std::vector<std::byte> bytes;

            while (stream >> token)
            {
                token = Trim(token);
                if (token.empty())
                {
                    continue;
                }

                try
                {
                    unsigned long value = std::stoul(token, nullptr, (token.rfind("0x", 0) == 0 || token.rfind("0X", 0) == 0) ? 16 : 16);
                    if (value > 0xFFul)
                    {
                        return std::nullopt;
                    }
                    bytes.push_back(static_cast<std::byte>(value & 0xFFu));
                }
                catch (const std::exception&)
                {
                    return std::nullopt;
                }
            }

            if (bytes.empty())
            {
                return std::nullopt;
            }
            return bytes;
        }

        bool ExecutePointerTrace(AppContext& context, std::uintptr_t baseAddress, const std::vector<std::intptr_t>& offsets)
        {
            if (!context.driverAvailable || !context.accessor.IsBound())
            {
                std::cout << "[warn] Pointer trace requires driver connection.\n";
                return false;
            }

            std::vector<tsh::protocol::PointerTraceNode> nodes;
            if (!context.driverChannel.PointerTrace(context.attachedPid, baseAddress, offsets, nodes))
            {
                std::cout << "[error] Pointer trace failed.\n";
                return false;
            }

            context.pointerChain = nodes;

            std::cout << "Depth\tAddress\t\tValue\n";
            std::cout << "----------------------------------------\n";
            for (std::size_t i = 0; i < nodes.size(); ++i)
            {
                const auto& node = nodes[i];
                std::cout << i << "\t0x" << std::hex << std::uppercase << node.address << "\t0x" << node.value << std::dec << '\n';
            }

            return true;
        }

        bool ExecuteMonitorSnapshot(AppContext& context, const std::vector<MonitorEntry>& entries)
        {
            if (!context.accessor.IsBound())
            {
                std::cout << "[warn] Attach to a process first.\n";
                return false;
            }

            std::vector<tsh::protocol::MonitorEntry> protocolEntries;
            protocolEntries.reserve(entries.size());
            for (const auto& entry : entries)
            {
                protocolEntries.push_back(tsh::protocol::MonitorEntry{ entry.address, static_cast<std::uint32_t>(entry.size) });
            }

            std::vector<tsh::protocol::MonitorSample> samples;
            if (context.driverAvailable && context.driverChannel.MonitorSnapshot(context.attachedPid, protocolEntries, samples))
            {
                // Driver snapshot succeeded.
            }
            else
            {
                samples.reserve(entries.size());
                std::vector<std::byte> buffer;
                for (const auto& entry : entries)
                {
                    buffer.resize(entry.size);
                    tsh::protocol::MonitorSample sample{};
                    sample.address = entry.address;
                    sample.requestedSize = static_cast<std::uint32_t>(entry.size);

                    if (context.accessor.Read(entry.address, std::span<std::byte>(buffer.data(), buffer.size())))
                    {
                        sample.capturedSize = static_cast<std::uint32_t>(buffer.size());
                        sample.status = ERROR_SUCCESS;
                        sample.data.assign(buffer.begin(), buffer.end());
                    }
                    else
                    {
                        sample.capturedSize = 0;
                        sample.status = ERROR_PARTIAL_COPY;
                    }

                    samples.push_back(std::move(sample));
                }
            }

            context.monitorSamples = samples;

            std::cout << "Addr\tSize\tStatus\tData\n";
            std::cout << "----------------------------------------------\n";
            for (const auto& sample : samples)
            {
                std::cout << "0x" << std::hex << std::uppercase << sample.address << std::dec
                          << "\t" << sample.capturedSize
                          << "\t0x" << std::hex << sample.status << std::dec << "\t";

                for (std::uint32_t i = 0; i < sample.capturedSize; ++i)
                {
                    std::cout << std::hex << std::setw(2) << std::setfill('0')
                              << static_cast<unsigned int>(std::to_integer<unsigned char>(sample.data[i])) << ' ';
                }
                std::cout << std::dec << std::setfill(' ') << '\n';
            }

            return true;
        }

        bool ExecutePatch(AppContext& context, std::uintptr_t address, const std::vector<std::byte>& bytes)
        {
            if (!context.driverAvailable || !context.accessor.IsBound())
            {
                std::cout << "[warn] Patch requires driver access.\n";
                return false;
            }

            tsh::protocol::PatchResponse response{};
            if (!context.driverChannel.ApplyPatch(context.attachedPid, address, bytes, 0, response))
            {
                std::cout << "[error] Patch request failed.\n";
                return false;
            }

            std::cout << "[patch] status=0x" << std::hex << std::uppercase << response.status
                      << " bytes_written=" << std::dec << response.bytesWritten << '\n';

            return response.status == ERROR_SUCCESS;
        }

        void ExecuteRefine(AppContext& context, const std::vector<std::byte>& pattern)
        {
            if (!context.session.IsActive() || context.session.Results().empty())
            {
                std::cout << "[warn] No active results to refine.\n";
                return;
            }

            if (pattern.empty())
            {
                std::cout << "[warn] Pattern cannot be empty.\n";
                return;
            }

            auto refined = context.scanner.RefineHits(
                context.accessor,
                context.session.Type(),
                std::span<const std::byte>(pattern.data(), pattern.size()),
                context.session.Results());

            context.session.UpdateResults(std::move(refined));

            std::cout << "[info] Refine complete. Hits: " << context.session.Results().size() << '\n';
        }

        void ProcessCommand(AppContext& context, const std::string& line)
        {
            std::istringstream stream(line);
            std::string verb;
            stream >> verb;
            const std::string command = ToLower(verb);

            if (command == "help" || command == "?")
            {
                ShowHelp();
                return;
            }

            if (command == "list")
            {
                const auto processes = context.processManager.EnumerateProcesses();
                std::cout << "[info] " << processes.size() << " processes discovered.\n";
                PrintProcesses(processes, 32);
                return;
            }

            if (command == "attach")
            {
                std::uint32_t pid = 0;
                if (!(stream >> pid))
                {
                    std::cout << "[warn] Usage: attach <pid>\n";
                    return;
                }
                AttachProcess(context, pid);
                return;
            }

            if (command == "regions")
            {
                ShowRegionsSummary(context);
                return;
            }

            if (command == "driver")
            {
                ShowDriverStatus(context);
                return;
            }

            if (command == "scan")
            {
                std::string typeToken;
                if (!(stream >> typeToken))
                {
                    std::cout << "[warn] Usage: scan <type> <value>\n";
                    return;
                }

                auto type = ParseDataType(typeToken);
                if (!type)
                {
                    std::cout << "[warn] Unknown data type '" << typeToken << "'.\n";
                    return;
                }

                std::string valueLiteral;
                std::getline(stream, valueLiteral);
                valueLiteral = Trim(valueLiteral);
                if (valueLiteral.empty())
                {
                    std::cout << "[warn] Value cannot be empty.\n";
                    return;
                }

                auto pattern = BuildPattern(*type, valueLiteral);
                if (!pattern || pattern->empty())
                {
                    std::cout << "[warn] Failed to build pattern from value.\n";
                    return;
                }

                ExecuteScan(context, *type, *pattern);
                return;
            }

            if (command == "refine")
            {
                if (!context.session.IsActive())
                {
                    std::cout << "[warn] No active scan session.\n";
                    return;
                }

                std::string valueLiteral;
                std::getline(stream, valueLiteral);
                valueLiteral = Trim(valueLiteral);
                if (valueLiteral.empty())
                {
                    std::cout << "[warn] Usage: refine <value>\n";
                    return;
                }

                auto pattern = BuildPattern(context.session.Type(), valueLiteral);
                if (!pattern)
                {
                    std::cout << "[warn] Failed to build pattern from value.\n";
                    return;
                }

                ExecuteRefine(context, *pattern);
                return;
            }

            if (command == "results")
            {
                std::size_t limit = 16;
                if (!(stream >> limit))
                {
                    limit = 16;
                }
                ShowResults(context, limit);
                return;
            }

            if (command == "pointer")
            {
                std::string baseToken;
                if (!(stream >> baseToken))
                {
                    std::cout << "[warn] Usage: pointer <address> <offsets...>\n";
                    return;
                }

                auto base = ParseAddress(baseToken);
                if (!base)
                {
                    std::cout << "[warn] Invalid base address.\n";
                    return;
                }

                std::vector<std::intptr_t> offsets;
                std::string offsetToken;
                while (stream >> offsetToken)
                {
                    auto offset = ParseOffset(offsetToken);
                    if (!offset)
                    {
                        std::cout << "[warn] Invalid offset '" << offsetToken << "'.\n";
                        return;
                    }
                    offsets.push_back(*offset);
                }

                if (offsets.empty())
                {
                    std::cout << "[warn] Provide at least one offset.\n";
                    return;
                }

                ExecutePointerTrace(context, *base, offsets);
                return;
            }

            if (command == "monitor")
            {
                std::string args;
                std::getline(stream, args);
                args = Trim(args);
                if (args.empty())
                {
                    std::cout << "[warn] Usage: monitor <addr[:size],...>\n";
                    return;
                }

                auto entries = ParseMonitorEntries(args);
                if (!entries)
                {
                    std::cout << "[warn] Invalid monitor entry list.\n";
                    return;
                }

                ExecuteMonitorSnapshot(context, *entries);
                return;
            }

            if (command == "patch")
            {
                std::string addrToken;
                if (!(stream >> addrToken))
                {
                    std::cout << "[warn] Usage: patch <address> <hex bytes>\n";
                    return;
                }

                auto address = ParseAddress(addrToken);
                if (!address)
                {
                    std::cout << "[warn] Invalid address.\n";
                    return;
                }

                std::string byteLiteral;
                std::getline(stream, byteLiteral);
                byteLiteral = Trim(byteLiteral);
                if (byteLiteral.empty())
                {
                    std::cout << "[warn] Provide byte sequence.\n";
                    return;
                }

                auto bytes = ParseHexBytes(byteLiteral);
                if (!bytes)
                {
                    std::cout << "[warn] Invalid byte sequence.\n";
                    return;
                }

                ExecutePatch(context, *address, *bytes);
                return;
            }

            if (command == "clear")
            {
                ResetSession(context, false);
                return;
            }

            if (command == "quit" || command == "exit")
            {
                throw std::runtime_error("exit");
            }

            std::cout << "[warn] Unknown command. Type 'help' for a list of commands.\n";
        }
    } // namespace

    void InitializeConsole()
    {
        SetupConsole();
    }

    int Run()
    {
        AppContext context;

        context.driverAvailable = context.driverChannel.Open();
        if (context.driverAvailable)
        {
            std::uint32_t response = 0;
            if (context.driverChannel.Ping(0xDEADBEEF, response))
            {
                std::cout << "[driver] ping 0xDEADBEEF -> 0x" << std::hex << std::uppercase << response << std::dec << '\n';
            }
            else
            {
                std::cout << "[driver] ping failed; continuing in degraded mode.\n";
            }
        }
        else
        {
            std::cout << "[driver] unavailable. Falling back to Win32 APIs where possible.\n";
        }

        PrintBanner();

        std::string line;
        while (true)
        {
            std::cout << "tsh> ";
            if (!std::getline(std::cin, line))
            {
                break;
            }

            line = Trim(line);
            if (line.empty())
            {
                continue;
            }

            try
            {
                ProcessCommand(context, line);
            }
            catch (const std::runtime_error& ex)
            {
                if (std::string_view(ex.what()) == "exit")
                {
                    break;
                }
                throw;
            }
        }

        return 0;
    }
} // namespace tsh::app

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
#ifdef TSH_WITH_IMGUI
    int argc = 0;
    bool useImGui = false;
    LPWSTR* argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
    if (argv != nullptr)
    {
        for (int i = 0; i < argc; ++i)
        {
            if (_wcsicmp(argv[i], L"--imgui") == 0 || _wcsicmp(argv[i], L"-imgui") == 0)
            {
                useImGui = true;
                break;
            }
        }
        ::LocalFree(argv);
    }

    if (useImGui)
    {
        try
        {
            return tsh::ui::RunImGuiShell();
        }
        catch (const std::exception& ex)
        {
            MessageBoxA(nullptr, ex.what(), "TSH - ImGui Error", MB_ICONERROR | MB_OK);
            return EXIT_FAILURE;
        }
        catch (...)
        {
            MessageBoxA(nullptr, "Unknown ImGui failure.", "TSH - ImGui Error", MB_ICONERROR | MB_OK);
            return EXIT_FAILURE;
        }
    }
#endif

    try
    {
        tsh::app::InitializeConsole();
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
