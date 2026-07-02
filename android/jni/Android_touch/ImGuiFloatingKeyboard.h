#pragma once

#include "imgui.h"
#include "imgui_internal.h"
#include <string>
#include <cstring>
#include <cstdio>

namespace ImGuiFloatingKeyboard
{
    namespace Internal
    {
        static bool is_visible = false;
        static char *target_buffer = nullptr;
        static size_t target_buffer_size = 0;
        static bool shift_active = false;
        static bool caps_lock_active = false;
        static std::string window_title = "Floating Keyboard";
        static bool request_focus = false;
        static int keyboard_mode = 0;

        static ImVec2 last_display_size = ImVec2(0, 0);
        static bool need_reposition = true;

        // ===== 缩放设置（固定1.4，不可修改）=====
        static constexpr float keyboard_scale = 1.4f;
    }

    // 获取缩放比例（只读）
    inline float GetScale() { return Internal::keyboard_scale; }

    inline void Open(char *buffer, size_t buffer_size, const char *title = "Keyboard")
    {
        Internal::target_buffer = buffer;
        Internal::target_buffer_size = buffer_size;
        Internal::is_visible = true;
        Internal::shift_active = false;
        Internal::caps_lock_active = false;
        Internal::window_title = title;
        Internal::request_focus = true;
        Internal::keyboard_mode = 0;
        Internal::need_reposition = true;
    }

    inline void Close()
    {
        Internal::is_visible = false;
        Internal::target_buffer = nullptr;
    }

    inline bool IsVisible() { return Internal::is_visible; }

    // ========== 辅助函数 ==========
    static inline void InsertChar(char c)
    {
        if (!Internal::target_buffer)
            return;
        size_t len = strlen(Internal::target_buffer);
        if (len < Internal::target_buffer_size - 1)
        {
            Internal::target_buffer[len] = c;
            Internal::target_buffer[len + 1] = '\0';
        }
    }

    static inline void InsertString(const char *str)
    {
        if (!Internal::target_buffer || !str)
            return;
        size_t current_len = strlen(Internal::target_buffer);
        size_t str_len = strlen(str);
        size_t available = Internal::target_buffer_size - current_len - 1;
        size_t copy_len = (str_len < available) ? str_len : available;

        if (copy_len > 0)
        {
            memcpy(Internal::target_buffer + current_len, str, copy_len);
            Internal::target_buffer[current_len + copy_len] = '\0';
        }
    }

    static inline void DeleteChar()
    {
        if (!Internal::target_buffer)
            return;
        size_t len = strlen(Internal::target_buffer);
        if (len > 0)
            Internal::target_buffer[len - 1] = '\0';
    }

    static inline void ClearAll()
    {
        if (Internal::target_buffer)
            Internal::target_buffer[0] = '\0';
    }

    // ========== 剪贴板功能 ==========
    static inline void CopyToClipboard()
    {
        if (Internal::target_buffer && strlen(Internal::target_buffer) > 0)
        {
            ImGui::SetClipboardText(Internal::target_buffer);
        }
    }

    static inline void PasteFromClipboard()
    {
        if (!Internal::target_buffer)
            return;
        const char *clipboard = ImGui::GetClipboardText();
        if (clipboard && strlen(clipboard) > 0)
        {
            InsertString(clipboard);
        }
    }


    // ========== 按键渲染 ==========
    static inline bool KeyButton(const char *label, const char *id, ImVec2 size,
                                 ImVec4 color = ImVec4(0.25f, 0.25f, 0.28f, 1.0f))
    {
        char full_id[64];
        snprintf(full_id, sizeof(full_id), "%s##%s", label, id);

        ImGui::PushStyleColor(ImGuiCol_Button, color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              ImVec4(color.x + 0.12f, color.y + 0.12f, color.z + 0.12f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              ImVec4(color.x + 0.22f, color.y + 0.22f, color.z + 0.22f, 1.0f));

        bool pressed = ImGui::Button(full_id, size);
        ImGui::PopStyleColor(3);
        return pressed;
    }

    // ========== 主绘制函数 ==========
    inline void Draw()
    {
        if (!Internal::is_visible)
            return;

        ImGuiIO &io = ImGui::GetIO();
        constexpr float scale = Internal::keyboard_scale; // 固定1.4

        // 检测屏幕变化
        if (Internal::last_display_size.x != io.DisplaySize.x ||
            Internal::last_display_size.y != io.DisplaySize.y)
        {
            Internal::need_reposition = true;
            Internal::last_display_size = io.DisplaySize;
        }

        float screen_w = io.DisplaySize.x;
        float screen_h = io.DisplaySize.y;

        // ===== 计算缩放后的尺寸（更大的基础宽度）=====
        float base_width = 820.0f; // 增大基础宽度
        float kb_width = base_width * scale;

        // 确保不超过屏幕
        if (kb_width > screen_w * 0.98f)
            kb_width = screen_w * 0.98f;
        if (kb_width < 400.0f)
            kb_width = 400.0f;

        // 按键高度（基础值 * 缩放）
        float base_key_height = 78.0f; // 增大按键高度
        float key_height = base_key_height * scale;

        // 间距
        float spacing = 5.0f * scale;

        // ===== 窗口位置 =====
        if (Internal::need_reposition)
        {
            float pos_x = (screen_w - kb_width) * 0.5f;
            float pos_y = screen_h * 0.15f; // 更靠上
            if (pos_x < 10)
                pos_x = 10;
            if (pos_y < 10)
                pos_y = 10;
            ImGui::SetNextWindowPos(ImVec2(pos_x, pos_y), ImGuiCond_Always);
            Internal::need_reposition = false;
        }

        ImGui::SetNextWindowSize(ImVec2(kb_width, 0), ImGuiCond_Always);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings;

        if (Internal::request_focus)
        {
            ImGui::SetNextWindowFocus();
            Internal::request_focus = false;
        }

        // ===== 样式 =====
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.13f, 0.13f, 0.15f, 0.97f));
        ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.18f, 0.18f, 0.22f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.22f, 0.22f, 0.28f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f * scale);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f * scale);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f * scale, 12.0f * scale));

        if (ImGui::Begin(Internal::window_title.c_str(), &Internal::is_visible, flags))
        {
            // ===== 字体缩放 =====
            ImGui::SetWindowFontScale(scale);

            if (!Internal::target_buffer)
            {
                Close();
                ImGui::SetWindowFontScale(1.0f);
                ImGui::End();
                ImGui::PopStyleVar(3);
                ImGui::PopStyleColor(3);
                return;
            }

            // 窗口边界修正
            ImVec2 win_pos = ImGui::GetWindowPos();
            ImVec2 win_size = ImGui::GetWindowSize();
            bool need_fix = false;
            if (win_pos.x < 0)
            {
                win_pos.x = 5;
                need_fix = true;
            }
            if (win_pos.y < 0)
            {
                win_pos.y = 5;
                need_fix = true;
            }
            if (win_pos.x + win_size.x > screen_w)
            {
                win_pos.x = screen_w - win_size.x - 5;
                need_fix = true;
            }
            if (win_pos.y + win_size.y > screen_h)
            {
                win_pos.y = screen_h - win_size.y - 5;
                need_fix = true;
            }
            if (need_fix)
                ImGui::SetWindowPos(win_pos);

            float avail_width = ImGui::GetContentRegionAvail().x;
            float std_key_w = (avail_width - spacing * 9) / 10.0f;

            // ===== 输入预览 + 复制/粘贴/清空按钮 =====
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.08f, 0.08f, 0.1f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f * scale, 12.0f * scale));

            // 按钮尺寸（3个按钮）
            float func_btn_w = 70.0f * scale;
            float func_btn_h = 38.0f * scale;
            float total_btns_w = func_btn_w * 3 + spacing * 3; // ← 改为3个按钮

            ImGui::SetNextItemWidth(avail_width - total_btns_w);
            ImGui::InputText("##preview", Internal::target_buffer, Internal::target_buffer_size, ImGuiInputTextFlags_ReadOnly);
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();

            ImGui::SameLine();
            if (KeyButton("复制", "copy", ImVec2(func_btn_w, func_btn_h), ImVec4(0.2f, 0.4f, 0.5f, 1.0f)))
                CopyToClipboard();

            ImGui::SameLine();
            if (KeyButton("粘贴", "paste", ImVec2(func_btn_w, func_btn_h), ImVec4(0.3f, 0.45f, 0.25f, 1.0f)))
                PasteFromClipboard();

            ImGui::SameLine();
            if (KeyButton("清空", "clear", ImVec2(func_btn_w, func_btn_h), ImVec4(0.5f, 0.25f, 0.2f, 1.0f)))
                ClearAll();

            ImGui::Separator();
            ImGui::Spacing();

            // ===== 按键 =====
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, spacing));

            bool is_upper = Internal::shift_active || Internal::caps_lock_active;

            // ==================== 字母键盘 ====================
            if (Internal::keyboard_mode == 0)
            {
                // 第1行
                const char *row1 = is_upper ? "QWERTYUIOP" : "qwertyuiop";
                for (int i = 0; row1[i]; ++i)
                {
                    char lbl[2] = {row1[i], 0};
                    char id[16];
                    snprintf(id, 16, "r1_%d", i);
                    if (KeyButton(lbl, id, ImVec2(std_key_w, key_height)))
                    {
                        InsertChar(row1[i]);
                        if (Internal::shift_active && !Internal::caps_lock_active)
                            Internal::shift_active = false;
                    }
                    if (row1[i + 1])
                        ImGui::SameLine();
                }

                // 第2行
                const char *row2 = is_upper ? "ASDFGHJKL" : "asdfghjkl";
                float row2_offset = std_key_w * 0.5f;
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + row2_offset);
                for (int i = 0; row2[i]; ++i)
                {
                    char lbl[2] = {row2[i], 0};
                    char id[16];
                    snprintf(id, 16, "r2_%d", i);
                    if (KeyButton(lbl, id, ImVec2(std_key_w, key_height)))
                    {
                        InsertChar(row2[i]);
                        if (Internal::shift_active && !Internal::caps_lock_active)
                            Internal::shift_active = false;
                    }
                    if (row2[i + 1])
                        ImGui::SameLine();
                }

                // 第3行
                const char *row3 = is_upper ? "ZXCVBNM" : "zxcvbnm";
                float wide_key_w = std_key_w * 1.5f;
                float row3_key_w = (avail_width - wide_key_w * 2 - spacing * 8) / 7.0f;

                ImVec4 shift_color = (Internal::shift_active || Internal::caps_lock_active)
                                         ? ImVec4(0.3f, 0.45f, 0.7f, 1.0f)
                                         : ImVec4(0.3f, 0.3f, 0.35f, 1.0f);
                const char *shift_label = Internal::caps_lock_active ? "CAPS" : (is_upper ? "SHIFT" : "shift");

                if (KeyButton(shift_label, "shift", ImVec2(wide_key_w, key_height), shift_color))
                    Internal::shift_active = !Internal::shift_active;
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
                {
                    Internal::caps_lock_active = !Internal::caps_lock_active;
                    Internal::shift_active = false;
                }
                ImGui::SameLine();

                for (int i = 0; row3[i]; ++i)
                {
                    char lbl[2] = {row3[i], 0};
                    char id[16];
                    snprintf(id, 16, "r3_%d", i);
                    if (KeyButton(lbl, id, ImVec2(row3_key_w, key_height)))
                    {
                        InsertChar(row3[i]);
                        if (Internal::shift_active && !Internal::caps_lock_active)
                            Internal::shift_active = false;
                    }
                    ImGui::SameLine();
                }

                if (KeyButton("DEL", "bksp", ImVec2(wide_key_w, key_height), ImVec4(0.55f, 0.2f, 0.2f, 1.0f)))
                    DeleteChar();

                // 第4行
                float mode_key_w = std_key_w * 1.3f;
                float punct_key_w = std_key_w;
                float ok_key_w = std_key_w * 1.5f;
                float space_w = avail_width - mode_key_w - punct_key_w * 2 - ok_key_w - spacing * 4;

                if (KeyButton("?123", "tonum", ImVec2(mode_key_w, key_height), ImVec4(0.3f, 0.3f, 0.38f, 1.0f)))
                    Internal::keyboard_mode = 1;
                ImGui::SameLine();

                if (KeyButton(",", "comma", ImVec2(punct_key_w, key_height)))
                    InsertChar(',');
                ImGui::SameLine();

                if (KeyButton("SPACE", "space", ImVec2(space_w, key_height), ImVec4(0.22f, 0.22f, 0.26f, 1.0f)))
                    InsertChar(' ');
                ImGui::SameLine();

                if (KeyButton(".", "period", ImVec2(punct_key_w, key_height)))
                    InsertChar('.');
                ImGui::SameLine();

                if (KeyButton("OK", "enter", ImVec2(ok_key_w, key_height), ImVec4(0.2f, 0.5f, 0.3f, 1.0f)))
                    Close();
            }
            // ==================== 数字符号键盘1 ====================
            else if (Internal::keyboard_mode == 1)
            {
                const char *row1 = "1234567890";
                for (int i = 0; row1[i]; ++i)
                {
                    char lbl[2] = {row1[i], 0};
                    char id[16];
                    snprintf(id, 16, "n1_%d", i);
                    if (KeyButton(lbl, id, ImVec2(std_key_w, key_height)))
                        InsertChar(row1[i]);
                    if (row1[i + 1])
                        ImGui::SameLine();
                }

                const char *row2 = "@#$_&-+()/";
                for (int i = 0; row2[i]; ++i)
                {
                    char lbl[2] = {row2[i], 0};
                    char id[16];
                    snprintf(id, 16, "n2_%d", i);
                    if (KeyButton(lbl, id, ImVec2(std_key_w, key_height)))
                        InsertChar(row2[i]);
                    if (row2[i + 1])
                        ImGui::SameLine();
                }

                float wide_key_w = std_key_w * 1.5f;
                float row3_key_w = (avail_width - wide_key_w * 2 - spacing * 8) / 7.0f;

                if (KeyButton("#+=", "tosym", ImVec2(wide_key_w, key_height), ImVec4(0.3f, 0.3f, 0.38f, 1.0f)))
                    Internal::keyboard_mode = 2;
                ImGui::SameLine();

                const char *row3 = "*\"':;!?";
                for (int i = 0; row3[i]; ++i)
                {
                    char lbl[2] = {row3[i], 0};
                    char id[16];
                    snprintf(id, 16, "n3_%d", i);
                    if (KeyButton(lbl, id, ImVec2(row3_key_w, key_height)))
                        InsertChar(row3[i]);
                    ImGui::SameLine();
                }

                if (KeyButton("DEL", "bksp2", ImVec2(wide_key_w, key_height), ImVec4(0.55f, 0.2f, 0.2f, 1.0f)))
                    DeleteChar();

                float mode_key_w = std_key_w * 1.3f;
                float punct_key_w = std_key_w;
                float ok_key_w = std_key_w * 1.5f;
                float space_w = avail_width - mode_key_w - punct_key_w * 2 - ok_key_w - spacing * 4;

                if (KeyButton("ABC", "toabc", ImVec2(mode_key_w, key_height), ImVec4(0.3f, 0.3f, 0.38f, 1.0f)))
                    Internal::keyboard_mode = 0;
                ImGui::SameLine();

                if (KeyButton(",", "comma2", ImVec2(punct_key_w, key_height)))
                    InsertChar(',');
                ImGui::SameLine();

                if (KeyButton("SPACE", "space2", ImVec2(space_w, key_height), ImVec4(0.22f, 0.22f, 0.26f, 1.0f)))
                    InsertChar(' ');
                ImGui::SameLine();

                if (KeyButton(".", "period2", ImVec2(punct_key_w, key_height)))
                    InsertChar('.');
                ImGui::SameLine();

                if (KeyButton("OK", "enter2", ImVec2(ok_key_w, key_height), ImVec4(0.2f, 0.5f, 0.3f, 1.0f)))
                    Close();
            }
            // ==================== 符号键盘2 ====================
            else if (Internal::keyboard_mode == 2)
            {
                const char *row1 = "~`|\\<>{}[]";
                for (int i = 0; row1[i]; ++i)
                {
                    char lbl[2] = {row1[i], 0};
                    char id[16];
                    snprintf(id, 16, "s1_%d", i);
                    if (KeyButton(lbl, id, ImVec2(std_key_w, key_height)))
                        InsertChar(row1[i]);
                    if (row1[i + 1])
                        ImGui::SameLine();
                }

                const char *row2 = "^%=*+!?;:/";
                for (int i = 0; row2[i]; ++i)
                {
                    char lbl[2] = {row2[i], 0};
                    char id[16];
                    snprintf(id, 16, "s2_%d", i);
                    if (KeyButton(lbl, id, ImVec2(std_key_w, key_height)))
                        InsertChar(row2[i]);
                    if (row2[i + 1])
                        ImGui::SameLine();
                }

                float wide_key_w = std_key_w * 1.5f;
                float row3_key_w = (avail_width - wide_key_w * 2 - spacing * 8) / 7.0f;

                if (KeyButton("?123", "tonum2", ImVec2(wide_key_w, key_height), ImVec4(0.3f, 0.3f, 0.38f, 1.0f)))
                    Internal::keyboard_mode = 1;
                ImGui::SameLine();

                const char *row3 = "@#$&()\"";
                for (int i = 0; row3[i]; ++i)
                {
                    char lbl[2] = {row3[i], 0};
                    char id[16];
                    snprintf(id, 16, "s3_%d", i);
                    if (KeyButton(lbl, id, ImVec2(row3_key_w, key_height)))
                        InsertChar(row3[i]);
                    ImGui::SameLine();
                }

                if (KeyButton("DEL", "bksp3", ImVec2(wide_key_w, key_height), ImVec4(0.55f, 0.2f, 0.2f, 1.0f)))
                    DeleteChar();

                float mode_key_w = std_key_w * 1.3f;
                float punct_key_w = std_key_w;
                float ok_key_w = std_key_w * 1.5f;
                float space_w = avail_width - mode_key_w - punct_key_w * 2 - ok_key_w - spacing * 4;

                if (KeyButton("ABC", "toabc2", ImVec2(mode_key_w, key_height), ImVec4(0.3f, 0.3f, 0.38f, 1.0f)))
                    Internal::keyboard_mode = 0;
                ImGui::SameLine();

                if (KeyButton(",", "comma3", ImVec2(punct_key_w, key_height)))
                    InsertChar(',');
                ImGui::SameLine();

                if (KeyButton("SPACE", "space3", ImVec2(space_w, key_height), ImVec4(0.22f, 0.22f, 0.26f, 1.0f)))
                    InsertChar(' ');
                ImGui::SameLine();

                if (KeyButton(".", "period3", ImVec2(punct_key_w, key_height)))
                    InsertChar('.');
                ImGui::SameLine();

                if (KeyButton("OK", "enter3", ImVec2(ok_key_w, key_height), ImVec4(0.2f, 0.5f, 0.3f, 1.0f)))
                    Close();
            }

            ImGui::PopStyleVar();
            ImGui::SetWindowFontScale(1.0f);
        }
        ImGui::End();

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(3);
    }

} // namespace ImGuiFloatingKeyboard