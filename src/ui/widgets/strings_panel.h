#pragma once
#include "core/analysis/analysis_db.h"
#include <imgui.h>
#include <functional>

namespace hype {

class StringsPanel {
public:
    using NavCB = std::function<void(va_t)>;
    void set_data(const AnalysisDB* db) { db_ = db; }
    void set_nav(NavCB cb) { nav_ = std::move(cb); }
    void render();

private:
    const AnalysisDB* db_ = nullptr;
    NavCB             nav_;
    char              filter_[256] = {};
};

}
