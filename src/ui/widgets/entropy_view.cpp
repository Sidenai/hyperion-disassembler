#include "entropy_view.h"
#include <fmt/format.h>
#include <cmath>

namespace hype {

void EntropyView::rebuild() {
    blocks_.clear();
    if (!img_) return;

    for (auto& seg : img_->segments) {
        size_t n = seg.data.size() / BLOCK_SZ;
        for (size_t i = 0; i < n; ++i) {
            const u8* p = seg.data.data() + i * BLOCK_SZ;
            int freq[256] = {};
            for (size_t j = 0; j < BLOCK_SZ; ++j) freq[p[j]]++;
            float ent = 0.0f;
            for (int f : freq) {
                if (!f) continue;
                float prob = static_cast<float>(f) / BLOCK_SZ;
                ent -= prob * std::log2f(prob);
            }
            blocks_.push_back({seg.va + i * BLOCK_SZ, ent});
        }
    }
    dirty_ = false;
}

void EntropyView::render() {
    ImGui::Begin("Entropy");
    if (!img_) { ImGui::TextDisabled("No data"); ImGui::End(); return; }
    if (dirty_) rebuild();
    if (blocks_.empty()) { ImGui::End(); return; }

    ImVec2 avail = ImGui::GetContentRegionAvail();
    float bar_w = avail.x / static_cast<float>(blocks_.size());
    if (bar_w < 1.0f) bar_w = 1.0f;
    float h = avail.y - 20;
    if (h < 40) h = 40;

    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    for (size_t i = 0; i < blocks_.size(); ++i) {
        float t = blocks_[i].entropy / 8.0f;
        t = std::clamp(t, 0.0f, 1.0f);

        u8 r = static_cast<u8>(t * 255);
        u8 b = static_cast<u8>((1.0f - t) * 255);
        u8 g = static_cast<u8>((t < 0.5f ? t * 2 : (1.0f - t) * 2) * 120);
        ImU32 col = IM_COL32(r, g, b, 255);

        float x = origin.x + i * bar_w;
        float bar_h = t * h;
        dl->AddRectFilled(ImVec2(x, origin.y + h - bar_h), ImVec2(x + bar_w, origin.y + h), col);
    }

    ImGui::InvisibleButton("##ent", ImVec2(avail.x, h));
    if (ImGui::IsItemHovered()) {
        float mx = ImGui::GetIO().MousePos.x - origin.x;
        int idx = static_cast<int>(mx / bar_w);
        idx = std::clamp(idx, 0, static_cast<int>(blocks_.size()) - 1);
        ImGui::SetTooltip("0x%llX  entropy: %.2f", (unsigned long long)blocks_[idx].addr, blocks_[idx].entropy);

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            if (nav_) nav_(blocks_[idx].addr);
    }

    ImGui::End();
}

}
