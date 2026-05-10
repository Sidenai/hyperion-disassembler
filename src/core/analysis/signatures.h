#pragma once
#include "core/types.h"
#include "analysis_db.h"
#include "core/loader/pe_loader.h"
#include "flirt_loader.h"
#include <vector>
#include <string>

namespace hype {

struct Signature {
    std::string name;
    std::string library;
    std::vector<u8> pattern;
    std::vector<u8> mask;   // 0xFF = exact, 0x00 = wildcard
};

class SignatureMatcher {
public:
    SignatureMatcher();

    int match_functions(AnalysisDB& db, const PEImage& img);
    int sig_count() const { return static_cast<int>(sigs_.size()); }
    FlirtLoader& flirt() { return flirt_; }
    const FlirtLoader& flirt() const { return flirt_; }

private:
    bool match_one(const u8* data, size_t len, const Signature& sig) const;
    const u8* func_bytes(va_t addr, const PEImage& img, size_t* out_len) const;

    std::vector<Signature> sigs_;
    FlirtLoader flirt_;
};

}
