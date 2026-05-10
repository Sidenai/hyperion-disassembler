#pragma once
#include "core/loader/pe_loader.h"
#include <imgui.h>
#include <functional>
#include <vector>

namespace hype {

class EntropyView {
public:
    using NavCB = std::function<void(va_t)>;
    void set_data(const PEImage* img) { img_ = img; dirty_ = true; }
    void set_nav(NavCB cb) { nav_ = std::move(cb); }
    void render();

private:
    void rebuild();

    const PEImage* img_ = nullptr;
    NavCB          nav_;
    bool           dirty_ = true;

    struct Block { va_t addr; float entropy; };
    std::vector<Block> blocks_;
    static constexpr size_t BLOCK_SZ = 256;
};

}
