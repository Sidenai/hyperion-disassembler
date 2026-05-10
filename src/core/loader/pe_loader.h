#pragma once
#include "core/types.h"
#include <filesystem>
#include <optional>

namespace hype {

struct RuntimeFunc {
    va_t start;
    va_t end;
};

struct PEImage {
    Arch                       arch;
    va_t                       base;
    va_t                       entry;
    std::vector<Segment>       segments;
    std::vector<Import>        imports;
    std::vector<Export>        exports;
    std::vector<RuntimeFunc>   runtime_funcs;
    std::vector<u8>            raw;
};

class PELoader {
public:
    std::optional<PEImage> load(const std::filesystem::path& path);

private:
    bool parse_headers(PEImage& img);
    bool parse_sections(PEImage& img);
    bool parse_imports(PEImage& img);
    bool parse_exports(PEImage& img);
    void parse_exceptions(PEImage& img);

    const u8* base_ = nullptr;
    size_t    size_ = 0;
};

}
