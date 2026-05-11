#pragma once
#include "core/analysis/analysis_db.h"
#include "core/loader/pe_loader.h"
#include <string>
#include <functional>

struct lua_State;

namespace hype {

class LuaEngine {
public:
    LuaEngine();
    ~LuaEngine();

    void init(AnalysisDB* db, PEImage* img);
    std::string execute(const std::string& code);

    void set_navigate_cb(std::function<void(va_t)> cb) { nav_cb_ = std::move(cb); }

private:
    void register_api();

    lua_State* L_ = nullptr;
    AnalysisDB* db_ = nullptr;
    PEImage* img_ = nullptr;
    std::string output_;
    std::function<void(va_t)> nav_cb_;
};

}
