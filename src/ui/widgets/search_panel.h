#pragma once
#include "core/analysis/analysis_db.h"
#include "core/loader/pe_loader.h"
#include <imgui.h>
#include <functional>
#include <vector>
#include <string>

namespace hype {

class SearchPanel {
public:
    using NavCB = std::function<void(va_t)>;

    void set_data(const AnalysisDB* db, const PEImage* img) { db_ = db; img_ = img; }
    void set_nav(NavCB cb) { nav_ = std::move(cb); }
    void open_text()   { show_ = true; mode_ = Mode::Text; }
    void open_binary() { show_ = true; mode_ = Mode::Binary; }
    void render();

private:
    enum class Mode { Text, Binary, Immediate };

    void search_text();
    void search_binary();
    void search_immediate();

    const AnalysisDB* db_ = nullptr;
    const PEImage*    img_ = nullptr;
    NavCB             nav_;
    bool              show_ = false;
    Mode              mode_ = Mode::Text;
    char              query_[512] = {};
    char              hex_pat_[512] = {};
    char              imm_buf_[64] = {};

    struct Result { va_t addr; std::string desc; };
    std::vector<Result> results_;
};

}
