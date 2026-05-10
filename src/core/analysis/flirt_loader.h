#pragma once
#include "core/types.h"
#include <string>
#include <vector>
#include <filesystem>

namespace hype {

struct FlirtSigInfo {
    std::string filename;
    std::string library_name;
    u8  version = 0;
    u8  arch = 0;
    u16 file_types = 0;
    u16 os_types = 0;
    u16 app_types = 0;
    u16 features = 0;
    u32 n_modules = 0;
    u16 crc16 = 0;
    std::vector<std::string> extracted_names;
    bool valid = false;
};

class FlirtLoader {
public:
    void load_directory(const std::filesystem::path& dir);
    const std::vector<FlirtSigInfo>& sigs() const { return sigs_; }
    int total_names() const;

    static std::string arch_str(u8 arch);
    static std::string file_types_str(u16 ft);

private:
    bool parse_file(const std::filesystem::path& path, FlirtSigInfo& out);
    bool decompress_and_extract_names(const u8* data, size_t len, FlirtSigInfo& out);

    std::vector<FlirtSigInfo> sigs_;
};

}
