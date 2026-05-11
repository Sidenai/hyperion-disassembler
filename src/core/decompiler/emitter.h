#pragma once
#include "core/decompiler/cf_struct.h"
#include "core/decompiler/type_infer.h"
#include "core/decompiler/pseudo_gen.h"
#include "core/analysis/analysis_db.h"
#include "core/analysis/rtti.h"
#include <unordered_map>
#include <unordered_set>

namespace hype {

class Emitter {
public:
    std::vector<PseudoLine> emit(const CFunc& func, const TypeInfer* ti = nullptr,
                                 const AnalysisDB* db = nullptr,
                                 const RTTIParser* rtti = nullptr);

private:
    void emit_stmt(const CStmt& s, int indent, std::vector<PseudoLine>& out, bool& returned);
    std::string expr_str(const CExpr& e);
    std::string cond_str(const CExpr& e);
    std::string varnode_str(const Varnode& vn);
    std::string type_str(int var_id);
    std::string format_imm(u64 val, bool addr_ctx = false);
    bool is_dead_stmt(const CStmt& s) const;
    bool is_dead_body(const std::vector<CStmt>& body) const;
    void build_inline_map(const CFunc& func);
    void collect_uses(const CExpr& e);
    void collect_stmt_uses(const CStmt& s);
    void build_call_result_map(const CFunc& func);
    bool is_arg_setup(const CStmt& s, size_t idx, const std::vector<CStmt>& stmts) const;
    bool is_rax_used_after(size_t idx, const std::vector<CStmt>& stmts) const;
    bool func_has_meaningful_return(const CFunc& func) const;
    bool is_trailing_return(const CStmt& s) const;
    const RTTIClass* find_rtti_for_call(const std::string& name) const;

    const TypeInfer* ti_ = nullptr;
    const AnalysisDB* db_ = nullptr;
    const RTTIParser* rtti_ = nullptr;
    std::unordered_map<int, const CStmt*> temp_defs_;
    std::unordered_map<int, int> temp_use_count_;
    std::unordered_set<int> inline_temps_;
    std::unordered_set<int> call_result_vars_;
    bool is_void_func_ = false;
    std::string func_name_;
};

}
