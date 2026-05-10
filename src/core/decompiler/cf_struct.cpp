#include "cf_struct.h"
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

namespace hype {

int CFStructure::block_index(const IRFunc& func, va_t addr) {
    for (int i = 0; i < (int)func.blocks.size(); ++i)
        if (func.blocks[i].addr == addr) return i;
    return -1;
}

std::vector<int> CFStructure::compute_order(const IRFunc& func) {
    std::vector<int> order;
    order.reserve(func.blocks.size());
    for (int i = 0; i < (int)func.blocks.size(); ++i)
        order.push_back(i);
    return order;
}

static void emit_stmts_from_block(const IRBlock& blk, std::vector<CStmt>& out) {
    for (auto& s : blk.stmts) {
        if (s.op == IROp::Nop) continue;
        CStmt cs;
        cs.addr = s.addr;
        if (s.op == IROp::Ret) {
            cs.kind = StmtKind::Return;
            cs.expr = s.src;
        } else {
            cs.kind = StmtKind::Expr;
            IRExpr assign;
            assign.op = s.op;
            assign.val = s.dst;
            assign.args = {s.src};
            cs.expr = std::move(assign);
        }
        out.push_back(std::move(cs));
    }
}

void CFStructure::structure_region(const IRFunc& func, const std::vector<int>& block_order,
                                   int start, int end, std::vector<CStmt>& out) {
    std::unordered_set<int> visited;

    for (int i = start; i < end; ++i) {
        if (visited.count(i)) continue;
        visited.insert(i);
        int bi = block_order[i];
        auto& blk = func.blocks[bi];

        if (blk.cond_true && blk.cond_false) {
            int true_idx = block_index(func, blk.cond_true);
            int false_idx = block_index(func, blk.cond_false);

            // back-edge detection: while loop
            if (true_idx >= 0 && true_idx <= bi) {
                emit_stmts_from_block(blk, out);
                CStmt ws;
                ws.kind = StmtKind::While;
                ws.cond = blk.cond;
                ws.addr = blk.addr;

                std::vector<CStmt> loop_body;
                for (int li = true_idx; li < bi; ++li) {
                    if (visited.count(li)) continue;
                    visited.insert(li);
                    emit_stmts_from_block(func.blocks[block_order[li]], loop_body);
                }
                ws.body = std::move(loop_body);
                out.push_back(std::move(ws));
                continue;
            }

            if (false_idx >= 0 && false_idx <= bi) {
                emit_stmts_from_block(blk, out);
                CStmt ws;
                ws.kind = StmtKind::DoWhile;
                ws.cond = blk.cond;
                ws.addr = blk.addr;
                out.push_back(std::move(ws));
                continue;
            }

            emit_stmts_from_block(blk, out);

            CStmt ifs;
            ifs.addr = blk.addr;
            ifs.cond = blk.cond;

            int merge_point = -1;
            if (true_idx >= 0 && false_idx >= 0) {
                auto& tblk = func.blocks[true_idx];
                auto& fblk = func.blocks[false_idx];
                for (va_t ts : tblk.succs) {
                    int ti = block_index(func, ts);
                    if (ti == false_idx || ti > std::max(true_idx, false_idx)) {
                        merge_point = ti; break;
                    }
                }
                if (merge_point < 0) {
                    for (va_t fs : fblk.succs) {
                        int fi = block_index(func, fs);
                        if (fi == true_idx || fi > std::max(true_idx, false_idx)) {
                            merge_point = fi; break;
                        }
                    }
                }

                // if/else with convergence
                if (merge_point >= 0 && true_idx < (int)func.blocks.size()) {
                    ifs.kind = StmtKind::IfElse;
                    visited.insert(true_idx);
                    emit_stmts_from_block(func.blocks[true_idx], ifs.body);
                    visited.insert(false_idx);
                    emit_stmts_from_block(func.blocks[false_idx], ifs.else_body);
                } else {
                    ifs.kind = StmtKind::If;
                    visited.insert(true_idx);
                    emit_stmts_from_block(func.blocks[true_idx], ifs.body);
                }
            } else if (true_idx >= 0) {
                ifs.kind = StmtKind::If;
                visited.insert(true_idx);
                emit_stmts_from_block(func.blocks[true_idx], ifs.body);
            } else {
                ifs.kind = StmtKind::Goto;
                ifs.goto_target = blk.cond_true;
            }
            out.push_back(std::move(ifs));
        } else if (blk.cond_true && !blk.cond_false) {
            emit_stmts_from_block(blk, out);
            if (block_index(func, blk.cond_true) <= bi) {
                CStmt ws;
                ws.kind = StmtKind::While;
                ws.cond = blk.cond;
                ws.addr = blk.addr;
                out.push_back(std::move(ws));
            } else {
                CStmt ifs;
                ifs.kind = StmtKind::If;
                ifs.cond = blk.cond;
                ifs.addr = blk.addr;
                CStmt gt;
                gt.kind = StmtKind::Goto;
                gt.goto_target = blk.cond_true;
                ifs.body.push_back(std::move(gt));
                out.push_back(std::move(ifs));
            }
        } else {
            emit_stmts_from_block(blk, out);

            if (!blk.has_ret && blk.succs.size() == 1) {
                int succ_idx = block_index(func, blk.succs[0]);
                if (succ_idx >= 0 && succ_idx <= bi) {
                    CStmt gt;
                    gt.kind = StmtKind::Goto;
                    gt.goto_target = blk.succs[0];
                    gt.addr = blk.addr;
                    out.push_back(std::move(gt));
                }
            }
        }
    }
}

CFunc CFStructure::structure(const IRFunc& func) {
    CFunc cf;
    cf.name = func.name;
    cf.entry = func.entry;
    cf.params = func.params;
    cf.locals = func.locals;

    auto order = compute_order(func);
    structure_region(func, order, 0, (int)order.size(), cf.body);
    return cf;
}

}
