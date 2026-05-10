#include "analyzer.h"
#include "core/disasm/disassembler.h"
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <algorithm>
#include <queue>
#include <unordered_set>
#include <cstring>

namespace hype {

Analyzer::Analyzer(PEImage& img, WorkerPool& pool)
    : img_(img), pool_(pool), sched_(pool) {
    disasm_.set_arch(img.arch);
}

const u8* Analyzer::va_to_ptr(va_t addr, size_t* max_len) {
    for (auto& seg : img_.segments) {
        if (seg.contains(addr)) {
            size_t off = static_cast<size_t>(addr - seg.va);
            if (max_len) *max_len = seg.data.size() - off;
            return seg.data.data() + off;
        }
    }
    return nullptr;
}

void Analyzer::run() {
    spdlog::info("analysis: starting");
    progress_ = 0.0f;

    linear_sweep();
    progress_ = 0.15f;

    recursive_descent();
    progress_ = 0.35f;

    detect_functions();
    progress_ = 0.50f;

    sigmatch_.match_functions(db_, img_);
    progress_ = 0.55f;

    build_cfgs();
    progress_ = 0.70f;

    build_xrefs();
    progress_ = 0.85f;

    find_strings();
    apply_names();
    progress_ = 1.0f;

    spdlog::info("analysis: done - {} insns, {} funcs, {} xrefs, {} strings",
                 db_.insns.size(), db_.funcs.size(), db_.xrefs.size(), db_.strings.size());
}

void Analyzer::linear_sweep() {
    std::vector<std::future<std::vector<Insn>>> futures;
    for (auto& seg : img_.segments) {
        if (!seg.executable() || seg.data.empty()) continue;
        futures.push_back(pool_.submit([this, &seg]() {
            return disasm_.decode_range(seg.va, seg.data.data(), seg.data.size());
        }));
    }
    for (auto& f : futures) {
        for (auto& insn : f.get())
            db_.insns[insn.addr] = std::move(insn);
    }
}

void Analyzer::recursive_descent() {
    std::unordered_set<va_t> visited;
    descend(img_.entry, visited);
    for (auto& exp : img_.exports)
        descend(exp.addr, visited);
}

void Analyzer::descend(va_t addr, std::unordered_set<va_t>& visited) {
    std::queue<va_t> wl;
    wl.push(addr);

    while (!wl.empty()) {
        va_t cur = wl.front(); wl.pop();
        if (visited.count(cur)) continue;
        visited.insert(cur);

        size_t max_len = 0;
        const u8* ptr = va_to_ptr(cur, &max_len);
        if (!ptr || !max_len) continue;

        size_t off = 0;
        while (off < max_len) {
            Insn insn{};
            if (!disasm_.decode(cur + off, ptr + off, max_len - off, insn))
                break;
            db_.insns[insn.addr] = insn;
            off += insn.len;

            if (insn.is_ret()) break;
            if (insn.is_call()) {
                va_t t = insn.branch_target();
                if (t && !visited.count(t)) wl.push(t);
            }
            if (insn.is_branch()) {
                va_t t = insn.branch_target();
                if (t && !visited.count(t)) wl.push(t);
                if (insn.type == InsnType::Jmp) break;
            }
        }
    }
}

void Analyzer::detect_functions() {
    std::unordered_set<va_t> entries;
    entries.insert(img_.entry);
    for (auto& exp : img_.exports)
        entries.insert(exp.addr);

    for (auto& [addr, insn] : db_.insns) {
        if (insn.is_call()) {
            va_t t = insn.branch_target();
            if (t && db_.insns.count(t)) entries.insert(t);
        }
    }

    for (va_t e : entries) {
        Function func;
        func.entry = e;
        func.name = fmt::format("sub_{:X}", e);
        db_.add_func(std::move(func));
    }
    spdlog::info("detected {} functions", db_.funcs.size());
}

void Analyzer::build_cfgs() {
    for (auto& [entry, func] : db_.funcs) {
        std::unordered_set<va_t> visited;
        std::queue<va_t> wl;
        wl.push(entry);

        while (!wl.empty()) {
            va_t bb_start = wl.front(); wl.pop();
            if (visited.count(bb_start)) continue;
            visited.insert(bb_start);

            BasicBlock bb;
            bb.start = bb_start;
            va_t cur = bb_start;

            while (db_.insns.count(cur)) {
                auto& insn = db_.insns[cur];
                bb.insns.push_back(insn);
                cur += insn.len;

                if (insn.is_ret()) break;
                if (insn.type == InsnType::Jmp) {
                    va_t t = insn.branch_target();
                    if (t) { bb.succs.push_back(t); wl.push(t); }
                    break;
                }
                if (insn.is_cond_jmp()) {
                    va_t t = insn.branch_target();
                    if (t) { bb.succs.push_back(t); wl.push(t); }
                    bb.succs.push_back(cur); wl.push(cur);
                    break;
                }
                if (cur != entry && db_.funcs.count(cur)) break;
            }

            bb.end = cur;
            func.block_addrs.push_back(bb.start);
            func.blocks[bb.start] = std::move(bb);
        }

        for (auto& [ba, block] : func.blocks)
            for (va_t s : block.succs)
                if (func.blocks.count(s))
                    func.blocks[s].preds.push_back(ba);

        func.analyzed = true;
    }
}

void Analyzer::build_xrefs() {
    for (auto& [addr, insn] : db_.insns) {
        if (insn.is_call()) {
            va_t t = insn.branch_target();
            if (t) db_.add_xref({addr, t, XrefType::CodeCall});
        } else if (insn.is_branch()) {
            va_t t = insn.branch_target();
            if (t) db_.add_xref({addr, t, XrefType::CodeJump});
        }
        for (u8 i = 0; i < insn.op_count; ++i) {
            auto& op = insn.ops[i];
            if (op.type == OpType::Mem && op.val)
                db_.add_xref({addr, op.val, XrefType::DataRead});
            else if (op.type == OpType::Imm && op.val > img_.base &&
                     op.val < img_.base + 0x10000000)
                db_.add_xref({addr, op.val, XrefType::DataOffset});
        }
    }
}

void Analyzer::find_strings() {
    for (auto& seg : img_.segments) {
        if (seg.data.empty()) continue;
        size_t i = 0;
        while (i < seg.data.size()) {
            if (seg.data[i] >= 0x20 && seg.data[i] < 0x7F) {
                size_t start = i;
                while (i < seg.data.size() && seg.data[i] >= 0x20 && seg.data[i] < 0x7F)
                    ++i;
                if (i - start >= 4 && i < seg.data.size() && seg.data[i] == 0) {
                    std::string s(seg.data.begin() + start, seg.data.begin() + i);
                    db_.strings.emplace_back(seg.va + start, std::move(s));
                }
            } else {
                ++i;
            }
        }
    }
    spdlog::info("found {} strings", db_.strings.size());
}

void Analyzer::apply_names() {
    db_.set_name(img_.entry, "entry_point");
    for (auto& imp : img_.imports)
        db_.set_name(imp.iat_addr, imp.dll + "!" + imp.name);
    for (auto& exp : img_.exports)
        db_.set_name(exp.addr, exp.name);
}

void Analyzer::apply_signatures() {
    sigmatch_.match_functions(db_, img_);
}

}
