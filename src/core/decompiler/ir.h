#pragma once
#include "core/types.h"
#include <string>
#include <vector>
#include <variant>

namespace hype {

enum class PcodeOp : u8 {
    COPY, LOAD, STORE,
    ADD, SUB, AND, OR, XOR,
    INT_LESS, INT_SLESS, INT_EQUAL, INT_NEQUAL,
    BOOL_AND, BOOL_OR, BOOL_NOT,
    CALL, RETURN, BRANCH, CBRANCH,
    SHIFT_LEFT, SHIFT_RIGHT,
    INT_MULT, INT_DIV,
    INT_ZEXT, INT_SEXT,
    PIECE, SUBPIECE,
    INT_NEGATE, INT_NOT,
    NOP
};

enum class VarnodeKind : u8 { Reg, Temp, Const, Ram, Stack };

struct Varnode {
    VarnodeKind kind  = VarnodeKind::Const;
    int         id    = -1;
    int         size  = 8;
    u64         offset = 0;
    std::string name;

    bool valid() const { return id >= 0 || kind == VarnodeKind::Const; }
    bool is_reg() const { return kind == VarnodeKind::Reg; }
    bool is_temp() const { return kind == VarnodeKind::Temp; }
    bool is_const() const { return kind == VarnodeKind::Const; }
    bool is_stack() const { return kind == VarnodeKind::Stack; }
    bool operator==(const Varnode& o) const { return kind == o.kind && id == o.id && offset == o.offset; }
    bool operator!=(const Varnode& o) const { return !(*this == o); }
};

struct PcodeInsn {
    PcodeOp              op = PcodeOp::NOP;
    Varnode              output;
    std::vector<Varnode> inputs;
    va_t                 addr = 0;
    int                  seq  = 0;

    static PcodeInsn make(PcodeOp op, Varnode out, std::vector<Varnode> in, va_t a = 0) {
        return {op, out, std::move(in), a, 0};
    }
};

struct PcodeBlock {
    int                    id = -1;
    va_t                   addr = 0;
    std::vector<PcodeInsn> ops;
    std::vector<int>       succs;
    std::vector<int>       preds;
    bool                   has_return = false;
};

struct PcodeFunc {
    va_t                    entry = 0;
    std::string             name;
    std::vector<PcodeBlock> blocks;
    int                     next_temp = 0;
    int                     next_ssa  = 0;
    std::vector<Varnode>    params;
    std::vector<Varnode>    locals;
};

inline Varnode vn_reg(int id, const char* name, int sz = 8) {
    return {VarnodeKind::Reg, id, sz, 0, name};
}
inline Varnode vn_temp(int id, int sz = 8) {
    return {VarnodeKind::Temp, id, sz, 0, {}};
}
inline Varnode vn_const(u64 val, int sz = 8) {
    return {VarnodeKind::Const, -1, sz, val, {}};
}
inline Varnode vn_stack(int id, i64 off, int sz = 8) {
    return {VarnodeKind::Stack, id, sz, static_cast<u64>(off), {}};
}

}
