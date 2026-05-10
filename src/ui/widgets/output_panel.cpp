#include "output_panel.h"

namespace hype {

void OutputPanel::log(const std::string& msg) {
    std::lock_guard lk(mtx_);
    lines_.push_back(msg);
    if (lines_.size() > 10000) lines_.pop_front();
    scroll_ = true;
}

void OutputPanel::render() {
    ImGui::Begin("Output");
    if (ImGui::Button("Clear")) clear();
    ImGui::Separator();

    ImGui::BeginChild("##log");
    std::lock_guard lk(mtx_);
    ImGuiListClipper clip;
    clip.Begin(static_cast<int>(lines_.size()));
    while (clip.Step())
        for (int i = clip.DisplayStart; i < clip.DisplayEnd; ++i)
            ImGui::TextUnformatted(lines_[i].c_str());
    if (scroll_) { ImGui::SetScrollHereY(1.f); scroll_ = false; }
    ImGui::EndChild();
    ImGui::End();
}

}
