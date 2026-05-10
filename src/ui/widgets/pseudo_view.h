#pragma once
#include "core/analysis/analysis_db.h"
#include "core/decompiler/decompiler.h"
#include <imgui.h>
#include <functional>
#include <vector>

namespace hype {

class PseudoView {
public:
    using NavCB = std::function<void(va_t)>;
    void set_data(const AnalysisDB* db) { db_ = db; lines_.clear(); func_ = 0; }
    void set_nav(NavCB cb) { nav_ = std::move(cb); }
    void show_function(va_t entry);
    void highlight_addr(va_t addr) { hl_ = addr; }
    void render();

private:
    const AnalysisDB*       db_ = nullptr;
    NavCB                   nav_;
    va_t                    func_ = 0;
    va_t                    hl_ = 0;
    std::vector<PseudoLine> lines_;
    Decompiler              dec_;
};

}
