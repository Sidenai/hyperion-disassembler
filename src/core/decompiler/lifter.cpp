#include "lifter.h"
#include <Zydis/Zydis.h>
#include <fmt/format.h>

namespace hype {

static const struct { u16 zreg; const char* name; int id; int size; } kRegTable[] = {
    {ZYDIS_REGISTER_RAX, "rax", 0, 8}, {ZYDIS_REGISTER_EAX, "eax", 0, 4}, {ZYDIS_REGISTER_AX, "ax", 0, 2}, {ZYDIS_REGISTER_AL, "al", 0, 1},
    {ZYDIS_REGISTER_RCX, "rcx", 1, 8}, {ZYDIS_REGISTER_ECX, "ecx", 1, 4}, {ZYDIS_REGISTER_CX, "cx", 1, 2}, {ZYDIS_REGISTER_CL, "cl", 1, 1},
    {ZYDIS_REGISTER_RDX, "rdx", 2, 8}, {ZYDIS_REGISTER_EDX, "edx", 2, 4}, {ZYDIS_REGISTER_DX, "dx", 2, 2}, {ZYDIS_REGISTER_DL, "dl", 2, 1},
    {ZYDIS_REGISTER_RBX, "rbx", 3, 8}, {ZYDIS_REGISTER_EBX, "ebx", 3, 4}, {ZYDIS_REGISTER_BX, "bx", 3, 2}, {ZYDIS_REGISTER_BL, "bl", 3, 1},
    {ZYDIS_REGISTER_RSP, "rsp", 4, 8}, {ZYDIS_REGISTER_ESP, "esp", 4, 4},
    {ZYDIS_REGISTER_RBP, "rbp", 5, 8}, {ZYDIS_REGISTER_EBP, "ebp", 5, 4},
    {ZYDIS_REGISTER_RSI, "rsi", 6, 8}, {ZYDIS_REGISTER_ESI, "esi", 6, 4},
    {ZYDIS_REGISTER_RDI, "rdi", 7, 8}, {ZYDIS_REGISTER_EDI, "edi", 7, 4},
    {ZYDIS_REGISTER_R8,  "r8",  8, 8}, {ZYDIS_REGISTER_R8D, "r8d", 8, 4},
    {ZYDIS_REGISTER_R9,  "r9",  9, 8}, {ZYDIS_REGISTER_R9D, "r9d", 9, 4},
    {ZYDIS_REGISTER_R10, "r10", 10, 8},{ZYDIS_REGISTER_R10D,"r10d",10, 4},
    {ZYDIS_REGISTER_R11, "r11", 11, 8},{ZYDIS_REGISTER_R11D,"r11d",11, 4},
    {ZYDIS_REGISTER_R12, "r12", 12, 8},{ZYDIS_REGISTER_R12D,"r12d",12, 4},
    {ZYDIS_REGISTER_R13, "r13", 13, 8},{ZYDIS_REGISTER_R13D,"r13d",13, 4},
    {ZYDIS_REGISTER_R14, "r14", 14, 8},{ZYDIS_REGISTER_R14D,"r14d",14, 4},
    {ZYDIS_REGISTER_R15, "r15", 15, 8},{ZYDIS_REGISTER_R15D,"r15d",15, 4},
};

IRVar Lifter::reg_var(u16 reg, u16 size) {
    for (auto& r : kRegTable) {
        if (r.zreg == reg)
            return IRVar{r.id, r.name, r.size};
    }
    IRVar v;
    v.id = 50 + (reg % 50);
    v.name = fmt::format("reg{}", reg);
    v.size = size / 8;
    return v;
}

IRVar Lifter::alloc_tmp() {
    IRVar v;
    v.id = next_var_;
    v.name = fmt::format("t{}", next_var_);
    v.size = 8;
    ++next_var_;
    return v;
}

IRExpr Lifter::mem_expr(const Operand& op) {
    IRExpr result = IRExpr::imm(0);
    bool has = false;

    if (op.mem.base && op.mem.base != ZYDIS_REGISTER_NONE) {
        result = IRExpr::var(reg_var(op.mem.base, 64));
        has = true;
    }

    if (op.mem.index && op.mem.index != ZYDIS_REGISTER_NONE) {
        IRExpr idx = IRExpr::var(reg_var(op.mem.index, 64));
        if (op.mem.scale > 1)
            idx = IRExpr::binop(IROp::Mul, idx, IRExpr::imm(op.mem.scale));
        result = has ? IRExpr::binop(IROp::Add, result, idx) : idx;
        has = true;
    }

    if (op.mem.disp != 0) {
        IRExpr d = IRExpr::imm(static_cast<u64>(op.mem.disp));
        result = has ? IRExpr::binop(IROp::Add, result, d) : d;
    } else if (!has) {
        result = IRExpr::imm(0);
    }

    return result;
}

IRExpr Lifter::operand_expr(const Insn& insn, int idx, const AnalysisDB& db) {
    if (idx >= insn.op_count) return IRExpr::imm(0);
    auto& op = insn.ops[idx];
    switch (op.type) {
    case OpType::Reg:
        return IRExpr::var(reg_var(op.reg, op.size));
    case OpType::Imm: {
        for (auto& [saddr, sval] : db.strings) {
            if (saddr == op.val)
                return IRExpr::str(sval.substr(0, 60));
        }
        return IRExpr::imm(op.val);
    }
    case OpType::Mem: {
        if (op.val) {
            for (auto& [saddr, sval] : db.strings) {
                if (saddr == op.val)
                    return IRExpr::str(sval.substr(0, 60));
            }
        }
        IRExpr addr = mem_expr(op);
        return IRExpr::load(std::move(addr), op.size / 8);
    }
    default:
        return IRExpr::imm(0);
    }
}

static IROp jcc_to_cmp(u16 mnemonic_id) {
    switch (mnemonic_id) {
    case ZYDIS_MNEMONIC_JZ:  return IROp::Eq;
    case ZYDIS_MNEMONIC_JNZ: return IROp::Ne;
    case ZYDIS_MNEMONIC_JL:  return IROp::SignedLt;
    case ZYDIS_MNEMONIC_JLE: return IROp::SignedLe;
    case ZYDIS_MNEMONIC_JNL: return IROp::SignedGe;
    case ZYDIS_MNEMONIC_JNLE:return IROp::SignedGt;
    case ZYDIS_MNEMONIC_JB:  return IROp::Lt;
    case ZYDIS_MNEMONIC_JBE: return IROp::Le;
    case ZYDIS_MNEMONIC_JNB: return IROp::Ge;
    case ZYDIS_MNEMONIC_JNBE:return IROp::Gt;
    default: return IROp::Ne;
    }
}

void Lifter::lift_insn(const Insn& insn, const AnalysisDB& db, IRBlock& out) {
    auto stmt = [&](IROp op, IRVar dst, IRExpr src) {
        out.stmts.push_back({op, dst, std::move(src), insn.addr});
    };

    switch (insn.type) {
    case InsnType::Mov: {
        auto& dst_op = insn.ops[0];
        IRExpr src = operand_expr(insn, 1, db);
        if (dst_op.type == OpType::Reg) {
            stmt(IROp::Assign, reg_var(dst_op.reg, dst_op.size), std::move(src));
        } else if (dst_op.type == OpType::Mem) {
            IRExpr addr = mem_expr(dst_op);
            IRExpr store_expr;
            store_expr.op = IROp::Store;
            store_expr.args = {std::move(addr), std::move(src)};
            store_expr.val = (u64)(dst_op.size / 8);
            auto tmp = alloc_tmp();
            stmt(IROp::Store, tmp, std::move(store_expr));
        }
        break;
    }
    case InsnType::Lea: {
        auto& dst_op = insn.ops[0];
        if (dst_op.type == OpType::Reg) {
            auto& src_op = insn.ops[1];
            IRExpr addr = mem_expr(src_op);
            stmt(IROp::Assign, reg_var(dst_op.reg, dst_op.size), std::move(addr));
        }
        break;
    }
    case InsnType::Add:
    case InsnType::Sub:
    case InsnType::And:
    case InsnType::Or:
    case InsnType::Xor:
    case InsnType::Shl:
    case InsnType::Shr: {
        auto& dst_op = insn.ops[0];
        if (dst_op.type == OpType::Reg) {
            IRVar dv = reg_var(dst_op.reg, dst_op.size);
            IRExpr src = operand_expr(insn, 1, db);
            IROp op = IROp::Add;
            switch (insn.type) {
            case InsnType::Add: op = IROp::Add; break;
            case InsnType::Sub: op = IROp::Sub; break;
            case InsnType::And: op = IROp::And; break;
            case InsnType::Or:  op = IROp::Or;  break;
            case InsnType::Xor: op = IROp::Xor; break;
            case InsnType::Shl: op = IROp::Shl; break;
            case InsnType::Shr: op = IROp::Shr; break;
            default: break;
            }
            if (insn.type == InsnType::Xor && insn.op_count >= 2 &&
                insn.ops[0].type == OpType::Reg && insn.ops[1].type == OpType::Reg &&
                insn.ops[0].reg == insn.ops[1].reg) {
                stmt(IROp::Assign, dv, IRExpr::imm(0));
            } else {
                stmt(IROp::Assign, dv, IRExpr::binop(op, IRExpr::var(dv), std::move(src)));
            }
        }
        break;
    }
    case InsnType::Mul: {
        IRVar rax{0, "rax", 8};
        IRExpr src = operand_expr(insn, insn.op_count > 1 ? 1 : 0, db);
        stmt(IROp::Assign, rax, IRExpr::binop(IROp::Mul, IRExpr::var(rax), std::move(src)));
        break;
    }
    case InsnType::Not: {
        auto& dst_op = insn.ops[0];
        if (dst_op.type == OpType::Reg) {
            IRVar dv = reg_var(dst_op.reg, dst_op.size);
            stmt(IROp::Assign, dv, IRExpr::unop(IROp::Not, IRExpr::var(dv)));
        }
        break;
    }
    case InsnType::Call: {
        va_t target = insn.branch_target();
        std::string name;
        if (target) {
            auto nit = db.names.find(target);
            name = (nit != db.names.end()) ? nit->second : fmt::format("sub_{:X}", target);
        } else {
            name = fmt::format("indirect_{:X}", insn.addr);
        }
        IRVar rcx{1, "rcx", 8}, rdx{2, "rdx", 8}, r8{8, "r8", 8}, r9{9, "r9", 8};
        std::vector<IRExpr> args = {
            IRExpr::var(rcx), IRExpr::var(rdx), IRExpr::var(r8), IRExpr::var(r9)
        };
        IRVar rax{0, "rax", 8};
        stmt(IROp::Call, rax, IRExpr::call(std::move(name), std::move(args)));
        break;
    }
    case InsnType::Ret:
        out.has_ret = true;
        stmt(IROp::Ret, IRVar{}, IRExpr::var(IRVar{0, "rax", 8}));
        break;
    case InsnType::Push: {
        IRVar sp{4, "rsp", 8};
        stmt(IROp::Assign, sp, IRExpr::binop(IROp::Sub, IRExpr::var(sp), IRExpr::imm(8)));
        break;
    }
    case InsnType::Pop: {
        auto& dst_op = insn.ops[0];
        IRVar sp{4, "rsp", 8};
        if (dst_op.type == OpType::Reg) {
            stmt(IROp::Assign, reg_var(dst_op.reg, dst_op.size), IRExpr::load(IRExpr::var(sp), 8));
        }
        stmt(IROp::Assign, sp, IRExpr::binop(IROp::Add, IRExpr::var(sp), IRExpr::imm(8)));
        break;
    }
    case InsnType::Cmp:
    case InsnType::Test: {
        IRVar cmp_var{60, "__cmp_l", 8};
        IRVar cmp_r{61, "__cmp_r", 8};
        IRExpr lhs = operand_expr(insn, 0, db);
        IRExpr rhs = operand_expr(insn, 1, db);
        stmt(IROp::Assign, cmp_var, std::move(lhs));
        stmt(IROp::Assign, cmp_r, std::move(rhs));
        break;
    }
    case InsnType::Jcc: {
        IROp cmp_op = jcc_to_cmp(insn.mnemonic_id);
        IRVar cmp_l{60, "__cmp_l", 8};
        IRVar cmp_r{61, "__cmp_r", 8};
        out.cond = IRExpr::binop(cmp_op, IRExpr::var(cmp_l), IRExpr::var(cmp_r));
        out.cond_true = insn.branch_target();
        break;
    }
    case InsnType::Jmp:
        break;
    case InsnType::Nop:
        break;
    default: {
        auto tmp = alloc_tmp();
        IRExpr asm_expr;
        asm_expr.op = IROp::Nop;
        asm_expr.val = fmt::format("{} {}", insn.mnemonic, insn.op_str);
        stmt(IROp::Nop, tmp, std::move(asm_expr));
        break;
    }
    }
}

IRBlock Lifter::lift_block(const BasicBlock& bb, const AnalysisDB& db) {
    IRBlock blk;
    blk.addr = bb.start;
    blk.succs = bb.succs;

    for (auto& insn : bb.insns)
        lift_insn(insn, db, blk);

    if (blk.cond_true && bb.succs.size() >= 2) {
        for (va_t s : bb.succs) {
            if (s != blk.cond_true)
                blk.cond_false = s;
        }
    } else if (blk.cond_true && bb.succs.size() == 1) {
        blk.cond_false = 0;
    }

    return blk;
}

void Lifter::detect_locals(IRFunc& func) {
    IRVar sp{4, "rsp", 8};
    for (auto& blk : func.blocks) {
        for (auto& s : blk.stmts) {
            if (s.src.op == IROp::Load && !s.src.args.empty()) {
                auto& addr = s.src.args[0];
                if (addr.op == IROp::Add && addr.args.size() == 2) {
                    if (addr.args[0].is_var() && addr.args[0].get_var().id == sp.id &&
                        addr.args[1].is_imm()) {
                        u64 off = addr.args[1].get_imm();
                        auto name = (off >= 0x28) ? fmt::format("arg_{:X}", off - 0x28)
                                                  : fmt::format("local_{:X}", off);
                        IRVar local;
                        local.id = next_var_++;
                        local.name = name;
                        local.size = 8;
                        func.locals.push_back(local);
                        s.src = IRExpr::var(local);
                    }
                }
            }
            if (s.src.op == IROp::Store && s.src.args.size() >= 2) {
                auto& addr = s.src.args[0];
                if (addr.op == IROp::Add && addr.args.size() == 2) {
                    if (addr.args[0].is_var() && addr.args[0].get_var().id == sp.id &&
                        addr.args[1].is_imm()) {
                        u64 off = addr.args[1].get_imm();
                        auto name = (off >= 0x28) ? fmt::format("arg_{:X}", off - 0x28)
                                                  : fmt::format("local_{:X}", off);
                        IRVar local;
                        local.id = next_var_++;
                        local.name = name;
                        local.size = 8;
                        func.locals.push_back(local);
                        s.dst = local;
                        s.op = IROp::Assign;
                        s.src = s.src.args[1];
                    }
                }
            }
        }
    }
}

IRFunc Lifter::lift(const Function& func, const AnalysisDB& db) {
    next_var_ = 64;
    cur_func_ = &func;

    IRFunc ir;
    ir.entry = func.entry;
    ir.name = func.name;

    std::vector<va_t> order;
    for (auto& [addr, _] : func.blocks)
        order.push_back(addr);
    std::sort(order.begin(), order.end());

    for (va_t baddr : order) {
        auto it = func.blocks.find(baddr);
        if (it == func.blocks.end()) continue;
        ir.blocks.push_back(lift_block(it->second, db));
    }

    ir.params = {
        IRVar{1, "arg1", 8}, IRVar{2, "arg2", 8},
        IRVar{8, "arg3", 8}, IRVar{9, "arg4", 8}
    };
    ir.next_var = next_var_;

    detect_locals(ir);
    return ir;
}

}
