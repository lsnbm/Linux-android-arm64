#pragma once

#include "imgui.h"
#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>

namespace ImGuiFloatingKeyboard
{
    enum class Result { None, Accepted, Cancelled };

    namespace Internal
    {
        inline char *target_buffer = nullptr;
        inline size_t target_buffer_size = 0;
        inline std::string window_title;
        inline bool just_opened = false;
        inline bool shift_active = false, caps_lock_active = false;
        enum class Mode { Letters, Numbers, Symbols };
        inline Mode mode = Mode::Letters;
        inline const char *result_buffer = nullptr;
        inline Result result = Result::None;
    }

    inline void Open(char *buffer, size_t buffer_size, const char *title = "Keyboard")
    {
        if (!buffer || !buffer_size) return;
        buffer[buffer_size - 1] = '\0';
        Internal::target_buffer = buffer;
        Internal::target_buffer_size = buffer_size;
        Internal::window_title = title ? title : "Keyboard";
        Internal::just_opened = true;
        Internal::shift_active = Internal::caps_lock_active = false;
        Internal::mode = Internal::Mode::Letters;
        Internal::result_buffer = nullptr;
        Internal::result = Result::None;
    }

    inline Result ConsumeResult(const char *buffer)
    {
        if (Internal::result_buffer != buffer) return Result::None;
        Internal::result_buffer = nullptr;
        return std::exchange(Internal::result, Result::None);
    }

    template <size_t N> inline bool InputButton(char (&buffer)[N], const char *placeholder, ImVec2 size, const char *title = nullptr)
    {
        std::string label = buffer[0] ? buffer : placeholder;
        label += "###keyboard_input";
        ImGui::PushID(static_cast<const void *>(buffer));
        if (ImGui::Button(label.c_str(), size)) Open(buffer, N, title ? title : placeholder);
        ImGui::PopID();
        return ConsumeResult(buffer) == Result::Accepted;
    }

    template <size_t N> inline bool ActionButton(const char *label, ImVec2 size, char (&buffer)[N], const char *title, ImVec4 color = {}, bool clearOnOpen = true)
    {
        ImGui::PushID(static_cast<const void *>(buffer));
        if (color.w > 0.0f) ImGui::PushStyleColor(ImGuiCol_Button, color);
        if (ImGui::Button(label, size))
        {
            if (clearOnOpen) buffer[0] = '\0';
            Open(buffer, N, title);
        }
        if (color.w > 0.0f) ImGui::PopStyleColor();
        ImGui::PopID();
        return ConsumeResult(buffer) == Result::Accepted;
    }

    namespace Internal
    {
        inline void Append(std::string_view text)
        {
            const size_t length = std::strlen(target_buffer);
            size_t count = std::min(text.size(), target_buffer_size - length - 1);
            // ImGui 使用 UTF-8；容量不足时，不拷贝跨越末尾的半个字符。
            if (count < text.size())
                while (count && (static_cast<unsigned char>(text[count]) & 0xc0) == 0x80) --count;
            std::memmove(target_buffer + length, text.data(), count);
            target_buffer[length + count] = '\0';
        }

        inline bool KeyButton(const char *label, ImVec2 size, ImVec4 color = ImVec4(0.25f, 0.25f, 0.28f, 1.0f))
        {
            ImGui::PushStyleColor(ImGuiCol_Button, color);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x + 0.12f, color.y + 0.12f, color.z + 0.12f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(color.x + 0.22f, color.y + 0.22f, color.z + 0.22f, 1.0f));
            const bool pressed = ImGui::Button(label, size);
            ImGui::PopStyleColor(3);
            return pressed;
        }

        inline void CharacterRow(const char *keys, ImVec2 size, bool letters)
        {
            for (; *keys; ++keys)
            {
                char label[] = {*keys, '\0'};
                if (KeyButton(label, size))
                {
                    Append({keys, 1});
                    if (letters && !caps_lock_active) shift_active = false;
                }
                if (keys[1]) ImGui::SameLine();
            }
        }
    }

    inline void Draw()
    {
        using namespace Internal;
        if (!target_buffer) return;

        constexpr float scale = 1.4f, spacing = 5.0f * scale, key_height = 78.0f * scale;
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        static ImVec2 last_display{};
        const float width = std::min(820.0f * scale, display.x * 0.98f);
        if (just_opened || last_display.x != display.x || last_display.y != display.y)
        {
            ImGui::SetNextWindowPos(ImVec2(std::max(10.0f, (display.x - width) * 0.5f), std::max(10.0f, display.y * 0.15f)));
            last_display = display;
        }
        if (just_opened)
        {
            ImGui::SetNextWindowFocus();
            just_opened = false;
        }
        ImGui::SetNextWindowSize(ImVec2(width, 0));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.13f, 0.13f, 0.15f, 0.97f));
        ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.18f, 0.18f, 0.22f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.22f, 0.22f, 0.28f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f * scale);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f * scale);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f * scale, 12.0f * scale));

        bool open = true, accepted = false;
        if (ImGui::Begin(window_title.c_str(), &open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
        {
            ImGui::SetWindowFontScale(scale);
            const ImVec2 pos = ImGui::GetWindowPos(), size = ImGui::GetWindowSize();
            const ImVec2 bounded(std::clamp(pos.x, 0.0f, std::max(0.0f, display.x - size.x)),
                                 std::clamp(pos.y, 0.0f, std::max(0.0f, display.y - size.y)));
            if (pos.x != bounded.x || pos.y != bounded.y) ImGui::SetWindowPos(bounded);

            const float available = ImGui::GetContentRegionAvail().x;
            const ImVec2 edit_size(70.0f * scale, 38.0f * scale);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.08f, 0.08f, 0.1f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f * scale, 12.0f * scale));
            ImGui::SetNextItemWidth(available - 3 * (edit_size.x + ImGui::GetStyle().ItemSpacing.x));
            ImGui::InputText("##preview", target_buffer, target_buffer_size, ImGuiInputTextFlags_ReadOnly);
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
            ImGui::SameLine();
            if (KeyButton("复制", edit_size, ImVec4(0.2f, 0.4f, 0.5f, 1.0f)) && *target_buffer) ImGui::SetClipboardText(target_buffer);
            ImGui::SameLine();
            if (KeyButton("粘贴", edit_size, ImVec4(0.3f, 0.45f, 0.25f, 1.0f)))
            {
                if (const char *clipboard = ImGui::GetClipboardText()) Append(clipboard);
            }
            ImGui::SameLine();
            if (KeyButton("清空", edit_size, ImVec4(0.5f, 0.25f, 0.2f, 1.0f))) target_buffer[0] = '\0';
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, spacing));

            const bool letters = mode == Mode::Letters, numbers = mode == Mode::Numbers;
            const bool upper = shift_active || caps_lock_active;
            static constexpr const char *rows[][3] = {
                {"qwertyuiop", "asdfghjkl", "zxcvbnm"},
                {"1234567890", "@#$_&-+()/", "*\"':;!?"},
                {"~`|\\<>{}[]", "^%=*+!?;:/", "@#$&()\""},
                {"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"},
            };
            const auto &layout = rows[letters && upper ? 3 : static_cast<int>(mode)];
            const ImVec2 key((available - 9 * spacing) / 10, key_height), wide(key.x * 1.5f, key_height);
            const ImVec4 mode_color(0.3f, 0.3f, 0.38f, 1.0f);
            ImGui::PushID(0);
            CharacterRow(layout[0], key, letters);
            ImGui::PopID();
            if (letters) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + key.x * 0.5f);
            ImGui::PushID(1);
            CharacterRow(layout[1], key, letters);
            ImGui::PopID();

            if (letters)
            {
                KeyButton(caps_lock_active ? "CAPS###shift" : (upper ? "SHIFT###shift" : "shift###shift"), wide,
                          upper ? ImVec4(0.3f, 0.45f, 0.7f, 1.0f) : ImVec4(0.3f, 0.3f, 0.35f, 1.0f));
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
                {
                    caps_lock_active = !caps_lock_active;
                    shift_active = false;
                }
                else if (ImGui::IsItemActivated() && !caps_lock_active) shift_active = !shift_active;
            }
            else if (KeyButton(numbers ? "#+=" : "?123", wide, mode_color)) mode = numbers ? Mode::Symbols : Mode::Numbers;
            ImGui::SameLine();
            ImGui::PushID(2);
            CharacterRow(layout[2], ImVec2((available - wide.x * 2 - spacing * 8) / 7, key_height), letters);
            ImGui::PopID();
            ImGui::SameLine();
            if (KeyButton("DEL", wide, ImVec4(0.55f, 0.2f, 0.2f, 1.0f)) && *target_buffer)
            {
                size_t last = std::strlen(target_buffer) - 1;
                while (last && (static_cast<unsigned char>(target_buffer[last]) & 0xc0) == 0x80) --last;
                target_buffer[last] = '\0';
            }

            const ImVec2 mode_key(key.x * 1.3f, key_height);
            if (KeyButton(letters ? "?123" : "ABC", mode_key, mode_color)) mode = letters ? Mode::Numbers : Mode::Letters;
            ImGui::SameLine();
            if (KeyButton(",", key)) Append(",");
            ImGui::SameLine();
            if (KeyButton("SPACE", ImVec2(available - mode_key.x - 2 * key.x - wide.x - 4 * spacing, key_height), ImVec4(0.22f, 0.22f, 0.26f, 1.0f))) Append(" ");
            ImGui::SameLine();
            if (KeyButton(".", key)) Append(".");
            ImGui::SameLine();
            accepted = KeyButton("OK", wide, ImVec4(0.2f, 0.5f, 0.3f, 1.0f));
            ImGui::PopStyleVar();
            ImGui::SetWindowFontScale(1.0f);
        }
        ImGui::End();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(3);
        if (accepted || !open)
        {
            result_buffer = std::exchange(target_buffer, nullptr);
            result = accepted ? Result::Accepted : Result::Cancelled;
        }
    }
}