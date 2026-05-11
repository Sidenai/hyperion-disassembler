#pragma once
#include "core/decompiler/ir.h"
#include <string>
#include <vector>

namespace hype {

enum class StmtKind { Expr, Assign, If, IfElse, While, DoWhile, For, Goto, Return, Block, Switch };

struct CExpr {
    PcodeOp              op = PcodeOp::NOP;
    Varnode              vn;
    std::string          call_name;
    std::vector<CExpr>   args;
    bool                 negated = false;

    static CExpr var(Varnode v) { CExpr e; e.op = PcodeOp::COPY; e.vn = v; return e; }
    static CExpr imm(u64 val, int sz = 8) { CExpr e; e.op = PcodeOp::COPY; e.vn = vn_const(val, sz); return e; }
    static CExpr binop(PcodeOp p, CExpr l, CExpr r) {
        CExpr e; e.op = p; e.args = {std::move(l), std::move(r)}; return e;
    }
    static CExpr call(std::string name, std::vector<CExpr> a) {
        CExpr e; e.op = PcodeOp::CALL; e.call_name = std::move(name); e.args = std::move(a); return e;
    }
    static CExpr deref(CExpr addr) {
        CExpr e; e.op = PcodeOp::LOAD; e.args = {std::move(addr)}; return e;
    }
    bool is_var() const { return op == PcodeOp::COPY && !vn.is_const() && args.empty(); }
    bool is_const() const { return op == PcodeOp::COPY && vn.is_const() && args.empty(); }
};

struct CStmt {
    StmtKind              kind = StmtKind::Expr;
    CExpr                 expr;
    CExpr                 cond;
    Varnode               dst;
    std::vector<CStmt>    body;
    std::vector<CStmt>    else_body;
    va_t                  addr = 0;
    va_t                  goto_target = 0;
    CExpr                 for_init;
    CExpr                 for_incr;
    Varnode               for_var;
};

struct CFunc {
    std::string           name;
    va_t                  entry = 0;
    std::vector<Varnode>  params;
    std::vector<Varnode>  locals;
    std::vector<CStmt>    body;
};

class CFStructure {
public:
    CFunc structure(const PcodeFunc& func);

private:
    void structure_region(const PcodeFunc& func, int start, int end, std::vector<CStmt>& out);
    void emit_block_stmts(const PcodeBlock& blk, std::vector<CStmt>& out);
    CExpr build_expr(const PcodeInsn& op);
    CExpr varnode_expr(const Varnode& vn);
    int find_branch_target(const PcodeBlock& blk);
    bool is_back_edge(int from, int to);
    bool try_for_loop(const PcodeFunc& func, int header_idx, int back_edge_src,
                      const std::vector<CStmt>& pre_stmts, std::vector<CStmt>& out);
};

}
