#include "classes_view.h"
#include "ui/theme.h"
#include <fmt/format.h>
#include <algorithm>

namespace hype {

void ClassesView::render() {
    ImGui::Begin("Classes");
    if (!rtti_ || rtti_->classes().empty()) {
        ImGui::TextDisabled("No RTTI data (load a C++ binary)");
        ImGui::End();
        return;
    }

    auto& classes = rtti_->classes();
    ImGui::InputTextWithHint("##cf", "Filter classes...", filter_, sizeof(filter_));
    ImGui::SameLine();
    ImGui::TextDisabled("(%d classes)", (int)classes.size());
    ImGui::Separator();

    std::string filt(filter_);
    float left_w = ImGui::GetContentRegionAvail().x * 0.4f;

    ImGui::BeginChild("##class_list", ImVec2(left_w, 0), true);
    for (int i = 0; i < (int)classes.size(); ++i) {
        auto& cls = classes[i];
        if (!filt.empty() && cls.demangled_name.find(filt) == std::string::npos)
            continue;

        bool sel = (i == selected_);
        auto lbl = fmt::format("{}##{}", cls.demangled_name, i);
        if (ImGui::Selectable(lbl.c_str(), sel))
            selected_ = i;
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##class_detail", ImVec2(0, 0), true);
    if (selected_ >= 0 && selected_ < (int)classes.size()) {
        auto& cls = classes[selected_];

        ImGui::TextColored(ImVec4(0.85f, 0.7f, 0.4f, 1), "%s", cls.demangled_name.c_str());
        ImGui::TextDisabled("Mangled: %s", cls.mangled_name.c_str());
        ImGui::Separator();

        auto vtbl_lbl = fmt::format("vtable: {:016X}", cls.vtable);
        if (ImGui::Selectable(vtbl_lbl.c_str()))
            if (nav_) nav_(cls.vtable);

        ImGui::Spacing();
        ImGui::Text("Virtual Methods (%d):", (int)cls.methods.size());
        ImGui::Separator();

        if (ImGui::BeginTable("##methods", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("Idx", ImGuiTableColumnFlags_WidthFixed, 40);
            ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 140);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (int mi = 0; mi < (int)cls.methods.size(); ++mi) {
                va_t maddr = cls.methods[mi];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%d", mi);
                ImGui::TableNextColumn();

                auto albl = fmt::format("{:016X}##m{}_{}", maddr, selected_, mi);
                if (ImGui::Selectable(albl.c_str(), false, ImGuiSelectableFlags_SpanAllColumns))
                    if (nav_) nav_(maddr);

                ImGui::TableNextColumn();
                if (db_) {
                    auto nit = db_->names.find(maddr);
                    if (nit != db_->names.end())
                        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(col::func()), "%s", nit->second.c_str());
                    else
                        ImGui::TextDisabled("sub_%X", (unsigned)(maddr - (db_->image_base ? db_->image_base : 0)));
                }
            }
            ImGui::EndTable();
        }
    } else {
        ImGui::TextDisabled("Select a class");
    }
    ImGui::EndChild();

    ImGui::End();
}

}
