#include "flirt_loader.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <cstring>
#include <zlib.h>

namespace hype {

void FlirtLoader::load_directory(const std::filesystem::path& dir) {
    if (!std::filesystem::exists(dir)) return;

    for (auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() != ".sig") continue;
        FlirtSigInfo info;
        info.filename = entry.path().filename().string();
        if (parse_file(entry.path(), info))
            sigs_.push_back(std::move(info));
    }
    spdlog::info("flirt: loaded {} signature files, {} total names",
        sigs_.size(), total_names());
}

int FlirtLoader::total_names() const {
    int n = 0;
    for (auto& s : sigs_) n += static_cast<int>(s.extracted_names.size());
    return n;
}

std::string FlirtLoader::arch_str(u8 arch) {
    switch (arch) {
    case 0:  return "x86";
    case 1:  return "z80";
    case 2:  return "i860";
    case 3:  return "8051";
    case 4:  return "tms";
    case 5:  return "6502";
    case 6:  return "pdp";
    case 7:  return "68k";
    case 8:  return "java";
    case 9:  return "6800";
    case 10: return "st7";
    case 11: return "mc6812";
    case 12: return "mips";
    case 13: return "arm";
    case 14: return "tmsc6";
    case 15: return "ppc";
    case 16: return "80196";
    case 17: return "z8";
    case 18: return "sh";
    case 19: return "net";
    case 20: return "avr";
    case 21: return "h8";
    case 22: return "pic";
    case 23: return "sparc";
    case 24: return "alpha";
    case 25: return "hppa";
    case 26: return "h8500";
    case 27: return "tricore";
    case 28: return "dsp56k";
    case 29: return "c166";
    case 30: return "st20";
    case 31: return "ia64";
    case 32: return "i960";
    case 33: return "f2mc";
    case 34: return "tms320c54";
    case 35: return "tms320c55";
    case 36: return "trimedia";
    case 37: return "m32r";
    case 38: return "nec78k";
    case 39: return "ceva";
    default: return "unk(" + std::to_string(arch) + ")";
    }
}

std::string FlirtLoader::file_types_str(u16 ft) {
    std::string r;
    if (ft & 0x0001) r += "dos ";
    if (ft & 0x0002) r += "ne ";
    if (ft & 0x0004) r += "pe ";
    if (ft & 0x0008) r += "elf ";
    if (ft & 0x0010) r += "coff ";
    if (ft & 0x0020) r += "aout ";
    if (ft & 0x0040) r += "nlm ";
    if (ft & 0x0080) r += "macho ";
    if (r.empty()) r = "any";
    else r.pop_back();
    return r;
}

bool FlirtLoader::parse_file(const std::filesystem::path& path, FlirtSigInfo& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    f.seekg(0, std::ios::end);
    auto fsize = f.tellg();
    if (fsize < 20) return false;
    f.seekg(0);

    std::vector<u8> buf(static_cast<size_t>(fsize));
    f.read(reinterpret_cast<char*>(buf.data()), fsize);

    if (std::memcmp(buf.data(), "IDASGN", 6) != 0) return false;

    out.version    = buf[6];
    out.arch       = buf[7];
    out.file_types = buf[8] | (buf[9] << 8);
    out.os_types   = buf[10] | (buf[11] << 8);
    out.app_types  = buf[12] | (buf[13] << 8);
    out.features   = buf[14] | (buf[15] << 8);
    out.n_modules  = buf[16];
    out.crc16      = buf[17] | (buf[18] << 8);

    size_t pos = 19;

    // skip crc string (null-terminated)
    while (pos < buf.size() && buf[pos] != 0) ++pos;
    if (pos >= buf.size()) { out.valid = true; return true; }
    ++pos; // skip null

    // library name length + name
    if (pos >= buf.size()) { out.valid = true; return true; }
    u8 lib_name_len = buf[pos++];
    if (pos + lib_name_len > buf.size()) { out.valid = true; return true; }
    out.library_name.assign(reinterpret_cast<char*>(buf.data() + pos), lib_name_len);
    pos += lib_name_len;

    // version 6+ has additional fields before compressed data
    if (out.version >= 8) {
        if (pos + 2 > buf.size()) { out.valid = true; return true; }
        u16 n_modules_hi = buf[pos] | (buf[pos+1] << 8);
        if (out.n_modules == 0) out.n_modules = n_modules_hi;
        pos += 2;
    }
    if (out.version >= 10) {
        if (pos + 4 > buf.size()) { out.valid = true; return true; }
        pos += 4; // pattern_size(2) + flags2(2)
    }

    out.valid = true;

    // try to decompress remainder and extract names
    if (pos < buf.size() && out.version >= 5) {
        decompress_and_extract_names(buf.data() + pos, buf.size() - pos, out);
    }

    return true;
}

bool FlirtLoader::decompress_and_extract_names(const u8* data, size_t len, FlirtSigInfo& out) {
    if (len < 2) return false;

    std::vector<u8> decompressed;
    decompressed.resize(len * 20); // generous estimate

    z_stream zs{};
    zs.next_in = const_cast<Bytef*>(data);
    zs.avail_in = static_cast<uInt>(len);
    zs.next_out = decompressed.data();
    zs.avail_out = static_cast<uInt>(decompressed.size());

    if (inflateInit(&zs) != Z_OK) return false;

    int ret = inflate(&zs, Z_FINISH);
    if (ret != Z_STREAM_END && ret != Z_OK && ret != Z_BUF_ERROR) {
        inflateEnd(&zs);
        // try raw deflate
        zs = {};
        zs.next_in = const_cast<Bytef*>(data);
        zs.avail_in = static_cast<uInt>(len);
        zs.next_out = decompressed.data();
        zs.avail_out = static_cast<uInt>(decompressed.size());
        if (inflateInit2(&zs, -15) != Z_OK) return false;
        ret = inflate(&zs, Z_FINISH);
        if (ret != Z_STREAM_END && ret != Z_OK) {
            inflateEnd(&zs);
            return false;
        }
    }

    size_t decomp_size = zs.total_out;
    inflateEnd(&zs);
    decompressed.resize(decomp_size);

    // scan for ASCII function name strings
    auto is_name_char = [](u8 c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '_' || c == '@' || c == '?' || c == '$';
    };
    auto is_name_start = [](u8 c) {
        return c == '_' || c == '?' || c == '@' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    };

    size_t i = 0;
    while (i < decomp_size) {
        if (is_name_start(decompressed[i])) {
            size_t start = i;
            while (i < decomp_size && is_name_char(decompressed[i])) ++i;
            size_t name_len = i - start;
            if (name_len >= 4 && name_len <= 200) {
                std::string name(reinterpret_cast<char*>(decompressed.data() + start), name_len);
                // filter out garbage: must have at least one lowercase or meaningful prefix
                bool good = name[0] == '_' || name[0] == '?' ||
                    (name[0] >= 'a' && name[0] <= 'z') ||
                    (name.size() > 1 && name[1] >= 'a' && name[1] <= 'z');
                if (good) out.extracted_names.push_back(std::move(name));
            }
        } else {
            ++i;
        }
    }

    // deduplicate
    std::sort(out.extracted_names.begin(), out.extracted_names.end());
    out.extracted_names.erase(
        std::unique(out.extracted_names.begin(), out.extracted_names.end()),
        out.extracted_names.end());

    return true;
}

}
