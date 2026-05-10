#include "pseudo_gen.h"
#include <fmt/format.h>
#include <unordered_set>

namespace hype {

// Simplified C-like output from disassembly (proper decompiler uses Remill/Rellic)
std::vector<PseudoLine> PseudoGen::generate(const Function& func, const AnalysisDB& db) {
    std::vector<PseudoLine> out;

    auto sig = fmt::format("void {}()", func.name);
    out.push_back({0, sig + " {", func.entry});

    // Emit blocks in address order
    std::vector<va_t> order;
    for (auto& [addr, _] : func.blocks)
        order.push_back(addr);
    std::sort(order.begin(), order.end());

    for (va_t baddr : order) {
        auto bit = func.blocks.find(baddr);
        if (bit == func.blocks.end()) continue;
        out.push_back({0, fmt::format("loc_{:X}:", baddr), baddr});
        emit_block(bit->second, db, out, 1);
    }

    out.push_back({0, "}", 0});
    return out;
}

void PseudoGen::emit_block(const BasicBlock& bb, const AnalysisDB& db,
                           std::vector<PseudoLine>& out, int indent) {
    for (auto& insn : bb.insns) {
        std::string line;
        switch (insn.type) {
        case InsnType::Mov:
        case InsnType::Lea:
            if (insn.op_count >= 2)
                line = fmt::format("{} = {};", operand_to_c(insn, 0), operand_to_c(insn, 1));
            break;
        case InsnType::Call: {
            va_t target = insn.branch_target();
            std::string name;
            if (target) {
                auto nit = db.names.find(target);
                if (nit != db.names.end()) name = nit->second;
                else name = fmt::format("sub_{:X}", target);
            } else {
                name = operand_to_c(insn, 0);
            }
            line = fmt::format("{}();", name);
            break;
        }
        case InsnType::Ret:
            line = "return;";
            break;
        case InsnType::Push:
            line = fmt::format("push({});", operand_to_c(insn, 0));
            break;
        case InsnType::Pop:
            line = fmt::format("{} = pop();", operand_to_c(insn, 0));
            break;
        case InsnType::Cmp:
        case InsnType::Test:
            if (insn.op_count >= 2)
                line = fmt::format("// cmp {} , {}", operand_to_c(insn, 0), operand_to_c(insn, 1));
            break;
        case InsnType::Jcc:
            line = fmt::format("if (cond) goto loc_{:X};", insn.branch_target());
            break;
        case InsnType::Jmp:
            line = fmt::format("goto loc_{:X};", insn.branch_target());
            break;
        case InsnType::Add:
            if (insn.op_count >= 2)
                line = fmt::format("{} += {};", operand_to_c(insn, 0), operand_to_c(insn, 1));
            break;
        case InsnType::Sub:
            if (insn.op_count >= 2)
                line = fmt::format("{} -= {};", operand_to_c(insn, 0), operand_to_c(insn, 1));
            break;
        case InsnType::Xor:
            if (insn.op_count >= 2) {
                auto a = operand_to_c(insn, 0);
                auto b = operand_to_c(insn, 1);
                if (a == b) line = fmt::format("{} = 0;", a);
                else line = fmt::format("{} ^= {};", a, b);
            }
            break;
        case InsnType::And:
            if (insn.op_count >= 2)
                line = fmt::format("{} &= {};", operand_to_c(insn, 0), operand_to_c(insn, 1));
            break;
        case InsnType::Or:
            if (insn.op_count >= 2)
                line = fmt::format("{} |= {};", operand_to_c(insn, 0), operand_to_c(insn, 1));
            break;
        case InsnType::Nop:
            break;
        default:
            line = fmt::format("__asm {{ {} {} }}", insn.mnemonic, insn.op_str);
            break;
        }
        if (!line.empty())
            out.push_back({indent, line, insn.addr});
    }
}

std::string PseudoGen::operand_to_c(const Insn& insn, int op_idx) {
    if (op_idx >= insn.op_count) return "?";
    auto& op = insn.ops[op_idx];
    switch (op.type) {
    case OpType::Reg:
        return insn.op_str;  // simplified - real impl maps register names
    case OpType::Imm:
        return fmt::format("0x{:X}", op.val);
    case OpType::Mem:
        if (op.val)
            return fmt::format("*(0x{:X})", op.val);
        return fmt::format("[mem]");
    default:
        return "?";
    }
}

}
