#include "imports_panel.h"
#include "ui/theme.h"
#include <fmt/format.h>

namespace hype {

va_t ImportsPanel::find_caller(va_t iat_addr) {
    if (!db_) return 0;

    // priority 1: find a CALL instruction whose memory operand resolves to this IAT addr
    auto xit = db_->xrefs_to.find(iat_addr);
    if (xit != db_->xrefs_to.end()) {
        for (auto& xr : xit->second) {
            auto iit = db_->insns.find(xr.from);
            if (iit != db_->insns.end() && iit->second.is_call())
                return xr.from;
        }
        // priority 2: any code xref (not just calls)
        for (auto& xr : xit->second) {
            if (db_->insns.count(xr.from))
                return xr.from;
        }
    }

    // priority 3: brute force scan for call with matching operand
    for (auto& [addr, insn] : db_->insns) {
        if (!insn.is_call()) continue;
        for (int k = 0; k < insn.op_count; ++k) {
            if (insn.ops[k].type == OpType::Mem && insn.ops[k].val == iat_addr)
                return addr;
            if (insn.ops[k].type == OpType::Imm && insn.ops[k].val == iat_addr)
                return addr;
        }
    }
    return 0;
}

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
                for (int i = 0; i < (int)img_->imports.size(); ++i) {
                    auto& imp = img_->imports[i];
                    if (!filt.empty() && imp.name.find(filt) == std::string::npos && imp.dll.find(filt) == std::string::npos)
                        continue;
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    auto lbl = fmt::format("{:016X}##i{}", imp.iat_addr, i);
                    if (ImGui::Selectable(lbl.c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                        if (nav_) {
                            va_t caller = find_caller(imp.iat_addr);
                            if (caller)
                                nav_(caller);
                        }
                    }
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
                for (int i = 0; i < (int)img_->exports.size(); ++i) {
                    auto& exp = img_->exports[i];
                    if (!filt.empty() && exp.name.find(filt) == std::string::npos) continue;
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    auto lbl = fmt::format("{:016X}##e{}", exp.addr, i);
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
