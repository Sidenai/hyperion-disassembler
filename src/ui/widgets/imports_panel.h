#pragma once
#include "core/loader/pe_loader.h"
#include <imgui.h>
#include <functional>

namespace hype {

class ImportsPanel {
public:
    using NavCB = std::function<void(va_t)>;
    void set_data(const PEImage* img) { img_ = img; }
    void set_nav(NavCB cb) { nav_ = std::move(cb); }
    void render();

private:
    const PEImage* img_ = nullptr;
    NavCB          nav_;
    char           filter_[256] = {};
};

}
