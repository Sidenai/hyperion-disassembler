#pragma once
#include "core/decompiler/ir.h"
#include <unordered_map>

namespace hype {

struct SymState {
    std::unordered_map<int, IRExpr>         regs;
    std::unordered_map<int64_t, IRExpr>     mem;

    void set_reg(int id, IRExpr e) { regs[id] = std::move(e); }
    const IRExpr* get_reg(int id) const {
        auto it = regs.find(id);
        return it != regs.end() ? &it->second : nullptr;
    }
    void set_mem(int64_t offset, IRExpr e) { mem[offset] = std::move(e); }
    const IRExpr* get_mem(int64_t offset) const {
        auto it = mem.find(offset);
        return it != mem.end() ? &it->second : nullptr;
    }
};

class SymExec {
public:
    void run(IRFunc& func);

private:
    IRExpr resolve(const IRExpr& e, const SymState& state);
    bool   is_stack_addr(const IRExpr& e, int64_t& offset);
    bool   exprs_equal(const IRExpr& a, const IRExpr& b);
    void   exec_block(IRBlock& blk, SymState& state);
    void   eliminate_dead(IRFunc& func);
    int    count_uses(const IRFunc& func, int var_id);
    bool   uses_var(const IRExpr& expr, int var_id);
};

}
