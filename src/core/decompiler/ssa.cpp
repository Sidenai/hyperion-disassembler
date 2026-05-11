#include "ssa.h"
#include <algorithm>
#include <queue>

namespace hype {

int SSABuilder::var_key(const Varnode& vn) const {
    if (vn.is_reg()) return vn.id;
    if (vn.is_temp()) return 1000 + vn.id;
    if (vn.is_stack()) return 2000 + vn.id;
    return -1;
}

int SSABuilder::new_version(const Varnode& vn) {
    (void)vn;
    return ++ssa_counter_;
}

void SSABuilder::compute_dominators() {
    idom_.assign(num_blocks_, -1);
    idom_[0] = 0;

    bool changed = true;
    while (changed) {
        changed = false;
        for (int b = 1; b < num_blocks_; ++b) {
            auto& preds = func_->blocks[b].preds;
            int new_idom = -1;
            for (int p : preds) {
                if (idom_[p] < 0) continue;
                if (new_idom < 0) { new_idom = p; continue; }
                int f1 = new_idom, f2 = p;
                while (f1 != f2) {
                    while (f1 > f2) f1 = idom_[f1];
                    while (f2 > f1) f2 = idom_[f2];
                }
                new_idom = f1;
            }
            if (new_idom >= 0 && idom_[b] != new_idom) {
                idom_[b] = new_idom;
                changed = true;
            }
        }
    }

    dom_children_.resize(num_blocks_);
    for (int b = 1; b < num_blocks_; ++b)
        if (idom_[b] >= 0)
            dom_children_[idom_[b]].push_back(b);
}

void SSABuilder::compute_dom_frontiers() {
    dom_frontier_.resize(num_blocks_);
    for (int b = 0; b < num_blocks_; ++b) {
        auto& preds = func_->blocks[b].preds;
        if (preds.size() < 2) continue;
        for (int p : preds) {
            int runner = p;
            while (runner >= 0 && runner != idom_[b]) {
                dom_frontier_[runner].push_back(b);
                runner = idom_[runner];
            }
        }
    }
    for (auto& df : dom_frontier_) {
        std::sort(df.begin(), df.end());
        df.erase(std::unique(df.begin(), df.end()), df.end());
    }
}

void SSABuilder::insert_phis() {
    std::unordered_map<int, std::vector<int>> def_blocks;
    for (int b = 0; b < num_blocks_; ++b) {
        for (auto& op : func_->blocks[b].ops) {
            int k = var_key(op.output);
            if (k >= 0) {
                def_blocks[k].push_back(b);
                all_vars_.insert(k);
            }
        }
    }

    for (auto& [v, defs] : def_blocks) {
        std::queue<int> worklist;
        std::unordered_set<int> has_phi;
        std::unordered_set<int> processed;

        for (int d : defs) worklist.push(d);

        while (!worklist.empty()) {
            int d = worklist.front(); worklist.pop();
            if (processed.count(d)) continue;
            processed.insert(d);

            for (int f : dom_frontier_[d]) {
                if (has_phi.count(f)) continue;
                has_phi.insert(f);

                int num_preds = (int)func_->blocks[f].preds.size();
                PcodeInsn phi;
                phi.op = PcodeOp::COPY;
                phi.output.kind = (v < 1000) ? VarnodeKind::Reg :
                                  (v < 2000) ? VarnodeKind::Temp : VarnodeKind::Stack;
                phi.output.id = (v < 1000) ? v : (v < 2000) ? v - 1000 : v - 2000;
                phi.output.size = 8;
                phi.seq = -1; // mark as phi
                for (int i = 0; i < num_preds; ++i)
                    phi.inputs.push_back(phi.output);
                func_->blocks[f].ops.insert(func_->blocks[f].ops.begin(), std::move(phi));

                worklist.push(f);
            }
        }
    }
}

void SSABuilder::rename() {
    std::unordered_map<int, std::vector<int>> stacks;
    for (int v : all_vars_)
        stacks[v].push_back(0);
    rename_block(0, stacks);
}

void SSABuilder::rename_block(int blk_idx, std::unordered_map<int, std::vector<int>>& stacks) {
    if (blk_idx < 0 || blk_idx >= num_blocks_) return;

    auto& blk = func_->blocks[blk_idx];
    std::unordered_map<int, int> pushed;

    for (auto& op : blk.ops) {
        // rename inputs (skip phi inputs handled separately)
        if (op.seq != -1) {
            for (auto& in : op.inputs) {
                int k = var_key(in);
                if (k >= 0 && stacks.count(k) && !stacks[k].empty())
                    in.offset = stacks[k].back();
            }
        }

        // rename output
        int k = var_key(op.output);
        if (k >= 0) {
            int ver = new_version(op.output);
            stacks[k].push_back(ver);
            pushed[k]++;
            op.output.offset = ver;
        }
    }

    // update phi inputs in successors
    for (int s : blk.succs) {
        if (s < 0 || s >= num_blocks_) continue;
        auto& succ = func_->blocks[s];
        int pred_idx = -1;
        for (int i = 0; i < (int)succ.preds.size(); ++i)
            if (succ.preds[i] == blk_idx) { pred_idx = i; break; }
        if (pred_idx < 0) continue;

        for (auto& op : succ.ops) {
            if (op.seq != -1) break; // phis are at the front
            if (pred_idx < (int)op.inputs.size()) {
                int k = var_key(op.inputs[pred_idx]);
                if (k >= 0 && stacks.count(k) && !stacks[k].empty())
                    op.inputs[pred_idx].offset = stacks[k].back();
            }
        }
    }

    for (int c : dom_children_[blk_idx])
        rename_block(c, stacks);

    for (auto& [k, cnt] : pushed)
        for (int i = 0; i < cnt; ++i)
            stacks[k].pop_back();
}

void SSABuilder::build(PcodeFunc& func) {
    func_ = &func;
    num_blocks_ = (int)func.blocks.size();
    if (num_blocks_ == 0) return;

    ssa_counter_ = 0;
    all_vars_.clear();
    idom_.clear();
    dom_frontier_.clear();
    dom_children_.clear();

    compute_dominators();
    compute_dom_frontiers();
    insert_phis();
    rename();

    func.next_ssa = ssa_counter_;
}

}
