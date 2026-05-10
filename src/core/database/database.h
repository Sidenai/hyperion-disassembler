#pragma once
#include "core/analysis/analysis_db.h"
#include "core/loader/pe_loader.h"
#include <filesystem>

namespace hype {

class Database {
public:
    bool save(const std::filesystem::path& dir, const PEImage& img, const AnalysisDB& db);
    bool load(const std::filesystem::path& dir, PEImage& img, AnalysisDB& db);
};

}
