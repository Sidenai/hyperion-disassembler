#pragma once
#include <functional>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>
#include <memory>

namespace hype {

class WorkerPool {
public:
    explicit WorkerPool(unsigned n = 0);
    ~WorkerPool();

    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using R = std::invoke_result_t<F, Args...>;
        auto task = std::make_shared<std::packaged_task<R()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        auto fut = task->get_future();
        {
            std::lock_guard lk(mtx_);
            queue_.emplace([task]() { (*task)(); });
            ++pending_;
        }
        cv_.notify_one();
        return fut;
    }

    void wait_idle();
    unsigned thread_count() const { return static_cast<unsigned>(workers_.size()); }

private:
    std::vector<std::jthread>         workers_;
    std::queue<std::function<void()>> queue_;
    std::mutex                        mtx_;
    std::condition_variable           cv_;
    std::condition_variable           idle_cv_;
    std::atomic<bool>                 stop_{false};
    std::atomic<unsigned>             pending_{0};
};

}
