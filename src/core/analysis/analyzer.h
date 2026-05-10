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
    void recursive_descent();
    void detect_functions();
    void build_cfgs();
    void build_xrefs();
    void find_strings();
    void apply_names();

    void descend(va_t addr, std::unordered_set<va_t>& visited);
    const u8* va_to_ptr(va_t addr, size_t* max_len = nullptr);

    PEImage&           img_;
    Disassembler       disasm_;
    AnalysisDB         db_;
    WorkerPool&        pool_;
    TaskScheduler      sched_;
    SignatureMatcher   sigmatch_;
    std::atomic<float> progress_{0.f};
};

}
