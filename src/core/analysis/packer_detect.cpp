#include "packer_detect.h"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <fmt/format.h>

namespace hype {

namespace {

bool sect_name_match(const Segment& seg, const char* name) {
    return seg.name == name;
}

bool has_section(const PEImage& img, const char* name) {
    for (auto& s : img.segments)
        if (sect_name_match(s, name)) return true;
    return false;
}

bool has_wx_section(const PEImage& img) {
    for (auto& s : img.segments)
        if (s.writable() && s.executable()) return true;
    return false;
}

} // anon

float PackerDetector::section_entropy(const Segment& seg) const {
    if (seg.data.empty()) return 0.f;
    u32 freq[256] = {};
    for (u8 b : seg.data) ++freq[b];
    double ent = 0.0;
    double total = static_cast<double>(seg.data.size());
    for (int i = 0; i < 256; ++i) {
        if (freq[i] == 0) continue;
        double p = freq[i] / total;
        ent -= p * std::log2(p);
    }
    return static_cast<float>(ent);
}

bool PackerDetector::entry_in_section(const PEImage& img, const Segment& seg) const {
    return img.entry >= seg.va && img.entry < seg.va + seg.size;
}

int PackerDetector::section_index_of_entry(const PEImage& img) const {
    for (int i = 0; i < static_cast<int>(img.segments.size()); ++i) {
        if (entry_in_section(img, img.segments[i])) return i;
    }
    return -1;
}

std::vector<PackerInfo> PackerDetector::detect(const PEImage& img) {
    std::vector<PackerInfo> results;
    check_upx(img, results);
    check_themida(img, results);
    check_vmprotect(img, results);
    check_aspack(img, results);
    check_pecompact(img, results);
    check_mpress(img, results);
    if (results.empty())
        check_generic(img, results);
    return results;
}

bool PackerDetector::check_upx(const PEImage& img, std::vector<PackerInfo>& out) {
    bool has_upx_sec = has_section(img, "UPX0") || has_section(img, "UPX1") || has_section(img, ".UPX");
    int ep_idx = section_index_of_entry(img);
    bool ep_last = ep_idx >= 0 && ep_idx == static_cast<int>(img.segments.size()) - 1;
    bool low_imports = img.imports.size() <= 3;

    float conf = 0.f;
    std::string det;
    if (has_upx_sec) { conf += 0.5f; det += "UPX section names; "; }
    if (ep_last)     { conf += 0.25f; det += "EP in last section; "; }
    if (low_imports) { conf += 0.2f; det += fmt::format("{} imports; ", img.imports.size()); }

    if (conf >= 0.4f) {
        conf = (std::min)(conf, 1.f);
        out.push_back({"UPX", conf, det});
        return true;
    }
    return false;
}

bool PackerDetector::check_themida(const PEImage& img, std::vector<PackerInfo>& out) {
    bool has_sec = has_section(img, ".themida") || has_section(img, ".winlice");
    float max_ent = 0.f;
    for (auto& seg : img.segments) {
        if (seg.executable()) max_ent = (std::max)(max_ent, section_entropy(seg));
    }
    bool high_ent = max_ent > 7.8f;

    float conf = 0.f;
    std::string det;
    if (has_sec)  { conf += 0.6f; det += "Themida/WinLicense section; "; }
    if (high_ent) { conf += 0.3f; det += fmt::format("high entropy {:.2f}; ", max_ent); }

    if (conf >= 0.5f) {
        conf = (std::min)(conf, 1.f);
        out.push_back({"Themida/WinLicense", conf, det});
        return true;
    }
    return false;
}

bool PackerDetector::check_vmprotect(const PEImage& img, std::vector<PackerInfo>& out) {
    bool has_sec = has_section(img, ".vmp0") || has_section(img, ".vmp1");
    int ep_idx = section_index_of_entry(img);
    bool ep_non_first = ep_idx > 0;
    float max_ent = 0.f;
    for (auto& seg : img.segments) {
        if (seg.executable()) max_ent = (std::max)(max_ent, section_entropy(seg));
    }
    bool high_ent = max_ent > 7.5f;

    float conf = 0.f;
    std::string det;
    if (has_sec)       { conf += 0.55f; det += "VMProtect section; "; }
    if (ep_non_first)  { conf += 0.2f; det += "EP not in first section; "; }
    if (high_ent)      { conf += 0.2f; det += fmt::format("entropy {:.2f}; ", max_ent); }

    if (conf >= 0.5f) {
        conf = (std::min)(conf, 1.f);
        out.push_back({"VMProtect", conf, det});
        return true;
    }
    return false;
}

bool PackerDetector::check_aspack(const PEImage& img, std::vector<PackerInfo>& out) {
    if (has_section(img, ".aspack") || has_section(img, ".adata")) {
        out.push_back({"ASPack", 0.85f, "ASPack section names"});
        return true;
    }
    return false;
}

bool PackerDetector::check_pecompact(const PEImage& img, std::vector<PackerInfo>& out) {
    if (has_section(img, ".pec") || has_section(img, "PEC2")) {
        out.push_back({"PECompact", 0.85f, "PECompact section names"});
        return true;
    }
    return false;
}

bool PackerDetector::check_mpress(const PEImage& img, std::vector<PackerInfo>& out) {
    if (has_section(img, ".MPRESS1") || has_section(img, ".MPRESS2")) {
        out.push_back({"MPRESS", 0.9f, "MPRESS section names"});
        return true;
    }
    return false;
}

void PackerDetector::check_generic(const PEImage& img, std::vector<PackerInfo>& out) {
    float score = 0.f;
    std::string det;

    int ep_idx = section_index_of_entry(img);
    bool ep_in_text = false;
    if (ep_idx >= 0 && ep_idx < static_cast<int>(img.segments.size()))
        ep_in_text = img.segments[ep_idx].name == ".text";

    if (!ep_in_text && ep_idx >= 0) {
        score += 0.15f;
        det += "EP not in .text; ";
    }

    int high_ent_count = 0;
    for (auto& seg : img.segments) {
        if (seg.data.empty()) continue;
        if (section_entropy(seg) > 7.5f) ++high_ent_count;
    }
    if (high_ent_count > 0 && high_ent_count >= static_cast<int>(img.segments.size()) - 1) {
        score += 0.3f;
        det += fmt::format("{}/{} sections high entropy; ", high_ent_count, img.segments.size());
    }

    if (img.imports.size() < 5) {
        score += 0.2f;
        det += fmt::format("only {} imports; ", img.imports.size());
    }

    if (has_wx_section(img)) {
        score += 0.15f;
        det += "W+X section; ";
    }

    if (score >= 0.4f) {
        score = (std::min)(score, 1.f);
        out.push_back({"Generic packer", score, det});
    }
}

}
