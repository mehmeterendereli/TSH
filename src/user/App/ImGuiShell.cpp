#include "user/ui/ImGuiShell.hpp"

#ifdef TSH_WITH_IMGUI

#include <imgui.h>
#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <cstdio>
#include <cstdint>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "shared/Protocol.hpp"
#include "user/DriverChannel.hpp"
#include "user/MemoryAccessor.hpp"
#include "user/PatternScanner.hpp"
#include "user/ProcessManager.hpp"
#include "user/ScanSession.hpp"

namespace tsh::ui
{
    namespace
    {
        struct ImGuiState
        {
            tsh::user::ProcessManager processManager;
            tsh::user::DriverChannel  driverChannel;
            tsh::user::MemoryAccessor accessor;

            std::vector<tsh::user::ProcessInfo> processes;
            std::vector<tsh::protocol::PointerTraceNode> pointerChain;
            std::vector<tsh::protocol::MonitorSample>    monitorSamples;
            std::vector<float>                           monitorGraph;

            bool         driverAvailable{ false };
            std::uint32_t attachedPid{ 0 };
            int          selectedProcess{ -1 };

            char pointerBase[64]{"0x0"};
            char pointerOffsets[256]{"0x0"};
            char monitorConfig[256]{"0x0:4"};
            char patchAddress[64]{"0x0"};
            char patchBytes[256]{"90"};

            std::string statusMessage;
            tsh::protocol::PatchResponse lastPatch{};
            bool patchIssued{ false };
        };

        std::string Trim(const std::string& text)
        {
            const auto first = text.find_first_not_of(" \t\r\n");
            if (first == std::string::npos)
            {
                return {};
            }
            const auto last = text.find_last_not_of(" \t\r\n");
            return text.substr(first, last - first + 1);
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

        std::optional<std::vector<std::intptr_t>> ParseOffsetList(const std::string& literal)
        {
            std::vector<std::intptr_t> offsets;
            std::istringstream stream(literal);
            std::string token;
            while (stream >> token)
            {
                auto offset = ParseOffset(token);
                if (!offset)
                {
                    return std::nullopt;
                }
                offsets.push_back(*offset);
            }
            if (offsets.empty())
            {
                return std::nullopt;
            }
            return offsets;
        }

        std::optional<std::vector<std::byte>> ParseBytes(const std::string& literal)
        {
            std::vector<std::byte> bytes;
            std::istringstream stream(literal);
            std::string token;
            while (stream >> token)
            {
                token = Trim(token);
                if (token.empty())
                {
                    continue;
                }
                try
                {
                    auto value = std::stoul(token, nullptr, (token.rfind("0x", 0) == 0 || token.rfind("0X", 0) == 0) ? 16 : 16);
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

        std::optional<std::vector<tsh::user::MonitorEntry>> ParseMonitor(const std::string& literal)
        {
            std::vector<tsh::user::MonitorEntry> entries;
            std::istringstream stream(literal);
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

                entries.push_back({ *address, size });
            }
            if (entries.empty())
            {
                return std::nullopt;
            }
            return entries;
        }

        void RefreshProcesses(ImGuiState& state)
        {
            state.processes = state.processManager.EnumerateProcesses();
            if (state.processes.empty())
            {
                state.selectedProcess = -1;
            }
            else if (state.selectedProcess >= static_cast<int>(state.processes.size()))
            {
                state.selectedProcess = 0;
            }
        }

        bool Attach(ImGuiState& state, std::uint32_t pid)
        {
            state.accessor.Reset();
            if (!state.processManager.Attach(pid))
            {
                state.statusMessage = "Attach failed.";
                return false;
            }

            state.attachedPid = pid;
            state.accessor.Bind(&state.driverChannel, state.processManager.ProcessHandle(), pid, state.driverAvailable);
            state.statusMessage = "Attached to PID " + std::to_string(pid);
            return true;
        }

        bool ExecutePointer(ImGuiState& state, const std::string& baseLiteral, const std::string& offsetsLiteral)
        {
            auto base = ParseAddress(Trim(baseLiteral));
            if (!base)
            {
                state.statusMessage = "Invalid pointer base.";
                return false;
            }

            auto offsets = ParseOffsetList(offsetsLiteral);
            if (!offsets)
            {
                state.statusMessage = "Invalid offsets.";
                return false;
            }

            std::vector<tsh::protocol::PointerTraceNode> nodes;
            if (!state.driverChannel.PointerTrace(state.attachedPid, *base, *offsets, nodes))
            {
                state.statusMessage = "Pointer trace failed.";
                return false;
            }

            state.pointerChain = std::move(nodes);
            state.statusMessage = "Pointer trace complete.";
            return true;
        }

        bool ExecuteMonitor(ImGuiState& state, const std::string& literal)
        {
            auto entries = ParseMonitor(literal);
            if (!entries)
            {
                state.statusMessage = "Invalid monitor specification.";
                return false;
            }

            std::vector<tsh::protocol::MonitorEntry> protocolEntries;
            protocolEntries.reserve(entries->size());
            for (const auto& entry : *entries)
            {
                protocolEntries.emplace_back(tsh::protocol::MonitorEntry{ entry.address, static_cast<std::uint32_t>(entry.size) });
            }

            std::vector<tsh::protocol::MonitorSample> samples;
            if (state.driverAvailable && state.driverChannel.MonitorSnapshot(state.attachedPid, protocolEntries, samples))
            {
                // ok
            }
            else
            {
                samples.reserve(entries->size());
                std::vector<std::byte> buffer;
                for (const auto& entry : *entries)
                {
                    buffer.resize(entry.size);
                    tsh::protocol::MonitorSample sample{};
                    sample.address = entry.address;
                    sample.requestedSize = static_cast<std::uint32_t>(entry.size);
                    if (state.accessor.Read(entry.address, std::span<std::byte>(buffer.data(), buffer.size())))
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

            state.monitorSamples = std::move(samples);
            state.monitorGraph.clear();
            for (const auto& sample : state.monitorSamples)
            {
                for (std::uint32_t i = 0; i < sample.capturedSize; ++i)
                {
                    state.monitorGraph.push_back(static_cast<float>(std::to_integer<unsigned char>(sample.data[i])));
                }
            }

            state.statusMessage = "Monitor snapshot collected.";
            return true;
        }

        bool ExecutePatch(ImGuiState& state, const std::string& addrLiteral, const std::string& bytesLiteral)
        {
            auto address = ParseAddress(Trim(addrLiteral));
            if (!address)
            {
                state.statusMessage = "Invalid patch address.";
                return false;
            }

            auto bytes = ParseBytes(bytesLiteral);
            if (!bytes)
            {
                state.statusMessage = "Invalid byte sequence.";
                return false;
            }

            tsh::protocol::PatchResponse response{};
            if (!state.driverChannel.ApplyPatch(state.attachedPid, *address, *bytes, 0, response))
            {
                state.statusMessage = "Patch request failed.";
                return false;
            }

            state.lastPatch = response;
            state.patchIssued = true;
            state.statusMessage = "Patch applied with status 0x" + std::to_string(response.status);
            return response.status == ERROR_SUCCESS;
        }

        void DrawProcessPanel(ImGuiState& state)
        {
            if (ImGui::Begin("Processes"))
            {
                if (ImGui::Button("Refresh"))
                {
                    RefreshProcesses(state);
                }
                ImGui::SameLine();
                ImGui::Text("Driver: %s", state.driverAvailable ? "connected" : "offline");

                if (!state.processes.empty())
                {
                    std::vector<const char*> labels;
                    labels.reserve(state.processes.size());
                    for (const auto& proc : state.processes)
                    {
                        labels.push_back(proc.name.c_str());
                    }

                    ImGui::ListBox("##processes", &state.selectedProcess, labels.data(), static_cast<int>(labels.size()), 12);

                    if (state.selectedProcess >= 0 && state.selectedProcess < static_cast<int>(state.processes.size()))
                    {
                        const auto pid = state.processes[static_cast<std::size_t>(state.selectedProcess)].processId;
                        ImGui::Text("Selected PID: %u", pid);
                        if (ImGui::Button("Attach"))
                        {
                            Attach(state, pid);
                        }
                    }
                }
                else
                {
                    ImGui::TextUnformatted("No processes available.");
                }
            }
            ImGui::End();
        }

        void DrawPointerPanel(ImGuiState& state)
        {
            if (ImGui::Begin("Pointer Trace"))
            {
                ImGui::InputText("Base", state.pointerBase, IM_ARRAYSIZE(state.pointerBase));
                ImGui::InputTextMultiline("Offsets", state.pointerOffsets, IM_ARRAYSIZE(state.pointerOffsets), ImVec2(-FLT_MIN, 60.0f));
                if (ImGui::Button("Trace Pointer") && state.attachedPid != 0)
                {
                    ExecutePointer(state, state.pointerBase, state.pointerOffsets);
                }

                if (!state.pointerChain.empty() && ImGui::BeginTable("PointerChain", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                {
                    ImGui::TableSetupColumn("Depth");
                    ImGui::TableSetupColumn("Address");
                    ImGui::TableSetupColumn("Value");
                    ImGui::TableHeadersRow();

                    for (std::size_t i = 0; i < state.pointerChain.size(); ++i)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%zu", i);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("0x%llX", static_cast<unsigned long long>(state.pointerChain[i].address));
                        ImGui::TableSetColumnIndex(2);
                        ImGui::Text("0x%llX", static_cast<unsigned long long>(state.pointerChain[i].value));
                    }
                    ImGui::EndTable();
                }
            }
            ImGui::End();
        }

        void DrawMonitorPanel(ImGuiState& state)
        {
            if (ImGui::Begin("Monitor"))
            {
                ImGui::InputTextMultiline("Entries", state.monitorConfig, IM_ARRAYSIZE(state.monitorConfig), ImVec2(-FLT_MIN, 60.0f));
                if (ImGui::Button("Snapshot") && state.attachedPid != 0)
                {
                    ExecuteMonitor(state, state.monitorConfig);
                }

                if (!state.monitorSamples.empty())
                {
                    if (ImGui::BeginTable("MonitorTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                    {
                        ImGui::TableSetupColumn("Address");
                        ImGui::TableSetupColumn("Size");
                        ImGui::TableSetupColumn("Status");
                        ImGui::TableSetupColumn("Data");
                        ImGui::TableHeadersRow();
                        for (const auto& sample : state.monitorSamples)
                        {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::Text("0x%llX", static_cast<unsigned long long>(sample.address));
                            ImGui::TableSetColumnIndex(1);
                            ImGui::Text("%u", sample.capturedSize);
                            ImGui::TableSetColumnIndex(2);
                            ImGui::Text("0x%X", sample.status);
                            ImGui::TableSetColumnIndex(3);

                            std::string preview;
                            preview.reserve(sample.capturedSize * 3);
                            for (std::uint32_t i = 0; i < sample.capturedSize && i < 16; ++i)
                            {
                                char buf[4];
                                std::snprintf(buf, sizeof(buf), "%02X ", static_cast<unsigned int>(std::to_integer<unsigned char>(sample.data[i])));
                                preview += buf;
                            }
                            ImGui::TextUnformatted(preview.c_str());
                        }
                        ImGui::EndTable();
                    }

                    if (!state.monitorGraph.empty())
                    {
                        ImGui::PlotLines("Sample Bytes", state.monitorGraph.data(), static_cast<int>(state.monitorGraph.size()), 0, nullptr, 0.0f, 255.0f, ImVec2(-FLT_MIN, 80.0f));
                    }
                }
            }
            ImGui::End();
        }

        void DrawPatchPanel(ImGuiState& state)
        {
            if (ImGui::Begin("Patch"))
            {
                ImGui::InputText("Address", state.patchAddress, IM_ARRAYSIZE(state.patchAddress));
                ImGui::InputTextMultiline("Bytes", state.patchBytes, IM_ARRAYSIZE(state.patchBytes), ImVec2(-FLT_MIN, 60.0f));
                if (ImGui::Button("Apply Patch") && state.attachedPid != 0 && state.driverAvailable)
                {
                    ExecutePatch(state, state.patchAddress, state.patchBytes);
                }

                if (state.patchIssued)
                {
                    ImGui::Text("Last patch status: 0x%X, bytes=%u", state.lastPatch.status, state.lastPatch.bytesWritten);
                }
            }
            ImGui::End();
        }

        void DrawStatusPanel(ImGuiState& state)
        {
            if (ImGui::Begin("Status"))
            {
                if (!state.statusMessage.empty())
                {
                    ImGui::TextUnformatted(state.statusMessage.c_str());
                }
                else
                {
                    ImGui::TextUnformatted("Ready.");
                }
                if (ImGui::Button("Close"))
                {
                    ImGui::GetIO().WantQuit = true;
                }
            }
            ImGui::End();
        }
    } // namespace

    int RunImGuiShell()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(1280.0f, 720.0f);
        io.DeltaTime = 1.0f / 60.0f;

        ImGui::StyleColorsDark();

        ImGuiState state;
        state.driverAvailable = state.driverChannel.Open();
        RefreshProcesses(state);

        bool running = true;
        int frameCount = 0;
        while (running)
        {
            ImGui::NewFrame();

            DrawProcessPanel(state);
            DrawPointerPanel(state);
            DrawMonitorPanel(state);
            DrawPatchPanel(state);
            DrawStatusPanel(state);

            ImGui::Render();

            // Without a platform backend we terminate after first frame.
            ++frameCount;
            if (io.WantQuit || frameCount > 0)
            {
                running = false;
            }
        }

        ImGui::DestroyContext();
        return 0;
    }
} // namespace tsh::ui

#endif // TSH_WITH_IMGUI
