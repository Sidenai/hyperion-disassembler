#pragma once
#include "core/analysis/analysis_db.h"
#include "core/loader/pe_loader.h"
#include <string>
#include <vector>
#include <functional>
#include <filesystem>

struct lua_State;

namespace hype {

// A single action registered by a plugin via register_menu_item().
struct PluginMenuItem {
    std::string label;
    int         cb_ref = -1; // Lua registry reference (luaL_ref)
};

// One loaded plugin file.
struct PluginEntry {
    std::string name;        // from register_plugin()
    std::string desc;        // from register_plugin()
    std::string path;        // filesystem path of the .lua file
    std::vector<PluginMenuItem> items;
    bool        error    = false;
    std::string error_msg;
};

// One row in a plugin results window.
struct ResultsRow {
    va_t                     addr;   // address to navigate to when clicked
    std::vector<std::string> cols;   // column values (must match headers count)
};

// A results window opened by a plugin via open_results().
struct ResultsWindow {
    std::string              title;
    std::vector<std::string> headers;
    std::vector<ResultsRow>  rows;
    bool                     open    = true;
    char                     filter[256] = {};
    int                      selected = -1;
};

class LuaEngine {
public:
    LuaEngine();
    ~LuaEngine();

    void init(AnalysisDB* db, PEImage* img);
    std::string execute(const std::string& code);

    void set_navigate_cb(std::function<void(va_t)> cb) { nav_cb_ = std::move(cb); }

    // Plugin system — public interface used by App
    void load_plugins(const std::filesystem::path& dir);
    void invoke_menu_item(int plugin_idx, int item_idx);
    void run_analysis_complete_callbacks();
    void check_hotkeys(); // call once per frame from App::handle_keys()

    const std::vector<PluginEntry>& plugins() const { return plugins_; }
    std::vector<PluginEntry>&       plugins()        { return plugins_; }

    // Results windows — rendered by App each frame
    std::vector<ResultsWindow>&       result_windows()       { return result_windows_; }
    const std::vector<ResultsWindow>& result_windows() const { return result_windows_; }

    // Returns whatever l_print accumulated during the last invoke_menu_item call.
    const std::string& last_output() const { return output_; }

    // Called by Lua C-function wrappers inside the anonymous namespace
    const std::string& current_plugin_name() const { return current_plugin_; }
    void add_hotkey(const std::string& key_str, int ref)  { hotkeys_.emplace_back(key_str, ref); }
    void add_analysis_cb(int ref)                          { on_analysis_cbs_.push_back(ref); }
    void push_result_window(ResultsWindow w)               { result_windows_.push_back(std::move(w)); }

private:
    void register_api();

    lua_State*  L_   = nullptr;
    AnalysisDB* db_  = nullptr;
    PEImage*    img_ = nullptr;
    std::string output_;
    std::function<void(va_t)> nav_cb_;

    // Plugin registry
    std::vector<PluginEntry>                   plugins_;
    std::vector<std::pair<std::string, int>>   hotkeys_;          // {key_string, cb_ref}
    std::vector<int>                           on_analysis_cbs_;  // Lua registry refs
    std::string                                current_plugin_;   // set while loading a .lua file

    // Results windows
    std::vector<ResultsWindow>                 result_windows_;
};

} // namespace hype
