#pragma once
#include "core/analysis/analysis_db.h"
#include <imgui.h>

namespace hype {

class TypesPanel {
public:
    void set_data(AnalysisDB* db) { db_ = db; }
    void render();

private:
    void render_list();
    void render_detail();

    AnalysisDB* db_ = nullptr;
    u32         selected_ = 0;
    char        filter_[128] = {};
    char        new_name_[128] = {};
    char        field_name_[128] = {};
    char        field_offset_[32] = {};
    char        member_name_[128] = {};
    char        member_val_[32] = {};
    int         field_type_idx_ = 0;
    int         new_size_[1] = {64};
};

}
