#pragma once
#include "core/decompiler/cf_struct.h"
#include "core/decompiler/type_infer.h"
#include "core/decompiler/pseudo_gen.h"

namespace hype {

class Emitter {
public:
    std::vector<PseudoLine> emit(const CFunc& func, const TypeInfer* ti = nullptr);

private:
    void emit_stmt(const CStmt& s, int indent, std::vector<PseudoLine>& out);
    std::string expr_str(const IRExpr& e);
    std::string cond_str(const IRExpr& e);
    std::string var_str(const IRVar& v);
    std::string type_str(int var_id);
    std::string format_imm(u64 val);
    std::string format_deref(const IRExpr& e);
    bool        is_zero_cmp(const IRExpr& e, bool& is_eq);

    const TypeInfer* ti_ = nullptr;
};

}
