#include "user/ui/ImGuiShell.hpp"

#ifdef TSH_WITH_IMGUI

#include <imgui.h>

namespace tsh::ui
{
    int RunImGuiShell()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(1280.0f, 720.0f);

        ImGui::StyleColorsDark();

        // Placeholder render loop; integrate backend-specific frame pump here.
        ImGui::NewFrame();
        ImGui::Begin("TSH Diagnostics");
        ImGui::TextUnformatted("ImGui front-end initialization stub.");
        ImGui::TextUnformatted("Populate this space with process lists, scan panels, and monitors.");
        ImGui::End();
        ImGui::Render();

        ImGui::DestroyContext();
        return 0;
    }
} // namespace tsh::ui

#endif // TSH_WITH_IMGUI
