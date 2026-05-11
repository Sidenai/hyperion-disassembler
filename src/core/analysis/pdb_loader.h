#pragma once
#include "analysis_db.h"
#include <filesystem>

namespace hype {

class PDBLoader {
public:
    bool load(const std::filesystem::path& pdb_path, va_t base, AnalysisDB& db);
    bool found() const { return found_; }
    const std::string& status() const { return status_; }

    static std::filesystem::path pdb_for(const std::filesystem::path& binary);

private:
    bool found_ = false;
    std::string status_;
};

}
