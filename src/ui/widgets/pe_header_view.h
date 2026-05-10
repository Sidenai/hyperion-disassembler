#pragma once
#include "core/loader/pe_loader.h"
#include "core/analysis/packer_detect.h"
#include <imgui.h>

namespace hype {

class PEHeaderView {
public:
    void set_data(PEImage* img) { img_ = img; }
    void set_packer_results(const std::vector<PackerInfo>* results) { packer_results_ = results; }
    void render();

private:
    void render_dos_header();
    void render_nt_headers();
    void render_optional_header();
    void render_data_directories();
    void render_sections();
    void render_packer_detection();

    PEImage* img_ = nullptr;
    const std::vector<PackerInfo>* packer_results_ = nullptr;
    bool     visible_ = false;

    friend class App;
};

}
