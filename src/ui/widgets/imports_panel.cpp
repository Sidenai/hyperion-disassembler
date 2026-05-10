#include "imports_panel.h"
#include "ui/theme.h"
#include <fmt/format.h>

namespace hype {

void ImportsPanel::render() {
    ImGui::Begin("Imports / Exports");
    if (!img_) { ImGui::End(); return; }

    ImGui::InputTextWithHint("##if", "Filter...", filter_, sizeof(filter_));
    std::string filt(filter_);

    if (ImGui::BeginTabBar("##ietab")) {
        if (ImGui::BeginTabItem("Imports")) {
            if (ImGui::BeginTable("##it", 3, ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("IAT", ImGuiTableColumnFlags_WidthFixed, 140);
                ImGui::TableSetupColumn("DLL", ImGuiTableColumnFlags_WidthFixed, 120);
                ImGui::TableSetupColumn("Name");
                ImGui::TableHeadersRow();
                for (auto& imp : img_->imports) {
                    if (!filt.empty() && imp.name.find(filt) == std::string::npos && imp.dll.find(filt) == std::string::npos)
                        continue;
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    auto lbl = fmt::format("{:016X}", imp.iat_addr);
                    if (ImGui::Selectable(lbl.c_str(), false, ImGuiSelectableFlags_SpanAllColumns))
                        if (nav_) nav_(imp.iat_addr);
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", imp.dll.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", imp.name.c_str());
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Exports")) {
            if (ImGui::BeginTable("##et", 3, ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 140);
                ImGui::TableSetupColumn("Ord", ImGuiTableColumnFlags_WidthFixed, 50);
                ImGui::TableSetupColumn("Name");
                ImGui::TableHeadersRow();
                for (auto& exp : img_->exports) {
                    if (!filt.empty() && exp.name.find(filt) == std::string::npos) continue;
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    auto lbl = fmt::format("{:016X}", exp.addr);
                    if (ImGui::Selectable(lbl.c_str(), false, ImGuiSelectableFlags_SpanAllColumns))
                        if (nav_) nav_(exp.addr);
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", exp.ordinal);
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", exp.name.c_str());
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

}
