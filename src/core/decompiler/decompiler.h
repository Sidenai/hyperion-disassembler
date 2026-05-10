#pragma once
#include "core/decompiler/lifter.h"
#include "core/decompiler/sym_exec.h"
#include "core/decompiler/type_infer.h"
#include "core/decompiler/expr_prop.h"
#include "core/decompiler/cf_struct.h"
#include "core/decompiler/emitter.h"
#include "core/decompiler/pseudo_gen.h"

namespace hype {

class Decompiler {
public:
    std::vector<PseudoLine> decompile(const Function& func, const AnalysisDB& db);

private:
    Lifter      lifter_;
    SymExec     sym_;
    TypeInfer   ti_;
    ExprProp    prop_;
    CFStructure cf_;
    Emitter     emitter_;
};

}
