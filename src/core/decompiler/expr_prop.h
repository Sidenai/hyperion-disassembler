#pragma once
#include "core/decompiler/ir.h"

namespace hype {

class ExprProp {
public:
    void run(IRFunc& func);

private:
    void propagate(IRFunc& func);
    void dead_store_elim(IRFunc& func);
    void const_fold(IRFunc& func);
    void cleanup_internals(IRFunc& func);
    IRExpr substitute(const IRExpr& expr, int var_id, const IRExpr& replacement);
    bool uses_var(const IRExpr& expr, int var_id);
    int  count_uses(const IRFunc& func, int var_id, int skip_block, int skip_stmt);
};

}
