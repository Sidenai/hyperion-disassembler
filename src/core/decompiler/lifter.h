#pragma once
#include "core/decompiler/ir.h"
#include "core/analysis/analysis_db.h"
#include <unordered_map>

namespace hype {

class Lifter {
public:
    PcodeFunc lift(const Function& func, const AnalysisDB& db);

private:
    void lift_block(const BasicBlock& bb, const AnalysisDB& db, PcodeBlock& out);
    void lift_insn(const Insn& insn, const AnalysisDB& db, PcodeBlock& out);

    Varnode reg_vn(u16 zreg, u16 bits);
    Varnode alloc_temp(int sz = 8);
    Varnode operand_read(const Insn& insn, int idx, const AnalysisDB& db, PcodeBlock& out);
    void    operand_write(const Insn& insn, int idx, Varnode val, PcodeBlock& out);

    void emit(PcodeBlock& b, PcodeOp op, Varnode out, std::vector<Varnode> in, va_t a = 0);
    void emit_flags(const Insn& insn, Varnode lhs, Varnode rhs, PcodeBlock& out, bool is_test = false);

    int next_temp_ = 256;
    va_t cur_addr_ = 0;
    int  cur_seq_  = 0;
    std::unordered_map<va_t, int> addr_to_block_;
};

}
