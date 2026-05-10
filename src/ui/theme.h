#pragma once
#include <imgui.h>

namespace hype {

enum class Theme : int { BinaryNinja, IDA, Midnight };

inline Theme g_theme = Theme::BinaryNinja;

inline void apply_theme_binja() {
    auto& s = ImGui::GetStyle();
    auto* c = s.Colors;

    s.WindowRounding    = 0.0f;
    s.FrameRounding     = 3.0f;
    s.ScrollbarRounding = 6.0f;
    s.TabRounding       = 3.0f;
    s.GrabRounding      = 3.0f;
    s.WindowBorderSize  = 1.0f;
    s.FrameBorderSize   = 0.0f;
    s.PopupBorderSize   = 1.0f;
    s.WindowPadding     = {10, 10};
    s.FramePadding      = {6, 4};
    s.ItemSpacing       = {8, 5};
    s.ItemInnerSpacing  = {6, 4};
    s.IndentSpacing     = 20.0f;
    s.ScrollbarSize     = 12.0f;

    ImVec4 bg       = {0.098f, 0.102f, 0.122f, 1.0f};
    ImVec4 bg_deep  = {0.071f, 0.075f, 0.094f, 1.0f};
    ImVec4 surface  = {0.129f, 0.133f, 0.157f, 1.0f};
    ImVec4 border   = {0.180f, 0.188f, 0.220f, 1.0f};
    ImVec4 accent   = {0.337f, 0.502f, 0.827f, 1.0f};
    ImVec4 accent_h = {0.420f, 0.580f, 0.900f, 1.0f};
    ImVec4 accent_a = {0.260f, 0.420f, 0.750f, 1.0f};
    ImVec4 text     = {0.882f, 0.894f, 0.922f, 1.0f};
    ImVec4 text_dim = {0.478f, 0.498f, 0.557f, 1.0f};
    ImVec4 hover    = {0.165f, 0.173f, 0.204f, 1.0f};

    c[ImGuiCol_WindowBg]             = bg;
    c[ImGuiCol_ChildBg]              = {0, 0, 0, 0};
    c[ImGuiCol_PopupBg]              = {0.082f, 0.086f, 0.106f, 0.96f};
    c[ImGuiCol_Border]               = border;
    c[ImGuiCol_BorderShadow]         = {0, 0, 0, 0};
    c[ImGuiCol_FrameBg]              = surface;
    c[ImGuiCol_FrameBgHovered]       = hover;
    c[ImGuiCol_FrameBgActive]        = {0.188f, 0.200f, 0.240f, 1.0f};
    c[ImGuiCol_TitleBg]              = bg_deep;
    c[ImGuiCol_TitleBgActive]        = {0.090f, 0.094f, 0.114f, 1.0f};
    c[ImGuiCol_TitleBgCollapsed]     = bg_deep;
    c[ImGuiCol_MenuBarBg]            = bg_deep;
    c[ImGuiCol_ScrollbarBg]          = bg_deep;
    c[ImGuiCol_ScrollbarGrab]        = {0.22f, 0.23f, 0.27f, 1.0f};
    c[ImGuiCol_ScrollbarGrabHovered] = {0.30f, 0.32f, 0.37f, 1.0f};
    c[ImGuiCol_ScrollbarGrabActive]  = accent;
    c[ImGuiCol_CheckMark]            = accent;
    c[ImGuiCol_SliderGrab]           = accent;
    c[ImGuiCol_SliderGrabActive]     = accent_h;
    c[ImGuiCol_Button]               = surface;
    c[ImGuiCol_ButtonHovered]        = hover;
    c[ImGuiCol_ButtonActive]         = accent_a;
    c[ImGuiCol_Header]               = surface;
    c[ImGuiCol_HeaderHovered]        = hover;
    c[ImGuiCol_HeaderActive]         = accent_a;
    c[ImGuiCol_Separator]            = border;
    c[ImGuiCol_SeparatorHovered]     = accent;
    c[ImGuiCol_SeparatorActive]      = accent_h;
    c[ImGuiCol_ResizeGrip]           = {0, 0, 0, 0};
    c[ImGuiCol_ResizeGripHovered]    = accent;
    c[ImGuiCol_ResizeGripActive]     = accent_h;
    c[ImGuiCol_Tab]                  = surface;
    c[ImGuiCol_TabHovered]           = accent;
    c[ImGuiCol_TabSelected]          = {0.145f, 0.157f, 0.196f, 1.0f};
    c[ImGuiCol_TabDimmed]            = bg_deep;
    c[ImGuiCol_TabDimmedSelected]    = surface;
    c[ImGuiCol_DockingPreview]       = accent;
    c[ImGuiCol_DockingEmptyBg]       = bg_deep;
    c[ImGuiCol_PlotLines]            = accent;
    c[ImGuiCol_PlotLinesHovered]     = accent_h;
    c[ImGuiCol_PlotHistogram]        = accent;
    c[ImGuiCol_PlotHistogramHovered] = accent_h;
    c[ImGuiCol_TableHeaderBg]        = bg_deep;
    c[ImGuiCol_TableBorderStrong]    = border;
    c[ImGuiCol_TableBorderLight]     = {0.145f, 0.153f, 0.180f, 1.0f};
    c[ImGuiCol_TableRowBg]           = {0, 0, 0, 0};
    c[ImGuiCol_TableRowBgAlt]        = {0.04f, 0.04f, 0.06f, 0.4f};
    c[ImGuiCol_Text]                 = text;
    c[ImGuiCol_TextDisabled]         = text_dim;
    c[ImGuiCol_TextSelectedBg]       = {0.26f, 0.42f, 0.75f, 0.4f};
    c[ImGuiCol_NavHighlight]         = accent;
    c[ImGuiCol_ModalWindowDimBg]     = {0, 0, 0, 0.6f};
}

inline void apply_theme_ida() {
    auto& s = ImGui::GetStyle();
    auto* c = s.Colors;

    s.WindowRounding    = 0.0f;
    s.FrameRounding     = 1.0f;
    s.ScrollbarRounding = 1.0f;
    s.TabRounding       = 1.0f;
    s.GrabRounding      = 1.0f;
    s.WindowBorderSize  = 1.0f;
    s.FrameBorderSize   = 0.0f;
    s.WindowPadding     = {6, 6};
    s.FramePadding      = {4, 3};
    s.ItemSpacing       = {6, 3};

    ImVec4 bg       = {0.16f, 0.16f, 0.18f, 1.0f};
    ImVec4 bg_dark  = {0.12f, 0.12f, 0.14f, 1.0f};
    ImVec4 panel    = {0.19f, 0.19f, 0.21f, 1.0f};
    ImVec4 border   = {0.28f, 0.28f, 0.30f, 1.0f};
    ImVec4 accent   = {0.35f, 0.55f, 0.85f, 1.0f};
    ImVec4 text     = {0.90f, 0.90f, 0.90f, 1.0f};
    ImVec4 text_dim = {0.55f, 0.55f, 0.58f, 1.0f};

    c[ImGuiCol_WindowBg]         = bg;
    c[ImGuiCol_ChildBg]          = {0,0,0,0};
    c[ImGuiCol_PopupBg]          = bg_dark;
    c[ImGuiCol_Border]           = border;
    c[ImGuiCol_FrameBg]          = panel;
    c[ImGuiCol_FrameBgHovered]   = {0.24f, 0.24f, 0.26f, 1.0f};
    c[ImGuiCol_FrameBgActive]    = {0.26f, 0.26f, 0.30f, 1.0f};
    c[ImGuiCol_TitleBg]          = bg_dark;
    c[ImGuiCol_TitleBgActive]    = {0.14f, 0.14f, 0.16f, 1.0f};
    c[ImGuiCol_TitleBgCollapsed] = bg_dark;
    c[ImGuiCol_MenuBarBg]        = bg_dark;
    c[ImGuiCol_ScrollbarBg]      = bg_dark;
    c[ImGuiCol_ScrollbarGrab]    = {0.30f, 0.30f, 0.33f, 1.0f};
    c[ImGuiCol_ScrollbarGrabHovered] = {0.38f, 0.38f, 0.40f, 1.0f};
    c[ImGuiCol_ScrollbarGrabActive]  = accent;
    c[ImGuiCol_CheckMark]        = accent;
    c[ImGuiCol_Button]           = panel;
    c[ImGuiCol_ButtonHovered]    = {0.26f, 0.26f, 0.28f, 1.0f};
    c[ImGuiCol_ButtonActive]     = accent;
    c[ImGuiCol_Header]           = panel;
    c[ImGuiCol_HeaderHovered]    = {0.26f, 0.26f, 0.28f, 1.0f};
    c[ImGuiCol_HeaderActive]     = accent;
    c[ImGuiCol_Separator]        = border;
    c[ImGuiCol_Tab]              = panel;
    c[ImGuiCol_TabHovered]       = accent;
    c[ImGuiCol_TabSelected]      = {0.22f, 0.22f, 0.25f, 1.0f};
    c[ImGuiCol_DockingPreview]   = accent;
    c[ImGuiCol_DockingEmptyBg]   = bg_dark;
    c[ImGuiCol_Text]             = text;
    c[ImGuiCol_TextDisabled]     = text_dim;
    c[ImGuiCol_TableHeaderBg]    = bg_dark;
    c[ImGuiCol_TableBorderStrong]= border;
    c[ImGuiCol_TableBorderLight] = {0.22f, 0.22f, 0.24f, 1.0f};
    c[ImGuiCol_TableRowBg]       = {0, 0, 0, 0};
    c[ImGuiCol_TableRowBgAlt]    = {0.04f, 0.04f, 0.04f, 0.3f};
}

inline void apply_theme_midnight() {
    auto& s = ImGui::GetStyle();
    auto* c = s.Colors;

    s.WindowRounding    = 0.0f;
    s.FrameRounding     = 4.0f;
    s.ScrollbarRounding = 8.0f;
    s.TabRounding       = 4.0f;
    s.GrabRounding      = 4.0f;
    s.WindowBorderSize  = 0.0f;
    s.FrameBorderSize   = 0.0f;
    s.WindowPadding     = {10, 10};
    s.FramePadding      = {6, 4};
    s.ItemSpacing       = {8, 5};

    ImVec4 bg       = {0.06f, 0.06f, 0.09f, 1.0f};
    ImVec4 surface  = {0.09f, 0.09f, 0.13f, 1.0f};
    ImVec4 border   = {0.14f, 0.14f, 0.19f, 1.0f};
    ImVec4 accent   = {0.55f, 0.35f, 0.85f, 1.0f};
    ImVec4 accent_h = {0.65f, 0.45f, 0.95f, 1.0f};
    ImVec4 text     = {0.85f, 0.85f, 0.90f, 1.0f};
    ImVec4 text_dim = {0.42f, 0.42f, 0.50f, 1.0f};

    c[ImGuiCol_WindowBg]         = bg;
    c[ImGuiCol_ChildBg]          = {0,0,0,0};
    c[ImGuiCol_PopupBg]          = {0.05f, 0.05f, 0.07f, 0.96f};
    c[ImGuiCol_Border]           = border;
    c[ImGuiCol_FrameBg]          = surface;
    c[ImGuiCol_FrameBgHovered]   = {0.12f, 0.12f, 0.17f, 1.0f};
    c[ImGuiCol_FrameBgActive]    = {0.15f, 0.14f, 0.20f, 1.0f};
    c[ImGuiCol_TitleBg]          = bg;
    c[ImGuiCol_TitleBgActive]    = {0.07f, 0.07f, 0.10f, 1.0f};
    c[ImGuiCol_TitleBgCollapsed] = bg;
    c[ImGuiCol_MenuBarBg]        = bg;
    c[ImGuiCol_ScrollbarBg]      = bg;
    c[ImGuiCol_ScrollbarGrab]    = {0.18f, 0.17f, 0.24f, 1.0f};
    c[ImGuiCol_ScrollbarGrabHovered] = {0.25f, 0.24f, 0.32f, 1.0f};
    c[ImGuiCol_ScrollbarGrabActive]  = accent;
    c[ImGuiCol_CheckMark]        = accent;
    c[ImGuiCol_Button]           = surface;
    c[ImGuiCol_ButtonHovered]    = {0.14f, 0.13f, 0.19f, 1.0f};
    c[ImGuiCol_ButtonActive]     = accent;
    c[ImGuiCol_Header]           = surface;
    c[ImGuiCol_HeaderHovered]    = {0.14f, 0.13f, 0.19f, 1.0f};
    c[ImGuiCol_HeaderActive]     = accent;
    c[ImGuiCol_Separator]        = border;
    c[ImGuiCol_Tab]              = surface;
    c[ImGuiCol_TabHovered]       = accent;
    c[ImGuiCol_TabSelected]      = {0.12f, 0.11f, 0.16f, 1.0f};
    c[ImGuiCol_DockingPreview]   = accent;
    c[ImGuiCol_DockingEmptyBg]   = bg;
    c[ImGuiCol_Text]             = text;
    c[ImGuiCol_TextDisabled]     = text_dim;
    c[ImGuiCol_TableHeaderBg]    = bg;
    c[ImGuiCol_TableBorderStrong]= border;
    c[ImGuiCol_TableBorderLight] = {0.11f, 0.11f, 0.15f, 1.0f};
    c[ImGuiCol_TableRowBg]       = {0, 0, 0, 0};
    c[ImGuiCol_TableRowBgAlt]    = {0.03f, 0.03f, 0.05f, 0.4f};
}

inline void apply_theme() {
    switch (g_theme) {
        case Theme::BinaryNinja: apply_theme_binja(); break;
        case Theme::IDA:         apply_theme_ida(); break;
        case Theme::Midnight:    apply_theme_midnight(); break;
    }
}

namespace col {
    inline ImU32 addr()    { return IM_COL32(90, 150, 220, 255); }
    inline ImU32 mnem()    { return IM_COL32(220, 220, 160, 255); }
    inline ImU32 reg()     { return IM_COL32(130, 210, 150, 255); }
    inline ImU32 imm()     { return IM_COL32(175, 140, 220, 255); }
    inline ImU32 str()     { return IM_COL32(220, 150, 90, 255); }
    inline ImU32 comment() { return IM_COL32(95, 110, 95, 255); }
    inline ImU32 func()    { return IM_COL32(230, 180, 80, 255); }
    inline ImU32 xref()    { return IM_COL32(90, 180, 210, 255); }
}

}
