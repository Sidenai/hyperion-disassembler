#pragma once
#include "core/decompiler/ir.h"

namespace hype {

class DCE {
public:
    void run(PcodeFunc& func);

private:
    bool is_dead_flag(const PcodeInsn& op, const PcodeFunc& func);
    bool is_dead_stack_op(const PcodeInsn& op, const PcodeFunc& func);
    bool is_used(const Varnode& vn, const PcodeFunc& func, int skip_blk, int skip_idx);
    bool is_flag_reg(int id) const { return id >= 100 && id <= 103; }
    bool is_callee_saved(int id) const;
};

}
