#pragma once
#include "core/types.h"
#include <string>
#include <vector>
#include <variant>

namespace hype {

enum class IROp : u8 {
    Assign, Add, Sub, Mul, Div,
    And, Or, Xor, Shl, Shr, Not, Neg,
    Load, Store, Call, Ret, Cond, Goto, Phi, Nop,
    Eq, Ne, Lt, Le, Gt, Ge, SignedLt, SignedLe, SignedGt, SignedGe
};

struct IRVar {
    int         id   = -1;
    std::string name;
    int         size = 8;

    bool valid() const { return id >= 0; }
    bool operator==(const IRVar& o) const { return id == o.id; }
    bool operator!=(const IRVar& o) const { return id != o.id; }
};

struct IRExpr {
    IROp                                op  = IROp::Nop;
    std::variant<IRVar, u64, std::string> val;
    std::vector<IRExpr>                 args;

    static IRExpr var(IRVar v) { IRExpr e; e.op = IROp::Assign; e.val = v; return e; }
    static IRExpr imm(u64 v) { IRExpr e; e.op = IROp::Assign; e.val = v; return e; }
    static IRExpr str(std::string s) { IRExpr e; e.op = IROp::Assign; e.val = std::move(s); return e; }
    static IRExpr binop(IROp op, IRExpr l, IRExpr r) {
        IRExpr e; e.op = op; e.args = {std::move(l), std::move(r)}; return e;
    }
    static IRExpr unop(IROp op, IRExpr a) {
        IRExpr e; e.op = op; e.args = {std::move(a)}; return e;
    }
    static IRExpr call(std::string name, std::vector<IRExpr> a) {
        IRExpr e; e.op = IROp::Call; e.val = std::move(name); e.args = std::move(a); return e;
    }
    static IRExpr load(IRExpr addr, int sz) {
        IRExpr e; e.op = IROp::Load; e.val = (u64)sz; e.args = {std::move(addr)}; return e;
    }

    bool is_var() const { return op == IROp::Assign && std::holds_alternative<IRVar>(val) && args.empty(); }
    bool is_imm() const { return op == IROp::Assign && std::holds_alternative<u64>(val) && args.empty(); }
    IRVar get_var() const { return std::get<IRVar>(val); }
    u64   get_imm() const { return std::get<u64>(val); }
};

struct IRStmt {
    IROp   op  = IROp::Nop;
    IRVar  dst;
    IRExpr src;
    va_t   addr = 0;
};

struct IRBlock {
    va_t               addr = 0;
    std::vector<IRStmt> stmts;
    std::vector<va_t>  succs;
    IRExpr             cond;
    va_t               cond_true  = 0;
    va_t               cond_false = 0;
    bool               has_ret    = false;
};

struct IRFunc {
    va_t               entry = 0;
    std::string        name;
    std::vector<IRBlock> blocks;
    std::vector<IRVar> params;
    std::vector<IRVar> locals;
    int                next_var = 0;
};

}
