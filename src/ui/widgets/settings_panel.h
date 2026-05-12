#pragma once
#include <imgui.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <filesystem>

namespace hype {

struct KeybindEntry {
    ImGuiKey key = ImGuiKey_None;
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
};

struct KeybindManager {
    std::unordered_map<std::string, KeybindEntry> binds;

    void set_defaults();
    bool check(const std::string& action) const;
    void set(const std::string& action, ImGuiKey key, bool ctrl = false, bool shift = false, bool alt = false);
    void load(const std::string& path);
    void save(const std::string& path) const;

    static std::string key_name(ImGuiKey key);
    static ImGuiKey key_from_name(const std::string& name);
    std::string display_string(const std::string& action) const;
};

struct Settings {
    float font_size = 16.0f;
    int theme_index = 0; // 0=Hyperion, 1=IDA, 2=Midnight, 3=Custom
    float accent_color[3] = {0.294f, 0.471f, 0.882f};
    float bg_color[3] = {0.067f, 0.071f, 0.090f};
    float text_color[3] = {0.906f, 0.918f, 0.945f};

    bool show_bytes = true;
    int max_bytes = 6;
    int address_width = 16;
    int tab_size = 4;
    bool auto_beautify = false;
    char mono_font_path[512] = {};

    int max_threads = 0;
    int autosave_interval = 60;
    int max_decompiler_blocks = 200;

    // background image
    std::string bg_image_path;
    float bg_opacity = 0.15f;

    // extra appearance
    float window_opacity = 1.0f;
    float cursor_line_color[4] = {0.157f, 0.216f, 0.333f, 1.0f};
    float func_header_color[4] = {0.235f, 0.275f, 0.353f, 1.0f};
    float border_radius = 4.0f;
    float scrollbar_width = 10.0f;
    int font_index = 0;

    void load(const std::filesystem::path& path);
    void save(const std::filesystem::path& path) const;
};

struct ThemeColors {
    float bg[4] = {0.067f, 0.071f, 0.090f, 1.0f};
    float text[4] = {0.906f, 0.918f, 0.945f, 1.0f};
    float accent[4] = {0.294f, 0.471f, 0.882f, 1.0f};
    float surface[4] = {0.090f, 0.094f, 0.118f, 1.0f};
    float border[4] = {0.133f, 0.141f, 0.176f, 1.0f};

    void export_hth(const std::filesystem::path& path) const;
    bool import_hth(const std::filesystem::path& path);
};

class SettingsPanel {
public:
    SettingsPanel();

    void render();
    bool visible() const { return visible_; }
    void show() { visible_ = true; }

    Settings& settings() { return settings_; }
    const Settings& settings() const { return settings_; }
    KeybindManager& keybinds() { return keybinds_; }
    const KeybindManager& keybinds() const { return keybinds_; }
    ThemeColors& custom_theme() { return custom_colors_; }

    bool font_rebuild_needed() const { return font_rebuild_; }
    void clear_font_rebuild() { font_rebuild_ = false; }
    bool theme_changed() const { return theme_changed_; }
    void clear_theme_changed() { theme_changed_ = false; }

    unsigned int bg_texture() const { return bg_texture_; }
    int bg_width() const { return bg_width_; }
    int bg_height() const { return bg_height_; }
    void load_bg_image(const std::string& path);
    void clear_bg_image();

    void load_all();
    void save_all();

    struct FontEntry { std::string name; std::string path; };
    const std::vector<FontEntry>& available_fonts() const { return available_fonts_; }

private:
    void render_appearance_tab();
    void render_editor_tab();
    void render_keybinds_tab();
    void render_advanced_tab();
    void scan_system_fonts();

    std::filesystem::path ini_path() const;
    std::filesystem::path keybind_path() const;

    Settings settings_;
    KeybindManager keybinds_;
    ThemeColors custom_colors_;
    bool visible_ = false;
    bool font_rebuild_ = false;
    bool theme_changed_ = false;

    unsigned int bg_texture_ = 0;
    int bg_width_ = 0, bg_height_ = 0;

    std::vector<FontEntry> available_fonts_;

    std::string listening_action_;
    std::vector<std::pair<std::string, std::string>> keybind_display_;
    void rebuild_keybind_display();

    struct ThemeFile { std::string name; std::filesystem::path path; };
    std::vector<ThemeFile> community_themes_;
    void scan_themes_folder();
};

}
