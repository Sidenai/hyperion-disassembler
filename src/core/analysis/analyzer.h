#pragma once
#include "core/loader/pe_loader.h"
#include "core/disasm/disassembler.h"
#include "analysis_db.h"
#include "signatures.h"
#include "threading/worker_pool.h"
#include "threading/task_scheduler.h"
#include <atomic>

namespace hype {

class Analyzer {
public:
    Analyzer(PEImage& img, WorkerPool& pool);

    void run();
    void apply_signatures();
    float progress() const { return progress_.load(); }
    AnalysisDB& db() { return db_; }
    const AnalysisDB& db() const { return db_; }
    SignatureMatcher& sig_matcher() { return sigmatch_; }
    const SignatureMatcher& sig_matcher() const { return sigmatch_; }

private:
    void linear_sweep();
    void merge_tentative();
    void recursive_descent();
    void detect_functions();
    void detect_thunks();
    void build_cfgs();
    void remove_junk_code();
    void detect_switches();
    void build_xrefs();
    void find_strings();
    void find_string_refs();
    void detect_vtables();
    void detect_globals();
    void apply_names();
    void detect_noreturn();
    void detect_tail_calls();
    void detect_calling_conventions();
    void propagate_dataflow();
    void detect_loops();
    void recover_structs();

    void descend(va_t addr, std::unordered_set<va_t>& visited);
    const u8* va_to_ptr(va_t addr, size_t* max_len = nullptr);
    bool is_iat_addr(va_t addr) const;
    bool is_code_addr(va_t addr) const;
    bool in_section(va_t addr, const char* name) const;
    const Segment* section_for(va_t addr) const;

    PEImage&           img_;
    Disassembler       disasm_;
    AnalysisDB         db_;
    WorkerPool&        pool_;
    TaskScheduler      sched_;
    SignatureMatcher   sigmatch_;
    std::atomic<float> progress_{0.f};
    std::unordered_map<va_t, Insn> tentative_;
};

}
