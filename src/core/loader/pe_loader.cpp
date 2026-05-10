#include "pe_loader.h"
#include <fstream>
#include <cstring>
#include <spdlog/spdlog.h>
#include <fmt/format.h>

namespace hype {

namespace {

#pragma pack(push, 1)
struct DosHdr {
    u16 magic;
    u8  pad[58];
    u32 lfanew;
};

struct CoffHdr {
    u16 machine;
    u16 num_sections;
    u32 timestamp;
    u32 sym_table;
    u32 num_symbols;
    u16 opt_size;
    u16 characteristics;
};

struct DataDir {
    u32 rva;
    u32 size;
};

struct OptHdr64 {
    u16 magic;
    u8  link_maj, link_min;
    u32 code_sz, idata_sz, udata_sz;
    u32 entry_rva, code_base;
    u64 image_base;
    u32 sect_align, file_align;
    u16 os_maj, os_min, img_maj, img_min, sub_maj, sub_min;
    u32 win32ver;
    u32 image_sz, hdr_sz, checksum;
    u16 subsys, dll_chars;
    u64 stack_res, stack_com, heap_res, heap_com;
    u32 loader_flags, num_dd;
    DataDir dd[16];
};

struct OptHdr32 {
    u16 magic;
    u8  link_maj, link_min;
    u32 code_sz, idata_sz, udata_sz;
    u32 entry_rva, code_base, data_base;
    u32 image_base;
    u32 sect_align, file_align;
    u16 os_maj, os_min, img_maj, img_min, sub_maj, sub_min;
    u32 win32ver;
    u32 image_sz, hdr_sz, checksum;
    u16 subsys, dll_chars;
    u32 stack_res, stack_com, heap_res, heap_com;
    u32 loader_flags, num_dd;
    DataDir dd[16];
};

struct SectHdr {
    char name[8];
    u32  vsize;
    u32  vrva;
    u32  raw_sz;
    u32  raw_ptr;
    u32  reloc_ptr;
    u32  line_ptr;
    u16  num_relocs;
    u16  num_lines;
    u32  chars;
};

struct ImpDesc {
    u32 orig_thunk;
    u32 timestamp;
    u32 forwarder;
    u32 name_rva;
    u32 thunk;
};

struct ExpDir {
    u32 chars, timestamp;
    u16 maj, min;
    u32 name_rva, ord_base;
    u32 num_funcs, num_names;
    u32 funcs_rva, names_rva, ords_rva;
};
#pragma pack(pop)

template<typename T>
const T* ptr_at(const u8* base, size_t off, size_t total) {
    if (off + sizeof(T) > total) return nullptr;
    return reinterpret_cast<const T*>(base + off);
}

u32 rva_to_raw(const PEImage& img, u32 rva) {
    for (auto& seg : img.segments) {
        u32 seg_rva = static_cast<u32>(seg.va - img.base);
        if (rva >= seg_rva && rva < seg_rva + seg.file_sz)
            return static_cast<u32>(seg.file_off + (rva - seg_rva));
    }
    return 0;
}

} // anon

std::optional<PEImage> PELoader::load(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        spdlog::error("cannot open: {}", path.string());
        return std::nullopt;
    }

    PEImage img;
    img.raw.resize(static_cast<size_t>(f.tellg()));
    f.seekg(0);
    f.read(reinterpret_cast<char*>(img.raw.data()), img.raw.size());

    base_ = img.raw.data();
    size_ = img.raw.size();

    if (!parse_headers(img)) return std::nullopt;
    if (!parse_sections(img)) return std::nullopt;
    parse_imports(img);
    parse_exports(img);

    spdlog::info("PE: {} segs, {} imports, {} exports",
                 img.segments.size(), img.imports.size(), img.exports.size());
    return img;
}

bool PELoader::parse_headers(PEImage& img) {
    auto dos = ptr_at<DosHdr>(base_, 0, size_);
    if (!dos || dos->magic != 0x5A4D) return false;

    u32 pe_off = dos->lfanew;
    if (pe_off + 4 > size_) return false;
    if (std::memcmp(base_ + pe_off, "PE\0\0", 4) != 0) return false;

    auto coff = ptr_at<CoffHdr>(base_, pe_off + 4, size_);
    if (!coff) return false;

    u32 opt_off = pe_off + 4 + sizeof(CoffHdr);
    u16 magic = 0;
    if (opt_off + 2 <= size_)
        std::memcpy(&magic, base_ + opt_off, 2);

    if (magic == 0x20B) {
        img.arch = Arch::X64;
        auto opt = ptr_at<OptHdr64>(base_, opt_off, size_);
        if (!opt) return false;
        img.base = opt->image_base;
        img.entry = img.base + opt->entry_rva;
    } else if (magic == 0x10B) {
        img.arch = Arch::X86;
        auto opt = ptr_at<OptHdr32>(base_, opt_off, size_);
        if (!opt) return false;
        img.base = opt->image_base;
        img.entry = img.base + opt->entry_rva;
    } else {
        spdlog::error("unknown PE optional magic: 0x{:X}", magic);
        return false;
    }
    return true;
}

bool PELoader::parse_sections(PEImage& img) {
    auto dos = ptr_at<DosHdr>(base_, 0, size_);
    u32 pe_off = dos->lfanew;
    auto coff = ptr_at<CoffHdr>(base_, pe_off + 4, size_);
    u32 sec_off = pe_off + 4 + sizeof(CoffHdr) + coff->opt_size;

    for (u16 i = 0; i < coff->num_sections; ++i) {
        auto sh = ptr_at<SectHdr>(base_, sec_off + i * sizeof(SectHdr), size_);
        if (!sh) break;

        Segment seg;
        seg.name.assign(sh->name, strnlen(sh->name, 8));
        seg.va = img.base + sh->vrva;
        seg.size = sh->vsize;
        seg.file_off = sh->raw_ptr;
        seg.file_sz = sh->raw_sz;
        seg.flags = sh->chars;

        if (sh->raw_ptr + sh->raw_sz <= size_) {
            seg.data.assign(base_ + sh->raw_ptr, base_ + sh->raw_ptr + sh->raw_sz);
            if (sh->vsize > sh->raw_sz)
                seg.data.resize(sh->vsize, 0);
        }
        img.segments.push_back(std::move(seg));
    }
    return true;
}

bool PELoader::parse_imports(PEImage& img) {
    auto dos = ptr_at<DosHdr>(base_, 0, size_);
    u32 pe_off = dos->lfanew;
    u32 opt_off = pe_off + 4 + sizeof(CoffHdr);
    u16 magic = 0;
    std::memcpy(&magic, base_ + opt_off, 2);

    DataDir idir{};
    if (magic == 0x20B) {
        auto opt = ptr_at<OptHdr64>(base_, opt_off, size_);
        if (!opt || opt->num_dd < 2) return false;
        idir = opt->dd[1];
    } else {
        auto opt = ptr_at<OptHdr32>(base_, opt_off, size_);
        if (!opt || opt->num_dd < 2) return false;
        idir = opt->dd[1];
    }
    if (idir.rva == 0) return true;

    u32 off = rva_to_raw(img, idir.rva);
    if (!off) return false;
    bool x64 = (img.arch == Arch::X64);

    for (;;) {
        auto desc = ptr_at<ImpDesc>(base_, off, size_);
        if (!desc || (!desc->orig_thunk && !desc->thunk)) break;

        u32 nm_off = rva_to_raw(img, desc->name_rva);
        std::string dll;
        if (nm_off && nm_off < size_)
            dll = reinterpret_cast<const char*>(base_ + nm_off);

        u32 thunk_rva = desc->orig_thunk ? desc->orig_thunk : desc->thunk;
        u32 iat_rva = desc->thunk;
        u32 t_off = rva_to_raw(img, thunk_rva);

        while (t_off && t_off < size_) {
            u64 entry = 0;
            if (x64) {
                if (t_off + 8 > size_) break;
                std::memcpy(&entry, base_ + t_off, 8);
                t_off += 8;
            } else {
                u32 e32 = 0;
                if (t_off + 4 > size_) break;
                std::memcpy(&e32, base_ + t_off, 4);
                entry = e32;
                t_off += 4;
            }
            if (!entry) break;

            Import imp;
            imp.dll = dll;
            imp.iat_addr = img.base + iat_rva;
            iat_rva += x64 ? 8 : 4;

            u64 ord_flag = x64 ? (1ULL << 63) : (1ULL << 31);
            if (entry & ord_flag) {
                imp.ordinal = static_cast<u16>(entry & 0xFFFF);
                imp.name = fmt::format("ord_{}", imp.ordinal);
            } else {
                u32 h_off = rva_to_raw(img, static_cast<u32>(entry));
                if (h_off + 2 < size_) {
                    std::memcpy(&imp.ordinal, base_ + h_off, 2);
                    imp.name = reinterpret_cast<const char*>(base_ + h_off + 2);
                }
            }
            img.imports.push_back(std::move(imp));
        }
        off += sizeof(ImpDesc);
    }
    return true;
}

bool PELoader::parse_exports(PEImage& img) {
    auto dos = ptr_at<DosHdr>(base_, 0, size_);
    u32 pe_off = dos->lfanew;
    u32 opt_off = pe_off + 4 + sizeof(CoffHdr);
    u16 magic = 0;
    std::memcpy(&magic, base_ + opt_off, 2);

    DataDir edir{};
    if (magic == 0x20B) {
        auto opt = ptr_at<OptHdr64>(base_, opt_off, size_);
        if (!opt || opt->num_dd < 1) return false;
        edir = opt->dd[0];
    } else {
        auto opt = ptr_at<OptHdr32>(base_, opt_off, size_);
        if (!opt || opt->num_dd < 1) return false;
        edir = opt->dd[0];
    }
    if (edir.rva == 0) return true;

    u32 off = rva_to_raw(img, edir.rva);
    if (!off) return false;

    auto dir = ptr_at<ExpDir>(base_, off, size_);
    if (!dir) return false;

    u32 funcs_off = rva_to_raw(img, dir->funcs_rva);
    u32 names_off = rva_to_raw(img, dir->names_rva);
    u32 ords_off  = rva_to_raw(img, dir->ords_rva);

    for (u32 i = 0; i < dir->num_names && names_off && ords_off; ++i) {
        u32 name_rva = 0;
        u16 ord_idx = 0;
        std::memcpy(&name_rva, base_ + names_off + i * 4, 4);
        std::memcpy(&ord_idx, base_ + ords_off + i * 2, 2);

        u32 func_rva = 0;
        if (funcs_off && ord_idx < dir->num_funcs)
            std::memcpy(&func_rva, base_ + funcs_off + ord_idx * 4, 4);

        Export exp;
        u32 n_off = rva_to_raw(img, name_rva);
        if (n_off && n_off < size_)
            exp.name = reinterpret_cast<const char*>(base_ + n_off);
        exp.ordinal = static_cast<u16>(ord_idx + dir->ord_base);
        exp.addr = img.base + func_rva;
        img.exports.push_back(std::move(exp));
    }
    return true;
}

}
