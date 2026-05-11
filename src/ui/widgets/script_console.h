#pragma once
#include "scripting/lua_engine.h"
#include <imgui.h>
#include <string>
#include <deque>
#include <functional>

namespace hype {

class ScriptConsole {
public:
    void set_engine(LuaEngine* eng) { engine_ = eng; }
    void set_nav(std::function<void(va_t)> cb) { nav_cb_ = std::move(cb); }
    void render();

private:
    LuaEngine* engine_ = nullptr;
    std::function<void(va_t)> nav_cb_;
    char input_[2048] = {};
    std::deque<std::string> output_;
    std::deque<std::string> history_;
    int hist_idx_ = -1;
    bool scroll_bottom_ = true;
};

}
