#pragma once
#include "core/types.h"
#include "debugger/instrumentation_cb.h"
#include <string>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>

#ifdef _WIN32
#include <windows.h>
#else
using HANDLE = void*;
struct DEBUG_EVENT { int dummy; };
struct LARGE_INTEGER { long long QuadPart; };
#endif

namespace hype {

enum class DebugMode { Normal, Stealth };

struct Breakpoint {
    va_t addr;
    u8 original_byte;
    bool enabled;
    bool is_hardware;
    int hw_slot;
};

struct ThreadInfo {
    u32 id;
    HANDLE handle;
    va_t teb;
};

struct ModuleInfo {
    std::string name;
    va_t base;
    u32 size;
};

class DebugEngine {
public:
    ~DebugEngine();

    bool attach(u32 pid, DebugMode mode = DebugMode::Normal);
    bool detach();
    bool is_attached() const { return attached_; }
    bool is_running() const { return running_; }

    void run();
    void pause();
    void step_into();
    void step_over();
    void step_out();

    bool set_breakpoint(va_t addr);
    bool remove_breakpoint(va_t addr);
    bool set_hw_breakpoint(va_t addr, int slot);
    bool remove_hw_breakpoint(int slot);

    bool read_memory(va_t addr, void* buf, size_t len);
    bool write_memory(va_t addr, const void* buf, size_t len);

    struct Registers {
        u64 rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp, rip;
        u64 r8, r9, r10, r11, r12, r13, r14, r15;
        u32 eflags;
        u64 dr0, dr1, dr2, dr3, dr6, dr7;
    };

    Registers get_registers(u32 tid = 0);
    bool set_registers(const Registers& regs, u32 tid = 0);

    const std::vector<ThreadInfo>& threads() const { return threads_; }
    const std::vector<ModuleInfo>& modules() const { return modules_; }
    const std::vector<Breakpoint>& breakpoints() const { return breakpoints_; }

    struct DebugEvent {
        enum Type { None, Breakpoint_Hit, SingleStep, Exception, ThreadCreate, ThreadExit, ModuleLoad, ModuleUnload, ProcessExit };
        Type type = None;
        va_t addr = 0;
        u32 tid = 0;
        std::string detail;
    };

    DebugEvent poll_event();
    u32 pid() const { return pid_; }

    using LogCB = std::function<void(const std::string&)>;
    void set_log_cb(LogCB cb) { log_cb_ = std::move(cb); }

private:
    void debug_loop();
    void handle_exception(const DEBUG_EVENT& ev);
    void handle_breakpoint_hit(va_t addr, u32 tid);
    void handle_single_step(u32 tid);
    void update_modules();
    void update_threads();
    void restore_breakpoints_for_step(va_t addr);
    HANDLE thread_handle(u32 tid);
    void emit(DebugEvent ev);
    void log(const std::string& msg);

    void apply_stealth(u32 pid);
    void hide_peb_debugger_flag();
    void fix_heap_flags();
    void hook_nt_query_information();
    void hook_nt_set_information_thread();
    void hook_nt_close();
    void hook_nt_query_object();
    void hook_qpc();
    void hide_debug_object();

    struct InlineHook {
        va_t target;
        u8 original[14];
        va_t trampoline;
        size_t trampoline_size;
    };

    va_t resolve_target_export(const char* module, const char* func);
    va_t alloc_remote(size_t size);
    bool install_hook(va_t target, const u8* code, size_t code_size, const char* name);
    void uninstall_hooks();

    std::vector<InlineHook> hooks_;
    i64 qpc_offset_ = 0;
    va_t qpc_offset_remote_ = 0;
    LARGE_INTEGER pause_start_{};

    HANDLE process_ = nullptr;
    u32 pid_ = 0;
    bool attached_ = false;
    std::atomic<bool> running_{false};
    bool single_step_ = false;
    va_t step_over_bp_ = 0;
    DebugMode mode_ = DebugMode::Normal;

    std::vector<Breakpoint> breakpoints_;
    std::vector<ThreadInfo> threads_;
    std::vector<ModuleInfo> modules_;
    std::queue<DebugEvent> events_;
    std::thread debug_thread_;
    Registers cached_regs_{};
    std::vector<u32> suspended_tids_;
    std::mutex mtx_;
    std::atomic<bool> should_break_{false};
    std::atomic<bool> shutdown_{false};
    u32 active_tid_ = 0;

    LogCB log_cb_;

    InstrumentationBreakpoints icb_;
};

}
