#include "hex_view.h"
#include "ui/theme.h"
#include "ui/fonts.h"
#include <fmt/format.h>
#include <algorithm>

namespace hype {

void HexView::nop_selection() {
    if (!img_ || sel_start_ == sel_end_) return;
    va_t lo = std::min(sel_start_, sel_end_);
    va_t hi = std::max(sel_start_, sel_end_);
    for (auto& seg : img_->segments) {
        if (!seg.contains(lo)) continue;
        if (seg.data.empty()) break;
        size_t off_lo = static_cast<size_t>(lo - seg.va);
        size_t off_hi = static_cast<size_t>(hi - seg.va);
        off_hi = std::min(off_hi, seg.data.size() - 1);
        for (size_t i = off_lo; i <= off_hi; ++i)
            seg.data[i] = 0x90;
        break;
    }
}

void HexView::find_pattern_matches() {
    match_addrs_.clear();
    match_len_ = 0;
    if (!img_ || sel_start_ == sel_end_) return;
    if (seg_idx_ >= (int)img_->segments.size()) return;

    va_t lo = std::min(sel_start_, sel_end_);
    va_t hi = std::max(sel_start_, sel_end_);
    int pat_len = static_cast<int>(hi - lo + 1);
    if (pat_len < 1 || pat_len > 128) return;

    auto& seg = img_->segments[seg_idx_];
    if (!seg.contains(lo)) return;
    size_t pat_off = static_cast<size_t>(lo - seg.va);
    if (pat_off + pat_len > seg.data.size()) return;

    const u8* pat = seg.data.data() + pat_off;
    match_len_ = pat_len;

    size_t limit = seg.data.size() - pat_len;
    for (size_t i = 0; i <= limit; ++i) {
        if (i == pat_off) continue;
        if (std::memcmp(seg.data.data() + i, pat, pat_len) == 0)
            match_addrs_.push_back(seg.va + i);
    }
}

void HexView::render() {
    ImGui::Begin("Hex View");
    if (!img_ || img_->segments.empty()) {
        ImGui::TextDisabled("No data loaded");
        ImGui::End();
        return;
    }

    if (seg_idx_ >= (int)img_->segments.size()) seg_idx_ = 0;
    auto* seg = &img_->segments[seg_idx_];

    if (ImGui::BeginCombo("##seg", seg->name.c_str(), ImGuiComboFlags_WidthFitPreview)) {
        for (int i = 0; i < (int)img_->segments.size(); ++i) {
            bool sel = (i == seg_idx_);
            auto lbl = fmt::format("{} [{:X}]", img_->segments[i].name, img_->segments[i].va);
            if (ImGui::Selectable(lbl.c_str(), sel)) { seg_idx_ = i; seg = &img_->segments[i]; base_ = seg->va; }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Sync", &sync_);
    ImGui::SameLine();
    std::string perms;
    if (seg->readable()) perms += "R";
    if (seg->writable()) perms += "W";
    if (seg->executable()) perms += "X";
    ImGui::TextDisabled("%s  0x%llX bytes  [%s]", seg->name.c_str(),
        (unsigned long long)seg->data.size(), perms.c_str());

    if (sel_start_ != sel_end_) {
        if (sel_start_ != match_sel_start_ || sel_end_ != match_sel_end_) {
            match_sel_start_ = sel_start_;
            match_sel_end_ = sel_end_;
            find_pattern_matches();
        }
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "%d matches", (int)match_addrs_.size());
    } else {
        match_addrs_.clear();
        match_sel_start_ = match_sel_end_ = 0;
    }

    if (cursor_) {
        ImGui::SameLine(ImGui::GetWindowWidth() - 200);
        ImGui::TextDisabled("Cursor: 0x%llX", (unsigned long long)cursor_);
    }
    ImGui::Separator();

    if (base_ < seg->va || base_ >= seg->va + seg->size) base_ = seg->va;
    if (!cursor_) cursor_ = base_;

    int cols = 16;
    size_t seg_size = seg->data.size();
    int total_lines = static_cast<int>((seg_size + cols - 1) / cols);

    if (g_fonts.mono) ImGui::PushFont(g_fonts.mono);
    ImGui::BeginChild("##hexc", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    if (scroll_pending_) {
        size_t off = 0;
        if (base_ >= seg->va) off = static_cast<size_t>(base_ - seg->va);
        int line = static_cast<int>(off / cols);
        float lh = ImGui::GetTextLineHeightWithSpacing();
        ImGui::SetScrollY(static_cast<float>(line) * lh);
        scroll_pending_ = false;
    }

    float lh = ImGui::GetTextLineHeightWithSpacing();
    ImGuiListClipper clip;
    clip.Begin(total_lines, lh);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    while (clip.Step()) {
        for (int i = clip.DisplayStart; i < clip.DisplayEnd; ++i) {
            size_t line_off = static_cast<size_t>(i) * cols;
            va_t line_addr = seg->va + line_off;
            ImVec2 pos = ImGui::GetCursorScreenPos();
            float x = pos.x;
            float y = pos.y;

            auto as = fmt::format("{:016X}", line_addr);
            dl->AddText(ImVec2(x, y), col::addr(), as.c_str());
            x += ImGui::CalcTextSize("0000000000000000").x + 12;

            float hex_start_x = x;

            for (int c = 0; c < cols; ++c) {
                size_t idx = line_off + c;
                va_t byte_addr = line_addr + c;

                if (idx < seg_size) {
                    u8 b = seg->data[idx];
                    auto bs = fmt::format("{:02X}", b);

                    bool is_cursor_byte = (byte_addr == cursor_);
                    bool in_sel = sel_start_ != sel_end_ &&
                        byte_addr >= std::min(sel_start_, sel_end_) &&
                        byte_addr <= std::max(sel_start_, sel_end_);

                    bool in_match = false;
                    if (!match_addrs_.empty() && match_len_ > 0) {
                        for (auto ma : match_addrs_) {
                            if (byte_addr >= ma && byte_addr < ma + match_len_) {
                                in_match = true;
                                break;
                            }
                        }
                    }

                    if (is_cursor_byte) {
                        ImVec2 bpos(x - 1, y);
                        ImVec2 bsz(ImGui::CalcTextSize("00").x + 2, lh - 2);
                        dl->AddRectFilled(bpos, ImVec2(bpos.x + bsz.x, bpos.y + bsz.y),
                            IM_COL32(60, 80, 130, 255));
                        dl->AddRect(bpos, ImVec2(bpos.x + bsz.x, bpos.y + bsz.y),
                            IM_COL32(100, 140, 220, 255));
                    } else if (in_sel) {
                        ImVec2 bpos(x - 1, y);
                        ImVec2 bsz(ImGui::CalcTextSize("00").x + 2, lh - 2);
                        dl->AddRectFilled(bpos, ImVec2(bpos.x + bsz.x, bpos.y + bsz.y),
                            IM_COL32(50, 60, 90, 200));
                    } else if (in_match) {
                        ImVec2 bpos(x - 1, y);
                        ImVec2 bsz(ImGui::CalcTextSize("00").x + 2, lh - 2);
                        dl->AddRectFilled(bpos, ImVec2(bpos.x + bsz.x, bpos.y + bsz.y),
                            IM_COL32(180, 120, 30, 160));
                    }

                    ImU32 bc = IM_COL32(180, 180, 180, 255);
                    if (b == 0x00) bc = IM_COL32(55, 55, 60, 255);
                    else if (b == 0xFF) bc = IM_COL32(200, 120, 120, 255);
                    else if (b == 0xCC || b == 0x90) bc = IM_COL32(140, 80, 80, 255);
                    else if (b >= 0x20 && b < 0x7F) bc = IM_COL32(140, 195, 140, 255);

                    dl->AddText(ImVec2(x, y), bc, bs.c_str());
                }

                x += ImGui::CalcTextSize("00").x + 5;
                if (c == 7) x += 6;
            }

            x += 14;

            for (int c = 0; c < cols; ++c) {
                size_t idx = line_off + c;
                if (idx < seg_size) {
                    u8 b = seg->data[idx];
                    char ch = (b >= 0x20 && b < 0x7F) ? (char)b : '.';
                    va_t byte_addr = line_addr + c;
                    bool is_cursor_byte = (byte_addr == cursor_);

                    ImU32 ac = (ch != '.') ? IM_COL32(170, 195, 170, 255) : IM_COL32(55, 55, 60, 255);
                    if (is_cursor_byte) ac = IM_COL32(220, 240, 255, 255);

                    char buf[2] = {ch, 0};
                    dl->AddText(ImVec2(x, y), ac, buf);
                }
                x += ImGui::CalcTextSize("W").x;
            }

            float row_w = x - pos.x;
            ImGui::PushID(i);
            ImGui::InvisibleButton("##hxr", ImVec2(row_w, lh));

            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                float mx = ImGui::GetIO().MousePos.x;
                float byte_w = ImGui::CalcTextSize("00").x + 5;
                float rel = mx - hex_start_x;
                if (rel >= 0) {
                    int col_guess = static_cast<int>(rel / byte_w);
                    if (col_guess > 7) col_guess = static_cast<int>((rel - 6) / byte_w);
                    col_guess = std::clamp(col_guess, 0, cols - 1);
                    cursor_ = line_addr + col_guess;
                    if (!ImGui::GetIO().KeyShift) sel_start_ = sel_end_ = 0;
                    else { if (!sel_start_) sel_start_ = cursor_; sel_end_ = cursor_; }
                }
            }

            if (ImGui::IsItemHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                float mx = ImGui::GetIO().MousePos.x;
                float byte_w = ImGui::CalcTextSize("00").x + 5;
                float rel = mx - hex_start_x;
                if (rel >= 0) {
                    int col_guess = static_cast<int>(rel / byte_w);
                    if (col_guess > 7) col_guess = static_cast<int>((rel - 6) / byte_w);
                    col_guess = std::clamp(col_guess, 0, cols - 1);
                    sel_end_ = line_addr + col_guess;
                    if (!sel_start_) sel_start_ = cursor_;
                }
            }

            ImGui::PopID();
        }
    }

    if (ImGui::IsWindowFocused()) {
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) cursor_++;
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) && cursor_ > seg->va) cursor_--;
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) cursor_ += cols;
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && cursor_ >= seg->va + cols) cursor_ -= cols;
        if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) cursor_ += cols * 16;
        if (ImGui::IsKeyPressed(ImGuiKey_PageUp) && cursor_ >= seg->va + cols * 16) cursor_ -= cols * 16;

        for (int k = 0; k < 16; ++k) {
            ImGuiKey key = (k < 10) ? (ImGuiKey)((int)ImGuiKey_0 + k) : (ImGuiKey)((int)ImGuiKey_A + k - 10);
            if (ImGui::IsKeyPressed(key) && !ImGui::GetIO().KeyCtrl) {
                size_t off = static_cast<size_t>(cursor_ - seg->va);
                if (off < seg->data.size()) {
                    u8& b = seg->data[off];
                    if (edit_nibble_ == 0) {
                        b = static_cast<u8>((b & 0x0F) | (k << 4));
                        edit_nibble_ = 1;
                    } else {
                        b = static_cast<u8>((b & 0xF0) | k);
                        edit_nibble_ = 0;
                        cursor_++;
                    }
                }
            }
        }
    }

    ImGui::EndChild();
    if (g_fonts.mono) ImGui::PopFont();
    ImGui::End();
}

}
