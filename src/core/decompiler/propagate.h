#pragma once
#include "core/decompiler/ir.h"

namespace hype {

class Propagate {
public:
    void run(PcodeFunc& func);

private:
    void eliminate_identity(PcodeFunc& func);
    void copy_propagation(PcodeFunc& func);
    void constant_fold(PcodeFunc& func);
    void fold_flags(PcodeFunc& func);
    void fold_arg_setup(PcodeFunc& func);
    void dead_copy_elim(PcodeFunc& func);
    void inline_single_use(PcodeFunc& func);
    bool is_used(const Varnode& vn, const PcodeFunc& func, int skip_blk, int skip_idx);
    void replace_uses(PcodeFunc& func, const Varnode& old_vn, const Varnode& new_vn, int def_blk, int def_idx);
};

}
