#pragma once
#include "core/decompiler/ir.h"
#include <unordered_map>
#include <string>
#include <memory>

namespace hype {

enum class DTypeKind : u8 {
    Void, Bool, Char, WChar,
    Int8, Int16, Int32, Int64,
    UInt8, UInt16, UInt32, UInt64,
    Float, Double,
    Pointer, Array, FuncPtr, SizeT
};

struct DecompType {
    DTypeKind                   kind = DTypeKind::Int64;
    std::shared_ptr<DecompType> inner;
    int                         array_count = 0;
    bool                        is_const = false;

    static DecompType make_void()    { return {DTypeKind::Void}; }
    static DecompType make_bool()    { return {DTypeKind::Bool}; }
    static DecompType make_char()    { return {DTypeKind::Char}; }
    static DecompType make_int(int bits, bool sign = true) {
        switch (bits) {
        case 8:  return {sign ? DTypeKind::Int8  : DTypeKind::UInt8};
        case 16: return {sign ? DTypeKind::Int16 : DTypeKind::UInt16};
        case 32: return {sign ? DTypeKind::Int32 : DTypeKind::UInt32};
        default: return {sign ? DTypeKind::Int64 : DTypeKind::UInt64};
        }
    }
    static DecompType make_sizet()   { return {DTypeKind::SizeT}; }
    static DecompType make_ptr(DecompType pointee, bool is_const = false) {
        DecompType t;
        t.kind = DTypeKind::Pointer;
        t.inner = std::make_shared<DecompType>(std::move(pointee));
        t.is_const = is_const;
        return t;
    }

    std::string to_string() const;
    bool is_pointer() const { return kind == DTypeKind::Pointer; }
    bool is_integer() const {
        return kind >= DTypeKind::Int8 && kind <= DTypeKind::UInt64;
    }
    int bit_width() const;
};

struct KnownFunc {
    std::string              name;
    DecompType               ret_type;
    std::vector<DecompType>  param_types;
    std::vector<std::string> param_names;
};

class TypeInfer {
public:
    void run(IRFunc& func);

    DecompType get_type(int var_id) const;
    std::string get_var_name(int var_id) const;
    const std::unordered_map<int, DecompType>& types() const { return types_; }
    const std::unordered_map<int, std::string>& names() const { return names_; }
    DecompType return_type() const { return ret_type_; }

private:
    void init_known_funcs();
    void infer_from_sizes(const IRFunc& func);
    void infer_from_calls(const IRFunc& func);
    void infer_from_cmp(const IRFunc& func);
    void infer_from_usage(const IRFunc& func);
    void propagate(const IRFunc& func);
    void name_variables(const IRFunc& func);
    void set_type(int var_id, DecompType t);

    std::unordered_map<int, DecompType>  types_;
    std::unordered_map<int, std::string> names_;
    std::vector<KnownFunc>               known_funcs_;
    DecompType                           ret_type_{DTypeKind::Int64};
};

}
