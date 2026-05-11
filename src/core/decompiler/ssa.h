#pragma once
#include "core/decompiler/ir.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace hype {

class SSABuilder {
public:
    void build(PcodeFunc& func);

private:
    void compute_dominators();
    void compute_dom_frontiers();
    void insert_phis();
    void rename();
    void rename_block(int blk_idx, std::unordered_map<int, std::vector<int>>& stacks);

    int new_version(const Varnode& vn);
    int var_key(const Varnode& vn) const;

    PcodeFunc* func_ = nullptr;
    int num_blocks_ = 0;
    std::vector<int> idom_;
    std::vector<std::vector<int>> dom_frontier_;
    std::vector<std::vector<int>> dom_children_;
    std::unordered_set<int> all_vars_;
    int ssa_counter_ = 0;
};

}
