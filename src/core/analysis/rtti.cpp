#include "rtti.h"
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <cstring>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#endif

namespace hype {

namespace {

const u8* va_to_ptr(PEImage& img, va_t addr, size_t* max_len = nullptr) {
    for (auto& seg : img.segments) {
        if (seg.contains(addr)) {
            size_t off = static_cast<size_t>(addr - seg.va);
            if (max_len) *max_len = seg.data.size() - off;
            return seg.data.data() + off;
        }
    }
    return nullptr;
}

bool is_code_addr(PEImage& img, va_t addr) {
    for (auto& seg : img.segments)
        if (seg.executable() && seg.contains(addr)) return true;
    return false;
}

const Segment* find_rdata(PEImage& img) {
    for (auto& seg : img.segments)
        if (seg.name == ".rdata" && !seg.data.empty()) return &seg;
    return nullptr;
}

std::string demangle_rtti(const std::string& mangled) {
#ifdef _WIN32
    std::string decorated = "?" + mangled.substr(4);
    char buf[1024];
    DWORD r = UnDecorateSymbolName(decorated.c_str(), buf, sizeof(buf),
        UNDNAME_NAME_ONLY);
    if (r > 0) return buf;
#endif
    auto pos = mangled.find("@@");
    if (pos == std::string::npos) return mangled;
    std::string raw = mangled.substr(4, pos - 4);

    std::string result;
    size_t i = raw.size();
    while (i > 0) {
        auto at = raw.rfind('@', i - 1);
        if (at == std::string::npos) {
            result += raw.substr(0, i);
            break;
        }
        result += raw.substr(at + 1, i - at - 1) + "::";
        i = at;
    }
    return result;
}

} // namespace

void RTTIParser::parse(PEImage& img, AnalysisDB& db) {
    if (img.arch != Arch::X64) return;

    find_type_descriptors(img);
    if (classes_.empty()) return;

    find_vtables(img, db);
    name_methods(img, db);

    spdlog::info("RTTI: found {} classes", classes_.size());
}

void RTTIParser::find_type_descriptors(PEImage& img) {
    constexpr std::string_view kPattern = ".?AV";

    for (auto& seg : img.segments) {
        if (seg.executable() || seg.data.empty()) continue;
        if (seg.name != ".rdata" && seg.name != ".data") continue;

        const u8* data = seg.data.data();
        size_t sz = seg.data.size();

        for (size_t i = 0; i + 24 < sz; ++i) {
            if (data[i] != '.' || data[i + 1] != '?' ||
                data[i + 2] != 'A' || data[i + 3] != 'V')
                continue;

            size_t end = i + 4;
            while (end < sz && data[end] != 0) ++end;
            if (end >= sz) continue;
            if (end - i < 8) continue;

            std::string name(reinterpret_cast<const char*>(data + i), end - i);
            if (name.find("@@") == std::string::npos) continue;

            // type descriptor starts 2 * sizeof(void*) before the name
            va_t td_addr = seg.va + i - 16;

            RTTIClass cls;
            cls.mangled_name = name;
            cls.demangled_name = demangle_rtti(name);
            cls.type_descriptor = td_addr;
            classes_.push_back(std::move(cls));
        }
    }
}

void RTTIParser::find_vtables(PEImage& img, [[maybe_unused]] AnalysisDB& db) {
    const Segment* rdata = find_rdata(img);
    if (!rdata) return;

    // Map: type_descriptor_rva -> class index
    std::unordered_map<u32, size_t> td_rva_to_idx;
    for (size_t i = 0; i < classes_.size(); ++i) {
        u32 rva = static_cast<u32>(classes_[i].type_descriptor - img.base);
        td_rva_to_idx[rva] = i;
    }

    const u8* data = rdata->data.data();
    size_t sz = rdata->data.size();

    // Find Complete Object Locators by scanning for signature + known td RVA
    // COL layout (x64): [sig:4][offset:4][cdOffset:4][pTypeDesc:4][pClassHier:4][pSelf:4] = 24 bytes
    struct COLEntry { va_t col_addr; size_t class_idx; };
    std::vector<COLEntry> cols;

    for (size_t i = 0; i + 24 <= sz; i += 4) {
        u32 sig = 0;
        std::memcpy(&sig, data + i, 4);
        if (sig != 1) continue;

        u32 td_rva = 0;
        std::memcpy(&td_rva, data + i + 12, 4);

        auto it = td_rva_to_idx.find(td_rva);
        if (it == td_rva_to_idx.end()) continue;

        u32 self_rva = 0;
        std::memcpy(&self_rva, data + i + 20, 4);
        u32 expected_self = static_cast<u32>((rdata->va + i) - img.base);
        if (self_rva != expected_self) continue;

        va_t col_addr = rdata->va + i;
        cols.push_back({col_addr, it->second});
    }

    // For each COL, find the pointer to it in .rdata (vtable[-1])
    // The qword AFTER that pointer is the vtable start
    std::unordered_map<va_t, size_t> col_addr_to_idx;
    for (auto& [addr, idx] : cols)
        col_addr_to_idx[addr] = idx;

    for (size_t i = 0; i + 8 <= sz; i += 8) {
        va_t val = 0;
        std::memcpy(&val, data + i, 8);

        auto it = col_addr_to_idx.find(val);
        if (it == col_addr_to_idx.end()) continue;

        va_t vtable_addr = rdata->va + i + 8;
        size_t cls_idx = it->second;

        if (classes_[cls_idx].vtable != 0) continue;
        classes_[cls_idx].complete_locator = val;
        classes_[cls_idx].vtable = vtable_addr;

        // Read method pointers from vtable
        size_t remaining = sz - (i + 8);
        const u8* vt_data = data + i + 8;

        for (size_t j = 0; j + 8 <= remaining; j += 8) {
            va_t method = 0;
            std::memcpy(&method, vt_data + j, 8);
            if (!is_code_addr(img, method)) break;
            classes_[cls_idx].methods.push_back(method);
        }
    }

    // Remove classes where we couldn't find a vtable
    std::erase_if(classes_, [](const RTTIClass& c) { return c.vtable == 0; });
}

void RTTIParser::name_methods([[maybe_unused]] PEImage& img, AnalysisDB& db) {
    for (auto& cls : classes_) {
        db.set_name(cls.vtable, cls.demangled_name + "::vftable");

        for (size_t i = 0; i < cls.methods.size(); ++i) {
            va_t method = cls.methods[i];

            auto existing = db.names.find(method);
            if (existing != db.names.end() && !existing->second.empty() &&
                existing->second.rfind("sub_", 0) != 0) {
                std::string cur = existing->second;
                if (cur.find("::") == std::string::npos)
                    db.set_name(method, cls.demangled_name + "::" + cur);
            } else {
                db.set_name(method, fmt::format("{}::vfunc_{}", cls.demangled_name, i));
            }

            auto fit = db.funcs.find(method);
            if (fit != db.funcs.end()) {
                auto name_it = db.names.find(method);
                if (name_it != db.names.end())
                    fit->second.name = name_it->second;
            }
        }
    }
}

}
