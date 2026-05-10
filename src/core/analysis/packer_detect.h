#pragma once
#include "core/loader/pe_loader.h"
#include <string>
#include <vector>

namespace hype {

struct PackerInfo {
    std::string name;
    float       confidence;
    std::string details;
};

class PackerDetector {
public:
    std::vector<PackerInfo> detect(const PEImage& img);

private:
    bool check_upx(const PEImage& img, std::vector<PackerInfo>& out);
    bool check_themida(const PEImage& img, std::vector<PackerInfo>& out);
    bool check_vmprotect(const PEImage& img, std::vector<PackerInfo>& out);
    bool check_aspack(const PEImage& img, std::vector<PackerInfo>& out);
    bool check_pecompact(const PEImage& img, std::vector<PackerInfo>& out);
    bool check_mpress(const PEImage& img, std::vector<PackerInfo>& out);
    void check_generic(const PEImage& img, std::vector<PackerInfo>& out);

    float section_entropy(const Segment& seg) const;
    bool  entry_in_section(const PEImage& img, const Segment& seg) const;
    int   section_index_of_entry(const PEImage& img) const;
};

}
