#pragma once
#include "core/loader/pe_loader.h"
#include "core/analysis/analysis_db.h"
#include <imgui.h>
#include <functional>

namespace hype {

class ImportsPanel {
public:
    using NavCB = std::function<void(va_t)>;
    void set_data(const PEImage* img) { img_ = img; }
    void set_db(const AnalysisDB* db) { db_ = db; }
    void set_nav(NavCB cb) { nav_ = std::move(cb); }
    void render();

private:
    va_t find_caller(va_t iat_addr);

    const PEImage*    img_ = nullptr;
    const AnalysisDB* db_ = nullptr;
    NavCB             nav_;
    char              filter_[256] = {};
};

}
