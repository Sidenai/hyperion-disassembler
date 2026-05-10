#pragma once
#include "core/analysis/analysis_db.h"
#include "core/loader/pe_loader.h"
#include <imgui.h>
#include <functional>

namespace hype {

class XrefsPanel {
public:
    using NavCB = std::function<void(va_t)>;
    void set_data(const AnalysisDB* db) { db_ = db; }
    void set_img(const PEImage* img) { img_ = img; }
    void set_nav(NavCB cb) { nav_ = std::move(cb); }
    void show_for(va_t addr) { target_ = addr; }
    void show_popup(va_t addr) { popup_target_ = addr; open_popup_ = true; }
    void render();

private:
    void render_popup();
    std::string func_for(va_t addr) const;
    std::string insn_text(va_t addr) const;

    const AnalysisDB* db_ = nullptr;
    const PEImage*    img_ = nullptr;
    NavCB             nav_;
    va_t              target_ = 0;
    va_t              popup_target_ = 0;
    bool              open_popup_ = false;
};

}
