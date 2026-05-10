#include "expr_prop.h"
#include <unordered_set>

namespace hype {

bool ExprProp::uses_var(const IRExpr& expr, int var_id) {
    if (expr.is_var() && expr.get_var().id == var_id) return true;
    for (auto& a : expr.args)
        if (uses_var(a, var_id)) return true;
    return false;
}

IRExpr ExprProp::substitute(const IRExpr& expr, int var_id, const IRExpr& replacement) {
    if (expr.is_var() && expr.get_var().id == var_id)
        return replacement;
    IRExpr result = expr;
    for (auto& a : result.args)
        a = substitute(a, var_id, replacement);
    return result;
}

int ExprProp::count_uses(const IRFunc& func, int var_id, int skip_block, int skip_stmt) {
    int count = 0;
    for (int bi = 0; bi < (int)func.blocks.size(); ++bi) {
        auto& blk = func.blocks[bi];
        for (int si = 0; si < (int)blk.stmts.size(); ++si) {
            if (bi == skip_block && si == skip_stmt) continue;
            if (uses_var(blk.stmts[si].src, var_id)) ++count;
        }
        if (uses_var(blk.cond, var_id)) ++count;
    }
    return count;
}

void ExprProp::propagate(IRFunc& func) {
    for (int pass = 0; pass < 4; ++pass) {
        bool changed = false;
        for (int bi = 0; bi < (int)func.blocks.size(); ++bi) {
            auto& blk = func.blocks[bi];
            for (int si = 0; si < (int)blk.stmts.size(); ++si) {
                auto& s = blk.stmts[si];
                if (s.op != IROp::Assign) continue;
                if (!s.dst.valid()) continue;
                if (s.dst.id == 4) continue; // don't propagate rsp
                if (s.dst.name.starts_with("__")) continue;

                int uses = count_uses(func, s.dst.id, bi, si);
                if (uses == 1) {
                    for (int si2 = si + 1; si2 < (int)blk.stmts.size(); ++si2) {
                        if (uses_var(blk.stmts[si2].src, s.dst.id)) {
                            blk.stmts[si2].src = substitute(blk.stmts[si2].src, s.dst.id, s.src);
                            s.op = IROp::Nop;
                            changed = true;
                            break;
                        }
                    }
                    if (s.op == IROp::Nop) continue;
                    if (uses_var(blk.cond, s.dst.id)) {
                        blk.cond = substitute(blk.cond, s.dst.id, s.src);
                        s.op = IROp::Nop;
                        changed = true;
                    }
                }
            }
        }
        if (!changed) break;
    }
}

void ExprProp::dead_store_elim(IRFunc& func) {
    std::unordered_set<int> preserved = {0, 4}; // rax (return), rsp

    for (int bi = 0; bi < (int)func.blocks.size(); ++bi) {
        auto& blk = func.blocks[bi];
        for (int si = (int)blk.stmts.size() - 1; si >= 0; --si) {
            auto& s = blk.stmts[si];
            if (s.op == IROp::Nop) continue;
            if (s.op == IROp::Call || s.op == IROp::Store || s.op == IROp::Ret) continue;
            if (!s.dst.valid()) continue;
            if (preserved.count(s.dst.id)) continue;

            int uses = count_uses(func, s.dst.id, bi, si);
            if (uses == 0) {
                s.op = IROp::Nop;
            }
        }
    }
}

void ExprProp::const_fold(IRFunc& func) {
    for (auto& blk : func.blocks) {
        for (auto& s : blk.stmts) {
            if (s.src.args.size() == 2 && s.src.args[0].is_imm() && s.src.args[1].is_imm()) {
                u64 l = s.src.args[0].get_imm();
                u64 r = s.src.args[1].get_imm();
                u64 result = 0;
                bool folded = true;
                switch (s.src.op) {
                case IROp::Add: result = l + r; break;
                case IROp::Sub: result = l - r; break;
                case IROp::Mul: result = l * r; break;
                case IROp::And: result = l & r; break;
                case IROp::Or:  result = l | r; break;
                case IROp::Xor: result = l ^ r; break;
                case IROp::Shl: result = l << r; break;
                case IROp::Shr: result = l >> r; break;
                default: folded = false; break;
                }
                if (folded) s.src = IRExpr::imm(result);
            }
        }
    }
}

void ExprProp::cleanup_internals(IRFunc& func) {
    for (auto& blk : func.blocks) {
        for (auto& s : blk.stmts) {
            if (s.op == IROp::Assign && s.dst.valid() && s.dst.name.starts_with("__"))
                s.op = IROp::Nop;
        }
    }
}

void ExprProp::run(IRFunc& func) {
    propagate(func);
    const_fold(func);
    propagate(func);
    dead_store_elim(func);
    cleanup_internals(func);
}

}
