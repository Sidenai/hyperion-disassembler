#include "cf_struct.h"
#include <unordered_set>
#include <algorithm>

namespace hype {

CExpr CFStructure::varnode_expr(const Varnode& vn) {
    return CExpr::var(vn);
}

CExpr CFStructure::build_expr(const PcodeInsn& op) {
    switch (op.op) {
    case PcodeOp::CALL: {
        std::string name;
        std::vector<CExpr> args;
        if (!op.inputs.empty()) {
            name = op.inputs[0].name.empty() ? "func" : op.inputs[0].name;
            for (size_t i = 1; i < op.inputs.size(); ++i)
                args.push_back(varnode_expr(op.inputs[i]));
        }
        return CExpr::call(std::move(name), std::move(args));
    }
    case PcodeOp::LOAD:
        if (!op.inputs.empty())
            return CExpr::deref(varnode_expr(op.inputs[0]));
        return CExpr::imm(0);
    case PcodeOp::ADD: case PcodeOp::SUB: case PcodeOp::AND: case PcodeOp::OR:
    case PcodeOp::XOR: case PcodeOp::SHIFT_LEFT: case PcodeOp::SHIFT_RIGHT:
    case PcodeOp::INT_MULT: case PcodeOp::INT_DIV:
    case PcodeOp::INT_EQUAL: case PcodeOp::INT_NEQUAL:
    case PcodeOp::INT_LESS: case PcodeOp::INT_SLESS:
        if (op.inputs.size() >= 2)
            return CExpr::binop(op.op, varnode_expr(op.inputs[0]), varnode_expr(op.inputs[1]));
        return CExpr::imm(0);
    case PcodeOp::COPY:
        if (!op.inputs.empty()) return varnode_expr(op.inputs[0]);
        return CExpr::imm(0);
    case PcodeOp::INT_NEGATE: case PcodeOp::INT_NOT: case PcodeOp::BOOL_NOT:
        if (!op.inputs.empty()) {
            CExpr e; e.op = op.op; e.args = {varnode_expr(op.inputs[0])}; return e;
        }
        return CExpr::imm(0);
    default:
        if (!op.inputs.empty()) return varnode_expr(op.inputs[0]);
        return CExpr::imm(0);
    }
}

int CFStructure::find_branch_target(const PcodeBlock& blk) {
    for (auto it = blk.ops.rbegin(); it != blk.ops.rend(); ++it) {
        if (it->op == PcodeOp::CBRANCH && !it->inputs.empty())
            return static_cast<int>(it->inputs[0].offset);
        if (it->op == PcodeOp::BRANCH && !it->inputs.empty())
            return static_cast<int>(it->inputs[0].offset);
    }
    return -1;
}

bool CFStructure::is_back_edge(int from, int to) {
    return to <= from;
}

void CFStructure::emit_block_stmts(const PcodeBlock& blk, std::vector<CStmt>& out) {
    for (auto& op : blk.ops) {
        if (op.op == PcodeOp::NOP) continue;
        if (op.op == PcodeOp::CBRANCH || op.op == PcodeOp::BRANCH) continue;
        if (op.op == PcodeOp::STORE) {
            CStmt s;
            s.kind = StmtKind::Assign;
            s.addr = op.addr;
            if (op.inputs.size() >= 2) {
                s.expr = CExpr::deref(varnode_expr(op.inputs[0]));
                s.cond = varnode_expr(op.inputs[1]);
                s.kind = StmtKind::Expr;
                CExpr store_expr;
                store_expr.op = PcodeOp::STORE;
                store_expr.args = {varnode_expr(op.inputs[0]), varnode_expr(op.inputs[1])};
                s.expr = std::move(store_expr);
            }
            out.push_back(std::move(s));
            continue;
        }
        if (op.op == PcodeOp::RETURN) {
            CStmt s;
            s.kind = StmtKind::Return;
            s.addr = op.addr;
            if (!op.inputs.empty()) s.expr = varnode_expr(op.inputs[0]);
            out.push_back(std::move(s));
            return;
        }
        if (op.op == PcodeOp::CALL) {
            CStmt s;
            s.kind = StmtKind::Assign;
            s.addr = op.addr;
            s.dst = op.output;
            s.expr = build_expr(op);
            out.push_back(std::move(s));
            continue;
        }
        if (op.output.valid()) {
            CStmt s;
            s.kind = StmtKind::Assign;
            s.addr = op.addr;
            s.dst = op.output;
            s.expr = build_expr(op);
            out.push_back(std::move(s));
        }
    }
}

void CFStructure::structure_region(const PcodeFunc& func, int start, int end, std::vector<CStmt>& out) {
    std::unordered_set<int> visited;

    for (int i = start; i < end; ++i) {
        if (visited.count(i)) continue;
        visited.insert(i);
        auto& blk = func.blocks[i];

        // find CBRANCH at end
        PcodeInsn const* cbranch = nullptr;
        for (auto it = blk.ops.rbegin(); it != blk.ops.rend(); ++it) {
            if (it->op == PcodeOp::CBRANCH) { cbranch = &*it; break; }
        }

        if (cbranch && blk.succs.size() >= 2) {
            emit_block_stmts(blk, out);

            int true_idx = -1, false_idx = -1;
            va_t target = cbranch->inputs[0].offset;

            for (int s : blk.succs) {
                if (s < (int)func.blocks.size() && func.blocks[s].addr == target)
                    true_idx = s;
                else
                    false_idx = s;
            }
            if (true_idx < 0 && blk.succs.size() >= 1) true_idx = blk.succs[0];
            if (false_idx < 0 && blk.succs.size() >= 2) false_idx = blk.succs[1];

            // build condition
            CExpr cond_expr;
            if (cbranch->inputs.size() >= 2)
                cond_expr = varnode_expr(cbranch->inputs[1]);
            else
                cond_expr = CExpr::imm(1);

            // back-edge: while loop
            if (true_idx >= 0 && is_back_edge(i, true_idx)) {
                // attempt for-loop reconstruction
                if (!out.empty() && try_for_loop(func, true_idx, i, out, out)) {
                    auto& fs = out.back();
                    for (int li = true_idx; li < i; ++li) {
                        if (visited.count(li)) continue;
                        visited.insert(li);
                        emit_block_stmts(func.blocks[li], fs.body);
                    }
                    continue;
                }

                CStmt ws;
                ws.kind = StmtKind::While;
                ws.cond = cond_expr;
                ws.addr = blk.addr;
                for (int li = true_idx; li < i; ++li) {
                    if (visited.count(li)) continue;
                    visited.insert(li);
                    emit_block_stmts(func.blocks[li], ws.body);
                }
                out.push_back(std::move(ws));
                continue;
            }

            if (false_idx >= 0 && is_back_edge(i, false_idx)) {
                CStmt ws;
                ws.kind = StmtKind::DoWhile;
                ws.cond = cond_expr;
                ws.addr = blk.addr;
                out.push_back(std::move(ws));
                continue;
            }

            // if/else
            CStmt ifs;
            ifs.addr = blk.addr;
            ifs.cond = cond_expr;

            if (true_idx >= 0 && true_idx < end && false_idx >= 0 && false_idx < end) {
                // check for merge point
                bool has_merge = false;
                if (true_idx < end && false_idx < end) {
                    auto& tblk = func.blocks[true_idx];
                    for (int ts : tblk.succs) {
                        if (ts == false_idx || ts > std::max(true_idx, false_idx)) {
                            has_merge = true; break;
                        }
                    }
                }

                if (has_merge) {
                    ifs.kind = StmtKind::IfElse;
                    visited.insert(true_idx);
                    emit_block_stmts(func.blocks[true_idx], ifs.body);
                    visited.insert(false_idx);
                    emit_block_stmts(func.blocks[false_idx], ifs.else_body);
                } else {
                    ifs.kind = StmtKind::If;
                    visited.insert(true_idx);
                    emit_block_stmts(func.blocks[true_idx], ifs.body);
                }
            } else if (true_idx >= 0 && true_idx < end) {
                ifs.kind = StmtKind::If;
                visited.insert(true_idx);
                emit_block_stmts(func.blocks[true_idx], ifs.body);
            } else {
                ifs.kind = StmtKind::Goto;
                ifs.goto_target = target;
            }
            out.push_back(std::move(ifs));
        } else {
            emit_block_stmts(blk, out);

            if (!blk.has_return && blk.succs.size() == 1) {
                int s = blk.succs[0];
                if (is_back_edge(i, s)) {
                    CStmt gt;
                    gt.kind = StmtKind::Goto;
                    gt.goto_target = (s < (int)func.blocks.size()) ? func.blocks[s].addr : 0;
                    gt.addr = blk.addr;
                    out.push_back(std::move(gt));
                }
            }
        }
    }
}

CFunc CFStructure::structure(const PcodeFunc& func) {
    CFunc cf;
    cf.name = func.name;
    cf.entry = func.entry;
    cf.params = func.params;
    cf.locals = func.locals;

    int n = (int)func.blocks.size();
    structure_region(func, 0, n, cf.body);
    return cf;
}

bool CFStructure::try_for_loop(const PcodeFunc& func, int header_idx, int back_edge_src,
                               const std::vector<CStmt>& pre_stmts, std::vector<CStmt>& out) {
    if (pre_stmts.empty()) return false;
    if (header_idx < 0 || header_idx >= (int)func.blocks.size()) return false;
    if (back_edge_src < 0 || back_edge_src >= (int)func.blocks.size()) return false;

    auto& last_pre = pre_stmts.back();
    if (last_pre.kind != StmtKind::Assign) return false;
    if (!last_pre.dst.valid()) return false;
    if (!last_pre.expr.is_const()) return false;

    auto& back_blk = func.blocks[back_edge_src];
    bool has_increment = false;
    CExpr incr_expr;
    Varnode ind_var = last_pre.dst;

    for (auto& op : back_blk.ops) {
        if (op.op == PcodeOp::ADD && op.output.valid() && op.output == ind_var &&
            op.inputs.size() >= 2 && op.inputs[0] == ind_var &&
            op.inputs[1].is_const() && op.inputs[1].offset == 1) {
            has_increment = true;
            incr_expr = CExpr::binop(PcodeOp::ADD, CExpr::var(ind_var), CExpr::imm(1));
            break;
        }
        if (op.op == PcodeOp::ADD && op.output.valid() && op.output == ind_var &&
            op.inputs.size() >= 2) {
            has_increment = true;
            incr_expr = build_expr(op);
            break;
        }
        if (op.op == PcodeOp::SUB && op.output.valid() && op.output == ind_var &&
            op.inputs.size() >= 2) {
            has_increment = true;
            incr_expr = build_expr(op);
            break;
        }
    }

    if (!has_increment) return false;

    auto& header = func.blocks[header_idx];
    CExpr cond_expr = CExpr::imm(1);
    for (auto it = header.ops.rbegin(); it != header.ops.rend(); ++it) {
        if (it->op == PcodeOp::CBRANCH && it->inputs.size() >= 2) {
            cond_expr = varnode_expr(it->inputs[1]);
            break;
        }
    }

    CStmt fs;
    fs.kind = StmtKind::For;
    fs.addr = header.addr;
    fs.cond = cond_expr;
    fs.for_init = last_pre.expr;
    fs.for_incr = incr_expr;
    fs.for_var = ind_var;
    out.push_back(std::move(fs));
    return true;
}

}
