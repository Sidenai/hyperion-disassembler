#pragma once
#include <GLFW/glfw3.h>
#include <functional>

namespace hype {

class Renderer {
public:
    bool init(int w, int h, const char* title);
    void shutdown();
    bool begin_frame();
    void end_frame();
    bool should_close() const;
    GLFWwindow* window() const { return wnd_; }
    void set_drop_callback(std::function<void(const char*)> cb) { drop_cb_ = cb; }

private:
    GLFWwindow* wnd_ = nullptr;
    std::function<void(const char*)> drop_cb_;

    friend void glfw_drop_cb(GLFWwindow*, int, const char**);
};

}
