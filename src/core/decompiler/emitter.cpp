#include "emitter.h"
#include <fmt/format.h>
#include <unordered_set>

namespace hype {

std::string Emitter::format_imm(u64 val) {
    if (val == 0) return "0";
    if (val <= 9) return std::to_string(val);
    if (val == (u64)(i64)-1) return "-1";
    i64 sv = (i64)val;
    if (sv < 0 && sv > -256) return fmt::format("-{}", -sv);
    if (val <= 0xFF) return fmt::format("0x{:X}", val);
    return fmt::format("0x{:X}", val);
}

std::string Emitter::type_str(int var_id) {
    if (!ti_) return "int64_t";
    auto t = ti_->get_type(var_id);
    return t.to_string();
}

std::string Emitter::var_str(const IRVar& v) {
    if (!v.valid()) return "?";
    if (ti_) {
        auto n = ti_->get_var_name(v.id);
        if (!n.empty()) return n;
    }
    return v.name;
}

std::string Emitter::format_deref(const IRExpr& e) {
    if (e.args.empty()) return "*(?)";
    auto& addr = e.args[0];
    std::string addr_s = expr_str(addr);
    if (ti_ && addr.is_var()) {
        auto t = ti_->get_type(addr.get_var().id);
        if (t.is_pointer()) {
            return fmt::format("*{}", addr_s);
        }
    }
    return fmt::format("*({})", addr_s);
}

bool Emitter::is_zero_cmp(const IRExpr& e, bool& is_eq) {
    if (e.args.size() != 2) return false;
    if (e.op != IROp::Eq && e.op != IROp::Ne) return false;
    if (e.args[1].is_imm() && e.args[1].get_imm() == 0) {
        is_eq = (e.op == IROp::Eq);
        return true;
    }
    if (e.args[0].is_imm() && e.args[0].get_imm() == 0) {
        is_eq = (e.op == IROp::Eq);
        return true;
    }
    return false;
}

std::string Emitter::expr_str(const IRExpr& e) {
    if (e.is_var()) return var_str(e.get_var());
    if (e.is_imm()) return format_imm(e.get_imm());

    if (e.op == IROp::Assign && std::holds_alternative<std::string>(e.val))
        return fmt::format("\"{}\"", std::get<std::string>(e.val));

    if (e.op == IROp::Call) {
        std::string name = std::holds_alternative<std::string>(e.val)
            ? std::get<std::string>(e.val) : "func";
        std::string args_s;
        for (int i = 0; i < (int)e.args.size(); ++i) {
            if (i) args_s += ", ";
            args_s += expr_str(e.args[i]);
        }
        return fmt::format("{}({})", name, args_s);
    }

    if (e.op == IROp::Load && !e.args.empty()) {
        return format_deref(e);
    }

    if (e.op == IROp::Nop && std::holds_alternative<std::string>(e.val)) {
        return fmt::format("__asm {{ {} }}", std::get<std::string>(e.val));
    }

    if (e.args.size() == 2) {
        std::string l = expr_str(e.args[0]);
        std::string r = expr_str(e.args[1]);
        const char* op = "?";
        switch (e.op) {
        case IROp::Add: op = "+"; break;
        case IROp::Sub: op = "-"; break;
        case IROp::Mul: op = "*"; break;
        case IROp::Div: op = "/"; break;
        case IROp::And: op = "&"; break;
        case IROp::Or:  op = "|"; break;
        case IROp::Xor: op = "^"; break;
        case IROp::Shl: op = "<<"; break;
        case IROp::Shr: op = ">>"; break;
        case IROp::Eq:  op = "=="; break;
        case IROp::Ne:  op = "!="; break;
        case IROp::Lt:  op = "<"; break;
        case IROp::Le:  op = "<="; break;
        case IROp::Gt:  op = ">"; break;
        case IROp::Ge:  op = ">="; break;
        case IROp::SignedLt: op = "<"; break;
        case IROp::SignedLe: op = "<="; break;
        case IROp::SignedGt: op = ">"; break;
        case IROp::SignedGe: op = ">="; break;
        default: op = "?"; break;
        }
        return fmt::format("{} {} {}", l, op, r);
    }

    if (e.args.size() == 1) {
        std::string a = expr_str(e.args[0]);
        switch (e.op) {
        case IROp::Not: return fmt::format("~{}", a);
        case IROp::Neg: return fmt::format("-{}", a);
        default: return a;
        }
    }

    if (std::holds_alternative<IRVar>(e.val))
        return var_str(std::get<IRVar>(e.val));
    if (std::holds_alternative<u64>(e.val))
        return format_imm(std::get<u64>(e.val));
    if (std::holds_alternative<std::string>(e.val))
        return std::get<std::string>(e.val);

    return "?";
}

std::string Emitter::cond_str(const IRExpr& e) {
    if (e.args.size() == 2) {
        bool is_eq = false;
        if (is_zero_cmp(e, is_eq)) {
            std::string operand;
            if (e.args[1].is_imm() && e.args[1].get_imm() == 0)
                operand = expr_str(e.args[0]);
            else
                operand = expr_str(e.args[1]);

            if (is_eq) return fmt::format("!{}", operand);
            return operand;
        }

        std::string l = expr_str(e.args[0]);
        std::string r = expr_str(e.args[1]);
        const char* op = "!=";
        switch (e.op) {
        case IROp::Eq:  op = "=="; break;
        case IROp::Ne:  op = "!="; break;
        case IROp::Lt:  op = "<"; break;
        case IROp::Le:  op = "<="; break;
        case IROp::Gt:  op = ">"; break;
        case IROp::Ge:  op = ">="; break;
        case IROp::SignedLt: op = "<"; break;
        case IROp::SignedLe: op = "<="; break;
        case IROp::SignedGt: op = ">"; break;
        case IROp::SignedGe: op = ">="; break;
        default: return fmt::format("{} != 0", expr_str(e));
        }
        return fmt::format("{} {} {}", l, op, r);
    }
    return fmt::format("{} != 0", expr_str(e));
}

void Emitter::emit_stmt(const CStmt& s, int indent, std::vector<PseudoLine>& out) {
    switch (s.kind) {
    case StmtKind::Expr: {
        auto& e = s.expr;
        if (std::holds_alternative<IRVar>(e.val)) {
            auto& dst = std::get<IRVar>(e.val);
            if (e.args.size() == 1) {
                auto& src = e.args[0];
                if (src.op == IROp::Call) {
                    if (dst.id == 0 && dst.name == "rax") {
                        out.push_back({indent, fmt::format("{};", expr_str(src)), s.addr});
                    } else {
                        out.push_back({indent, fmt::format("{} = {};", var_str(dst), expr_str(src)), s.addr});
                    }
                } else if (e.op == IROp::Store) {
                    if (src.args.size() >= 2)
                        out.push_back({indent, fmt::format("*({}) = {};", expr_str(src.args[0]), expr_str(src.args[1])), s.addr});
                } else {
                    out.push_back({indent, fmt::format("{} = {};", var_str(dst), expr_str(src)), s.addr});
                }
            }
        }
        break;
    }
    case StmtKind::Return: {
        if (s.expr.is_var()) {
            auto rv = s.expr.get_var();
            if (rv.id == 0 && rv.name == "rax") {
                out.push_back({indent, "return rax;", s.addr});
            } else {
                out.push_back({indent, fmt::format("return {};", var_str(rv)), s.addr});
            }
        } else {
            out.push_back({indent, fmt::format("return {};", expr_str(s.expr)), s.addr});
        }
        break;
    }
    case StmtKind::If:
        out.push_back({indent, fmt::format("if ({}) {{", cond_str(s.cond)), s.addr});
        for (auto& bs : s.body)
            emit_stmt(bs, indent + 1, out);
        out.push_back({indent, "}", 0});
        break;
    case StmtKind::IfElse:
        out.push_back({indent, fmt::format("if ({}) {{", cond_str(s.cond)), s.addr});
        for (auto& bs : s.body)
            emit_stmt(bs, indent + 1, out);
        out.push_back({indent, "} else {", 0});
        for (auto& es : s.else_body)
            emit_stmt(es, indent + 1, out);
        out.push_back({indent, "}", 0});
        break;
    case StmtKind::While:
        out.push_back({indent, fmt::format("while ({}) {{", cond_str(s.cond)), s.addr});
        for (auto& bs : s.body)
            emit_stmt(bs, indent + 1, out);
        out.push_back({indent, "}", 0});
        break;
    case StmtKind::DoWhile:
        out.push_back({indent, "do {", s.addr});
        for (auto& bs : s.body)
            emit_stmt(bs, indent + 1, out);
        out.push_back({indent, fmt::format("}} while ({});", cond_str(s.cond)), 0});
        break;
    case StmtKind::Goto:
        out.push_back({indent, fmt::format("goto loc_{:X};", s.goto_target), s.addr});
        break;
    case StmtKind::Block:
        for (auto& bs : s.body)
            emit_stmt(bs, indent, out);
        break;
    }
}

std::vector<PseudoLine> Emitter::emit(const CFunc& func, const TypeInfer* ti) {
    ti_ = ti;
    std::vector<PseudoLine> out;

    std::string ret_type_s = "int64_t";
    if (ti_) ret_type_s = ti_->return_type().to_string();

    std::string params_s;
    int param_count = 0;
    for (auto& p : func.params) {
        if (param_count >= 4) break;
        if (param_count) params_s += ", ";
        std::string pt = type_str(p.id);
        std::string pn = var_str(p);
        params_s += fmt::format("{} {}", pt, pn);
        ++param_count;
    }

    out.push_back({0, fmt::format("{} {}({}) {{", ret_type_s, func.name, params_s), func.entry});

    std::unordered_set<std::string> declared;
    if (!func.locals.empty()) {
        for (auto& l : func.locals) {
            std::string ln = var_str(l);
            if (declared.count(ln)) continue;
            declared.insert(ln);
            std::string lt = type_str(l.id);
            out.push_back({1, fmt::format("{} {};", lt, ln), 0});
        }
        out.push_back({0, "", 0});
    }

    for (auto& s : func.body)
        emit_stmt(s, 1, out);

    out.push_back({0, "}", 0});
    ti_ = nullptr;
    return out;
}

}
