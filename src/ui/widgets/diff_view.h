#pragma once
#include "core/analysis/bindiff.h"
#include <imgui.h>
#include <functional>
#include <vector>

namespace hype {

class DiffView {
public:
    using NavCB = std::function<void(va_t)>;

    void set_results(std::vector<DiffResult> r) { results_ = std::move(r); }
    void set_nav(NavCB cb) { nav_ = std::move(cb); }
    void render();

private:
    std::vector<DiffResult> results_;
    NavCB                   nav_;
    bool show_added_ = true;
    bool show_removed_ = true;
    bool show_modified_ = true;
    bool show_identical_ = true;
    char filter_[128] = {};
};

}
