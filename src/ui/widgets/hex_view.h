#pragma once
#include "core/types.h"
#include "core/loader/pe_loader.h"
#include <imgui.h>
#include <vector>
#include <functional>

namespace hype {

class HexView {
public:
    using NavCB = std::function<void(va_t)>;
    void set_data(PEImage* img) { img_ = img; }
    void set_nav(NavCB cb) { nav_ = std::move(cb); }
    void goto_addr(va_t addr) { base_ = addr; scroll_pending_ = true; }
    void sync_to(va_t addr) { if (sync_) goto_addr(addr); }
    bool synced() const { return sync_; }
    va_t cursor_addr() const { return cursor_; }
    void nop_selection();
    void render();

private:
    void find_pattern_matches();

    PEImage*    img_ = nullptr;
    NavCB       nav_;
    va_t        base_ = 0;
    va_t        cursor_ = 0;
    va_t        sel_start_ = 0;
    va_t        sel_end_ = 0;
    bool        editing_ = false;
    int         edit_nibble_ = 0;
    int         seg_idx_ = 0;
    bool        sync_ = false;
    bool        scroll_pending_ = false;

    std::vector<va_t> match_addrs_;
    va_t              match_sel_start_ = 0;
    va_t              match_sel_end_ = 0;
    int               match_len_ = 0;
};

}
