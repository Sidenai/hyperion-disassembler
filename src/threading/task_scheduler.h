#pragma once
#include "worker_pool.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace hype {

struct AnalysisTask {
    std::string              id;
    std::function<void()>    work;
    std::vector<std::string> deps;
};

class TaskScheduler {
public:
    explicit TaskScheduler(WorkerPool& pool) : pool_(pool) {}

    void add(AnalysisTask task);
    void run_all();
    void reset();
    float progress() const {
        int t = total_.load();
        return t > 0 ? static_cast<float>(done_.load()) / t : 0.f;
    }

private:
    void try_dispatch(const std::string& id);

    WorkerPool& pool_;
    std::unordered_map<std::string, AnalysisTask> tasks_;
    std::unordered_map<std::string, std::unordered_set<std::string>> rdeps_;
    std::unordered_map<std::string, std::atomic<int>> pending_;
    std::mutex mtx_;
    std::atomic<int> total_{0};
    std::atomic<int> done_{0};
};

}
