#include "emitter.h"
#include <fmt/format.h>
#include <unordered_set>

namespace hype {

std::string Emitter::format_imm(u64 val, bool addr_ctx) {
    if (addr_ctx && db_ && db_->image_base && val > db_->image_base) {
        auto nit = db_->names.find(val);
        if (nit != db_->names.end()) return nit->second;
        return fmt::format("data_{:X}", val - db_->image_base);
    }
    if (val == 0) return "0";
    if (val <= 9) return std::to_string(val);
    if (val == (u64)(i64)-1) return "-1";
    i64 sv = (i64)val;
    if (sv < 0 && sv > -256) return fmt::format("-{}", -sv);
    return fmt::format("0x{:X}", val);
}

std::string Emitter::type_str(int var_id) {
    if (!ti_) return "int64_t";
    return ti_->get_type(var_id).to_string();
}

std::string Emitter::varnode_str(const Varnode& vn) {
    if (vn.is_const()) return format_imm(vn.offset);
    if (ti_) {
        auto n = ti_->get_var_name(vn.id);
        if (!n.empty()) return n;
    }
    if (!vn.name.empty()) return vn.name;
    if (vn.is_reg()) {
        static const char* rnames[] = {
            "rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi",
            "r8","r9","r10","r11","r12","r13","r14","r15"
        };
        if (vn.id >= 0 && vn.id < 16) {
            if (vn.size == 4) {
                static const char* r32[] = {
                    "eax","ecx","edx","ebx","esp","ebp","esi","edi",
                    "r8d","r9d","r10d","r11d","r12d","r13d","r14d","r15d"
                };
                return r32[vn.id];
            }
            return rnames[vn.id];
        }
    }
    if (vn.is_temp()) return fmt::format("t{}", vn.id);
    if (vn.is_stack()) return fmt::format("var_{:X}", vn.offset & 0xFFFF);
    return "?";
}

std::string Emitter::expr_str(const CExpr& e) {
    if (e.is_var()) {
        if (e.vn.is_temp() && inline_temps_.count(e.vn.id)) {
            auto dit = temp_defs_.find(e.vn.id);
            if (dit != temp_defs_.end())
                return fmt::format("({})", expr_str(dit->second->expr));
        }
        return varnode_str(e.vn);
    }
    if (e.is_const()) return format_imm(e.vn.offset);

    if (e.op == PcodeOp::CALL) {
        std::string name = e.call_name;
        std::vector<std::string> arg_strs;
        size_t start_idx = 0;

        // Operator overload: emit infix notation
        if (name.find("operator") != std::string::npos) {
            const char* infix = nullptr;
            if (name.find("operator+") != std::string::npos && name.find("operator++") == std::string::npos) infix = "+";
            else if (name.find("operator-") != std::string::npos && name.find("operator--") == std::string::npos) infix = "-";
            else if (name.find("operator*") != std::string::npos) infix = "*";
            else if (name.find("operator/") != std::string::npos) infix = "/";
            else if (name.find("operator%") != std::string::npos) infix = "%";
            else if (name.find("operator==") != std::string::npos) infix = "==";
            else if (name.find("operator!=") != std::string::npos) infix = "!=";
            else if (name.find("operator<") != std::string::npos && name.find("operator<<") == std::string::npos) infix = "<";
            else if (name.find("operator>") != std::string::npos && name.find("operator>>") == std::string::npos) infix = ">";
            else if (name.find("operator<=") != std::string::npos) infix = "<=";
            else if (name.find("operator>=") != std::string::npos) infix = ">=";
            else if (name.find("operator<<") != std::string::npos) infix = "<<";
            else if (name.find("operator>>") != std::string::npos) infix = ">>";
            else if (name.find("operator&") != std::string::npos && name.find("operator&&") == std::string::npos) infix = "&";
            else if (name.find("operator|") != std::string::npos && name.find("operator||") == std::string::npos) infix = "|";
            else if (name.find("operator^") != std::string::npos) infix = "^";

            if (infix && e.args.size() >= 2)
                return fmt::format("{} {} {}", expr_str(e.args[0]), infix, expr_str(e.args[1]));
            if (infix && e.args.size() == 1)
                return fmt::format("{}({})", infix, expr_str(e.args[0]));
        }
        // MSVC mangled operators: ??H = operator+, ??G = operator-, ??D = operator*
        if (name.size() >= 3 && name[0] == '?' && name[1] == '?') {
            const char* infix = nullptr;
            char code = name[2];
            if (code == 'H') infix = "+";
            else if (code == 'G') infix = "-";
            else if (code == 'D') infix = "*";
            else if (code == 'K') infix = "/";
            else if (code == 'L') infix = "%";
            else if (code == '8') infix = "==";
            else if (code == '9') infix = "!=";

            if (infix && e.args.size() >= 2)
                return fmt::format("{} {} {}", expr_str(e.args[0]), infix, expr_str(e.args[1]));
        }

        // RTTI: obj->Class::method() style
        const RTTIClass* cls = find_rtti_for_call(name);
        std::string prefix;
        if (cls && !e.args.empty()) {
            start_idx = 1; // skip `this`
            prefix = fmt::format("{}->", expr_str(e.args[0]));
        }

        for (size_t i = start_idx; i < e.args.size(); ++i)
            arg_strs.push_back(expr_str(e.args[i]));

        std::string args_s;
        for (size_t i = 0; i < arg_strs.size(); ++i) {
            if (i) args_s += ", ";
            args_s += arg_strs[i];
        }
        return fmt::format("{}{}({})", prefix, name, args_s);
    }

    if (e.op == PcodeOp::LOAD && !e.args.empty()) {
        auto& addr_expr = e.args[0];
        if (addr_expr.is_const() && db_ && db_->image_base &&
            addr_expr.vn.offset > db_->image_base) {
            return fmt::format("*({})", format_imm(addr_expr.vn.offset, true));
        }
        return fmt::format("*({})", expr_str(e.args[0]));
    }

    if (e.op == PcodeOp::STORE && e.args.size() >= 2) {
        return fmt::format("*({})", expr_str(e.args[0]));
    }

    if (e.op == PcodeOp::BOOL_NOT && !e.args.empty()) {
        return fmt::format("!{}", expr_str(e.args[0]));
    }
    if (e.op == PcodeOp::INT_NEGATE && !e.args.empty()) {
        return fmt::format("~{}", expr_str(e.args[0]));
    }
    if (e.op == PcodeOp::INT_NOT && !e.args.empty()) {
        return fmt::format("~{}", expr_str(e.args[0]));
    }

    if (e.op == PcodeOp::INT_ZEXT && !e.args.empty()) {
        return fmt::format("(uint{}_t){}", e.vn.size * 8, expr_str(e.args[0]));
    }
    if (e.op == PcodeOp::INT_SEXT && !e.args.empty()) {
        return fmt::format("(int{}_t){}", e.vn.size * 8, expr_str(e.args[0]));
    }

    if (e.args.size() == 2) {
        std::string l = expr_str(e.args[0]);
        std::string r = expr_str(e.args[1]);
        const char* op = "?";
        switch (e.op) {
        case PcodeOp::ADD: op = "+"; break;
        case PcodeOp::SUB: op = "-"; break;
        case PcodeOp::AND: op = "&"; break;
        case PcodeOp::OR:  op = "|"; break;
        case PcodeOp::XOR: op = "^"; break;
        case PcodeOp::SHIFT_LEFT: op = "<<"; break;
        case PcodeOp::SHIFT_RIGHT: op = ">>"; break;
        case PcodeOp::INT_MULT: op = "*"; break;
        case PcodeOp::INT_DIV: op = "/"; break;
        case PcodeOp::INT_EQUAL: op = "=="; break;
        case PcodeOp::INT_NEQUAL: op = "!="; break;
        case PcodeOp::INT_LESS: op = "<"; break;
        case PcodeOp::INT_SLESS: op = "<"; break;
        case PcodeOp::BOOL_AND: op = "&&"; break;
        case PcodeOp::BOOL_OR: op = "||"; break;
        default: op = "?"; break;
        }
        return fmt::format("{} {} {}", l, op, r);
    }

    if (e.args.size() == 1)
        return expr_str(e.args[0]);

    return varnode_str(e.vn);
}

std::string Emitter::cond_str(const CExpr& e) {
    if (e.is_var()) {
        // If the variable came from a LOAD (tracked via inline), show dereference
        if (e.vn.is_temp() && inline_temps_.count(e.vn.id)) {
            auto dit = temp_defs_.find(e.vn.id);
            if (dit != temp_defs_.end() && dit->second->expr.op == PcodeOp::LOAD)
                return fmt::format("*({})", expr_str(dit->second->expr.args[0]));
        }
        return varnode_str(e.vn);
    }
    // LOAD in condition: show dereference explicitly
    if (e.op == PcodeOp::LOAD && !e.args.empty()) {
        return fmt::format("*({})", expr_str(e.args[0]));
    }
    if (e.op == PcodeOp::BOOL_NOT && !e.args.empty()) {
        auto& inner = e.args[0];
        if (inner.op == PcodeOp::LOAD && !inner.args.empty())
            return fmt::format("!*({})", expr_str(inner.args[0]));
        if (inner.is_var() && inner.vn.is_reg() && inner.vn.id >= 100)
            return fmt::format("!{}", varnode_str(inner.vn));
        return fmt::format("!({})", expr_str(inner));
    }
    if (e.op == PcodeOp::INT_EQUAL && e.args.size() == 2) {
        if (e.args[1].is_const() && e.args[1].vn.offset == 0)
            return fmt::format("!{}", expr_str(e.args[0]));
        return fmt::format("{} == {}", expr_str(e.args[0]), expr_str(e.args[1]));
    }
    if (e.op == PcodeOp::INT_NEQUAL && e.args.size() == 2) {
        if (e.args[1].is_const() && e.args[1].vn.offset == 0)
            return expr_str(e.args[0]);
        return fmt::format("{} != {}", expr_str(e.args[0]), expr_str(e.args[1]));
    }
    if (e.op == PcodeOp::INT_LESS && e.args.size() == 2)
        return fmt::format("{} < {}", expr_str(e.args[0]), expr_str(e.args[1]));
    if (e.op == PcodeOp::INT_SLESS && e.args.size() == 2)
        return fmt::format("{} < {}", expr_str(e.args[0]), expr_str(e.args[1]));
    return expr_str(e);
}

bool Emitter::is_dead_stmt(const CStmt& s) const {
    if (s.kind == StmtKind::Return) return false;
    if (s.kind == StmtKind::Goto) return false;
    if (s.kind == StmtKind::Expr) {
        if (s.expr.op == PcodeOp::CALL || s.expr.op == PcodeOp::STORE) return false;
        return true;
    }
    if (s.kind == StmtKind::Assign) {
        if (s.expr.op == PcodeOp::CALL) return false;
        if (s.dst.is_temp() && inline_temps_.count(s.dst.id)) return true;
        // Arg-setup registers before a call are dead as standalone statements
        if (s.dst.is_reg() && (s.dst.id == 1 || s.dst.id == 2 || s.dst.id == 8 || s.dst.id == 9))
            if (s.expr.op != PcodeOp::CALL) return true;
        return false;
    }
    if (s.kind == StmtKind::If || s.kind == StmtKind::IfElse) {
        return is_dead_body(s.body) && is_dead_body(s.else_body);
    }
    if (s.kind == StmtKind::While || s.kind == StmtKind::DoWhile || s.kind == StmtKind::For) {
        return is_dead_body(s.body);
    }
    if (s.kind == StmtKind::Block)
        return is_dead_body(s.body);
    return false;
}

bool Emitter::is_dead_body(const std::vector<CStmt>& body) const {
    for (auto& s : body)
        if (!is_dead_stmt(s)) return false;
    return true;
}

void Emitter::collect_uses(const CExpr& e) {
    if (e.is_var() && e.vn.is_temp())
        temp_use_count_[e.vn.id]++;
    for (auto& a : e.args) collect_uses(a);
}

void Emitter::collect_stmt_uses(const CStmt& s) {
    collect_uses(s.expr);
    collect_uses(s.cond);
    for (auto& bs : s.body) collect_stmt_uses(bs);
    for (auto& es : s.else_body) collect_stmt_uses(es);
}

void Emitter::build_inline_map(const CFunc& func) {
    temp_defs_.clear();
    temp_use_count_.clear();
    inline_temps_.clear();

    std::function<void(const std::vector<CStmt>&)> scan_defs = [&](const std::vector<CStmt>& stmts) {
        for (auto& s : stmts) {
            if (s.kind == StmtKind::Assign && s.dst.is_temp() && s.expr.op != PcodeOp::CALL)
                temp_defs_[s.dst.id] = &s;
            scan_defs(s.body);
            scan_defs(s.else_body);
        }
    };
    scan_defs(func.body);

    for (auto& s : func.body) collect_stmt_uses(s);

    for (auto& [id, stmt] : temp_defs_) {
        if (temp_use_count_[id] == 1)
            inline_temps_.insert(id);
    }
}

void Emitter::build_call_result_map(const CFunc& func) {
    call_result_vars_.clear();
    std::function<void(const std::vector<CStmt>&)> scan = [&](const std::vector<CStmt>& stmts) {
        for (auto& s : stmts) {
            if (s.kind == StmtKind::Assign && s.expr.op == PcodeOp::CALL &&
                s.dst.is_reg() && s.dst.id == 0)
                call_result_vars_.insert(s.dst.id);
            scan(s.body);
            scan(s.else_body);
        }
    };
    scan(func.body);
}

bool Emitter::is_rax_used_after(size_t idx, const std::vector<CStmt>& stmts) const {
    for (size_t i = idx + 1; i < stmts.size(); ++i) {
        auto& s = stmts[i];
        if (s.kind == StmtKind::Assign && s.expr.op == PcodeOp::CALL) break;
        // Check if rax appears in expression
        std::function<bool(const CExpr&)> uses_rax = [&](const CExpr& e) -> bool {
            if (e.is_var() && e.vn.is_reg() && e.vn.id == 0) return true;
            for (auto& a : e.args) if (uses_rax(a)) return true;
            return false;
        };
        if (uses_rax(s.expr) || uses_rax(s.cond)) return true;
        if (s.kind == StmtKind::Return && uses_rax(s.expr)) return true;
    }
    return false;
}

bool Emitter::func_has_meaningful_return(const CFunc& func) const {
    std::function<bool(const std::vector<CStmt>&)> check = [&](const std::vector<CStmt>& stmts) -> bool {
        for (auto& s : stmts) {
            if (s.kind == StmtKind::Return) {
                if (s.expr.is_var() && s.expr.vn.is_reg() && s.expr.vn.id == 0)
                    return false; // bare `return rax` without meaningful assignment
                if (!s.expr.is_const() || s.expr.vn.offset != 0)
                    return true;
            }
            if (check(s.body)) return true;
            if (check(s.else_body)) return true;
        }
        return false;
    };
    return check(func.body);
}

bool Emitter::is_trailing_return(const CStmt& s) const {
    if (s.kind != StmtKind::Return) return false;
    if (is_void_func_) return true;
    if (s.expr.is_var() && s.expr.vn.is_reg() && s.expr.vn.id == 0) return true;
    if (s.expr.is_const() && s.expr.vn.offset == 0) return true;
    return false;
}

const RTTIClass* Emitter::find_rtti_for_call(const std::string& name) const {
    if (!rtti_) return nullptr;
    for (auto& cls : rtti_->classes()) {
        if (name.find(cls.demangled_name + "::") != std::string::npos)
            return &cls;
    }
    return nullptr;
}

void Emitter::emit_stmt(const CStmt& s, int indent, std::vector<PseudoLine>& out, bool& returned) {
    if (returned) return;

    if (is_dead_stmt(s)) return;

    switch (s.kind) {
    case StmtKind::Assign: {
        if (s.dst.is_temp() && inline_temps_.count(s.dst.id)) return;
        std::string rhs = expr_str(s.expr);
        if (s.expr.op == PcodeOp::CALL) {
            // Check: is the result (rax) used? If not, emit as bare call
            if (s.dst.is_reg() && s.dst.id == 0) {
                out.push_back({indent, fmt::format("{};", rhs), s.addr});
            } else if (s.dst.valid()) {
                std::string lhs = "result";
                if (ti_) {
                    auto vn = ti_->get_var_name(s.dst.id);
                    if (!vn.empty()) lhs = vn;
                }
                out.push_back({indent, fmt::format("{} = {};", lhs, rhs), s.addr});
            } else {
                out.push_back({indent, fmt::format("{};", rhs), s.addr});
            }
        } else if (s.dst.valid()) {
            out.push_back({indent, fmt::format("{} = {};", varnode_str(s.dst), rhs), s.addr});
        } else {
            out.push_back({indent, fmt::format("{};", rhs), s.addr});
        }
        break;
    }
    case StmtKind::Expr: {
        if (s.expr.op == PcodeOp::STORE && s.expr.args.size() >= 2) {
            auto& addr_expr = s.expr.args[0];
            std::string addr_s;
            if (addr_expr.is_const() && db_ && db_->image_base &&
                addr_expr.vn.offset > db_->image_base)
                addr_s = format_imm(addr_expr.vn.offset, true);
            else
                addr_s = expr_str(addr_expr);
            out.push_back({indent, fmt::format("*({}) = {};", addr_s, expr_str(s.expr.args[1])), s.addr});
        } else {
            out.push_back({indent, fmt::format("{};", expr_str(s.expr)), s.addr});
        }
        break;
    }
    case StmtKind::Return: {
        if (is_trailing_return(s)) {
            // Don't emit trailing return for void or bare `return rax`/`return 0`
            returned = true;
            break;
        }
        std::string rv = expr_str(s.expr);
        if (rv == "rax" || rv.empty()) {
            if (!is_void_func_)
                out.push_back({indent, "return;", s.addr});
        } else {
            out.push_back({indent, fmt::format("return {};", rv), s.addr});
        }
        returned = true;
        break;
    }
    case StmtKind::If:
        out.push_back({indent, fmt::format("if ({}) {{", cond_str(s.cond)), s.addr});
        for (auto& bs : s.body) emit_stmt(bs, indent + 1, out, returned);
        out.push_back({indent, "}", 0});
        returned = false;
        break;
    case StmtKind::IfElse: {
        bool then_empty = is_dead_body(s.body);
        bool else_empty = is_dead_body(s.else_body);
        if (then_empty && else_empty) return;
        if (then_empty && !else_empty) {
            CExpr neg;
            neg.op = PcodeOp::BOOL_NOT;
            neg.args = {s.cond};
            out.push_back({indent, fmt::format("if ({}) {{", cond_str(neg)), s.addr});
            bool sub_ret = false;
            for (auto& es : s.else_body) emit_stmt(es, indent + 1, out, sub_ret);
            out.push_back({indent, "}", 0});
        } else if (else_empty) {
            out.push_back({indent, fmt::format("if ({}) {{", cond_str(s.cond)), s.addr});
            bool sub_ret = false;
            for (auto& bs : s.body) emit_stmt(bs, indent + 1, out, sub_ret);
            out.push_back({indent, "}", 0});
        } else {
            out.push_back({indent, fmt::format("if ({}) {{", cond_str(s.cond)), s.addr});
            bool sub_ret = false;
            for (auto& bs : s.body) emit_stmt(bs, indent + 1, out, sub_ret);
            out.push_back({indent, "} else {", 0});
            sub_ret = false;
            for (auto& es : s.else_body) emit_stmt(es, indent + 1, out, sub_ret);
            out.push_back({indent, "}", 0});
        }
        break;
    }
    case StmtKind::While:
        out.push_back({indent, fmt::format("while ({}) {{", cond_str(s.cond)), s.addr});
        { bool sub_ret = false;
        for (auto& bs : s.body) emit_stmt(bs, indent + 1, out, sub_ret); }
        out.push_back({indent, "}", 0});
        break;
    case StmtKind::DoWhile:
        out.push_back({indent, "do {", s.addr});
        { bool sub_ret = false;
        for (auto& bs : s.body) emit_stmt(bs, indent + 1, out, sub_ret); }
        out.push_back({indent, fmt::format("}} while ({});", cond_str(s.cond)), 0});
        break;
    case StmtKind::For: {
        std::string init_s, cond_s, incr_s;
        if (s.for_var.valid()) {
            init_s = fmt::format("{} = {}", varnode_str(s.for_var), expr_str(s.for_init));
            cond_s = cond_str(s.cond);
            incr_s = expr_str(s.for_incr);
        }
        out.push_back({indent, fmt::format("for ({}; {}; {}) {{", init_s, cond_s, incr_s), s.addr});
        { bool sub_ret = false;
        for (auto& bs : s.body) emit_stmt(bs, indent + 1, out, sub_ret); }
        out.push_back({indent, "}", 0});
        break;
    }
    case StmtKind::Goto:
        out.push_back({indent, fmt::format("goto loc_{:X};", s.goto_target), s.addr});
        break;
    case StmtKind::Block:
        for (auto& bs : s.body) emit_stmt(bs, indent, out, returned);
        break;
    case StmtKind::Switch:
        out.push_back({indent, "switch (...) { ... }", s.addr});
        break;
    }
}

static bool expr_uses_reg(const CExpr& e, int reg_id) {
    if (e.is_var() && e.vn.is_reg() && e.vn.id == reg_id) return true;
    for (auto& a : e.args)
        if (expr_uses_reg(a, reg_id)) return true;
    return false;
}

static bool stmt_uses_reg(const CStmt& s, int reg_id) {
    if (expr_uses_reg(s.expr, reg_id)) return true;
    if (expr_uses_reg(s.cond, reg_id)) return true;
    for (auto& bs : s.body) if (stmt_uses_reg(bs, reg_id)) return true;
    for (auto& es : s.else_body) if (stmt_uses_reg(es, reg_id)) return true;
    return false;
}

static bool func_uses_param(const CFunc& func, int reg_id) {
    for (auto& s : func.body)
        if (stmt_uses_reg(s, reg_id)) return true;
    return false;
}

std::vector<PseudoLine> Emitter::emit(const CFunc& func, const TypeInfer* ti,
                                       const AnalysisDB* db, const RTTIParser* rtti) {
    ti_ = ti;
    db_ = db;
    rtti_ = rtti;
    func_name_ = func.name;
    std::vector<PseudoLine> out;

    build_inline_map(func);
    build_call_result_map(func);

    is_void_func_ = !func_has_meaningful_return(func);

    std::string ret_type_s;
    if (is_void_func_)
        ret_type_s = "void";
    else if (ti_)
        ret_type_s = ti_->return_type().to_string();
    else
        ret_type_s = "int64_t";

    // Build parameter list — special-case main/WinMain
    std::string params_s;
    if (func.name == "main") {
        params_s = "int argc, char** argv, char** envp";
    } else if (func.name == "WinMain") {
        params_s = "HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd";
    } else {
        int pc = 0;
        for (auto& p : func.params) {
            if (pc >= 4) break;
            if (!func_uses_param(func, p.id)) { ++pc; continue; }
            if (!params_s.empty()) params_s += ", ";
            std::string pt = type_str(p.id);
            std::string pn = varnode_str(p);
            params_s += fmt::format("{} {}", pt, pn);
            ++pc;
        }
    }

    out.push_back({0, fmt::format("{} {}({}) {{", ret_type_s, func.name, params_s), func.entry});

    std::unordered_set<std::string> declared;
    for (auto& l : func.locals) {
        if (l.is_temp() && inline_temps_.count(l.id)) continue;
        // Skip caller-save regs that are just arg-setup
        if (l.is_reg() && (l.id == 1 || l.id == 2 || l.id == 8 || l.id == 9)) continue;
        std::string ln = varnode_str(l);
        if (declared.count(ln)) continue;
        declared.insert(ln);
        out.push_back({1, fmt::format("{} {};", type_str(l.id), ln), 0});
    }
    if (!func.locals.empty()) out.push_back({0, "", 0});

    bool returned = false;
    for (auto& s : func.body)
        emit_stmt(s, 1, out, returned);

    out.push_back({0, "}", 0});
    ti_ = nullptr;
    db_ = nullptr;
    rtti_ = nullptr;
    return out;
}

}
