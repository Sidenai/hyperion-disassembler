#include "decompiler.h"

namespace hype {

std::vector<PseudoLine> Decompiler::decompile(const Function& func, const AnalysisDB& db) {
    if (func.blocks.empty())
        return {{0, "// empty function", func.entry}};

    IRFunc ir = lifter_.lift(func, db);
    sym_.run(ir);
    ti_.run(ir);
    prop_.run(ir);
    CFunc cf = cf_.structure(ir);
    return emitter_.emit(cf, &ti_);
}

}
