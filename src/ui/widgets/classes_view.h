#pragma once
#include "core/analysis/rtti.h"
#include "core/analysis/analysis_db.h"
#include <imgui.h>
#include <functional>

namespace hype {

class ClassesView {
public:
    using NavCB = std::function<void(va_t)>;
    void set_data(const RTTIParser* rtti, const AnalysisDB* db) { rtti_ = rtti; db_ = db; }
    void set_nav(NavCB cb) { nav_ = std::move(cb); }
    void render();

private:
    const RTTIParser* rtti_ = nullptr;
    const AnalysisDB* db_ = nullptr;
    NavCB nav_;
    char filter_[256] = {};
    int selected_ = -1;
};

}
