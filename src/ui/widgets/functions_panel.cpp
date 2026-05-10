#include "functions_panel.h"
#include "ui/theme.h"
#include <fmt/format.h>
#include <algorithm>
#include <vector>

namespace hype {

void FunctionsPanel::render() {
    ImGui::Begin("Functions");
    if (!db_) { ImGui::TextDisabled("No analysis data"); ImGui::End(); return; }

    ImGui::InputTextWithHint("##ff", "Filter (name or address)...", filter_, sizeof(filter_));

    ImGui::SameLine();
    ImGui::TextDisabled("(%d)", (int)db_->funcs.size());
    ImGui::Separator();

    std::string filt(filter_);
    std::vector<const Function*> sorted;
    sorted.reserve(db_->funcs.size());
    for (auto& [_, f] : db_->funcs)
        if (filt.empty() || f.name.find(filt) != std::string::npos ||
            fmt::format("{:X}", f.entry).find(filt) != std::string::npos)
            sorted.push_back(&f);
    std::sort(sorted.begin(), sorted.end(), [](auto* a, auto* b) { return a->entry < b->entry; });

    if (ImGui::BeginTable("##ft", 3,
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp)) {

        ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 145);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Blocks", ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        ImGuiListClipper clip;
        clip.Begin(static_cast<int>(sorted.size()));
        while (clip.Step()) {
            for (int i = clip.DisplayStart; i < clip.DisplayEnd; ++i) {
                auto* f = sorted[i];
                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                auto lbl = fmt::format("{:016X}##f{}", f->entry, i);
                if (ImGui::Selectable(lbl.c_str(), false,
                    ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
                    if (nav_) nav_(f->entry);
                }

                ImGui::TableNextColumn();
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(col::func()), "%s", f->name.c_str());

                ImGui::TableNextColumn();
                ImGui::Text("%d", static_cast<int>(f->blocks.size()));
            }
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

}
