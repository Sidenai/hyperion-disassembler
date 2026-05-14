#pragma once
#include "core/types.h"
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace hype {

class InstrumentationBreakpoints {
public:
    ~InstrumentationBreakpoints();

    bool install(HANDLE process, u32 pid);
    void uninstall();
    bool is_installed() const { return installed_; }

    bool add_breakpoint(va_t addr);
    bool remove_breakpoint(va_t addr);

    bool check_hit();
    void resume();
    va_t hit_address() const { return hit_addr_; }

    const std::vector<va_t>& breakpoints() const { return breakpoints_; }

private:
    bool write_shellcode();
    bool patch_peb_callback();
    bool update_bp_list();

    HANDLE process_ = nullptr;
    u32 pid_ = 0;
    bool installed_ = false;

    va_t callback_addr_ = 0;
    va_t bp_list_addr_ = 0;
    va_t signal_addr_ = 0;
    va_t original_icb_ = 0;

    va_t hit_addr_ = 0;
    std::vector<va_t> breakpoints_;
};

}
