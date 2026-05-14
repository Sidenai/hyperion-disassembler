#pragma once
#include "core/types.h"
#include <string>
#include <vector>

namespace hype {

struct ProcessEntry {
    u32 pid;
    u32 ppid;
    std::string name;
    std::string path;
    u32 thread_count;
};

std::vector<ProcessEntry> enumerate_processes();

}
