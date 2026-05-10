#pragma once
#include <imgui.h>
#include <string>
#include <deque>
#include <mutex>

namespace hype {

class OutputPanel {
public:
    void log(const std::string& msg);
    void clear() { std::lock_guard lk(mtx_); lines_.clear(); }
    void render();

private:
    std::deque<std::string> lines_;
    std::mutex              mtx_;
    bool                    scroll_ = true;
};

}
