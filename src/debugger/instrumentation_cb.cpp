#include "instrumentation_cb.h"
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <winternl.h>
#include <cstring>

#pragma comment(lib, "ntdll.lib")

namespace hype {

namespace {

using NtQueryInformationProcess_t = NTSTATUS(NTAPI*)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);

constexpr u64 PEB_INSTRUMENTATION_CALLBACK_X64 = 0x2D8;
constexpr u32 MAX_BREAKPOINTS = 256;
constexpr u32 SHELLCODE_SIZE = 4096;
constexpr u32 BP_LIST_SIZE = 8 + MAX_BREAKPOINTS * 8;
constexpr u32 SIGNAL_SIZE = 64;

#pragma pack(push, 1)
struct BpListHeader {
    u32 count;
    u32 pad;
};

struct SignalData {
    u64 hit_addr;
    u32 hit_flag;
    u32 reserved;
};
#pragma pack(pop)

// x64 shellcode for the instrumentation callback
// RCX = return RIP, R10 = original RSP on entry
alignas(16) const u8 k_shellcode_template[] = {
    // save volatile regs
    0x50,                                           // push rax
    0x51,                                           // push rcx
    0x52,                                           // push rdx
    0x41, 0x50,                                     // push r8
    0x41, 0x51,                                     // push r9
    0x41, 0x52,                                     // push r10
    0x41, 0x53,                                     // push r11

    // mov rax, imm64 (bp_list_addr - patched)
    0x48, 0xB8,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // [13..20] bp_list_addr

    // mov edx, [rax]  (count)
    0x8B, 0x10,
    // lea r8, [rax+8]  (&list[0])
    0x4C, 0x8D, 0x40, 0x08,

    // xor r9d, r9d
    0x45, 0x31, 0xC9,

    // .loop:
    0x44, 0x39, 0xCA,                               // cmp edx, r9d
    0x7E, 0x0E,                                     // jle .no_hit (+14)
    0x49, 0x39, 0x0C, 0xC8,                         // cmp [r8 + r9*8], rcx
    0x74, 0x07,                                     // je .hit (+7)
    0x41, 0xFF, 0xC1,                               // inc r9d
    0xEB, 0xF0,                                     // jmp .loop (-16)

    // .hit:
    // mov rax, imm64 (signal_addr - patched)
    0x48, 0xB8,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // [47..54] signal_addr

    // mov [rax], rcx
    0x48, 0x89, 0x08,
    // mov dword [rax+8], 1
    0xC7, 0x40, 0x08, 0x01, 0x00, 0x00, 0x00,

    // .spin:
    0xF3, 0x90,                                     // pause
    0x83, 0x78, 0x08, 0x00,                         // cmp dword [rax+8], 0
    0x75, 0xF8,                                     // jne .spin

    // .no_hit:
    0x41, 0x5B,                                     // pop r11
    0x41, 0x5A,                                     // pop r10
    0x41, 0x59,                                     // pop r9
    0x41, 0x58,                                     // pop r8
    0x5A,                                           // pop rdx
    0x59,                                           // pop rcx
    0x58,                                           // pop rax

    // jmp rcx (return to original RIP)
    0xFF, 0xE1,
};

constexpr size_t BP_LIST_ADDR_OFFSET = 13;
constexpr size_t SIGNAL_ADDR_OFFSET = 47;

}

InstrumentationBreakpoints::~InstrumentationBreakpoints() {
    if (installed_) uninstall();
}

bool InstrumentationBreakpoints::install(HANDLE process, u32 pid) {
    if (installed_) return false;
    process_ = process;
    pid_ = pid;

    auto alloc = [&](size_t sz) -> va_t {
        void* p = VirtualAllocEx(process_, nullptr, sz,
            MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        return (va_t)p;
    };

    callback_addr_ = alloc(SHELLCODE_SIZE);
    bp_list_addr_ = alloc(BP_LIST_SIZE);
    signal_addr_ = alloc(SIGNAL_SIZE);

    if (!callback_addr_ || !bp_list_addr_ || !signal_addr_) {
        uninstall();
        return false;
    }

    BpListHeader hdr{0, 0};
    WriteProcessMemory(process_, (void*)bp_list_addr_, &hdr, sizeof(hdr), nullptr);

    SignalData sig{0, 0, 0};
    WriteProcessMemory(process_, (void*)signal_addr_, &sig, sizeof(sig), nullptr);

    if (!write_shellcode()) { uninstall(); return false; }
    if (!patch_peb_callback()) { uninstall(); return false; }

    installed_ = true;
    spdlog::info("[ICB] Installed: callback={:016X} bplist={:016X} signal={:016X}",
        callback_addr_, bp_list_addr_, signal_addr_);
    return true;
}

void InstrumentationBreakpoints::uninstall() {
    if (process_ && original_icb_ != callback_addr_) {
        auto NtQIP = (NtQueryInformationProcess_t)
            GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationProcess");
        if (NtQIP) {
            PROCESS_BASIC_INFORMATION pbi{};
            ULONG ret_len = 0;
            if (NtQIP(process_, ProcessBasicInformation, &pbi, sizeof(pbi), &ret_len) == 0) {
                va_t peb = (va_t)pbi.PebBaseAddress;
                WriteProcessMemory(process_, (void*)(peb + PEB_INSTRUMENTATION_CALLBACK_X64),
                    &original_icb_, 8, nullptr);
            }
        }
    }

    if (callback_addr_) { VirtualFreeEx(process_, (void*)callback_addr_, 0, MEM_RELEASE); callback_addr_ = 0; }
    if (bp_list_addr_) { VirtualFreeEx(process_, (void*)bp_list_addr_, 0, MEM_RELEASE); bp_list_addr_ = 0; }
    if (signal_addr_) { VirtualFreeEx(process_, (void*)signal_addr_, 0, MEM_RELEASE); signal_addr_ = 0; }

    installed_ = false;
    breakpoints_.clear();
    hit_addr_ = 0;
}

bool InstrumentationBreakpoints::add_breakpoint(va_t addr) {
    for (auto a : breakpoints_)
        if (a == addr) return true;
    if (breakpoints_.size() >= MAX_BREAKPOINTS) return false;

    breakpoints_.push_back(addr);
    return update_bp_list();
}

bool InstrumentationBreakpoints::remove_breakpoint(va_t addr) {
    auto it = std::find(breakpoints_.begin(), breakpoints_.end(), addr);
    if (it == breakpoints_.end()) return false;
    breakpoints_.erase(it);
    return update_bp_list();
}

bool InstrumentationBreakpoints::check_hit() {
    SignalData sig{};
    SIZE_T read = 0;
    if (!ReadProcessMemory(process_, (void*)signal_addr_, &sig, sizeof(sig), &read))
        return false;
    if (sig.hit_flag) {
        hit_addr_ = sig.hit_addr;
        return true;
    }
    return false;
}

void InstrumentationBreakpoints::resume() {
    u32 zero = 0;
    WriteProcessMemory(process_, (void*)(signal_addr_ + offsetof(SignalData, hit_flag)),
        &zero, sizeof(zero), nullptr);
}

bool InstrumentationBreakpoints::write_shellcode() {
    u8 code[sizeof(k_shellcode_template)];
    std::memcpy(code, k_shellcode_template, sizeof(code));

    std::memcpy(&code[BP_LIST_ADDR_OFFSET], &bp_list_addr_, 8);
    std::memcpy(&code[SIGNAL_ADDR_OFFSET], &signal_addr_, 8);

    SIZE_T written = 0;
    if (!WriteProcessMemory(process_, (void*)callback_addr_, code, sizeof(code), &written))
        return false;
    FlushInstructionCache(process_, (void*)callback_addr_, sizeof(code));
    return true;
}

bool InstrumentationBreakpoints::patch_peb_callback() {
    auto NtQIP = (NtQueryInformationProcess_t)
        GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationProcess");
    if (!NtQIP) return false;

    PROCESS_BASIC_INFORMATION pbi{};
    ULONG ret_len = 0;
    if (NtQIP(process_, ProcessBasicInformation, &pbi, sizeof(pbi), &ret_len) != 0)
        return false;

    va_t peb = (va_t)pbi.PebBaseAddress;
    va_t icb_field = peb + PEB_INSTRUMENTATION_CALLBACK_X64;

    ReadProcessMemory(process_, (void*)icb_field, &original_icb_, 8, nullptr);
    return WriteProcessMemory(process_, (void*)icb_field, &callback_addr_, 8, nullptr) != 0;
}

bool InstrumentationBreakpoints::update_bp_list() {
    BpListHeader hdr{(u32)breakpoints_.size(), 0};
    SIZE_T written = 0;

    if (!WriteProcessMemory(process_, (void*)bp_list_addr_, &hdr, sizeof(hdr), &written))
        return false;

    if (!breakpoints_.empty()) {
        if (!WriteProcessMemory(process_, (void*)(bp_list_addr_ + 8),
            breakpoints_.data(), breakpoints_.size() * sizeof(va_t), &written))
            return false;
    }
    return true;
}

}
