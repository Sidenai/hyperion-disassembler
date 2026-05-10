#pragma once
#include "core/decompiler/ir.h"
#include <string>
#include <vector>

namespace hype {

enum class StmtKind { Expr, If, IfElse, While, DoWhile, Goto, Return, Block };

struct CStmt {
    StmtKind              kind = StmtKind::Expr;
    IRExpr                expr;
    IRExpr                cond;
    std::vector<CStmt>    body;
    std::vector<CStmt>    else_body;
    va_t                  addr = 0;
    va_t                  goto_target = 0;
};

struct CFunc {
    std::string           name;
    va_t                  entry = 0;
    std::vector<IRVar>    params;
    std::vector<IRVar>    locals;
    std::vector<CStmt>    body;
};

class CFStructure {
public:
    CFunc structure(const IRFunc& func);

private:
    void structure_region(const IRFunc& func, const std::vector<int>& block_order,
                         int start, int end, std::vector<CStmt>& out);
    int  block_index(const IRFunc& func, va_t addr);
    bool dominates(const std::vector<std::vector<int>>& dom_tree, int a, int b);
    std::vector<int> compute_order(const IRFunc& func);
};

}
