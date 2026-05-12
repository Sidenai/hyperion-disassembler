#pragma once
#include <imgui.h>

namespace hype {

enum class Theme : int { BinaryNinja, IDA, Midnight, Custom };

inline Theme g_theme = Theme::BinaryNinja;

struct CustomThemeData {
    float bg[4]      = {0.067f, 0.071f, 0.090f, 1.0f};
    float text[4]    = {0.906f, 0.918f, 0.945f, 1.0f};
    float accent[4]  = {0.294f, 0.471f, 0.882f, 1.0f};
    float surface[4] = {0.090f, 0.094f, 0.118f, 1.0f};
    float border[4]  = {0.133f, 0.141f, 0.176f, 1.0f};
};

inline CustomThemeData g_custom_theme;

inline void apply_theme_binja() {
    auto& s = ImGui::GetStyle();
    auto* c = s.Colors;

    s.WindowRounding    = 4.0f;
    s.ChildRounding     = 4.0f;
    s.FrameRounding     = 6.0f;
    s.PopupRounding     = 6.0f;
    s.ScrollbarRounding = 12.0f;
    s.GrabRounding      = 6.0f;
    s.TabRounding       = 2.0f;
    s.WindowBorderSize  = 0.0f;
    s.ChildBorderSize   = 0.0f;
    s.FrameBorderSize   = 0.0f;
    s.PopupBorderSize   = 0.5f;
    s.TabBorderSize     = 0.0f;
    s.TabBarBorderSize  = 1.0f;
    s.WindowPadding     = {12, 12};
    s.FramePadding      = {12, 8};
    s.ItemSpacing       = {10, 6};
    s.ItemInnerSpacing  = {8, 4};
    s.IndentSpacing     = 20.0f;
    s.ScrollbarSize     = 10.0f;
    s.GrabMinSize       = 8.0f;
    s.WindowTitleAlign  = {0.5f, 0.5f};
    s.SeparatorTextAlign = {0.0f, 0.5f};

    ImVec4 bg       = {0.067f, 0.071f, 0.090f, 1.0f};
    ImVec4 bg_deep  = {0.047f, 0.051f, 0.067f, 1.0f};
    ImVec4 surface  = {0.090f, 0.094f, 0.118f, 1.0f};
    ImVec4 surface2 = {0.110f, 0.118f, 0.145f, 1.0f};
    ImVec4 border   = {0.133f, 0.141f, 0.176f, 1.0f};
    ImVec4 accent   = {0.294f, 0.471f, 0.882f, 1.0f};
    ImVec4 accent_h = {0.373f, 0.545f, 0.941f, 1.0f};
    ImVec4 accent_a = {0.216f, 0.388f, 0.784f, 1.0f};
    ImVec4 text     = {0.906f, 0.918f, 0.945f, 1.0f};
    ImVec4 text_dim = {0.420f, 0.440f, 0.510f, 1.0f};
    ImVec4 hover    = {0.133f, 0.141f, 0.180f, 1.0f};
    ImVec4 active   = {0.157f, 0.169f, 0.212f, 1.0f};

    // Tab bar: darker background, accent underline for active
    ImVec4 tab_bar_bg   = {0.040f, 0.043f, 0.058f, 1.0f};
    ImVec4 tab_active   = {0.090f, 0.094f, 0.118f, 1.0f};
    ImVec4 tab_inactive = {0.055f, 0.059f, 0.078f, 1.0f};

    c[ImGuiCol_WindowBg]             = bg;
    c[ImGuiCol_ChildBg]              = {0, 0, 0, 0};
    c[ImGuiCol_PopupBg]              = {0.059f, 0.063f, 0.082f, 0.97f};
    c[ImGuiCol_Border]               = border;
    c[ImGuiCol_BorderShadow]         = {0, 0, 0, 0};
    c[ImGuiCol_FrameBg]              = surface;
    c[ImGuiCol_FrameBgHovered]       = hover;
    c[ImGuiCol_FrameBgActive]        = active;
    c[ImGuiCol_TitleBg]              = bg_deep;
    c[ImGuiCol_TitleBgActive]        = {0.055f, 0.059f, 0.078f, 1.0f};
    c[ImGuiCol_TitleBgCollapsed]     = bg_deep;
    c[ImGuiCol_MenuBarBg]            = bg_deep;
    c[ImGuiCol_ScrollbarBg]          = {0, 0, 0, 0};
    c[ImGuiCol_ScrollbarGrab]        = {0.180f, 0.192f, 0.235f, 0.7f};
    c[ImGuiCol_ScrollbarGrabHovered] = {0.240f, 0.255f, 0.310f, 0.9f};
    c[ImGuiCol_ScrollbarGrabActive]  = accent;
    c[ImGuiCol_CheckMark]            = accent;
    c[ImGuiCol_SliderGrab]           = accent;
    c[ImGuiCol_SliderGrabActive]     = accent_h;
    c[ImGuiCol_Button]               = surface2;
    c[ImGuiCol_ButtonHovered]        = {accent.x * 0.3f, accent.y * 0.3f, accent.z * 0.3f, 0.20f};
    c[ImGuiCol_ButtonActive]         = accent_a;
    c[ImGuiCol_Header]               = surface;
    c[ImGuiCol_HeaderHovered]        = hover;
    c[ImGuiCol_HeaderActive]         = accent_a;
    c[ImGuiCol_Separator]            = {0.110f, 0.118f, 0.149f, 0.6f};
    c[ImGuiCol_SeparatorHovered]     = accent;
    c[ImGuiCol_SeparatorActive]      = accent_h;
    c[ImGuiCol_ResizeGrip]           = {0, 0, 0, 0};
    c[ImGuiCol_ResizeGripHovered]    = accent;
    c[ImGuiCol_ResizeGripActive]     = accent_h;
    c[ImGuiCol_Tab]                  = tab_inactive;
    c[ImGuiCol_TabHovered]           = accent;
    c[ImGuiCol_TabSelected]          = tab_active;
    c[ImGuiCol_TabDimmed]            = tab_bar_bg;
    c[ImGuiCol_TabDimmedSelected]    = tab_inactive;
    c[ImGuiCol_DockingPreview]       = accent;
    c[ImGuiCol_DockingEmptyBg]       = bg_deep;
    c[ImGuiCol_PlotLines]            = accent;
    c[ImGuiCol_PlotLinesHovered]     = accent_h;
    c[ImGuiCol_PlotHistogram]        = accent;
    c[ImGuiCol_PlotHistogramHovered] = accent_h;
    c[ImGuiCol_TableHeaderBg]        = {0.071f, 0.075f, 0.098f, 1.0f};
    c[ImGuiCol_TableBorderStrong]    = border;
    c[ImGuiCol_TableBorderLight]     = {0.098f, 0.106f, 0.133f, 0.5f};
    c[ImGuiCol_TableRowBg]           = {0, 0, 0, 0};
    c[ImGuiCol_TableRowBgAlt]        = {0.035f, 0.039f, 0.051f, 0.5f};
    c[ImGuiCol_Text]                 = text;
    c[ImGuiCol_TextDisabled]         = text_dim;
    c[ImGuiCol_TextSelectedBg]       = {0.22f, 0.38f, 0.72f, 0.45f};
    c[ImGuiCol_NavHighlight]         = accent;
    c[ImGuiCol_ModalWindowDimBg]     = {0.02f, 0.02f, 0.04f, 0.7f};
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
    s.TabBarBorderSize  = 1.0f;
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
    s.TabBarBorderSize  = 1.0f;
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

inline void apply_theme_custom() {
    apply_theme_binja();
    auto& s = ImGui::GetStyle();
    auto* c = s.Colors;
    auto& ct = g_custom_theme;

    ImVec4 bg      = {ct.bg[0], ct.bg[1], ct.bg[2], ct.bg[3]};
    ImVec4 text    = {ct.text[0], ct.text[1], ct.text[2], ct.text[3]};
    ImVec4 accent  = {ct.accent[0], ct.accent[1], ct.accent[2], ct.accent[3]};
    ImVec4 surface = {ct.surface[0], ct.surface[1], ct.surface[2], ct.surface[3]};
    ImVec4 border  = {ct.border[0], ct.border[1], ct.border[2], ct.border[3]};

    c[ImGuiCol_WindowBg]         = bg;
    c[ImGuiCol_PopupBg]          = {bg.x * 0.85f, bg.y * 0.85f, bg.z * 0.85f, 0.97f};
    c[ImGuiCol_Border]           = border;
    c[ImGuiCol_FrameBg]          = surface;
    c[ImGuiCol_TitleBg]          = {bg.x * 0.7f, bg.y * 0.7f, bg.z * 0.7f, 1.0f};
    c[ImGuiCol_TitleBgActive]    = {bg.x * 0.8f, bg.y * 0.8f, bg.z * 0.8f, 1.0f};
    c[ImGuiCol_MenuBarBg]        = {bg.x * 0.7f, bg.y * 0.7f, bg.z * 0.7f, 1.0f};
    c[ImGuiCol_CheckMark]        = accent;
    c[ImGuiCol_SliderGrab]       = accent;
    c[ImGuiCol_SliderGrabActive] = {accent.x * 1.2f, accent.y * 1.2f, accent.z * 1.2f, 1.0f};
    c[ImGuiCol_Button]           = surface;
    c[ImGuiCol_ButtonHovered]    = {accent.x * 0.3f, accent.y * 0.3f, accent.z * 0.3f, 0.3f};
    c[ImGuiCol_ButtonActive]     = accent;
    c[ImGuiCol_Header]           = surface;
    c[ImGuiCol_HeaderHovered]    = {accent.x * 0.3f, accent.y * 0.3f, accent.z * 0.3f, 0.3f};
    c[ImGuiCol_HeaderActive]     = accent;
    c[ImGuiCol_Tab]              = surface;
    c[ImGuiCol_TabHovered]       = accent;
    c[ImGuiCol_TabSelected]      = {surface.x * 1.2f, surface.y * 1.2f, surface.z * 1.2f, 1.0f};
    c[ImGuiCol_DockingPreview]   = accent;
    c[ImGuiCol_Text]             = text;
    c[ImGuiCol_TextDisabled]     = {text.x * 0.5f, text.y * 0.5f, text.z * 0.5f, 1.0f};
    c[ImGuiCol_ScrollbarGrabActive] = accent;
}

inline void apply_theme() {
    switch (g_theme) {
        case Theme::BinaryNinja: apply_theme_binja(); break;
        case Theme::IDA:         apply_theme_ida(); break;
        case Theme::Midnight:    apply_theme_midnight(); break;
        case Theme::Custom:      apply_theme_custom(); break;
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
