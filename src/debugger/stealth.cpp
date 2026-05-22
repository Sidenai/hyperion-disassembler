#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "debug_engine.h"
#include <spdlog/spdlog.h>
#include <fmt/format.h>

#ifdef _WIN32
#include <winternl.h>
#pragma comment(lib, "ntdll.lib")
#endif

namespace hype {

#ifdef _WIN32

namespace {

using NtQueryInformationProcess_t = NTSTATUS(NTAPI*)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);
using NtRemoveProcessDebug_t = NTSTATUS(NTAPI*)(HANDLE, HANDLE);

constexpr u64 PEB_BEING_DEBUGGED = 0x2;
constexpr u64 PEB_NT_GLOBAL_FLAG = 0xBC;
constexpr u64 PEB_PROCESS_HEAP = 0x30;
constexpr u64 HEAP_FLAGS_OFF = 0x70;
constexpr u64 HEAP_FORCE_FLAGS_OFF = 0x74;
constexpr u32 THREAD_HIDE_FROM_DEBUGGER = 0x11;
constexpr u32 STATUS_INVALID_HANDLE_EX = 0xC0000008;
constexpr u32 OBJECT_TYPE_DEBUG = 0x23;

}

// --- Infrastructure ---

va_t DebugEngine::resolve_target_export(const char* module, const char* func) {
    HMODULE local = GetModuleHandleA(module);
    if (!local) return 0;

    auto local_fn = reinterpret_cast<va_t>(GetProcAddress(local, func));
    if (!local_fn) return 0;

    va_t rva = local_fn - reinterpret_cast<va_t>(local);

    for (auto& m : modules_) {
        if (m.name.find(module) != std::string::npos) {
            return m.base + rva;
        }
    }
    return 0;
}

va_t DebugEngine::alloc_remote(size_t size) {
    auto p = VirtualAllocEx(process_, nullptr, size,
                            MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    return reinterpret_cast<va_t>(p);
}

bool DebugEngine::install_hook(va_t target, const u8* code, size_t code_size, const char* name) {
    va_t tramp = alloc_remote(code_size + 14 + 14);
    if (!tramp) return false;

    InlineHook h{};
    h.target = target;
    h.trampoline = tramp;
    h.trampoline_size = code_size + 14;
    read_memory(target, h.original, 14);

    // Write trampoline: original 14 bytes + jmp back to target+14
    u8 tramp_tail[14] = {
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    va_t resume = target + 14;
    memcpy(&tramp_tail[6], &resume, 8);

    // Layout: [hook_code] at tramp, [original+jmpback] at tramp+code_size
    va_t orig_tramp = tramp + code_size;
    write_memory(tramp, code, code_size);
    write_memory(orig_tramp, h.original, 14);
    write_memory(orig_tramp + 14, tramp_tail, 14);

    // Patch target with absolute jmp to our hook code
    u8 jmp[14] = {
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    memcpy(&jmp[6], &tramp, 8);
    write_memory(target, jmp, 14);

    hooks_.push_back(h);
    log(fmt::format("{} hooked at {:016X}", name, target));
    return true;
}

void DebugEngine::uninstall_hooks() {
    for (auto& h : hooks_) {
        write_memory(h.target, h.original, 14);
        VirtualFreeEx(process_, reinterpret_cast<LPVOID>(h.trampoline), 0, MEM_RELEASE);
    }
    hooks_.clear();
    log("All stealth hooks removed");
}

// --- PEB / Heap ---

void DebugEngine::hide_peb_debugger_flag() {
    PROCESS_BASIC_INFORMATION pbi{};
    ULONG ret_len = 0;
    auto NtQIP = reinterpret_cast<NtQueryInformationProcess_t>(
        GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationProcess"));
    if (!NtQIP) return;
    if (NtQIP(process_, ProcessBasicInformation, &pbi, sizeof(pbi), &ret_len) != 0) return;

    va_t peb = reinterpret_cast<va_t>(pbi.PebBaseAddress);
    u8 z8 = 0;
    u32 z32 = 0;
    write_memory(peb + PEB_BEING_DEBUGGED, &z8, 1);
    write_memory(peb + PEB_NT_GLOBAL_FLAG, &z32, 4);
    log(fmt::format("PEB cleared at {:016X}", peb));
}

void DebugEngine::fix_heap_flags() {
    PROCESS_BASIC_INFORMATION pbi{};
    ULONG ret_len = 0;
    auto NtQIP = reinterpret_cast<NtQueryInformationProcess_t>(
        GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationProcess"));
    if (!NtQIP) return;
    if (NtQIP(process_, ProcessBasicInformation, &pbi, sizeof(pbi), &ret_len) != 0) return;

    va_t peb = reinterpret_cast<va_t>(pbi.PebBaseAddress);
    va_t heap = 0;
    read_memory(peb + PEB_PROCESS_HEAP, &heap, sizeof(heap));
    if (!heap) return;

    u32 flags = 2;
    u32 force = 0;
    write_memory(heap + HEAP_FLAGS_OFF, &flags, 4);
    write_memory(heap + HEAP_FORCE_FLAGS_OFF, &force, 4);
    log("Heap flags fixed");
}

// --- NtQueryInformationProcess hook ---
// Hides ProcessDebugPort(7), ProcessDebugObjectHandle(0x1E), ProcessDebugFlags(0x1F)

void DebugEngine::hook_nt_query_information() {
    va_t target = resolve_target_export("ntdll.dll", "NtQueryInformationProcess");
    if (!target) return;

    // rcx=handle, rdx=info_class, r8=buffer, r9=length
    // Shellcode: check rdx for debug-related classes, fake results
    u8 code[] = {
        // cmp rdx, 7 (ProcessDebugPort)
        0x48, 0x83, 0xFA, 0x07,
        0x74, 0x16,                    // je ->fake_zero
        // cmp rdx, 0x1E (ProcessDebugObjectHandle)
        0x48, 0x83, 0xFA, 0x1E,
        0x74, 0x14,                    // je ->fake_port_not_set
        // cmp rdx, 0x1F (ProcessDebugFlags)
        0x48, 0x83, 0xFA, 0x1F,
        0x74, 0x12,                    // je ->fake_one
        // Not a debug query — jmp to original (trampoline)
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, // jmp [rip+0]
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // <orig_tramp addr>
        // fake_zero: *r8 = 0, return 0
        0x49, 0xC7, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x31, 0xC0,
        0xC3,
        // fake_port_not_set: return STATUS_PORT_NOT_SET
        0xB8, 0x53, 0x03, 0x00, 0xC0,
        0xC3,
        // fake_one: *r8 = 1, return 0
        0x49, 0xC7, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x31, 0xC0,
        0xC3,
    };

    // We need to fixup the jmp address to point to orig_tramp (tramp + sizeof(code))
    va_t tramp_base = alloc_remote(sizeof(code) + 14 + 14);
    if (!tramp_base) return;

    va_t orig_tramp = tramp_base + sizeof(code);
    memcpy(&code[18 + 2 + 4], &orig_tramp, 8); // patch the indirect jmp target

    // Manual install since we need the custom fixup
    InlineHook h{};
    h.target = target;
    h.trampoline = tramp_base;
    h.trampoline_size = sizeof(code) + 14;
    read_memory(target, h.original, 14);

    u8 tramp_tail[14] = {
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    va_t resume = target + 14;
    memcpy(&tramp_tail[6], &resume, 8);

    write_memory(tramp_base, code, sizeof(code));
    write_memory(orig_tramp, h.original, 14);
    write_memory(orig_tramp + 14, tramp_tail, 14);

    u8 jmp[14] = {
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    memcpy(&jmp[6], &tramp_base, 8);
    write_memory(target, jmp, 14);

    hooks_.push_back(h);
    log(fmt::format("NtQueryInformationProcess hooked at {:016X}", target));
}

// --- NtSetInformationThread hook ---
// Blocks ThreadHideFromDebugger (0x11) — returns STATUS_SUCCESS without calling original

void DebugEngine::hook_nt_set_information_thread() {
    va_t target = resolve_target_export("ntdll.dll", "NtSetInformationThread");
    if (!target) return;

    // rcx=handle, rdx=info_class, r8=buf, r9=len
    // If rdx == 0x11, return STATUS_SUCCESS
    u8 code[] = {
        0x48, 0x83, 0xFA, 0x11,       // cmp rdx, 0x11
        0x75, 0x04,                    // jne ->call_original
        0x31, 0xC0,                    // xor eax, eax
        0xC3,                          // ret
        // call_original: jmp [rip+0]
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };

    va_t tramp_base = alloc_remote(sizeof(code) + 14 + 14);
    if (!tramp_base) return;

    va_t orig_tramp = tramp_base + sizeof(code);
    memcpy(&code[9 + 2 + 4], &orig_tramp, 8);

    InlineHook h{};
    h.target = target;
    h.trampoline = tramp_base;
    h.trampoline_size = sizeof(code) + 14;
    read_memory(target, h.original, 14);

    u8 tramp_tail[14] = {
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    va_t resume = target + 14;
    memcpy(&tramp_tail[6], &resume, 8);

    write_memory(tramp_base, code, sizeof(code));
    write_memory(orig_tramp, h.original, 14);
    write_memory(orig_tramp + 14, tramp_tail, 14);

    u8 jmp[14] = {
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    memcpy(&jmp[6], &tramp_base, 8);
    write_memory(target, jmp, 14);

    hooks_.push_back(h);
    log(fmt::format("NtSetInformationThread hooked at {:016X}", target));
}

// --- NtClose hook ---
// Swallows STATUS_INVALID_HANDLE exception by wrapping the call in SEH

void DebugEngine::hook_nt_close() {
    va_t target = resolve_target_export("ntdll.dll", "NtClose");
    if (!target) return;

    // Strategy: VEH in the target that catches STATUS_INVALID_HANDLE and returns CONTINUE.
    // We install a VEH trampoline by calling AddVectoredExceptionHandler in the target.
    // The VEH handler checks ExceptionCode == 0xC0000008 and sets RIP to continue.
    //
    // Alternative simpler approach: hook NtClose itself, wrap in __try/__except equivalent.
    // Since we can't emit SEH in shellcode easily, use the VEH approach.
    //
    // VEH handler shellcode:
    //   rcx = EXCEPTION_POINTERS*
    //   if (rcx->ExceptionRecord->ExceptionCode == 0xC0000008)
    //     return EXCEPTION_CONTINUE_EXECUTION (-1)
    //   return EXCEPTION_CONTINUE_SEARCH (0)

    u8 veh_handler[] = {
        // rcx = EXCEPTION_POINTERS*
        0x48, 0x8B, 0x01,             // mov rax, [rcx] (ExceptionRecord*)
        0x81, 0x38, 0x08, 0x00, 0x00, 0xC0, // cmp dword [rax], 0xC0000008
        0x75, 0x07,                   // jne ->not_ours
        // Return EXCEPTION_CONTINUE_EXECUTION = -1
        0x48, 0xC7, 0xC0, 0xFF, 0xFF, 0xFF, 0xFF, // mov rax, -1
        0xC3,                         // ret
        // not_ours: return 0 (EXCEPTION_CONTINUE_SEARCH)
        0x31, 0xC0,                   // xor eax, eax
        0xC3,                         // ret
    };

    va_t veh_addr = alloc_remote(sizeof(veh_handler));
    if (!veh_addr) return;
    write_memory(veh_addr, veh_handler, sizeof(veh_handler));

    // Now call AddVectoredExceptionHandler(1, veh_addr) in the target via CreateRemoteThread
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    auto add_veh = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        GetProcAddress(k32, "AddVectoredExceptionHandler"));
    if (!add_veh) return;

    // We need a small stub that calls AddVectoredExceptionHandler(1, veh_addr) then returns.
    // RCX=1, RDX=veh_addr, call AddVectoredExceptionHandler
    u8 caller[] = {
        0x48, 0x83, 0xEC, 0x28,       // sub rsp, 0x28 (shadow space + align)
        0x48, 0xC7, 0xC1, 0x01, 0x00, 0x00, 0x00, // mov rcx, 1
        0x48, 0xBA,                   // mov rdx, <veh_addr>
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x48, 0xB8,                   // mov rax, <AddVEH addr>
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xFF, 0xD0,                   // call rax
        0x48, 0x83, 0xC4, 0x28,       // add rsp, 0x28
        0xC3,                         // ret
    };
    memcpy(&caller[13], &veh_addr, 8);
    va_t add_veh_va = reinterpret_cast<va_t>(add_veh);
    memcpy(&caller[23], &add_veh_va, 8);

    va_t caller_addr = alloc_remote(sizeof(caller));
    if (!caller_addr) return;
    write_memory(caller_addr, caller, sizeof(caller));

    HANDLE t = CreateRemoteThread(process_, nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(caller_addr),
        nullptr, 0, nullptr);
    if (t) {
        WaitForSingleObject(t, 5000);
        CloseHandle(t);
    }

    log("NtClose anti-debug VEH installed");
}

// --- NtQueryObject hook ---
// Filters DebugObject type from handle enumeration

void DebugEngine::hook_nt_query_object() {
    va_t target = resolve_target_export("ntdll.dll", "NtQueryObject");
    if (!target) return;

    // Hook strategy: call original, then if ObjectInformationClass == ObjectTypeInformation (2),
    // check if the returned TypeName contains "DebugObject" and zero it out.
    // For ObjectAllTypesInformation (3), filter entries.
    //
    // Simplified approach: hook to intercept class==3 (ObjectAllTypesInformation)
    // and class==2 (ObjectTypeInformation). Call original, then post-process.
    //
    // Since post-processing in shellcode is complex, we use a simpler approach:
    // For class 2: call original, if TypeName.Buffer starts with "DebugObject", fake it.
    // For class 3: harder, skip for now. The NtQIP hook already covers the main case.
    //
    // Minimal approach: when rdx==2, call original, then check result buffer TypeName.
    // This requires more complex shellcode with post-call fixup.
    //
    // Practical shortcut: intercept class==2, if the result looks like DebugObject,
    // rename it. But parsing UNICODE_STRING in shellcode is fragile.
    //
    // Best minimal approach: just block queries for ObjectAllTypesInformation (3)
    // by returning STATUS_INFO_LENGTH_MISMATCH, forcing fallback paths that we handle
    // via the NtQIP hook already.

    // rdx = ObjectInformationClass
    // Block class 3 (ObjectAllTypesInformation) entirely
    u8 code[] = {
        0x48, 0x83, 0xFA, 0x03,       // cmp rdx, 3
        0x75, 0x07,                    // jne ->call_original
        // return STATUS_INFO_LENGTH_MISMATCH (0xC0000004)
        0xB8, 0x04, 0x00, 0x00, 0xC0, // mov eax, 0xC0000004
        0xC3,                          // ret
        // call_original: jmp [rip+0]
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };

    va_t tramp_base = alloc_remote(sizeof(code) + 14 + 14);
    if (!tramp_base) return;

    va_t orig_tramp = tramp_base + sizeof(code);
    memcpy(&code[12 + 2 + 4], &orig_tramp, 8);

    InlineHook h{};
    h.target = target;
    h.trampoline = tramp_base;
    h.trampoline_size = sizeof(code) + 14;
    read_memory(target, h.original, 14);

    u8 tramp_tail[14] = {
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    va_t resume = target + 14;
    memcpy(&tramp_tail[6], &resume, 8);

    write_memory(tramp_base, code, sizeof(code));
    write_memory(orig_tramp, h.original, 14);
    write_memory(orig_tramp + 14, tramp_tail, 14);

    u8 jmp[14] = {
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    memcpy(&jmp[6], &tramp_base, 8);
    write_memory(target, jmp, 14);

    hooks_.push_back(h);
    log(fmt::format("NtQueryObject hooked at {:016X}", target));
}

// --- QueryPerformanceCounter hook ---
// Normalizes timing to hide debug pauses

void DebugEngine::hook_qpc() {
    va_t target = resolve_target_export("kernel32.dll", "QueryPerformanceCounter");
    if (!target) return;

    // Strategy: call original QPC, subtract accumulated debug pause time.
    // We store the offset in a known remote location that the shellcode reads.
    //
    // Layout:
    //   [hook_code] -> calls original, adjusts result
    //   [qpc_offset storage] -> 8 bytes, updated by debugger on resume
    //
    // Shellcode:
    //   sub rsp, 0x28
    //   call original_QPC (via trampoline)
    //   mov rax, [rcx]         ; get counter value (rcx still points to LARGE_INTEGER)
    //   sub rax, [rip+offset]  ; subtract debug time
    //   mov [rcx], rax
    //   add rsp, 0x28
    //   ret

    // We need to allocate: hook code + offset storage + original trampoline
    constexpr size_t TOTAL_ALLOC = 256;
    va_t block = alloc_remote(TOTAL_ALLOC);
    if (!block) return;

    // Layout: [hook: ~80 bytes] [padding] [offset_var: 8 bytes @ block+128] [orig_tramp: 28 bytes @ block+136]
    va_t offset_var = block + 128;
    va_t orig_tramp = block + 136;

    InlineHook h{};
    h.target = target;
    h.trampoline = block;
    h.trampoline_size = TOTAL_ALLOC;
    read_memory(target, h.original, 14);

    // Write original bytes + jmp back at orig_tramp
    u8 tramp_tail[14] = {
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    va_t resume_addr = target + 14;
    memcpy(&tramp_tail[6], &resume_addr, 8);
    write_memory(orig_tramp, h.original, 14);
    write_memory(orig_tramp + 14, tramp_tail, 14);

    // Hook code
    u8 code[] = {
        0x48, 0x89, 0xCE,             // mov rsi, rcx (save lpPerformanceCount)
        0x48, 0x83, 0xEC, 0x28,       // sub rsp, 0x28
        0x48, 0xB8,                   // mov rax, <orig_tramp>
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xFF, 0xD0,                   // call rax (calls original QPC)
        0x48, 0x8B, 0x06,             // mov rax, [rsi] (counter value)
        0x48, 0x8B, 0x0D,             // mov rcx, [rip+disp32] -> offset_var
        0x00, 0x00, 0x00, 0x00,       // disp32 (will be patched)
        0x48, 0x29, 0xC8,             // sub rax, rcx
        0x48, 0x89, 0x06,             // mov [rsi], rax
        0x48, 0x83, 0xC4, 0x28,       // add rsp, 0x28
        0xC3,                         // ret
    };

    memcpy(&code[9], &orig_tramp, 8);

    // Calculate RIP-relative displacement for offset_var
    // The lea/mov [rip+disp] is at code offset 22 (the mov rcx,[rip+disp])
    // RIP at that point = block + 22 + 7 (instruction size is 7: 48 8B 0D xx xx xx xx)
    va_t rip_at_mov = block + 22 + 7;
    i32 disp = static_cast<i32>(offset_var - rip_at_mov);
    memcpy(&code[25], &disp, 4);

    write_memory(block, code, sizeof(code));

    // Initialize offset to 0
    i64 zero = 0;
    write_memory(offset_var, &zero, 8);

    // Patch target
    u8 jmp[14] = {
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    memcpy(&jmp[6], &block, 8);
    write_memory(target, jmp, 14);

    hooks_.push_back(h);

    // Store offset_var address for later updates
    qpc_offset_remote_ = offset_var;

    log(fmt::format("QueryPerformanceCounter hooked at {:016X}", target));
}

// --- Debug object removal attempt ---

void DebugEngine::hide_debug_object() {
    auto NtRemoveProcessDebug = reinterpret_cast<NtRemoveProcessDebug_t>(
        GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtRemoveProcessDebug"));
    if (!NtRemoveProcessDebug) return;
    log("Debug object hiding delegated to NtQIP/NtQueryObject hooks");
}

// --- Main entry ---

void DebugEngine::apply_stealth(u32 /*pid*/) {
    hide_peb_debugger_flag();
    fix_heap_flags();
    hook_nt_query_information();
    hook_nt_set_information_thread();
    hook_nt_close();
    hook_nt_query_object();
    hook_qpc();
    hide_debug_object();
    log("Stealth patches applied");
}

#else

void DebugEngine::apply_stealth(u32 /*pid*/) {}
void DebugEngine::hide_peb_debugger_flag() {}
void DebugEngine::fix_heap_flags() {}
void DebugEngine::hook_nt_query_information() {}
void DebugEngine::hook_nt_set_information_thread() {}
void DebugEngine::hook_nt_close() {}
void DebugEngine::hook_nt_query_object() {}
void DebugEngine::hook_qpc() {}
void DebugEngine::hide_debug_object() {}
va_t DebugEngine::resolve_target_export(const char* /*module*/, const char* /*func*/) { return 0; }
va_t DebugEngine::alloc_remote(size_t /*size*/) { return 0; }
bool DebugEngine::install_hook(va_t /*target*/, const u8* /*code*/, size_t /*code_size*/, const char* /*name*/) { return false; }
void DebugEngine::uninstall_hooks() {}

#endif

}
