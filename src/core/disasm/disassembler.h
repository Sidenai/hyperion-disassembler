#pragma once
#include "core/types.h"
#include <string>
#include <vector>
#include <memory>

namespace hype {

enum class InsnType : u8 {
    Unknown, Nop, Mov, Push, Pop,
    Call, Ret, Jmp, Jcc,
    Cmp, Test,
    Add, Sub, Mul, Div,
    And, Or, Xor, Not, Shl, Shr,
    Lea, Int, Syscall, Other
};

enum class OpType : u8 { None, Reg, Imm, Mem };

struct Operand {
    OpType type = OpType::None;
    u64    val  = 0;
    u16    reg  = 0;
    u16    size = 0;
    struct { u16 base; u16 index; u8 scale; i64 disp; } mem{};
};

struct Insn {
    va_t        addr;
    u8          len;
    InsnType    type;
    u16         mnemonic_id;
    std::string mnemonic;
    std::string op_str;
    u8          bytes[15];
    Operand     ops[4];
    u8          op_count;

    bool is_branch()   const { return type == InsnType::Jmp || type == InsnType::Jcc; }
    bool is_call()     const { return type == InsnType::Call; }
    bool is_ret()      const { return type == InsnType::Ret; }
    bool is_cond_jmp() const { return type == InsnType::Jcc; }

    va_t branch_target() const {
        if (op_count > 0 && ops[0].type == OpType::Imm)
            return ops[0].val;
        return 0;
    }
};

class Disassembler {
public:
    Disassembler();
    ~Disassembler();

    void set_arch(Arch arch);
    bool decode(va_t addr, const u8* data, size_t len, Insn& out);
    std::vector<Insn> decode_range(va_t start, const u8* data, size_t len);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}
