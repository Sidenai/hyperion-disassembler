#pragma once
#include "core/loader/pe_loader.h"
#include "analysis_db.h"
#include <vector>
#include <string>

namespace hype {

struct RTTIClass {
    std::string   mangled_name;
    std::string   demangled_name;
    va_t          type_descriptor = 0;
    va_t          complete_locator = 0;
    va_t          vtable = 0;
    std::vector<va_t> methods;
};

class RTTIParser {
public:
    void parse(PEImage& img, AnalysisDB& db);
    const std::vector<RTTIClass>& classes() const { return classes_; }

private:
    void find_type_descriptors(PEImage& img);
    void find_vtables(PEImage& img, AnalysisDB& db);
    void name_methods(PEImage& img, AnalysisDB& db);

    std::vector<RTTIClass> classes_;
};

}
