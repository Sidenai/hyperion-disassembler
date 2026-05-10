#include "type_infer.h"
#include <fmt/format.h>
#include <algorithm>

namespace hype {

int DecompType::bit_width() const {
    switch (kind) {
    case DTypeKind::Bool: case DTypeKind::Int8: case DTypeKind::UInt8: case DTypeKind::Char: return 8;
    case DTypeKind::Int16: case DTypeKind::UInt16: case DTypeKind::WChar: return 16;
    case DTypeKind::Int32: case DTypeKind::UInt32: case DTypeKind::Float: return 32;
    case DTypeKind::Int64: case DTypeKind::UInt64: case DTypeKind::Double:
    case DTypeKind::Pointer: case DTypeKind::FuncPtr: case DTypeKind::SizeT: return 64;
    default: return 64;
    }
}

std::string DecompType::to_string() const {
    switch (kind) {
    case DTypeKind::Void:   return "void";
    case DTypeKind::Bool:   return "bool";
    case DTypeKind::Char:   return "char";
    case DTypeKind::WChar:  return "wchar_t";
    case DTypeKind::Int8:   return "int8_t";
    case DTypeKind::Int16:  return "int16_t";
    case DTypeKind::Int32:  return "int32_t";
    case DTypeKind::Int64:  return "int64_t";
    case DTypeKind::UInt8:  return "uint8_t";
    case DTypeKind::UInt16: return "uint16_t";
    case DTypeKind::UInt32: return "uint32_t";
    case DTypeKind::UInt64: return "uint64_t";
    case DTypeKind::Float:  return "float";
    case DTypeKind::Double: return "double";
    case DTypeKind::SizeT:  return "size_t";
    case DTypeKind::FuncPtr:return "void(*)()";
    case DTypeKind::Pointer: {
        if (!inner) return "void*";
        std::string base = inner->to_string();
        if (is_const) return fmt::format("const {}*", base);
        return fmt::format("{}*", base);
    }
    case DTypeKind::Array: {
        if (!inner) return "void[]";
        return fmt::format("{}[{}]", inner->to_string(), array_count);
    }
    }
    return "int64_t";
}

void TypeInfer::init_known_funcs() {
    auto char_ptr = DecompType::make_ptr(DecompType::make_char());
    auto const_char_ptr = DecompType::make_ptr(DecompType::make_char(), true);
    auto void_ptr = DecompType::make_ptr(DecompType::make_void());
    auto sizet = DecompType::make_sizet();
    auto int32 = DecompType::make_int(32);
    auto int64 = DecompType::make_int(64);
    auto uint32 = DecompType::make_int(32, false);

    known_funcs_ = {
        {"strlen",    sizet,   {const_char_ptr}, {"str"}},
        {"wcslen",    sizet,   {DecompType::make_ptr(DecompType::make_char())}, {"str"}},
        {"strcmp",    int32,   {const_char_ptr, const_char_ptr}, {"s1", "s2"}},
        {"strncmp",   int32,   {const_char_ptr, const_char_ptr, sizet}, {"s1", "s2", "n"}},
        {"strcpy",    char_ptr,{char_ptr, const_char_ptr}, {"dst", "src"}},
        {"strcat",    char_ptr,{char_ptr, const_char_ptr}, {"dst", "src"}},
        {"memcpy",    void_ptr,{void_ptr, void_ptr, sizet}, {"dst", "src", "size"}},
        {"memset",    void_ptr,{void_ptr, int32, sizet}, {"dst", "val", "size"}},
        {"malloc",    void_ptr,{sizet}, {"size"}},
        {"calloc",    void_ptr,{sizet, sizet}, {"count", "size"}},
        {"realloc",   void_ptr,{void_ptr, sizet}, {"ptr", "size"}},
        {"free",      DecompType::make_void(), {void_ptr}, {"ptr"}},
        {"printf",    int32,   {const_char_ptr}, {"fmt"}},
        {"sprintf",   int32,   {char_ptr, const_char_ptr}, {"buf", "fmt"}},
        {"puts",      int32,   {const_char_ptr}, {"str"}},
        {"atoi",      int32,   {const_char_ptr}, {"str"}},
        {"atol",      int64,   {const_char_ptr}, {"str"}},
        {"CreateFileA",     void_ptr, {const_char_ptr, uint32, uint32, void_ptr, uint32, uint32, void_ptr},
                            {"lpFileName", "dwDesiredAccess", "dwShareMode", "lpSecurityAttributes",
                             "dwCreationDisposition", "dwFlagsAndAttributes", "hTemplateFile"}},
        {"CloseHandle",     int32, {void_ptr}, {"hObject"}},
        {"ReadFile",        int32, {void_ptr, void_ptr, uint32, DecompType::make_ptr(uint32), void_ptr},
                            {"hFile", "lpBuffer", "nNumberOfBytesToRead", "lpNumberOfBytesRead", "lpOverlapped"}},
        {"WriteFile",       int32, {void_ptr, void_ptr, uint32, DecompType::make_ptr(uint32), void_ptr},
                            {"hFile", "lpBuffer", "nNumberOfBytesToWrite", "lpNumberOfBytesWritten", "lpOverlapped"}},
        {"VirtualAlloc",    void_ptr, {void_ptr, sizet, uint32, uint32},
                            {"lpAddress", "dwSize", "flAllocationType", "flProtect"}},
        {"VirtualFree",     int32, {void_ptr, sizet, uint32}, {"lpAddress", "dwSize", "dwFreeType"}},
        {"GetLastError",    uint32, {}, {}},
        {"GetProcAddress",  void_ptr, {void_ptr, const_char_ptr}, {"hModule", "lpProcName"}},
        {"LoadLibraryA",    void_ptr, {const_char_ptr}, {"lpLibFileName"}},
    };
}

void TypeInfer::set_type(int var_id, DecompType t) {
    auto it = types_.find(var_id);
    if (it == types_.end()) {
        types_[var_id] = std::move(t);
    } else {
        if (it->second.kind == DTypeKind::Int64 && t.kind != DTypeKind::Int64) {
            it->second = std::move(t);
        } else if (t.is_pointer() && !it->second.is_pointer()) {
            it->second = std::move(t);
        }
    }
}

void TypeInfer::infer_from_sizes(const IRFunc& func) {
    for (auto& blk : func.blocks) {
        for (auto& s : blk.stmts) {
            if (!s.dst.valid()) continue;
            if (s.dst.size == 4) set_type(s.dst.id, DecompType::make_int(32));
            else if (s.dst.size == 2) set_type(s.dst.id, DecompType::make_int(16));
            else if (s.dst.size == 1) set_type(s.dst.id, DecompType::make_int(8));
        }
    }
}

void TypeInfer::infer_from_calls(const IRFunc& func) {
    for (auto& blk : func.blocks) {
        for (auto& s : blk.stmts) {
            if (s.src.op != IROp::Call) continue;
            if (!std::holds_alternative<std::string>(s.src.val)) continue;
            auto& call_name = std::get<std::string>(s.src.val);

            for (auto& kf : known_funcs_) {
                if (call_name != kf.name) continue;

                if (s.dst.valid())
                    set_type(s.dst.id, kf.ret_type);

                for (size_t i = 0; i < kf.param_types.size() && i < s.src.args.size(); ++i) {
                    if (s.src.args[i].is_var()) {
                        int pid = s.src.args[i].get_var().id;
                        set_type(pid, kf.param_types[i]);
                    }
                }
                break;
            }
        }
    }
}

void TypeInfer::infer_from_cmp(const IRFunc& func) {
    for (auto& blk : func.blocks) {
        if (blk.cond.args.size() == 2) {
            bool is_cmp = blk.cond.op == IROp::Eq || blk.cond.op == IROp::Ne;
            if (is_cmp && blk.cond.args[1].is_imm() && blk.cond.args[1].get_imm() == 0) {
                if (blk.cond.args[0].is_var()) {
                    auto v = blk.cond.args[0].get_var();
                    auto it = types_.find(v.id);
                    if (it != types_.end() && it->second.is_pointer()) {
                        // already typed as pointer, keep it
                    }
                }
            }
        }
    }
}

void TypeInfer::infer_from_usage(const IRFunc& func) {
    for (auto& blk : func.blocks) {
        for (auto& s : blk.stmts) {
            if (s.src.op == IROp::Load && !s.src.args.empty()) {
                if (s.src.args[0].is_var()) {
                    int addr_id = s.src.args[0].get_var().id;
                    auto my_type = get_type(s.dst.id);
                    set_type(addr_id, DecompType::make_ptr(my_type));
                }
            }
            if (s.src.op == IROp::Assign && std::holds_alternative<std::string>(s.src.val)) {
                if (s.dst.valid())
                    set_type(s.dst.id, DecompType::make_ptr(DecompType::make_char(), true));
            }
        }
    }
}

void TypeInfer::propagate(const IRFunc& func) {
    for (int pass = 0; pass < 3; ++pass) {
        for (auto& blk : func.blocks) {
            for (auto& s : blk.stmts) {
                if (s.op != IROp::Assign || !s.dst.valid()) continue;
                if (s.src.is_var()) {
                    int src_id = s.src.get_var().id;
                    auto st = get_type(src_id);
                    if (st.kind != DTypeKind::Int64) set_type(s.dst.id, st);
                    auto dt = get_type(s.dst.id);
                    if (dt.kind != DTypeKind::Int64) set_type(src_id, dt);
                }
            }
        }
    }
}

void TypeInfer::name_variables(const IRFunc& func) {
    for (auto& blk : func.blocks) {
        for (auto& s : blk.stmts) {
            if (s.src.op == IROp::Call && s.dst.valid()) {
                if (!std::holds_alternative<std::string>(s.src.val)) continue;
                auto& call_name = std::get<std::string>(s.src.val);
                if (call_name == "strlen" || call_name == "wcslen")
                    names_[s.dst.id] = "len";
                else if (call_name == "malloc" || call_name == "calloc" || call_name == "VirtualAlloc")
                    names_[s.dst.id] = "buf";
                else if (call_name == "GetProcAddress")
                    names_[s.dst.id] = "proc";
                else if (call_name == "LoadLibraryA" || call_name == "LoadLibraryW")
                    names_[s.dst.id] = "hModule";
                else if (call_name == "CreateFileA" || call_name == "CreateFileW")
                    names_[s.dst.id] = "hFile";
                else if (call_name == "GetLastError")
                    names_[s.dst.id] = "err";
            }
        }
    }

    // detect loop counters
    for (auto& blk : func.blocks) {
        for (auto& s : blk.stmts) {
            if (s.op == IROp::Assign && s.dst.valid() && s.src.op == IROp::Add
                && s.src.args.size() == 2 && s.src.args[0].is_var()
                && s.src.args[0].get_var().id == s.dst.id
                && s.src.args[1].is_imm() && s.src.args[1].get_imm() == 1) {
                if (names_.find(s.dst.id) == names_.end())
                    names_[s.dst.id] = "i";
            }
        }
    }

    (void)func;
}

DecompType TypeInfer::get_type(int var_id) const {
    auto it = types_.find(var_id);
    if (it != types_.end()) return it->second;
    return DecompType{DTypeKind::Int64};
}

std::string TypeInfer::get_var_name(int var_id) const {
    auto it = names_.find(var_id);
    if (it != names_.end()) return it->second;
    return {};
}

void TypeInfer::run(IRFunc& func) {
    init_known_funcs();

    for (auto& p : func.params)
        set_type(p.id, DecompType{DTypeKind::Int64});

    infer_from_sizes(func);
    infer_from_calls(func);
    infer_from_cmp(func);
    infer_from_usage(func);
    propagate(func);

    // determine return type
    for (auto& blk : func.blocks) {
        for (auto& s : blk.stmts) {
            if (s.op == IROp::Ret && s.src.is_var()) {
                ret_type_ = get_type(s.src.get_var().id);
            }
        }
    }

    // propagate call return types to ret_type if function contains relevant calls
    for (auto& blk : func.blocks) {
        for (auto& s : blk.stmts) {
            if (s.src.op == IROp::Call && s.dst.valid() && s.dst.id == 0) {
                auto t = get_type(s.dst.id);
                if (t.kind != DTypeKind::Int64 && ret_type_.kind == DTypeKind::Int64)
                    ret_type_ = t;
            }
        }
    }

    name_variables(func);
}

}
