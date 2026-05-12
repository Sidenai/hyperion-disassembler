#pragma once
#include "core/decompiler/ir.h"
#include "core/analysis/analysis_db.h"
#include <unordered_map>
#include <string_view>

namespace hype {

class LifterARM64 {
public:
    PcodeFunc lift(const Function& func, const AnalysisDB& db);

private:
    void lift_block(const BasicBlock& bb, const AnalysisDB& db, PcodeBlock& out);
    void lift_insn(const Insn& insn, PcodeBlock& blk, const AnalysisDB& db);

    Varnode reg_var(int capstone_reg);
    Varnode parse_reg(std::string_view tok);
    Varnode alloc_temp(int sz = 8);
    void emit(PcodeBlock& b, PcodeOp op, Varnode out, std::vector<Varnode> in, va_t a = 0);

    struct MemOp { Varnode base; i64 offset; bool pre_index; bool valid; };
    struct ParsedOps { Varnode ops[4]; int count; MemOp mem; bool has_mem; };
    ParsedOps parse_operands(std::string_view op_str);

    int next_temp_ = 256;
    va_t cur_addr_ = 0;
    int  cur_seq_  = 0;
    std::unordered_map<va_t, int> addr_to_block_;
};

}
