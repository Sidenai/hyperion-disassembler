#include "debug_engine.h"
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <psapi.h>
#include <tlhelp32.h>

#pragma comment(lib, "psapi.lib")

namespace hype {

DebugEngine::~DebugEngine() {
    if (attached_) detach();
}

bool DebugEngine::attach(u32 pid, DebugMode mode) {
    if (attached_) return false;

    mode_ = mode;
    if (!DebugActiveProcess(pid)) {
        log(fmt::format("DebugActiveProcess failed: {}", GetLastError()));
        return false;
    }

    process_ = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!process_) {
        DebugActiveProcessStop(pid);
        log("OpenProcess failed");
        return false;
    }

    pid_ = pid;
    attached_ = true;
    running_ = true;
    shutdown_ = false;

    if (mode_ == DebugMode::Stealth)
        apply_stealth(pid);

    if (mode_ == DebugMode::Stealth)
        icb_.install(process_, pid);

    debug_thread_ = std::thread(&DebugEngine::debug_loop, this);
    log(fmt::format("Attached to PID {} ({})", pid, mode == DebugMode::Stealth ? "stealth" : "normal"));
    return true;
}

bool DebugEngine::detach() {
    if (!attached_) return false;

    shutdown_ = true;
    should_break_ = true;

    // resume any suspended threads FIRST
    for (u32 tid : suspended_tids_) {
        HANDLE ht = OpenThread(THREAD_SUSPEND_RESUME, FALSE, tid);
        if (ht) { ResumeThread(ht); CloseHandle(ht); }
    }
    suspended_tids_.clear();

    if (debug_thread_.joinable())
        debug_thread_.join();

    // remove stealth inline hooks
    uninstall_hooks();

    // uninstall instrumentation callback (restores PEB, frees shellcode)
    if (icb_.is_installed())
        icb_.uninstall();

    // restore all software breakpoints
    for (auto& bp : breakpoints_) {
        if (bp.enabled && !bp.is_hardware) {
            write_memory(bp.addr, &bp.original_byte, 1);
        }
    }
    breakpoints_.clear();

    DebugActiveProcessStop(pid_);
    CloseHandle(process_);
    process_ = nullptr;
    pid_ = 0;
    attached_ = false;
    running_ = false;
    threads_.clear();
    modules_.clear();

    std::lock_guard lk(mtx_);
    while (!events_.empty()) events_.pop();

    log("Detached");
    return true;
}

void DebugEngine::run() {
    if (!attached_) return;
    should_break_ = false;

    if (qpc_offset_remote_ && pause_start_.QuadPart) {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        qpc_offset_ += (now.QuadPart - pause_start_.QuadPart);
        write_memory(qpc_offset_remote_, &qpc_offset_, 8);
        pause_start_.QuadPart = 0;
    }

    if (icb_.is_installed())
        icb_.resume();

    for (u32 tid : suspended_tids_) {
        HANDLE ht = OpenThread(THREAD_SUSPEND_RESUME, FALSE, tid);
        if (ht) { ResumeThread(ht); CloseHandle(ht); }
    }
    suspended_tids_.clear();
    running_ = true;
}

void DebugEngine::pause() {
    if (!attached_) return;
    should_break_ = true;
    QueryPerformanceCounter(&pause_start_);

    // Enumerate threads fresh using toolhelp
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;

    std::vector<u32> tids;
    THREADENTRY32 te{};
    te.dwSize = sizeof(te);
    if (Thread32First(snap, &te)) {
        do {
            if (te.th32OwnerProcessID == pid_)
                tids.push_back(te.th32ThreadID);
        } while (Thread32Next(snap, &te));
    }
    CloseHandle(snap);

    suspended_tids_ = tids;

    // Suspend all and get context of first
    active_tid_ = 0;
    for (u32 tid : tids) {
        HANDLE ht = OpenThread(THREAD_ALL_ACCESS, FALSE, tid);
        if (!ht) continue;
        SuspendThread(ht);

        if (!active_tid_) {
            active_tid_ = tid;
            CONTEXT ctx{};
            ctx.ContextFlags = CONTEXT_ALL;
            if (GetThreadContext(ht, &ctx)) {
                cached_regs_.rax = ctx.Rax; cached_regs_.rbx = ctx.Rbx;
                cached_regs_.rcx = ctx.Rcx; cached_regs_.rdx = ctx.Rdx;
                cached_regs_.rsi = ctx.Rsi; cached_regs_.rdi = ctx.Rdi;
                cached_regs_.rbp = ctx.Rbp; cached_regs_.rsp = ctx.Rsp;
                cached_regs_.rip = ctx.Rip;
                cached_regs_.r8 = ctx.R8; cached_regs_.r9 = ctx.R9;
                cached_regs_.r10 = ctx.R10; cached_regs_.r11 = ctx.R11;
                cached_regs_.r12 = ctx.R12; cached_regs_.r13 = ctx.R13;
                cached_regs_.r14 = ctx.R14; cached_regs_.r15 = ctx.R15;
                cached_regs_.eflags = ctx.EFlags;
                events_.push({DebugEvent::Breakpoint_Hit, ctx.Rip, tid, fmt::format("Paused at {:016X}", ctx.Rip)});
            }
        }
        CloseHandle(ht);
    }

    running_ = false;

    // Also enumerate modules
    update_modules();
}

void DebugEngine::step_into() {
    if (!attached_ || running_) return;
    single_step_ = true;

    HANDLE ht = thread_handle(active_tid_);
    if (!ht) return;

    CONTEXT ctx{};
    ctx.ContextFlags = CONTEXT_FULL;
    GetThreadContext(ht, &ctx);
    ctx.EFlags |= 0x100; // TRAP_FLAG
    SetThreadContext(ht, &ctx);

    running_ = true;
    should_break_ = false;
}

void DebugEngine::step_over() {
    if (!attached_ || running_) return;

    HANDLE ht = thread_handle(active_tid_);
    if (!ht) return;

    CONTEXT ctx{};
    ctx.ContextFlags = CONTEXT_FULL;
    GetThreadContext(ht, &ctx);

    u8 insn[16];
    if (!read_memory(ctx.Rip, insn, sizeof(insn))) {
        step_into();
        return;
    }

    // Check if CALL instruction (E8, FF /2, 9A, etc.)
    bool is_call = false;
    int insn_len = 0;
    if (insn[0] == 0xE8) { is_call = true; insn_len = 5; }
    else if (insn[0] == 0xFF && (insn[1] & 0x38) == 0x10) { is_call = true; insn_len = 2 + ((insn[1] & 0x07) == 4 ? 1 : 0) + ((insn[1] >> 6) == 1 ? 1 : (insn[1] >> 6) == 2 ? 4 : 0); }
    else if (insn[0] == 0x9A) { is_call = true; insn_len = 7; }

    if (is_call && insn_len > 0) {
        va_t after = ctx.Rip + insn_len;
        step_over_bp_ = after;
        set_breakpoint(after);
        running_ = true;
        should_break_ = false;
    } else {
        step_into();
    }
}

void DebugEngine::step_out() {
    if (!attached_ || running_) return;

    HANDLE ht = thread_handle(active_tid_);
    if (!ht) return;

    CONTEXT ctx{};
    ctx.ContextFlags = CONTEXT_FULL;
    GetThreadContext(ht, &ctx);

    // Read return address from stack
    va_t ret_addr = 0;
    if (read_memory(ctx.Rsp, &ret_addr, sizeof(ret_addr))) {
        step_over_bp_ = ret_addr;
        set_breakpoint(ret_addr);
        running_ = true;
        should_break_ = false;
    }
}

bool DebugEngine::set_breakpoint(va_t addr) {
    std::lock_guard lk(mtx_);
    for (auto& bp : breakpoints_)
        if (bp.addr == addr) return bp.enabled;

    if (mode_ == DebugMode::Stealth && icb_.is_installed()) {
        if (!icb_.add_breakpoint(addr)) return false;
        breakpoints_.push_back({addr, 0, true, false, -1});
        return true;
    }

    u8 orig;
    if (!read_memory(addr, &orig, 1)) return false;

    u8 int3 = 0xCC;
    if (!write_memory(addr, &int3, 1)) return false;

    breakpoints_.push_back({addr, orig, true, false, -1});
    return true;
}

bool DebugEngine::remove_breakpoint(va_t addr) {
    std::lock_guard lk(mtx_);

    if (mode_ == DebugMode::Stealth && icb_.is_installed())
        icb_.remove_breakpoint(addr);

    for (auto it = breakpoints_.begin(); it != breakpoints_.end(); ++it) {
        if (it->addr == addr) {
            if (it->enabled && !it->is_hardware && !(mode_ == DebugMode::Stealth && icb_.is_installed()))
                write_memory(addr, &it->original_byte, 1);
            breakpoints_.erase(it);
            return true;
        }
    }
    return false;
}

bool DebugEngine::set_hw_breakpoint(va_t addr, int slot) {
    if (slot < 0 || slot > 3) return false;
    std::lock_guard lk(mtx_);

    for (auto& ti : threads_) {
        CONTEXT ctx{};
        ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        GetThreadContext(ti.handle, &ctx);

        (&ctx.Dr0)[slot] = addr;
        ctx.Dr7 |= (1ULL << (slot * 2)); // local enable
        ctx.Dr7 &= ~(0xFULL << (16 + slot * 4));

        SetThreadContext(ti.handle, &ctx);
    }

    breakpoints_.push_back({addr, 0, true, true, slot});
    return true;
}

bool DebugEngine::remove_hw_breakpoint(int slot) {
    if (slot < 0 || slot > 3) return false;
    std::lock_guard lk(mtx_);

    for (auto& ti : threads_) {
        CONTEXT ctx{};
        ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        GetThreadContext(ti.handle, &ctx);
        (&ctx.Dr0)[slot] = 0;
        ctx.Dr7 &= ~(1ULL << (slot * 2));
        SetThreadContext(ti.handle, &ctx);
    }

    for (auto it = breakpoints_.begin(); it != breakpoints_.end(); ++it) {
        if (it->is_hardware && it->hw_slot == slot) {
            breakpoints_.erase(it);
            break;
        }
    }
    return true;
}

bool DebugEngine::read_memory(va_t addr, void* buf, size_t len) {
    SIZE_T read = 0;
    return ReadProcessMemory(process_, (LPCVOID)addr, buf, len, &read) && read == len;
}

bool DebugEngine::write_memory(va_t addr, const void* buf, size_t len) {
    SIZE_T written = 0;
    DWORD old_prot;
    VirtualProtectEx(process_, (LPVOID)addr, len, PAGE_EXECUTE_READWRITE, &old_prot);
    BOOL ok = WriteProcessMemory(process_, (LPVOID)addr, buf, len, &written);
    VirtualProtectEx(process_, (LPVOID)addr, len, old_prot, &old_prot);
    FlushInstructionCache(process_, (LPCVOID)addr, len);
    return ok && written == len;
}

DebugEngine::Registers DebugEngine::get_registers(u32 tid) {
    if (!running_) return cached_regs_;

    Registers r{};
    HANDLE ht = thread_handle(tid ? tid : active_tid_);
    if (!ht) return r;

    CONTEXT ctx{};
    ctx.ContextFlags = CONTEXT_FULL | CONTEXT_DEBUG_REGISTERS;
    if (!GetThreadContext(ht, &ctx)) return r;

    r.rax = ctx.Rax; r.rbx = ctx.Rbx; r.rcx = ctx.Rcx; r.rdx = ctx.Rdx;
    r.rsi = ctx.Rsi; r.rdi = ctx.Rdi; r.rbp = ctx.Rbp; r.rsp = ctx.Rsp;
    r.rip = ctx.Rip;
    r.r8 = ctx.R8; r.r9 = ctx.R9; r.r10 = ctx.R10; r.r11 = ctx.R11;
    r.r12 = ctx.R12; r.r13 = ctx.R13; r.r14 = ctx.R14; r.r15 = ctx.R15;
    r.eflags = ctx.EFlags;
    r.dr0 = ctx.Dr0; r.dr1 = ctx.Dr1; r.dr2 = ctx.Dr2; r.dr3 = ctx.Dr3;
    r.dr6 = ctx.Dr6; r.dr7 = ctx.Dr7;
    return r;
}

bool DebugEngine::set_registers(const Registers& regs, u32 tid) {
    HANDLE ht = thread_handle(tid ? tid : active_tid_);
    if (!ht) return false;

    CONTEXT ctx{};
    ctx.ContextFlags = CONTEXT_FULL | CONTEXT_DEBUG_REGISTERS;
    GetThreadContext(ht, &ctx);

    ctx.Rax = regs.rax; ctx.Rbx = regs.rbx; ctx.Rcx = regs.rcx; ctx.Rdx = regs.rdx;
    ctx.Rsi = regs.rsi; ctx.Rdi = regs.rdi; ctx.Rbp = regs.rbp; ctx.Rsp = regs.rsp;
    ctx.Rip = regs.rip;
    ctx.R8 = regs.r8; ctx.R9 = regs.r9; ctx.R10 = regs.r10; ctx.R11 = regs.r11;
    ctx.R12 = regs.r12; ctx.R13 = regs.r13; ctx.R14 = regs.r14; ctx.R15 = regs.r15;
    ctx.EFlags = regs.eflags;
    ctx.Dr0 = regs.dr0; ctx.Dr1 = regs.dr1; ctx.Dr2 = regs.dr2; ctx.Dr3 = regs.dr3;
    ctx.Dr6 = regs.dr6; ctx.Dr7 = regs.dr7;

    return SetThreadContext(ht, &ctx) != 0;
}

DebugEngine::DebugEvent DebugEngine::poll_event() {
    std::lock_guard lk(mtx_);

    if (icb_.is_installed() && running_ && icb_.check_hit()) {
        va_t addr = icb_.hit_address();
        running_ = false;
        active_tid_ = 0;

        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snap != INVALID_HANDLE_VALUE) {
            THREADENTRY32 te{};
            te.dwSize = sizeof(te);
            if (Thread32First(snap, &te)) {
                do {
                    if (te.th32OwnerProcessID == pid_) {
                        HANDLE ht = OpenThread(THREAD_ALL_ACCESS, FALSE, te.th32ThreadID);
                        if (ht) {
                            SuspendThread(ht);
                            if (!active_tid_) {
                                CONTEXT ctx{};
                                ctx.ContextFlags = CONTEXT_ALL;
                                if (GetThreadContext(ht, &ctx) && ctx.Rip == addr) {
                                    active_tid_ = te.th32ThreadID;
                                    cached_regs_.rax = ctx.Rax; cached_regs_.rbx = ctx.Rbx;
                                    cached_regs_.rcx = ctx.Rcx; cached_regs_.rdx = ctx.Rdx;
                                    cached_regs_.rsi = ctx.Rsi; cached_regs_.rdi = ctx.Rdi;
                                    cached_regs_.rbp = ctx.Rbp; cached_regs_.rsp = ctx.Rsp;
                                    cached_regs_.rip = ctx.Rip;
                                    cached_regs_.r8 = ctx.R8; cached_regs_.r9 = ctx.R9;
                                    cached_regs_.r10 = ctx.R10; cached_regs_.r11 = ctx.R11;
                                    cached_regs_.r12 = ctx.R12; cached_regs_.r13 = ctx.R13;
                                    cached_regs_.r14 = ctx.R14; cached_regs_.r15 = ctx.R15;
                                    cached_regs_.eflags = ctx.EFlags;
                                }
                            }
                            CloseHandle(ht);
                        }
                    }
                } while (Thread32Next(snap, &te));
            }
            CloseHandle(snap);
        }

        events_.push({DebugEvent::Breakpoint_Hit, addr, active_tid_,
            fmt::format("ICB breakpoint at {:016X}", addr)});
    }

    if (events_.empty()) return {DebugEvent::None};
    auto ev = std::move(events_.front());
    events_.pop();
    return ev;
}

void DebugEngine::debug_loop() {
    DEBUG_EVENT ev{};
    while (!shutdown_) {
        if (!WaitForDebugEvent(&ev, 100)) continue;

        u32 continue_status = DBG_CONTINUE;

        switch (ev.dwDebugEventCode) {
        case EXCEPTION_DEBUG_EVENT:
            handle_exception(ev);
            if (ev.u.Exception.ExceptionRecord.ExceptionCode != EXCEPTION_BREAKPOINT &&
                ev.u.Exception.ExceptionRecord.ExceptionCode != EXCEPTION_SINGLE_STEP)
                continue_status = DBG_EXCEPTION_NOT_HANDLED;
            break;

        case CREATE_THREAD_DEBUG_EVENT:
            {
                ThreadInfo ti{ev.dwThreadId, ev.u.CreateThread.hThread, (va_t)ev.u.CreateThread.lpThreadLocalBase};
                std::lock_guard lk(mtx_);
                threads_.push_back(ti);
            }
            emit({DebugEvent::ThreadCreate, 0, ev.dwThreadId, fmt::format("Thread {} created", ev.dwThreadId)});
            break;

        case EXIT_THREAD_DEBUG_EVENT:
            {
                std::lock_guard lk(mtx_);
                threads_.erase(std::remove_if(threads_.begin(), threads_.end(),
                    [&](const ThreadInfo& t) { return t.id == ev.dwThreadId; }), threads_.end());
            }
            emit({DebugEvent::ThreadExit, 0, ev.dwThreadId, fmt::format("Thread {} exited ({})", ev.dwThreadId, ev.u.ExitThread.dwExitCode)});
            break;

        case CREATE_PROCESS_DEBUG_EVENT:
            {
                ThreadInfo ti{ev.dwThreadId, ev.u.CreateProcessInfo.hThread, (va_t)ev.u.CreateProcessInfo.lpThreadLocalBase};
                std::lock_guard lk(mtx_);
                threads_.push_back(ti);
                active_tid_ = ev.dwThreadId;
            }
            if (ev.u.CreateProcessInfo.hFile)
                CloseHandle(ev.u.CreateProcessInfo.hFile);
            update_modules();
            break;

        case EXIT_PROCESS_DEBUG_EVENT:
            emit({DebugEvent::ProcessExit, 0, 0, fmt::format("Process exited ({})", ev.u.ExitProcess.dwExitCode)});
            shutdown_ = true;
            break;

        case LOAD_DLL_DEBUG_EVENT:
            update_modules();
            {
                char name[MAX_PATH] = {};
                if (ev.u.LoadDll.lpImageName) {
                    void* pname = nullptr;
                    ReadProcessMemory(process_, ev.u.LoadDll.lpImageName, &pname, sizeof(pname), nullptr);
                    if (pname)
                        ReadProcessMemory(process_, pname, name, sizeof(name) - 1, nullptr);
                }
                emit({DebugEvent::ModuleLoad, (va_t)ev.u.LoadDll.lpBaseOfDll, 0, name});
            }
            if (ev.u.LoadDll.hFile)
                CloseHandle(ev.u.LoadDll.hFile);
            break;

        case UNLOAD_DLL_DEBUG_EVENT:
            update_modules();
            emit({DebugEvent::ModuleUnload, (va_t)ev.u.UnloadDll.lpBaseOfDll, 0, ""});
            break;

        case OUTPUT_DEBUG_STRING_EVENT:
            {
                std::string s(ev.u.DebugString.nDebugStringLength, '\0');
                ReadProcessMemory(process_, ev.u.DebugString.lpDebugStringData, s.data(), s.size(), nullptr);
                log(fmt::format("[DBG] {}", s));
            }
            break;
        }

        ContinueDebugEvent(ev.dwProcessId, ev.dwThreadId, continue_status);
    }

    attached_ = false;
    running_ = false;
}

void DebugEngine::handle_exception(const DEBUG_EVENT& ev) {
    auto& exc = ev.u.Exception.ExceptionRecord;
    va_t addr = (va_t)exc.ExceptionAddress;
    u32 tid = ev.dwThreadId;

    switch (exc.ExceptionCode) {
    case EXCEPTION_BREAKPOINT:
        handle_breakpoint_hit(addr, tid);
        break;

    case EXCEPTION_SINGLE_STEP:
        handle_single_step(tid);
        break;

    default:
        emit({DebugEvent::Exception, addr, tid,
            fmt::format("Exception {:08X} at {:016X}", exc.ExceptionCode, addr)});
        if (exc.ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
            running_ = false;
            active_tid_ = tid;
        }
        break;
    }
}

void DebugEngine::handle_breakpoint_hit(va_t addr, u32 tid) {
    active_tid_ = tid;

    std::lock_guard lk(mtx_);
    for (auto& bp : breakpoints_) {
        if (bp.addr == addr && bp.enabled) {
            // Restore original byte
            if (!bp.is_hardware)
                write_memory(addr, &bp.original_byte, 1);

            // Back up RIP to the breakpoint address
            HANDLE ht = thread_handle(tid);
            if (ht) {
                CONTEXT ctx{};
                ctx.ContextFlags = CONTEXT_CONTROL;
                GetThreadContext(ht, &ctx);
                ctx.Rip = addr;
                ctx.EFlags |= 0x100; // Set trap to re-set BP after single step
                SetThreadContext(ht, &ctx);
            }

            // Check if this is a step-over temp BP
            if (step_over_bp_ == addr) {
                step_over_bp_ = 0;
                bp.enabled = false;
            }

            running_ = false;
            events_.push({DebugEvent::Breakpoint_Hit, addr, tid, fmt::format("Breakpoint at {:016X}", addr)});
            return;
        }
    }

    // System breakpoint (initial attach)
    running_ = !should_break_;
    if (!running_) {
        events_.push({DebugEvent::Breakpoint_Hit, addr, tid, "Initial breakpoint"});
    }
}

void DebugEngine::handle_single_step(u32 tid) {
    active_tid_ = tid;

    // Re-enable breakpoints that were temporarily disabled
    std::lock_guard lk(mtx_);
    for (auto& bp : breakpoints_) {
        if (bp.enabled && !bp.is_hardware) {
            u8 int3 = 0xCC;
            write_memory(bp.addr, &int3, 1);
        }
    }

    if (single_step_) {
        single_step_ = false;
        running_ = false;
        HANDLE ht = thread_handle(tid);
        va_t rip = 0;
        if (ht) {
            CONTEXT ctx{};
            ctx.ContextFlags = CONTEXT_CONTROL;
            GetThreadContext(ht, &ctx);
            rip = ctx.Rip;
        }
        events_.push({DebugEvent::SingleStep, rip, tid, ""});
    }
}

void DebugEngine::update_modules() {
    std::lock_guard lk(mtx_);
    modules_.clear();

    HMODULE mods[1024];
    DWORD needed;
    if (EnumProcessModules(process_, mods, sizeof(mods), &needed)) {
        for (DWORD i = 0; i < needed / sizeof(HMODULE); i++) {
            MODULEINFO mi{};
            char name[MAX_PATH] = {};
            GetModuleBaseNameA(process_, mods[i], name, sizeof(name));
            GetModuleInformation(process_, mods[i], &mi, sizeof(mi));
            modules_.push_back({name, (va_t)mi.lpBaseOfDll, (u32)mi.SizeOfImage});
        }
    }
}

void DebugEngine::update_threads() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;

    THREADENTRY32 te{};
    te.dwSize = sizeof(te);

    std::lock_guard lk(mtx_);
    if (Thread32First(snap, &te)) {
        do {
            if (te.th32OwnerProcessID == pid_) {
                bool found = false;
                for (auto& t : threads_)
                    if (t.id == te.th32ThreadID) { found = true; break; }
                if (!found) {
                    HANDLE ht = OpenThread(THREAD_ALL_ACCESS, FALSE, te.th32ThreadID);
                    if (ht) threads_.push_back({te.th32ThreadID, ht, 0});
                }
            }
        } while (Thread32Next(snap, &te));
    }
    CloseHandle(snap);
}

HANDLE DebugEngine::thread_handle(u32 tid) {
    for (auto& t : threads_)
        if (t.id == tid) return t.handle;
    return nullptr;
}

void DebugEngine::emit(DebugEvent ev) {
    std::lock_guard lk(mtx_);
    events_.push(std::move(ev));
}

void DebugEngine::log(const std::string& msg) {
    spdlog::info("[Dbg] {}", msg);
    if (log_cb_) log_cb_(msg);
}

}
