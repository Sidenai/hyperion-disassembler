#pragma once
#include "core/analysis/analysis_db.h"
#include "core/loader/pe_loader.h"
#include <filesystem>

namespace hype {

class IDAExport {
public:
    bool write(const std::filesystem::path& path, const PEImage& img, const AnalysisDB& db);
};

}
