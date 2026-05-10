#include "sym_exec.h"
#include <unordered_set>

namespace hype {

bool SymExec::uses_var(const IRExpr& expr, int var_id) {
    if (expr.is_var() && expr.get_var().id == var_id) return true;
    for (auto& a : expr.args)
        if (uses_var(a, var_id)) return true;
    return false;
}

int SymExec::count_uses(const IRFunc& func, int var_id) {
    int count = 0;
    for (auto& blk : func.blocks) {
        for (auto& s : blk.stmts) {
            if (uses_var(s.src, var_id)) ++count;
        }
        if (uses_var(blk.cond, var_id)) ++count;
    }
    return count;
}

bool SymExec::exprs_equal(const IRExpr& a, const IRExpr& b) {
    if (a.op != b.op) return false;
    if (a.is_var() && b.is_var()) return a.get_var().id == b.get_var().id;
    if (a.is_imm() && b.is_imm()) return a.get_imm() == b.get_imm();
    if (std::holds_alternative<std::string>(a.val) && std::holds_alternative<std::string>(b.val))
        return std::get<std::string>(a.val) == std::get<std::string>(b.val);
    if (a.args.size() != b.args.size()) return false;
    for (size_t i = 0; i < a.args.size(); ++i)
        if (!exprs_equal(a.args[i], b.args[i])) return false;
    return true;
}

bool SymExec::is_stack_addr(const IRExpr& e, int64_t& offset) {
    if (e.is_var() && e.get_var().id == 5) { // rbp
        offset = 0;
        return true;
    }
    if (e.is_var() && e.get_var().id == 4) { // rsp
        offset = 0x1000;
        return true;
    }
    if (e.op == IROp::Add && e.args.size() == 2) {
        if (e.args[0].is_var() && (e.args[0].get_var().id == 5 || e.args[0].get_var().id == 4)
            && e.args[1].is_imm()) {
            int64_t base = (e.args[0].get_var().id == 4) ? 0x1000 : 0;
            offset = base + static_cast<int64_t>(e.args[1].get_imm());
            return true;
        }
    }
    if (e.op == IROp::Sub && e.args.size() == 2) {
        if (e.args[0].is_var() && (e.args[0].get_var().id == 5 || e.args[0].get_var().id == 4)
            && e.args[1].is_imm()) {
            int64_t base = (e.args[0].get_var().id == 4) ? 0x1000 : 0;
            offset = base - static_cast<int64_t>(e.args[1].get_imm());
            return true;
        }
    }
    return false;
}

IRExpr SymExec::resolve(const IRExpr& e, const SymState& state) {
    if (e.is_var()) {
        int id = e.get_var().id;
        if (id == 4 || id == 5) return e; // keep rsp/rbp symbolic
        auto* resolved = state.get_reg(id);
        if (resolved) return *resolved;
        return e;
    }
    if (e.is_imm()) return e;
    if (e.op == IROp::Assign && std::holds_alternative<std::string>(e.val)) return e;

    if (e.op == IROp::Load && !e.args.empty()) {
        IRExpr addr = resolve(e.args[0], state);
        int64_t off;
        if (is_stack_addr(addr, off)) {
            auto* mem_val = state.get_mem(off);
            if (mem_val) return *mem_val;
        }
        IRExpr result;
        result.op = IROp::Load;
        result.val = e.val;
        result.args = {std::move(addr)};
        return result;
    }

    IRExpr result;
    result.op = e.op;
    result.val = e.val;
    result.args.reserve(e.args.size());
    for (auto& a : e.args)
        result.args.push_back(resolve(a, state));

    // constant fold after resolution
    if (result.args.size() == 2 && result.args[0].is_imm() && result.args[1].is_imm()) {
        u64 l = result.args[0].get_imm();
        u64 r = result.args[1].get_imm();
        u64 v = 0;
        bool ok = true;
        switch (result.op) {
        case IROp::Add: v = l + r; break;
        case IROp::Sub: v = l - r; break;
        case IROp::Mul: v = l * r; break;
        case IROp::And: v = l & r; break;
        case IROp::Or:  v = l | r; break;
        case IROp::Xor: v = l ^ r; break;
        case IROp::Shl: v = l << r; break;
        case IROp::Shr: v = l >> r; break;
        default: ok = false; break;
        }
        if (ok) return IRExpr::imm(v);
    }

    // fold Add(x, 0) / Sub(x, 0) → x
    if ((result.op == IROp::Add || result.op == IROp::Sub)
        && result.args.size() == 2 && result.args[1].is_imm() && result.args[1].get_imm() == 0) {
        return result.args[0];
    }

    return result;
}

void SymExec::exec_block(IRBlock& blk, SymState& state) {
    for (auto& s : blk.stmts) {
        if (s.op == IROp::Nop) continue;

        if (s.op == IROp::Store || (s.src.op == IROp::Store && s.src.args.size() >= 2)) {
            IRExpr addr = resolve(s.src.args[0], state);
            IRExpr val = resolve(s.src.args[1], state);
            int64_t off;
            if (is_stack_addr(addr, off)) {
                state.set_mem(off, val);
            }
            s.src.args[0] = std::move(addr);
            s.src.args[1] = std::move(val);
            continue;
        }

        if (s.op == IROp::Ret) {
            s.src = resolve(s.src, state);
            continue;
        }

        if (s.op == IROp::Call || s.src.op == IROp::Call) {
            IRExpr resolved_src = resolve(s.src, state);
            s.src = std::move(resolved_src);
            if (s.dst.valid()) {
                state.set_reg(s.dst.id, IRExpr::var(s.dst));
            }
            continue;
        }

        IRExpr resolved = resolve(s.src, state);
        s.src = resolved;
        if (s.dst.valid() && s.op == IROp::Assign) {
            if (s.dst.id != 4 && s.dst.id != 5) {
                state.set_reg(s.dst.id, resolved);
            }
        }
    }

    blk.cond = resolve(blk.cond, state);
}

void SymExec::eliminate_dead(IRFunc& func) {
    std::unordered_set<int> preserved = {0, 4, 5};

    for (int pass = 0; pass < 3; ++pass) {
        bool changed = false;
        for (auto& blk : func.blocks) {
            for (auto& s : blk.stmts) {
                if (s.op == IROp::Nop) continue;
                if (s.op == IROp::Call || s.op == IROp::Store || s.op == IROp::Ret) continue;
                if (s.src.op == IROp::Call || s.src.op == IROp::Store) continue;
                if (!s.dst.valid()) continue;
                if (preserved.count(s.dst.id)) continue;
                int uses = count_uses(func, s.dst.id);
                if (uses == 0) {
                    s.op = IROp::Nop;
                    changed = true;
                }
            }
        }
        if (!changed) break;
    }
}

void SymExec::run(IRFunc& func) {
    SymState state;

    for (auto& p : func.params)
        state.set_reg(p.id, IRExpr::var(p));

    for (auto& blk : func.blocks)
        exec_block(blk, state);

    eliminate_dead(func);
}

}
