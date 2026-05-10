#pragma once
#include "core/analysis/analysis_db.h"
#include <imgui.h>
#include <functional>
#include <vector>
#include <unordered_map>

namespace hype {

class GraphView {
public:
    using NavCB = std::function<void(va_t)>;
    void set_data(const AnalysisDB* db) { db_ = db; }
    void set_nav(NavCB cb) { nav_ = std::move(cb); }
    void show_function(va_t entry);
    void render();

private:
    struct Node { va_t addr; float x, y, w, h; int layer; };
    void layout();

    const AnalysisDB*  db_ = nullptr;
    NavCB              nav_;
    va_t               func_ = 0;
    std::vector<Node>  nodes_;
    bool               dirty_ = true;
    ImVec2             scroll_{0, 0};
    float              zoom_ = 1.0f;
};

}
