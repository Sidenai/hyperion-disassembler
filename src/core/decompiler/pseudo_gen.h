#pragma once
#include "core/analysis/analysis_db.h"
#include <string>
#include <vector>

namespace hype {

struct PseudoLine {
    int         indent;
    std::string text;
    va_t        addr;  // 0 if no address mapping
};

class PseudoGen {
public:
    std::vector<PseudoLine> generate(const Function& func, const AnalysisDB& db);

private:
    void emit_block(const BasicBlock& bb, const AnalysisDB& db,
                    std::vector<PseudoLine>& out, int indent);
    std::string operand_to_c(const Insn& insn, int op_idx);
};

}
