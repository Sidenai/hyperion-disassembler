#pragma once
#include <imgui.h>
#include <imgui_internal.h>

namespace hype {

struct Fonts {
    ImFont* ui      = nullptr;
    ImFont* mono    = nullptr;
    ImFont* heading = nullptr;
};

inline Fonts g_fonts;

inline void panel_header(const char* title) {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    float h = g_fonts.heading ? g_fonts.heading->LegacySize + 12.0f : 32.0f;

    auto* dl = ImGui::GetWindowDrawList();
    ImU32 bg = IM_COL32(30, 32, 42, 255);
    ImU32 border = IM_COL32(75, 120, 225, 180);
    dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), bg);
    dl->AddLine(ImVec2(pos.x, pos.y + h - 1), ImVec2(pos.x + w, pos.y + h - 1), border);

    if (g_fonts.heading) ImGui::PushFont(g_fonts.heading);
    ImGui::SetCursorScreenPos(ImVec2(pos.x + 10, pos.y + 4));
    ImGui::TextUnformatted(title);
    if (g_fonts.heading) ImGui::PopFont();

    ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + h + 4));
}

inline void render_status_bar(const char* left, const char* center, const char* right) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    float bar_h = 26.0f;
    ImVec2 bar_pos(vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - bar_h);
    ImVec2 bar_end(vp->WorkPos.x + vp->WorkSize.x, vp->WorkPos.y + vp->WorkSize.y);

    ImGui::SetNextWindowPos(bar_pos);
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, bar_h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.04f, 0.043f, 0.058f, 1.0f));

    ImGui::Begin("##statusbar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoNav);

    ImGui::TextUnformatted(left);

    float center_w = ImGui::CalcTextSize(center).x;
    float avail = ImGui::GetContentRegionAvail().x;
    ImGui::SameLine(avail * 0.5f - center_w * 0.5f);
    ImGui::TextDisabled("%s", center);

    float right_w = ImGui::CalcTextSize(right).x;
    ImGui::SameLine(avail - right_w - 4);
    ImGui::TextDisabled("%s", right);

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

}
