#pragma once
#include "core/types.h"
#include <vector>
#include <string>
#include <functional>
#include <memory>

namespace hype {

struct UndoAction {
    std::function<void()> undo;
    std::function<void()> redo;
    std::string           desc;
};

class UndoManager {
public:
    void push(UndoAction act) {
        redo_stack_.clear();
        undo_stack_.push_back(std::move(act));
        if (undo_stack_.size() > 4096)
            undo_stack_.erase(undo_stack_.begin());
    }

    bool can_undo() const { return !undo_stack_.empty(); }
    bool can_redo() const { return !redo_stack_.empty(); }

    std::string undo() {
        if (undo_stack_.empty()) return {};
        auto act = std::move(undo_stack_.back());
        undo_stack_.pop_back();
        act.undo();
        std::string d = act.desc;
        redo_stack_.push_back(std::move(act));
        return d;
    }

    std::string redo() {
        if (redo_stack_.empty()) return {};
        auto act = std::move(redo_stack_.back());
        redo_stack_.pop_back();
        act.redo();
        std::string d = act.desc;
        undo_stack_.push_back(std::move(act));
        return d;
    }

    void clear() { undo_stack_.clear(); redo_stack_.clear(); }

private:
    std::vector<UndoAction> undo_stack_;
    std::vector<UndoAction> redo_stack_;
};

}
