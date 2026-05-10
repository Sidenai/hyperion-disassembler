#pragma once
#include "core/analysis/analysis_db.h"
#include <imgui.h>
#include <functional>
#include <vector>

namespace hype {

class CallGraphView {
public:
    using NavCB = std::function<void(va_t)>;
    void set_data(const AnalysisDB* db) { db_ = db; }
    void set_nav(NavCB cb) { nav_ = std::move(cb); }
    void show_function(va_t entry);
    void render();

private:
    const AnalysisDB* db_ = nullptr;
    NavCB             nav_;
    va_t              target_ = 0;

    struct Entry { va_t addr; std::string name; };
    std::vector<Entry> callers_;
    std::vector<Entry> callees_;
};

}
