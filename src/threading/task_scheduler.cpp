#include "task_scheduler.h"

namespace hype {

void TaskScheduler::add(AnalysisTask task) {
    auto id = task.id;
    for (auto& dep : task.deps)
        rdeps_[dep].insert(id);
    pending_[id].store(static_cast<int>(task.deps.size()));
    tasks_.emplace(std::move(id), std::move(task));
    ++total_;
}

void TaskScheduler::run_all() {
    for (auto& [id, _] : tasks_) {
        if (pending_[id].load() == 0)
            try_dispatch(id);
    }
    pool_.wait_idle();
}

void TaskScheduler::reset() {
    tasks_.clear();
    rdeps_.clear();
    pending_.clear();
    total_ = 0;
    done_ = 0;
}

void TaskScheduler::try_dispatch(const std::string& id) {
    pool_.submit([this, id]() {
        tasks_[id].work();
        ++done_;
        std::lock_guard lk(mtx_);
        if (rdeps_.count(id)) {
            for (auto& dep_id : rdeps_[id]) {
                if (--pending_[dep_id] == 0)
                    try_dispatch(dep_id);
            }
        }
    });
}

}
