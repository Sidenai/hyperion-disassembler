#pragma once
#include "core/decompiler/ir.h"
#include "core/analysis/analysis_db.h"

namespace hype {

class Lifter {
public:
    IRFunc lift(const Function& func, const AnalysisDB& db);

private:
    IRBlock lift_block(const BasicBlock& bb, const AnalysisDB& db);
    void    lift_insn(const Insn& insn, const AnalysisDB& db, IRBlock& out);
    IRExpr  operand_expr(const Insn& insn, int idx, const AnalysisDB& db);
    IRExpr  mem_expr(const Operand& op);
    IRVar   reg_var(u16 reg, u16 size);
    IRVar   alloc_tmp();
    void    detect_locals(IRFunc& func);

    int                            next_var_ = 64;
    std::unordered_map<int, IRVar> reg_map_;
    const Function*                cur_func_ = nullptr;
};

}
