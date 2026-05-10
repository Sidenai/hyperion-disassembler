#include "types_panel.h"
#include <fmt/format.h>
#include <algorithm>

namespace hype {

void TypesPanel::render() {
    if (!ImGui::Begin("Types")) { ImGui::End(); return; }
    if (!db_) { ImGui::Text("No analysis loaded"); ImGui::End(); return; }

    float w = ImGui::GetContentRegionAvail().x;
    ImGui::BeginChild("##typelist", ImVec2(w * 0.35f, 0), ImGuiChildFlags_Borders);
    render_list();
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##typedetail", ImVec2(0, 0), ImGuiChildFlags_Borders);
    render_detail();
    ImGui::EndChild();

    ImGui::End();
}

void TypesPanel::render_list() {
    ImGui::InputTextWithHint("##filt", "Filter...", filter_, sizeof(filter_));
    ImGui::Separator();

    if (ImGui::Button("New Struct")) ImGui::OpenPopup("##newstruct");
    ImGui::SameLine();
    if (ImGui::Button("New Enum")) ImGui::OpenPopup("##newenum");
    ImGui::SameLine();
    if (ImGui::Button("Delete") && selected_) {
        db_->types.remove(selected_);
        selected_ = 0;
    }

    if (ImGui::BeginPopup("##newstruct")) {
        ImGui::InputText("Name", new_name_, sizeof(new_name_));
        ImGui::InputInt("Size", new_size_);
        if (ImGui::Button("Create") && new_name_[0]) {
            selected_ = db_->types.add_struct(new_name_, static_cast<u32>(new_size_[0]));
            new_name_[0] = 0; new_size_[0] = 64;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("##newenum")) {
        ImGui::InputText("Name", new_name_, sizeof(new_name_));
        if (ImGui::Button("Create") && new_name_[0]) {
            selected_ = db_->types.add_enum(new_name_);
            new_name_[0] = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();
    std::string fstr(filter_);
    for (auto& [id, td] : db_->types.all()) {
        if (!fstr.empty()) {
            std::string lower_name = td.name;
            std::string lower_filt = fstr;
            std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
            std::transform(lower_filt.begin(), lower_filt.end(), lower_filt.begin(), ::tolower);
            if (lower_name.find(lower_filt) == std::string::npos) continue;
        }
        const char* icon = td.kind == TypeKind::Struct ? "[S] " :
                           td.kind == TypeKind::Enum   ? "[E] " : "";
        auto label = fmt::format("{}{}", icon, td.name);
        if (ImGui::Selectable(label.c_str(), selected_ == id))
            selected_ = id;
    }
}

void TypesPanel::render_detail() {
    if (!db_) { ImGui::TextDisabled("No data"); return; }
    auto* td = db_->types.get(selected_);
    if (!td) { ImGui::TextDisabled("Select a type"); return; }

    ImGui::Text("%s  (size: %u, id: %u)", td->name.c_str(), td->size, td->id);
    ImGui::Separator();

    if (td->kind == TypeKind::Struct) {
        if (ImGui::BeginTable("##fields", 4,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("Offset");
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Size");
            ImGui::TableHeadersRow();

            int to_del = -1;
            for (int i = 0; i < (int)td->fields.size(); ++i) {
                auto& f = td->fields[i];
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("+0x%04X", f.offset);
                ImGui::TableNextColumn(); ImGui::Text("%s", f.name.c_str());
                ImGui::TableNextColumn();
                auto* ft = db_->types.get(f.type_id);
                ImGui::Text("%s", ft ? ft->name.c_str() : "?");
                ImGui::TableNextColumn(); ImGui::Text("%u", f.size);
                ImGui::PushID(i);
                ImGui::SameLine();
                if (ImGui::SmallButton("X")) to_del = i;
                ImGui::PopID();
            }
            if (to_del >= 0)
                td->fields.erase(td->fields.begin() + to_del);
            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::Text("Add Field:");
        ImGui::InputText("Name##fn", field_name_, sizeof(field_name_));
        ImGui::InputText("Offset (hex)##fo", field_offset_, sizeof(field_offset_));

        std::vector<const char*> type_names;
        std::vector<u32> type_ids;
        for (auto& [id, t] : db_->types.all()) {
            type_names.push_back(t.name.c_str());
            type_ids.push_back(id);
        }
        if (!type_names.empty()) {
            if (field_type_idx_ >= (int)type_names.size()) field_type_idx_ = 0;
            ImGui::Combo("Type##ft", &field_type_idx_, type_names.data(), (int)type_names.size());
        }

        if (ImGui::Button("Add Field") && field_name_[0] && !type_ids.empty()) {
            u32 off = static_cast<u32>(std::strtoul(field_offset_, nullptr, 16));
            db_->types.add_field(selected_, field_name_, type_ids[field_type_idx_], off);
            field_name_[0] = 0; field_offset_[0] = 0;
        }
    }
    else if (td->kind == TypeKind::Enum) {
        if (ImGui::BeginTable("##members", 2,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Value");
            ImGui::TableHeadersRow();

            int to_del = -1;
            for (int i = 0; i < (int)td->members.size(); ++i) {
                auto& m = td->members[i];
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("%s", m.name.c_str());
                ImGui::TableNextColumn(); ImGui::Text("%lld", (long long)m.value);
                ImGui::PushID(i);
                ImGui::SameLine();
                if (ImGui::SmallButton("X")) to_del = i;
                ImGui::PopID();
            }
            if (to_del >= 0)
                td->members.erase(td->members.begin() + to_del);
            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::Text("Add Member:");
        ImGui::InputText("Name##mn", member_name_, sizeof(member_name_));
        ImGui::InputText("Value##mv", member_val_, sizeof(member_val_));
        if (ImGui::Button("Add Member") && member_name_[0]) {
            i64 val = std::strtoll(member_val_, nullptr, 0);
            db_->types.add_member(selected_, member_name_, val);
            member_name_[0] = 0; member_val_[0] = 0;
        }
    }
}

}
