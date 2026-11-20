#pragma once

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "imgui/imgui.h"
#include "android_imgui_input/imgui_floating_keyboard.h"
#include "application/runtime.h"
#include "base/system_log.h"
#include "memory/signature_scanner.h"

// ============================================================================
// 布局构建器
// ============================================================================
namespace UI
{
    inline bool Btn(const char *label, ImVec2 size, ImVec4 col = {})
    {
        if (col.w > 0) ImGui::PushStyleColor(ImGuiCol_Button, col);
        bool r = ImGui::Button(label, size);
        if (col.w > 0) ImGui::PopStyleColor();
        return r;
    }

    // ---- 高级布局组件 ----

    // 带颜色的子窗口块
    template <typename F> void ColorChild(const char *id, ImVec2 size, ImVec4 bg, F &&body, ImGuiWindowFlags flags = 0)
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
        if (ImGui::BeginChild(id, size, true, flags)) body();
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    // 标签 + 值 行
    inline void LabelValue(ImVec4 labelCol, const char *label, ImVec4 valCol, const char *fmt, ...)
    {
        ImGui::TextColored(labelCol, "%s", label);
        ImGui::SameLine();
        va_list a;
        va_start(a, fmt);
        ImGui::TextColoredV(valCol, fmt, a);
        va_end(a);
    }

    // 上下箭头滚动条
    inline void ArrowScroll(const char *id, float w, float h, int &idx, int minIdx, int maxIdx)
    {
        if (ImGui::BeginChild(id, {w, h}, false, ImGuiWindowFlags_NoScrollbar))
        {
            ImGui::PushStyleColor(ImGuiCol_Button, {0.2f, 0.3f, 0.4f, 1.0f});
            ImGui::BeginDisabled(idx <= minIdx);
            if (ImGui::Button("▲", {w, h / 2 - 3})) --idx;
            ImGui::EndDisabled();
            ImGui::BeginDisabled(idx >= maxIdx);
            if (ImGui::Button("▼", {w, h / 2 - 3})) ++idx;
            ImGui::EndDisabled();
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();
    }

} // namespace UI
namespace Colors
{
    constexpr ImVec4 BG_DARK = {0.06f, 0.06f, 0.08f, 1.0f};
    constexpr ImVec4 BG_MID = {0.08f, 0.08f, 0.1f, 1.0f};
    constexpr ImVec4 BG_PANEL = {0.1f, 0.1f, 0.12f, 1.0f};
    constexpr ImVec4 BG_CARD = {0.12f, 0.12f, 0.14f, 1.0f};
    constexpr ImVec4 LABEL = {0.6f, 0.6f, 0.65f, 1};
    constexpr ImVec4 HINT = {0.5f, 0.5f, 0.5f, 1};
    constexpr ImVec4 ADDR_GREEN = {0.5f, 1, 0.5f, 1};
    constexpr ImVec4 ADDR_CYAN = {0.5f, 0.85f, 0.85f, 1};
    constexpr ImVec4 VAL_YELLOW = {1, 1, 0.6f, 1};
    constexpr ImVec4 WARN = {1, 0.8f, 0.2f, 1};
    constexpr ImVec4 ERR = {1, 0.4f, 0.4f, 1};
    constexpr ImVec4 OK = {0.4f, 0.9f, 0.4f, 1};
    constexpr ImVec4 TITLE = {0.9f, 0.7f, 0.4f, 1};
    constexpr ImVec4 LOCKED = {0.2f, 0.08f, 0.08f, 1};
    constexpr ImVec4 INFO_CYAN = {0.4f, 0.8f, 1.0f, 1};

    // 按钮颜色
    constexpr ImVec4 BTN_GREEN = {0.12f, 0.38f, 0.18f, 1.0f};
    constexpr ImVec4 BTN_BLUE = {0.12f, 0.25f, 0.4f, 1.0f};
    constexpr ImVec4 BTN_RED = {0.38f, 0.15f, 0.15f, 1.0f};
    constexpr ImVec4 BTN_TEAL = {0.15f, 0.28f, 0.4f, 1.0f};
    constexpr ImVec4 BTN_PURPLE = {0.35f, 0.25f, 0.45f, 1.0f};
    constexpr ImVec4 BTN_ORANGE = {0.35f, 0.25f, 0.15f, 1.0f};
    constexpr ImVec4 BTN_MINIMIZE = {0.15f, 0.4f, 0.6f, 1.0f};
    constexpr ImVec4 BTN_EXIT = {0.65f, 0.15f, 0.15f, 1.0f};
    constexpr ImVec4 BTN_LOCK = {0.15f, 0.28f, 0.4f, 1};
    constexpr ImVec4 BTN_UNLOCK = {0.4f, 0.15f, 0.15f, 1};
    constexpr ImVec4 BTN_COPY = {0.25f, 0.35f, 0.5f, 1};
    constexpr ImVec4 BTN_DEL = {0.4f, 0.1f, 0.1f, 1};
    constexpr ImVec4 BTN_ACTIVE = {0.2f, 0.32f, 0.5f, 1};
    constexpr ImVec4 BTN_INACTIVE = {0.12f, 0.12f, 0.15f, 1};
} // namespace Colors

// ============================================================================
// 主界面
// ============================================================================
class MainUI
{
    // UI 构建器的样式：布局缩放、边距和透明度。
    float scale = 2.0f, margin = 40.0f, opacity = 1.0f;
    float S(float value) const noexcept { return value * scale; }

    bool running_ = true;
    int lastStatus_ = 0;
    static constexpr int perPage = 100;

    struct ScanParams
    {
        Types::DataType dataType = Types::DataType::I32;
        Types::FuzzyMode fuzzyMode = Types::FuzzyMode::Unknown;
        int floatRounding = 0;
        bool unaligned = false;
        int page = 0;
        std::string lastStringPattern;
    } scanParams_;

    struct SavedParams
    {
        Types::DataType dataType = Types::DataType::I32;
        int page = 0;
    } savedParams_;

    struct PtrParams
    {
        int depth = 3;
        bool useManual = false, useArray = false;
    } ptrParams_;

    struct SigParams
    {
        int range = 20, lastChanged = -1, lastTotal = 0, lastScanCount = -1;
    } sigParams_;

    static constexpr const char *bpTypeLabels[] = {"读取", "写入", "读写", "执行"};
    static constexpr const char *bpScopeLabels[] = {"主线程", "子线程", "全部"};
    static constexpr const char *bpLenLabels[] = {"1字节", "2字节", "3字节", "4字节", "5字节", "6字节", "7字节", "8字节"};
    struct BpParams
    {
        struct PointRow
        {
            char addr[32] = {};
            int type = 3;
            int scope = 2;
            int len = 4;
        };

        bool pointExpandState[BP_CONFIG_MAX] = {};
        bool recordsExpandState[BP_CONFIG_MAX] = {};
        bool recordExpandState[BP_CONFIG_MAX * BP_RECORD_MAX] = {};
        PointRow points[16];
        int configPointCount = 1;

        int editingRecordIdx = -1;
        char regEditBuf[64] = {};
        int editingField = -1;
    } bpParams_;

    struct ModuleRow
    {
        std::string name;
        short index;
        uint8_t prot;
        uint64_t start;
        uint64_t end;
    };
    std::vector<ModuleRow> moduleRows_;

    struct MonitorParams
    {
        int lastStatus = 0;
        bool hasResult = false;
        std::string log;
        std::chrono::steady_clock::time_point nextLogRefresh{};
    } syscallParams_, cntvctParams_;

    struct EnvParams
    {
        bool hasResult = false;
        bool success = false;
        int pid = 0;
        Driver::env_params info{};
    } envParams_;

    std::vector<std::string> offsetLabels_;
    int selectedOffsetIdx_ = 1;

    struct Buf
    {
        char pid[32] = {}, value[64] = {}, savedAddr[32] = {}, base[32] = {};
        char modify[64] = {}, memOffset[32] = {}, savedOffset[32] = {}, moduleSearch[64] = {}, envThread[16] = {};
        char savedNote[257] = {};
        char ptrTarget[32] = {}, arrayBase[32] = {}, arrayCount[16] = "100", filterModule[64] = {};
        char sigScanAddr[32] = {}, sigVerifyAddr[32] = {};
        char viewAddr[32] = {};
    } buf_;

    struct State
    {
        int tab = 0, resultScrollIdx = 0, savedScrollIdx = 0;
        uintptr_t modifyAddr = 0, noteAddr = 0;
        bool showModify = false, showSavedNote = false, floating = false, dragging = false, dragMoved = false;
        ImVec2 floatPos = {50, 200}, dragOffset = {}, dragStartMouse = {};
        bool showType = false, showSavedType = false, showMode = false, showDepth = false, showOffset = false, showScale = false, showFormat = false;
        bool showBpType = false, showBpScope = false, showBpLen = false;
        int bpPopupPoint = -1;
    } state_;

    static std::optional<uintptr_t> ParseHexAddress(const char *buf)
    {
        const auto addr = MemUtils::ParseUInt64(buf, 16);
        return addr.has_value() && *addr ? std::optional<uintptr_t>(static_cast<uintptr_t>(*addr)) : std::nullopt;
    }

    void drawRegisterEditButton(const char *buttonId, int recordIndex, int regIndex, std::string_view name, std::string_view hexValue, ImVec2 size)
    {
        if (UI::Btn(buttonId, size, {0.4f, 0.3f, 0.15f, 1}))
        {
            bpParams_.editingRecordIdx = recordIndex;
            bpParams_.editingField = regIndex;
            std::snprintf(bpParams_.regEditBuf, sizeof(bpParams_.regEditBuf), "%.*s", static_cast<int>(std::min(hexValue.size(), sizeof(bpParams_.regEditBuf) - 1)), hexValue.data());
            gFloatingKeyboard->Open(bpParams_.regEditBuf, std::format("修改 {} (Hex)", name).c_str());
        }
        if (bpParams_.editingRecordIdx != recordIndex || bpParams_.editingField != regIndex) return;
        const auto result = gFloatingKeyboard->ConsumeResult(bpParams_.regEditBuf);
        if (result == FloatingKeyboard::Result::None) return;

        if (result == FloatingKeyboard::Result::Accepted)
        {
            if (const auto value = MemUtils::ParseUInt128(bpParams_.regEditBuf, 16))
            {
                for (auto &point : dr->GetBreakpointInfo().points)
                {
                    const int recordCount = std::clamp(point.record_count, 0, BP_RECORD_MAX);
                    if (recordIndex < recordCount)
                    {
                        lastStatus_ = MemUtils::HwbpWriteRegisterValue(point.records[recordIndex], regIndex, *value);
                        break;
                    }
                    recordIndex -= recordCount;
                }
            }
        }
        bpParams_.editingRecordIdx = -1;
        bpParams_.editingField = -1;
        bpParams_.regEditBuf[0] = 0;
    }

    // ---- 扫描逻辑 ----
    int startScan(std::string_view valueStr, bool isFirst)
    {
        int status = 0;
        scanParams_.page = 0;
        auto type = scanParams_.dataType;
        auto mode = scanParams_.fuzzyMode;
        const size_t alignment = scanParams_.unaligned ? 1 : 0;
        const auto pid = dr->GetGlobalPid();
        const auto lockedType = gMemScanner->dataType();
        const bool stringBaseline = gMemScanner->isStringScan();
        const bool hasBaseline = lockedType.has_value() || stringBaseline;

        if (isFirst && hasBaseline) return status = -EALREADY;
        if (!isFirst)
        {
            if (!hasBaseline) return status = -EINVAL;
            if (lockedType) type = *lockedType;
        }

        if (mode == Types::FuzzyMode::Pointer)
        {
            auto parsed = MemUtils::ParseUInt64(valueStr, 16);
            if (!parsed) return status = -EINVAL;
            status = gMemScanner->startAsync<int64_t>(pid, static_cast<int64_t>(MemUtils::Normalize(static_cast<uintptr_t>(*parsed))), Types::DataType::I64, mode, isFirst, int64_t{}, {}, MemUtils::FloatRounding::Rounded, alignment);
        }
        else if (mode == Types::FuzzyMode::String)
        {
            if (valueStr.empty()) return status = -EINVAL;
            status = gMemScanner->startStringAsync(pid, std::string(valueStr), isFirst);
            if (status != 0) return status;
            scanParams_.lastStringPattern = valueStr;
        }
        else
        {
            status = MemUtils::DispatchType(type,
                                           [&]<typename T>() -> int
                                           {
                                               int status = 0;
                                               if (mode == Types::FuzzyMode::Unknown || (mode >= Types::FuzzyMode::Increased && mode <= Types::FuzzyMode::Unchanged))
                                               {
                                                   status = gMemScanner->startAsync<T>(pid, T{}, type, mode, isFirst, T{}, {}, MemUtils::FloatRounding::Rounded, alignment);
                                               }
                                               else if (mode == Types::FuzzyMode::Range)
                                               {
                                                   const auto pos = valueStr.find('~');
                                                   if (pos == std::string_view::npos) return status = -EINVAL;
                                                   const auto minValue = MemUtils::ParseScanValue<T>(valueStr.substr(0, pos));
                                                   const auto maxValue = MemUtils::ParseScanValue<T>(valueStr.substr(pos + 1));
                                                   if (!minValue || !maxValue) return status = -EINVAL;
                                                   status = gMemScanner->startAsync<T>(pid, *minValue, type, mode, isFirst, *maxValue, {}, MemUtils::FloatRounding::Rounded, alignment);
                                               }
                                               else
                                               {
                                                   const auto value = MemUtils::ParseScanValue<T>(valueStr);
                                                   if (!value) return status = -EINVAL;
                                                   status = gMemScanner->startAsync<T>(pid, *value, type, mode, isFirst, T{}, valueStr, static_cast<MemUtils::FloatRounding>(scanParams_.floatRounding), alignment);
                                               }
                                               return status;
                                           });
        }
        return status;
    }

    template <typename F> void drawListPopup(const char *title, bool *show, float sx, float sy, float sw, float sh, float pw, float ph, F &&drawItems)
    {
        ImGui::SetNextWindowPos({sx + (sw - pw) / 2, sy + (sh - ph) / 2});
        ImGui::SetNextWindowSize({pw, ph});
        ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.1f, 0.1f, 0.13f, 0.98f});
        if (ImGui::Begin(title, show, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) drawItems(ImGui::GetContentRegionAvail().x);
        ImGui::End();
        ImGui::PopStyleColor();
    }

    void drawMonitorTab(const char *title, const char *logTag, const char *hint, const char *childId, const char *emptyText, ApplicationRuntime::Monitor monitor, MonitorParams &params)
    {
        const float w = ImGui::GetContentRegionAvail().x;
        const int targetPid = dr->GetGlobalPid();
        const int monitoredPid = gApplicationRuntime->monitoredPid(monitor);
        const bool active = monitoredPid > 0;
        const auto now = std::chrono::steady_clock::now();
        if (now >= params.nextLogRefresh)
        {
            params.log = SyscallLog::ReadDmesg(logTag);
            params.nextLogRefresh = now + std::chrono::seconds(1);
        }

        ImGui::TextColored(Colors::TITLE, "%s", title);
        ImGui::Dummy({0, S(8)});
        UI::LabelValue(Colors::LABEL, "当前目标 PID: ", targetPid > 0 ? Colors::ADDR_GREEN : Colors::ERR, "%d", targetPid);
        UI::LabelValue(Colors::LABEL, "监听状态: ", active ? Colors::OK : Colors::HINT, "%s", active ? "已监听" : "未监听");
        if (active) UI::LabelValue(Colors::LABEL, "监听 PID: ", Colors::ADDR_CYAN, "%d", monitoredPid);

        ImGui::Dummy({0, S(12)});
        ImGui::BeginDisabled(targetPid <= 0 || active);
        if (UI::Btn("开始监听", {w, S(54)}, Colors::BTN_GREEN))
        {
            params.lastStatus = gApplicationRuntime->startMonitor(monitor);
            params.hasResult = true;
        }
        ImGui::EndDisabled();

        ImGui::Dummy({0, S(8)});
        ImGui::BeginDisabled(!active);
        if (UI::Btn("取消监听", {w, S(54)}, Colors::BTN_RED))
        {
            params.lastStatus = gApplicationRuntime->stopMonitor(monitor);
            params.hasResult = true;
        }
        ImGui::EndDisabled();

        ImGui::Dummy({0, S(12)});
        if (params.hasResult)
        {
            if (params.lastStatus == 0) ImGui::TextColored(Colors::OK, "请求执行成功");
            else ImGui::TextColored(Colors::ERR, "请求执行失败，状态: %d", params.lastStatus);
        }

        ImGui::Dummy({0, S(8)});
        if (UI::Btn("刷新日志", {w, S(42)}, Colors::BTN_TEAL)) params.nextLogRefresh = {};
        ImGui::TextColored(Colors::HINT, "%s", hint);
        if (ImGui::BeginChild(childId, {w, 0}, true, ImGuiWindowFlags_HorizontalScrollbar)) ImGui::TextUnformatted(params.log.empty() ? emptyText : params.log.c_str());
        ImGui::EndChild();
    }

public:
    MainUI()
    {
        for (int i = 500; i <= 100000; i += 500)
        {
            offsetLabels_.push_back(std::to_string(i));
        }
        if (int pid = dr->GetGlobalPid(); pid > 0) snprintf(buf_.pid, sizeof(buf_.pid), "%d", pid);
    }

    bool isRunning() const noexcept { return running_; }

    bool draw(float width, float height)
    {
        auto &s = ImGui::GetStyle();
        s.Alpha = opacity;
        s.FramePadding = {S(10), S(10)};
        s.ItemSpacing = {S(6), S(6)};
        s.TouchExtraPadding = {8, 8};
        s.ScrollbarSize = S(22);
        s.GrabMinSize = S(18);
        s.WindowRounding = S(8);
        s.ChildRounding = S(6);
        s.FrameRounding = S(5);
        s.WindowPadding = {S(8), S(8)};
        s.WindowBorderSize = 0;
        if (state_.floating)
        {
            // ---- 悬浮按钮 ----
            float sz = S(65), m = margin;
            state_.floatPos.x = std::clamp(state_.floatPos.x, m, std::max(m, width - sz - m));
            state_.floatPos.y = std::clamp(state_.floatPos.y, m, std::max(m, height - sz - m));
            ImGui::SetNextWindowPos(state_.floatPos);
            ImGui::SetNextWindowSize({sz, sz});
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, sz / 2);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
            ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.2f, 0.5f, 0.8f, 0.9f});
            if (ImGui::Begin("##Float", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove))
            {
                auto &io = ImGui::GetIO();
                bool releasedAfterDrag = false;
                if (ImGui::IsWindowHovered() && io.MouseDown[0] && !state_.dragging)
                {
                    state_.dragging = true;
                    state_.dragMoved = false;
                    state_.dragStartMouse = io.MousePos;
                    state_.dragOffset = {io.MousePos.x - ImGui::GetWindowPos().x, io.MousePos.y - ImGui::GetWindowPos().y};
                }
                if (state_.dragging)
                {
                    if (io.MouseDown[0])
                    {
                        const float dx = io.MousePos.x - state_.dragStartMouse.x;
                        const float dy = io.MousePos.y - state_.dragStartMouse.y;
                        const float threshold = S(6);
                        if (dx * dx + dy * dy > threshold * threshold) state_.dragMoved = true;

                        state_.floatPos = {io.MousePos.x - state_.dragOffset.x, io.MousePos.y - state_.dragOffset.y};
                    }
                    else
                    {
                        releasedAfterDrag = state_.dragMoved;
                        state_.dragging = false;
                    }
                }
                if (ImGui::Button("M", {sz, sz}) && !state_.dragging && !releasedAfterDrag) state_.floating = false;
            }
            ImGui::End();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);
        }
        else
        {
            const float w = width - 2 * margin, h = height - 2 * margin;
            ImGui::SetNextWindowPos({margin, margin});
            ImGui::SetNextWindowSize({w, h});
            ImGui::PushStyleColor(ImGuiCol_WindowBg, Colors::BG_DARK);
            if (ImGui::Begin("##Main", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove))
            {
                const float contentWidth = ImGui::GetContentRegionAvail().x;
                {
                    // ---- 顶栏 ----
                    UI::ColorChild(
                        "Top", {contentWidth, S(55)}, Colors::BG_PANEL,
                        [&]
                        {
                            float bh = S(55) - S(12);
                            if (UI::Btn("收起", {S(55), bh}, Colors::BTN_MINIMIZE)) state_.floating = true;
                            ImGui::SameLine();
                            ImGui::SetCursorPosX((contentWidth - ImGui::CalcTextSize("内存扫描").x) / 2);
                            ImGui::SetCursorPosY((S(55) - ImGui::GetTextLineHeight()) / 2);
                            ImGui::Text("内存扫描");
                            ImGui::SameLine(contentWidth - (S(50) + S(85) + S(50) + S(18)));
                            ImGui::SetCursorPosY(S(6));
                            char sc[16];
                            snprintf(sc, sizeof(sc), "%.0f%%", scale * 100);
                            if (ImGui::Button(sc, {S(50), bh})) state_.showScale = !state_.showScale;
                            ImGui::SameLine();
                            if (gFloatingKeyboard->InputButton(buf_.pid, "PID", {S(85), bh}))
                            {
                                const auto pid = MemUtils::ParseScanValue<int>(buf_.pid);
                                lastStatus_ = pid ? gApplicationRuntime->selectTarget(*pid) : -EINVAL;
                                if (lastStatus_ == 0)
                                {
                                    moduleRows_.clear();
                                    envParams_ = {};
                                    syscallParams_.log.clear();
                                    syscallParams_.hasResult = false;
                                    cntvctParams_.log.clear();
                                    cntvctParams_.hasResult = false;
                                }
                                std::snprintf(buf_.pid, sizeof(buf_.pid), "%d", dr->GetGlobalPid());
                            }
                            ImGui::SameLine();
                            if (UI::Btn("退出", {S(50), bh}, Colors::BTN_EXIT)) running_ = false;
                        },
                        ImGuiWindowFlags_NoScrollbar);
                }
                ImGui::Dummy({0, S(4)});
                UI::ColorChild("Content", {contentWidth, ImGui::GetContentRegionAvail().y - S(64)}, Colors::BG_MID,
                               [&]
                               {
                                   if (lastStatus_ != 0) ImGui::TextColored(Colors::ERR, "操作失败，状态: %d", lastStatus_);
                                   switch (state_.tab)
                                   {
                                   case 0:
                                       {
                                           float w = ImGui::GetContentRegionAvail().x;
                                           const bool scanning = gMemScanner->isScanning();
                                           const auto lockedType = gMemScanner->dataType();
                                           const bool stringBaseline = gMemScanner->isStringScan();
                                           const bool hasBaseline = lockedType.has_value() || stringBaseline;
                                           const bool typeLocked = lockedType.has_value();
                                           if (lockedType) scanParams_.dataType = *lockedType;
                                           if (stringBaseline)
                                           {
                                               scanParams_.fuzzyMode = Types::FuzzyMode::String;
                                           }
                                           else if (lockedType && (scanParams_.fuzzyMode == Types::FuzzyMode::Unknown || scanParams_.fuzzyMode == Types::FuzzyMode::String || (scanParams_.fuzzyMode == Types::FuzzyMode::Pointer && *lockedType != Types::DataType::I64)))
                                           {
                                               scanParams_.fuzzyMode = Types::FuzzyMode::Equal;
                                           }
                                           else if (!hasBaseline && (scanParams_.fuzzyMode >= Types::FuzzyMode::Increased && scanParams_.fuzzyMode <= Types::FuzzyMode::Unchanged))
                                           {
                                               scanParams_.fuzzyMode = Types::FuzzyMode::Equal;
                                           }

                                           const bool isPtrMode = scanParams_.fuzzyMode == Types::FuzzyMode::Pointer;
                                           const bool isStringMode = scanParams_.fuzzyMode == Types::FuzzyMode::String;
                                           if (typeLocked || scanning) state_.showType = false;

                                           // 数据类型
                                           ImGui::TextColored(Colors::LABEL, "数据类型:");
                                           if (isPtrMode || isStringMode)
                                           {
                                               ImGui::BeginDisabled();
                                               ImGui::Button(isPtrMode ? "Int64（指针模式）" : "字符串模式忽略类型", {w, S(45)});
                                               ImGui::EndDisabled();
                                           }
                                           else
                                           {
                                               ImGui::BeginDisabled(typeLocked || scanning);
                                               if (ImGui::Button(Types::Labels::TYPE[static_cast<int>(scanParams_.dataType)], {w, S(45)})) state_.showType = true;
                                               ImGui::EndDisabled();
                                           }

                                           ImGui::Dummy({0, S(6)});
                                           ImGui::TextColored(Colors::LABEL, "搜索模式:");
                                           ImGui::BeginDisabled(scanning || stringBaseline);
                                           if (ImGui::Button(Types::Labels::FUZZY[static_cast<int>(scanParams_.fuzzyMode)], {w, S(45)})) state_.showMode = true;
                                           ImGui::EndDisabled();

                                           if (!isStringMode)
                                           {
                                               ImGui::BeginDisabled(scanning || hasBaseline);
                                               ImGui::Checkbox("逐字节扫描（含未对齐地址）", &scanParams_.unaligned);
                                               ImGui::EndDisabled();
                                           }

                                           if (scanParams_.fuzzyMode == Types::FuzzyMode::Equal && (scanParams_.dataType == Types::DataType::Float || scanParams_.dataType == Types::DataType::Double))
                                           {
                                               ImGui::Dummy({0, S(6)});
                                               ImGui::TextColored(Colors::LABEL, "浮点匹配:");
                                               ImGui::BeginDisabled(scanning);
                                               ImGui::SetNextItemWidth(w);
                                               ImGui::Combo("##FloatRounding", &scanParams_.floatRounding, "四舍五入\0截断区间\0扩大范围\0精确数值\0");
                                               ImGui::EndDisabled();
                                           }

                                           ImGui::Dummy({0, S(6)});
                                           ImGui::TextColored(Colors::LABEL, isPtrMode ? "目标地址(Hex):" : "搜索数值:");
                                           gFloatingKeyboard->InputButton(buf_.value, isPtrMode ? "输入Hex地址..." : "点击输入...", {w, S(52)}, isPtrMode ? "目标地址(Hex)" : "数值");

                                           if (isPtrMode) ImGui::TextColored(Colors::INFO_CYAN, "输入16进制地址，搜索指向该地址的指针");
                                           else if (isStringMode) ImGui::TextColored(Colors::INFO_CYAN, "按原始字节匹配，区分大小写；再次扫描会在当前结果中继续过滤");
                                           else if (scanParams_.fuzzyMode == Types::FuzzyMode::Range) ImGui::TextColored(Colors::INFO_CYAN, "格式: 最小值~最大值  例: 0~45  -2~2  0.1~6.5");

                                           ImGui::Dummy({0, S(10)});
                                           ImGui::BeginDisabled(scanning);
                                           float bw = (w - S(12)) / 3;
                                           ImGui::BeginDisabled(hasBaseline);
                                           if (UI::Btn("首次扫描", {bw, S(52)}, Colors::BTN_GREEN)) lastStatus_ = startScan(buf_.value, true);
                                           ImGui::EndDisabled();
                                           ImGui::SameLine();
                                           ImGui::BeginDisabled(!hasBaseline);
                                           if (UI::Btn("再次扫描", {bw, S(52)}, Colors::BTN_BLUE)) lastStatus_ = startScan(buf_.value, false);
                                           ImGui::EndDisabled();
                                           ImGui::SameLine();
                                           if (UI::Btn("清空", {bw, S(52)}, Colors::BTN_RED))
                                           {
                                               gMemScanner->clear();
                                               scanParams_.page = 0;
                                               scanParams_.lastStringPattern.clear();
                                           }
                                           ImGui::EndDisabled();

                                           ImGui::Dummy({0, S(6)});
                                           if (scanning)
                                           {
                                               ImGui::TextColored(Colors::WARN, "扫描中...");
                                               ImGui::ProgressBar(gMemScanner->progress(), {w, S(18)});
                                           }
                                           else
                                           {
                                               const size_t resultCount = gMemScanner->count();
                                               resultCount ? ImGui::TextColored(Colors::OK, "找到 %zu 个", resultCount) : ImGui::TextColored(Colors::HINT, "暂无结果");
                                           }
                                           do
                                           {
                                               // ================================================================
                                               // 扫描页
                                               // ================================================================
                                               if (gMemScanner->isScanning()) break;

                                               const size_t total = gMemScanner->count();
                                               if (!total) break;

                                               const auto resultType = gMemScanner->dataType().value_or(scanParams_.dataType);
                                               const bool pointerMode = gMemScanner->scanMode() == Types::FuzzyMode::Pointer;
                                               const bool stringMode = gMemScanner->isStringScan();
                                               const int maxPage = static_cast<int>((total - 1) / perPage);
                                               scanParams_.page = std::clamp(scanParams_.page, 0, maxPage);
                                               const auto data = gMemScanner->getPage(scanParams_.page * perPage, perPage);

                                               ImGui::Dummy({0, S(6)});
                                               ImGui::Separator();
                                               ImGui::TextColored(Colors::TITLE, "扫描结果");

                                               const float pageWidth = S(65);
                                               const float pageHeight = S(38);
                                               ImGui::BeginDisabled(scanParams_.page <= 0);
                                               if (ImGui::Button("上页##scan", {pageWidth, pageHeight}))
                                               {
                                                   --scanParams_.page;
                                                   state_.resultScrollIdx = 0;
                                               }
                                               ImGui::EndDisabled();
                                               ImGui::SameLine();
                                               ImGui::Text("%d/%d  共 %zu", scanParams_.page + 1, maxPage + 1, total);
                                               ImGui::SameLine(w - pageWidth);
                                               ImGui::BeginDisabled(scanParams_.page >= maxPage);
                                               if (ImGui::Button("下页##scan", {pageWidth, pageHeight}))
                                               {
                                                   ++scanParams_.page;
                                                   state_.resultScrollIdx = 0;
                                               }
                                               ImGui::EndDisabled();

                                               const float listHeight = ImGui::GetContentRegionAvail().y;
                                               const float contentWidth = w - S(56);
                                               const int maxIndex = std::max(0, static_cast<int>(data.size()) - static_cast<int>(listHeight / S(76)));
                                               state_.resultScrollIdx = std::clamp(state_.resultScrollIdx, 0, maxIndex);
                                               const size_t previewLength = std::clamp(scanParams_.lastStringPattern.size(), size_t(16), size_t(64));
                                               if (ImGui::BeginChild("ScanResultList", {contentWidth, listHeight}, false, ImGuiWindowFlags_NoScrollbar))
                                               {
                                                   const int endIndex = state_.resultScrollIdx + static_cast<int>(listHeight / S(76)) + 1;
                                                   for (int index = state_.resultScrollIdx; index < static_cast<int>(data.size()) && index < endIndex; ++index)
                                                   {
                                                       const uintptr_t address = data[index];
                                                       const bool saved = gSavedAddresses->contains(address);
                                                       ImGui::PushID(reinterpret_cast<void *>(address));
                                                       UI::ColorChild(
                                                           "ScanResult", {contentWidth - S(10), S(68)}, Colors::BG_PANEL,
                                                           [&]
                                                           {
                                                               const float cardWidth = ImGui::GetContentRegionAvail().x;
                                                               UI::LabelValue(Colors::LABEL, "地址: ", Colors::ADDR_GREEN, "%lX", address);
                                                               ImGui::SameLine(cardWidth * 0.42f);
                                                               if (pointerMode) UI::LabelValue(Colors::LABEL, "指向: ", Colors::VAL_YELLOW, "%s", MemUtils::ReadAsPointerString(address).c_str());
                                                               else if (stringMode) UI::LabelValue(Colors::LABEL, "文本: ", Colors::VAL_YELLOW, "%s", MemUtils::ReadAsText(address, previewLength).c_str());
                                                               else UI::LabelValue(Colors::LABEL, "数值: ", Colors::VAL_YELLOW, "%s", MemUtils::ReadAsString(address, resultType).c_str());

                                                               const float buttonWidth = (cardWidth - S(6)) / 2;
                                                               ImGui::BeginDisabled(saved);
                                                               if (UI::Btn(saved ? "已保存" : "保存", {buttonWidth, S(30)}, Colors::BTN_GREEN))
                                                               {
                                                                   if (gMemScanner->isStringScan())
                                                                   {
                                                                       lastStatus_ = gSavedAddresses->add(address, Types::DataType::I8, Types::SavedValueKind::Text, std::clamp(scanParams_.lastStringPattern.size(), size_t(1), size_t(256)));
                                                                   }
                                                                   else if (const auto type = gMemScanner->dataType())
                                                                   {
                                                                       lastStatus_ = gSavedAddresses->add(address, *type, gMemScanner->scanMode() == Types::FuzzyMode::Pointer ? Types::SavedValueKind::Pointer : Types::SavedValueKind::Numeric);
                                                                   }
                                                               }
                                                               ImGui::EndDisabled();
                                                               ImGui::SameLine();
                                                               if (UI::Btn("复制地址", {buttonWidth, S(30)}, Colors::BTN_COPY)) ImGui::SetClipboardText(std::format("{:X}", address).c_str());
                                                           },
                                                           ImGuiWindowFlags_NoScrollbar);
                                                       ImGui::PopID();
                                                       ImGui::Dummy({0, S(3)});
                                                   }
                                               }
                                               ImGui::EndChild();
                                               ImGui::SameLine();
                                               UI::ArrowScroll("ScanResultArrows", S(50), listHeight, state_.resultScrollIdx, 0, maxIndex);
                                           } while (false);
                                       }
                                       break;
                                   case 1:
                                       do
                                       {
                                           // ================================================================
                                           // 保存页
                                           // ================================================================
                                           const float width = ImGui::GetContentRegionAvail().x;
                                           const float buttonHeight = S(40);

                                           gFloatingKeyboard->InputButton(buf_.savedAddr, "手工添加 Hex 地址...", {width - S(210), buttonHeight}, "Hex地址");
                                           ImGui::SameLine();
                                           if (UI::Btn(Types::Labels::TYPE[static_cast<size_t>(savedParams_.dataType)], {S(90), buttonHeight}, Colors::BTN_INACTIVE)) state_.showSavedType = true;
                                           ImGui::SameLine();
                                           if (UI::Btn("添加", {S(108), buttonHeight}, Colors::BTN_GREEN))
                                           {
                                               if (const auto address = ParseHexAddress(buf_.savedAddr))
                                               {
                                                   lastStatus_ = gSavedAddresses->add(*address, savedParams_.dataType);
                                                   buf_.savedAddr[0] = 0;
                                               }
                                           }

                                           auto items = gSavedAddresses->snapshotStates();
                                           const size_t total = items.size();
                                           if (!total)
                                           {
                                               ImGui::Dummy({0, S(12)});
                                               ImGui::TextColored(Colors::HINT, "暂无保存地址，可从扫描页保存或手工添加");
                                               break;
                                           }

                                           const int maxPage = static_cast<int>((total - 1) / perPage);
                                           savedParams_.page = std::clamp(savedParams_.page, 0, maxPage);
                                           const size_t pageStart = static_cast<size_t>(savedParams_.page) * perPage;
                                           const std::span<const SavedAddressManager::ItemState> page(items.data() + pageStart, std::min<size_t>(perPage, total - pageStart));

                                           ImGui::Dummy({0, S(4)});
                                           const float pageWidth = S(65);
                                           ImGui::BeginDisabled(savedParams_.page <= 0);
                                           if (ImGui::Button("上页##saved", {pageWidth, buttonHeight}))
                                           {
                                               --savedParams_.page;
                                               state_.savedScrollIdx = 0;
                                           }
                                           ImGui::EndDisabled();
                                           ImGui::SameLine();
                                           ImGui::Text("%d/%d  已保存 %zu", savedParams_.page + 1, maxPage + 1, total);
                                           ImGui::SameLine(width - pageWidth);
                                           ImGui::BeginDisabled(savedParams_.page >= maxPage);
                                           if (ImGui::Button("下页##saved", {pageWidth, buttonHeight}))
                                           {
                                               ++savedParams_.page;
                                               state_.savedScrollIdx = 0;
                                           }
                                           ImGui::EndDisabled();

                                           const bool anyLocked = std::ranges::any_of(page, [](const auto &state) { return state.locked; });
                                            if (UI::Btn(anyLocked ? "解锁本页" : "锁定本页", {S(90), S(36)}, anyLocked ? Colors::BTN_UNLOCK : Colors::BTN_LOCK)) lastStatus_ = gSavedAddresses->togglePage(page, !anyLocked);
                                           ImGui::SameLine();
                                           if (gFloatingKeyboard->ActionButton("偏移全部", {S(90), S(36)}, buf_.savedOffset, "偏移量(Hex,可负)", Colors::BTN_ORANGE) && buf_.savedOffset[0])
                                           {
                                                if (const auto offset = MemUtils::ParseHexOffset(buf_.savedOffset)) lastStatus_ = gSavedAddresses->applyOffset(offset->offset, offset->negative);
                                               buf_.savedOffset[0] = 0;
                                               savedParams_.page = 0;
                                               state_.savedScrollIdx = 0;
                                               break;
                                           }
                                           ImGui::SameLine();
                                           if (UI::Btn("清空保存", {S(90), S(36)}, Colors::BTN_RED))
                                           {
                                               gSavedAddresses->clear();
                                               savedParams_.page = 0;
                                               state_.savedScrollIdx = 0;
                                               break;
                                           }

                                           ImGui::Separator();
                                           const float listHeight = ImGui::GetContentRegionAvail().y;
                                           const float contentWidth = width - S(56);
                                           const int maxIndex = std::max(0, static_cast<int>(page.size()) - static_cast<int>(listHeight / S(122)));
                                           state_.savedScrollIdx = std::clamp(state_.savedScrollIdx, 0, maxIndex);
                                           if (ImGui::BeginChild("SavedList", {contentWidth, listHeight}, false, ImGuiWindowFlags_NoScrollbar))
                                           {
                                               const int endIndex = state_.savedScrollIdx + static_cast<int>(listHeight / S(122)) + 1;
                                               for (int index = state_.savedScrollIdx; index < static_cast<int>(page.size()) && index < endIndex; ++index)
                                               {
                                                   const auto &state = page[index];
                                                   ImGui::PushID(reinterpret_cast<void *>(state.item.address));
                                                   UI::ColorChild(
                                                       "SavedItem", {contentWidth - S(10), S(114)}, state.locked ? Colors::LOCKED : Colors::BG_PANEL,
                                                       [&]
                                                       {
                                                           const float cardWidth = ImGui::GetContentRegionAvail().x;
                                                           UI::LabelValue(Colors::LABEL, "地址: ", state.locked ? ImVec4{1, 0.5f, 0.5f, 1} : Colors::ADDR_GREEN, "%lX", state.item.address);
                                                           ImGui::SameLine(cardWidth * 0.42f);
                                                           UI::LabelValue(Colors::LABEL, state.item.kind == Types::SavedValueKind::Pointer ? "指针" : state.item.kind == Types::SavedValueKind::Text ? "文本" : Types::Labels::TYPE[static_cast<size_t>(state.item.type)], Colors::VAL_YELLOW, ": %s", state.value.c_str());

                                                           ImGui::TextColored(Colors::HINT, "备注: %s", state.item.note.empty() ? "--" : state.item.note.c_str());
                                                           if (!state.item.note.empty() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", state.item.note.c_str());

                                                           const float actionWidth = (cardWidth - S(20)) / 5;
                                                           if (UI::Btn("修改", {actionWidth, S(36)}))
                                                           {
                                                               state_.modifyAddr = state.item.address;
                                                               std::snprintf(buf_.modify, sizeof(buf_.modify), "%s", state.value.c_str());
                                                               state_.showModify = true;
                                                               gFloatingKeyboard->Open(buf_.modify, state.item.kind == Types::SavedValueKind::Pointer ? "新地址(Hex)" : state.item.kind == Types::SavedValueKind::Text ? "新字符串" : "新数值");
                                                           }
                                                           ImGui::SameLine();
                                                           if (UI::Btn("备注", {actionWidth, S(36)}))
                                                           {
                                                               state_.noteAddr = state.item.address;
                                                               std::snprintf(buf_.savedNote, sizeof(buf_.savedNote), "%s", state.item.note.c_str());
                                                               state_.showSavedNote = true;
                                                               gFloatingKeyboard->Open(buf_.savedNote, "文字备注");
                                                           }
                                                           ImGui::SameLine();
                                                            if (UI::Btn(state.locked ? "解锁" : "锁定", {actionWidth, S(36)}, state.locked ? Colors::BTN_UNLOCK : Colors::BTN_LOCK)) lastStatus_ = gSavedAddresses->setLocked(state.item.address, !state.locked);
                                                           ImGui::SameLine();
                                                           if (UI::Btn("复制", {actionWidth, S(36)}, Colors::BTN_COPY)) ImGui::SetClipboardText(std::format("{:X}", state.item.address).c_str());
                                                           ImGui::SameLine();
                                                           if (UI::Btn("删除", {actionWidth, S(36)}, Colors::BTN_DEL)) lastStatus_ = gSavedAddresses->remove(state.item.address);
                                                       },
                                                       ImGuiWindowFlags_NoScrollbar);
                                                   ImGui::PopID();
                                                   ImGui::Dummy({0, S(4)});
                                               }
                                           }
                                           ImGui::EndChild();
                                           ImGui::SameLine();
                                           UI::ArrowScroll("SavedArrows", S(50), listHeight, state_.savedScrollIdx, 0, maxIndex);
                                       } while (false);
                                       break;
                                   case 2:
                                       do
                                       {
                                           // ================================================================
                                           // 内存浏览页
                                           // ================================================================
                                           if (const int status = gMemViewer->pollDisasm(); status != 0) lastStatus_ = status;

                                           float w = ImGui::GetContentRegionAvail().x, bh = S(42);
                                           float goW = S(55), ofsW = S(55), fmtW = S(110), refW = S(55);

                                           // 工具栏：一行五按钮
                                           gFloatingKeyboard->InputButton(buf_.viewAddr, "输入Hex地址...", {w - goW - ofsW - fmtW - refW - S(24), bh}, "Hex地址");
                                           ImGui::SameLine();
                                           if (UI::Btn("跳转", {goW, bh}, {0.15f, 0.4f, 0.25f, 1}))
                                           {
                                               if (auto addr = ParseHexAddress(buf_.viewAddr)) lastStatus_ = gMemViewer->open(*addr);
                                           }
                                           ImGui::SameLine();
                                           if (gFloatingKeyboard->ActionButton("偏移", {ofsW, bh}, buf_.memOffset, "偏移量(Hex,可负)", Colors::BTN_ORANGE) && buf_.memOffset[0])
                                           {
                                               lastStatus_ = gMemViewer->applyOffset(buf_.memOffset);
                                               buf_.memOffset[0] = 0;
                                           }
                                           ImGui::SameLine();
                                           if (UI::Btn(Types::Labels::VIEW_FORMAT[static_cast<size_t>(gMemViewer->format())], {fmtW, bh}, {0.18f, 0.25f, 0.35f, 1})) state_.showFormat = true;
                                           ImGui::SameLine();
                                           if (UI::Btn("刷新", {refW, bh}, Colors::BTN_TEAL)) lastStatus_ = gMemViewer->refresh();

                                           // 基址信息
                                           ImGui::Dummy({0, S(2)});
                                           if (gMemViewer->base())
                                           {
                                               UI::LabelValue(Colors::ADDR_CYAN, "基址: ", Colors::ADDR_GREEN, "%lX", gMemViewer->base());
                                               if (!gMemViewer->readSuccess())
                                               {
                                                   ImGui::SameLine();
                                                   ImGui::TextColored(Colors::ERR, "[读取失败]");
                                               }
                                           }
                                           else
                                           {
                                               ImGui::TextColored(Colors::HINT, "输入地址后点击跳转开始浏览");
                                           }
                                           ImGui::Separator();
                                           if (!gMemViewer->base()) break;

                                           // 读取失败提示
                                           if (!gMemViewer->readSuccess())
                                           {
                                               ImGui::Dummy({0, S(20)});
                                               ImGui::PushStyleColor(ImGuiCol_Text, {1, 0.5f, 0.5f, 1});
                                               ImGui::TextWrapped("无法读取内存，请检查：\n\n1. PID 是否正确并已同步\n"
                                                                  "2. 目标地址是否有效\n3. 驱动是否正常工作\n4. 目标进程是否仍在运行");
                                               ImGui::PopStyleColor();
                                               ImGui::Dummy({0, S(10)});
                                               if (ImGui::Button("重试", {S(80), S(36)})) lastStatus_ = gMemViewer->refresh();
                                               break;
                                           }

                                           // 数据显示 + 箭头
                                           auto fmt = gMemViewer->format();
                                           int64_t byteOffset = fmt == Types::ViewFormat::Disasm ? 4 : (fmt == Types::ViewFormat::Hex ? 16 : Types::GetViewSize(fmt));
                                           float cH = ImGui::GetContentRegionAvail().y, aW = S(50);
                                           float cW = ImGui::GetContentRegionAvail().x - aW - S(6);
                                           float rH = ImGui::GetTextLineHeight() + (fmt == Types::ViewFormat::Disasm ? S(14) : fmt == Types::ViewFormat::Hex ? S(8) : S(12));
                                           int rows = (int)(cH / rH) + 2;

                                           // 表格仍引用 buffer/disasmCache；完成全部样式和子窗口后再跳转。
                                           uintptr_t navigation = 0;
                                           if (ImGui::BeginChild("MemContent", {cW, cH}, false, ImGuiWindowFlags_NoScrollbar))
                                           {
                                               if (fmt == Types::ViewFormat::Disasm)
                                               {
                                                   if (gMemViewer->disasmBusy()) ImGui::TextColored(Colors::HINT, "反汇编中...");
                                                   else do
                                                   {
                                                       const uintptr_t base = gMemViewer->base();
                                                       const auto &lines = gMemViewer->getDisasm();
                                                       if (lines.empty())
                                                       {
                                                           ImGui::TextColored(Colors::ERR, "无法反汇编 (无效地址或非代码段)");
                                                           break;
                                                       }
                                                       ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {S(4), S(4)});
                                                       if (ImGui::BeginTable("Disasm", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                                                       {
                                                           ImGui::TableSetupColumn("地址", ImGuiTableColumnFlags_WidthFixed, S(110));
                                                           ImGui::TableSetupColumn("字节码", ImGuiTableColumnFlags_WidthFixed, S(90));
                                                           ImGui::TableSetupColumn("指令", ImGuiTableColumnFlags_WidthFixed, S(60));
                                                           ImGui::TableSetupColumn("操作数", ImGuiTableColumnFlags_WidthStretch);
                                                           ImGui::TableSetupColumn("操作", ImGuiTableColumnFlags_WidthFixed, S(80));
                                                           ImGui::TableHeadersRow();
                                                           for (int i = 0; i < std::min((int)lines.size(), rows); ++i)
                                                           {
                                                               const auto &line = lines[i];
                                                               if (!line.valid) continue;
                                                               ImGui::TableNextRow();
                                                               ImGui::PushID((void *)line.address);
                                                               ImGui::TableSetColumnIndex(0);
                                                               ImGui::TextColored(line.address == base ? ImVec4{0.4f, 1, 0.4f, 1} : ImVec4{0.5f, 0.85f, 0.9f, 1}, "%llX", (unsigned long long)line.address);
                                                               ImGui::TableSetColumnIndex(1);
                                                               char bytes[48] = {};
                                                               for (size_t j = 0; j < line.size && j < 8; ++j)
                                                               {
                                                                   snprintf(bytes + j * 3, sizeof(bytes) - j * 3, "%02X ", line.bytes[j]);
                                                               }
                                                               ImGui::TextColored({0.6f, 0.6f, 0.6f, 1}, "%s", bytes);
                                                               ImGui::TableSetColumnIndex(2);
                                                               const std::string_view mnemonic(line.mnemonic);
                                                               const bool isJump = mnemonic.starts_with('B') || mnemonic.starts_with("CB") || mnemonic.starts_with("TB");
                                                               ImVec4 mnemonicColor{1, 1, 1, 1};
                                                               if (isJump || mnemonic == "RET") mnemonicColor = {0.8f, 0.5f, 1, 1};
                                                               else if (mnemonic.starts_with("LD") || mnemonic.starts_with("ST")) mnemonicColor = {0.5f, 0.7f, 1, 1};
                                                               else if (mnemonic.starts_with("ADD") || mnemonic.starts_with("SUB") || mnemonic.starts_with("MUL") || mnemonic.starts_with("DIV")) mnemonicColor = {0.5f, 1, 0.5f, 1};
                                                               else if (mnemonic.starts_with("CMP") || mnemonic.starts_with("TST")) mnemonicColor = {1, 1, 0.5f, 1};
                                                               else if (mnemonic.starts_with("MOV")) mnemonicColor = {0.5f, 1, 1, 1};
                                                               else if (mnemonic == "NOP") mnemonicColor = {0.5f, 0.5f, 0.5f, 1};
                                                               ImGui::TextColored(mnemonicColor, "%s", line.mnemonic);
                                                               ImGui::TableSetColumnIndex(3);
                                                               ImGui::TextColored({0.9f, 0.9f, 0.7f, 1}, "%s", line.op_str);
                                                               ImGui::TableSetColumnIndex(4);
                                                               if (isJump)
                                                               {
                                                                   if (const char *targetText = std::strstr(line.op_str, "0X"))
                                                                       if (const auto target = ParseHexAddress(targetText); target && UI::Btn("跳", {S(35), S(24)}, Colors::BTN_PURPLE)) navigation = *target;
                                                                   ImGui::SameLine();
                                                               }
                                                               if (UI::Btn("存", {S(35), S(24)}, {0.2f, 0.4f, 0.25f, 1})) lastStatus_ = gSavedAddresses->add(line.address, Types::DataType::I32);
                                                               ImGui::PopID();
                                                           }
                                                           ImGui::EndTable();
                                                       }
                                                       ImGui::PopStyleVar();
                                                   } while (false);
                                               }
                                               else if (fmt == Types::ViewFormat::Hex) do
                                               {
                                                   const uintptr_t base = gMemViewer->base();
                                                   const auto buffer = gMemViewer->buffer();
                                                   if (buffer.empty())
                                                   {
                                                       ImGui::TextColored(Colors::HINT, "无数据");
                                                       break;
                                                   }
                                                   ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {S(3), S(3)});
                                                   if (ImGui::BeginTable("Hex", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                                                   {
                                                       ImGui::TableSetupColumn("地址", ImGuiTableColumnFlags_WidthFixed, S(85));
                                                       for (int i = 0; i < 4; ++i)
                                                       {
                                                           char h[4];
                                                           snprintf(h, sizeof(h), "%X", i);
                                                           ImGui::TableSetupColumn(h, ImGuiTableColumnFlags_WidthFixed, S(24));
                                                       }
                                                       ImGui::TableSetupColumn("ASCII", ImGuiTableColumnFlags_WidthStretch);
                                                       ImGui::TableSetupColumn("存", ImGuiTableColumnFlags_WidthFixed, S(38));
                                                       ImGui::TableSetupColumn("跳", ImGuiTableColumnFlags_WidthFixed, S(38));
                                                       ImGui::TableHeadersRow();
                                                       for (int i = 0; i < rows; ++i)
                                                       {
                                                           size_t off = i * 4;
                                                           if (off >= buffer.size()) break;
                                                           uintptr_t rowAddr = base + off;
                                                           ImGui::TableNextRow();
                                                           ImGui::PushID((void *)rowAddr);
                                                           ImGui::TableSetColumnIndex(0);
                                                           ImGui::TextColored(i == 0 ? ImVec4{0.4f, 1, 0.4f, 1} : ImVec4{0.5f, 0.75f, 0.85f, 1}, "%lX", rowAddr);
                                                           char ascii[5] = "....";
                                                           for (int c = 0; c < 4; ++c)
                                                           {
                                                               ImGui::TableSetColumnIndex(c + 1);
                                                               if (off + c < buffer.size())
                                                               {
                                                                   uint8_t b = buffer[off + c];
                                                                   b == 0 ? ImGui::TextColored({0.4f, 0.4f, 0.4f, 1}, ".") : ImGui::Text("%02X", b);
                                                                   ascii[c] = (b >= 32 && b < 127) ? (char)b : '.';
                                                               }
                                                               else
                                                               {
                                                                   ImGui::TextColored({0.3f, 0.3f, 0.3f, 1}, "??");
                                                                   ascii[c] = ' ';
                                                               }
                                                           }
                                                           ImGui::TableSetColumnIndex(5);
                                                           ImGui::TextColored({0.65f, 0.65f, 0.5f, 1}, "%s", ascii);
                                                           ImGui::TableSetColumnIndex(6);
                                                           if (UI::Btn("存", {S(32), S(22)}, {0.2f, 0.4f, 0.25f, 1})) lastStatus_ = gSavedAddresses->add(rowAddr, Types::DataType::I32);
                                                           ImGui::TableSetColumnIndex(7);
                                                           uintptr_t ptrVal = 0;
                                                           bool canJump = false;
                                                           const size_t avail = buffer.size() - off;
                                                           if (avail >= 8)
                                                           {
                                                               uint64_t raw = 0;
                                                               memcpy(&raw, buffer.data() + off, 8);
                                                               ptrVal = MemUtils::Normalize(raw);
                                                               canJump = MemUtils::IsValidAddr(ptrVal);
                                                           }
                                                           else if (avail >= 4)
                                                           {
                                                               uint32_t raw = 0;
                                                               memcpy(&raw, buffer.data() + off, 4);
                                                               ptrVal = MemUtils::Normalize((uint64_t)raw);
                                                               canJump = ptrVal > 0x10000 && ptrVal < 0xFFFFFFFF;
                                                           }
                                                           if (canJump)
                                                           {
                                                               if (UI::Btn("->", {S(32), S(22)}, Colors::BTN_PURPLE)) navigation = ptrVal;
                                                               if (ImGui::IsItemHovered()) ImGui::SetTooltip("跳转到: %lX", ptrVal);
                                                           }
                                                           else
                                                           {
                                                               ImGui::BeginDisabled();
                                                               ImGui::Button("-", {S(32), S(22)});
                                                               ImGui::EndDisabled();
                                                           }
                                                           ImGui::PopID();
                                                       }
                                                       ImGui::EndTable();
                                                   }
                                                   ImGui::PopStyleVar();
                                               } while (false);
                                               else
                                               {
                                                   // ================================================================
                                                   // 内存视图渲染
                                                   // ================================================================
                                                   const uintptr_t base = gMemViewer->base();
                                                   const auto buffer = gMemViewer->buffer();
                                                   ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {S(6), S(6)});
                                                   if (ImGui::BeginTable("Typed", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                                                   {
                                                       ImGui::TableSetupColumn("地址", ImGuiTableColumnFlags_WidthFixed, S(100));
                                                       ImGui::TableSetupColumn("数值", ImGuiTableColumnFlags_WidthStretch);
                                                       ImGui::TableSetupColumn("存", ImGuiTableColumnFlags_WidthFixed, S(50));
                                                       ImGui::TableSetupColumn("跳", ImGuiTableColumnFlags_WidthFixed, S(50));
                                                       ImGui::TableHeadersRow();
                                                       Types::DataType savedType = Types::DataType::I32;
                                                       switch (fmt)
                                                       {
                                                       case Types::ViewFormat::I8:
                                                           savedType = Types::DataType::I8;
                                                           break;
                                                       case Types::ViewFormat::I16:
                                                           savedType = Types::DataType::I16;
                                                           break;
                                                       case Types::ViewFormat::I64:
                                                       case Types::ViewFormat::Hexadecimal:
                                                           savedType = Types::DataType::I64;
                                                           break;
                                                       case Types::ViewFormat::Float:
                                                           savedType = Types::DataType::Float;
                                                           break;
                                                       case Types::ViewFormat::Double:
                                                           savedType = Types::DataType::Double;
                                                           break;
                                                       default:
                                                           break;
                                                       }
                                                       size_t step = Types::GetViewSize(fmt);
                                                       for (int i = 0; i < rows; ++i)
                                                       {
                                                           size_t off = i * step;
                                                           if (off + step > buffer.size()) break;
                                                           uintptr_t addr = base + off;
                                                           const uint8_t *p = buffer.data() + off;
                                                           uint64_t ptrVal = 0;
                                                           ImGui::TableNextRow();
                                                           ImGui::PushID((void *)addr);
                                                           ImGui::TableSetColumnIndex(0);
                                                           ImGui::TextColored(i == 0 ? ImVec4{0.4f, 1, 0.4f, 1} : Colors::ADDR_CYAN, "%lX", addr);
                                                           ImGui::TableSetColumnIndex(1);
                                                           switch (fmt)
                                                           {
                                                           case Types::ViewFormat::Hexadecimal:
                                                               ptrVal = *(const uint64_t *)p;
                                                               ImGui::TextColored({0.6f, 1, 0.6f, 1}, "%lX", ptrVal);
                                                               break;
                                                           case Types::ViewFormat::I8:
                                                               ImGui::Text("%d", *(const int8_t *)p);
                                                               break;
                                                           case Types::ViewFormat::I16:
                                                               ImGui::Text("%d", *(const int16_t *)p);
                                                               break;
                                                           case Types::ViewFormat::I32:
                                                               ptrVal = *(const uint32_t *)p;
                                                               ImGui::Text("%d", *(const int32_t *)p);
                                                               break;
                                                           case Types::ViewFormat::I64:
                                                               ptrVal = *(const uint64_t *)p;
                                                               ImGui::Text("%lld", (long long)*(const int64_t *)p);
                                                               break;
                                                           case Types::ViewFormat::Float:
                                                               ImGui::Text("%.11f", *(const float *)p);
                                                               break;
                                                           case Types::ViewFormat::Double:
                                                               ImGui::Text("%.11lf", *(const double *)p);
                                                               break;
                                                           default:
                                                               ImGui::Text("?");
                                                           }
                                                           ImGui::TableSetColumnIndex(2);
                                                           if (UI::Btn("存", {S(42), S(28)}, {0.2f, 0.4f, 0.25f, 1})) lastStatus_ = gSavedAddresses->add(addr, savedType);
                                                           ImGui::TableSetColumnIndex(3);
                                                           uintptr_t jump = MemUtils::Normalize(ptrVal);
                                                           if ((fmt == Types::ViewFormat::I32 || fmt == Types::ViewFormat::I64 || fmt == Types::ViewFormat::Hexadecimal) && MemUtils::IsValidAddr(jump))
                                                           {
                                                               if (UI::Btn("->", {S(42), S(28)}, Colors::BTN_PURPLE)) navigation = jump;
                                                           }
                                                           else
                                                           {
                                                               ImGui::BeginDisabled();
                                                               ImGui::Button("-", {S(42), S(28)});
                                                               ImGui::EndDisabled();
                                                           }
                                                           ImGui::PopID();
                                                       }
                                                       ImGui::EndTable();
                                                   }
                                                   ImGui::PopStyleVar();
                                               }
                                           }
                                           ImGui::EndChild();
                                           ImGui::SameLine();

                                           if (ImGui::BeginChild("MemArrows", {aW, cH}, false, ImGuiWindowFlags_NoScrollbar))
                                           {
                                               ImGui::PushStyleColor(ImGuiCol_Button, {0.2f, 0.3f, 0.4f, 1});
                                               ImGui::BeginDisabled(fmt == Types::ViewFormat::Disasm && gMemViewer->disasmBusy());
                                               if (ImGui::Button("▲##view_up", {aW, cH / 2 - S(3)})) lastStatus_ = gMemViewer->applyOffset(-byteOffset);
                                               if (ImGui::Button("▼##view_down", {aW, cH / 2 - S(3)})) lastStatus_ = gMemViewer->applyOffset(byteOffset);
                                               ImGui::EndDisabled();
                                               ImGui::PopStyleColor();
                                           }
                                           ImGui::EndChild();
                                           if (navigation != 0) lastStatus_ = gMemViewer->open(navigation);
                                       } while (false);
                                       break;
                                   case 3:
                                       {
                                           // ================================================================
                                           // 模块页
                                           // ================================================================
                                           float w = ImGui::GetContentRegionAvail().x;
                                           gFloatingKeyboard->InputButton(buf_.moduleSearch, "模块名或Dump范围", {w, S(42)}, "模块名，或地址范围如 0x5000-0x6000");
                                           ImGui::Dummy({0, S(4)});
                                           if (UI::Btn("刷新模块", {w, S(48)}, Colors::BTN_TEAL))
                                           {
                                               const std::unique_ptr<Driver::virtual_memory> snapshot(new Driver::virtual_memory(dr->GetMemoryInfo()));
                                               moduleRows_.clear();
                                               for (int i = 0; i < snapshot->module_count; ++i)
                                               {
                                                   const auto &mod = snapshot->modules[i];
                                                   const std::string name(MemUtils::BaseName(mod.name));
                                                   for (int j = 0; j < mod.seg_count; ++j)
                                                   {
                                                       const auto &seg = mod.segs[j];
                                                       moduleRows_.push_back({name, seg.index, seg.prot, seg.start, seg.end});
                                                   }
                                               }
                                           }
                                           ImGui::Dummy({0, S(6)});
                                           ImGui::BeginDisabled(gApplicationRuntime->dumpBusy());
                                           if (UI::Btn("Dump 模块/内存范围 (保存至 /sdcard/dump/)", {w, S(48)}, Colors::BTN_PURPLE) && buf_.moduleSearch[0])
                                           {
                                               lastStatus_ = gApplicationRuntime->startDump(buf_.moduleSearch);
                                           }
                                           ImGui::EndDisabled();
                                           if (!gApplicationRuntime->dumpBusy() && gApplicationRuntime->dumpStatus() != 0)
                                               ImGui::TextColored(Colors::ERR, "导出失败，状态: %d", gApplicationRuntime->dumpStatus());
                                           ImGui::Dummy({0, S(6)});

                                           if (ImGui::BeginChild("ModList", {0, 0}, false))
                                           {
                                               if (moduleRows_.empty())
                                               {
                                                   ImGui::TextColored(Colors::HINT, "暂无模块，请手动刷新");
                                               }
                                               else
                                               {
                                                   int displayCount = 0;
                                                   for (size_t i = 0; i < moduleRows_.size(); ++i)
                                                   {
                                                       const auto &seg = moduleRows_[i];
                                                       std::string_view name = seg.name;
                                                       if (buf_.moduleSearch[0] && name.find(buf_.moduleSearch) == std::string_view::npos) continue;
                                                       displayCount++;
                                                       ImGui::PushID(static_cast<int>(i));
                                                       UI::ColorChild(
                                                           "Mod", {w - S(20), 0}, Colors::BG_CARD,
                                                           [&]
                                                           {
                                                               ImGui::TextColored({0.7f, 0.85f, 1, 1}, "%.*s", (int)name.size(), name.data());
                                                               seg.index == -1 ? ImGui::TextColored({0.9f, 0.6f, 0.3f, 1}, "Segment: BSS  %c%c%c", (seg.prot & 1) ? 'R' : '-', (seg.prot & 2) ? 'W' : '-', (seg.prot & 4) ? 'X' : '-') : ImGui::TextColored(Colors::ADDR_GREEN, "Segment: %d  %c%c%c", seg.index, (seg.prot & 1) ? 'R' : '-', (seg.prot & 2) ? 'W' : '-', (seg.prot & 4) ? 'X' : '-');
                                                               ImGui::TextColored(Colors::HINT, "范围: ");
                                                               ImGui::SameLine();
                                                               ImGui::TextColored({0.4f, 1, 0.4f, 1}, "%llX - ", (unsigned long long)seg.start);
                                                               ImGui::SameLine();
                                                               ImGui::TextColored({1, 0.6f, 0.4f, 1}, "%llX", (unsigned long long)seg.end);
                                                           },
                                                           ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize);
                                                       ImGui::PopID();
                                                       ImGui::Dummy({0, S(4)});
                                                   }
                                                   if (!displayCount) ImGui::TextColored({0.6f, 0.4f, 0.4f, 1}, "未找到匹配 \"%s\" 的模块", buf_.moduleSearch);
                                               }
                                           }
                                           ImGui::EndChild();
                                       }
                                       break;
                                   case 4:
                                       {
                                           // ================================================================
                                           // 指针扫描页
                                           // ================================================================
                                           float w = ImGui::GetContentRegionAvail().x, bh = S(45);
                                           const auto pointerState = gPointerManager->state();
                                           const bool busy = pointerState.operation != PointerManager::Operation::Idle;
                                           ImGui::PushID("PtrScan");
                                           ImGui::TextColored(Colors::TITLE, "━━ 指针扫描 ━━");
                                           ImGui::Dummy({0, S(4)});

                                           ImGui::BeginDisabled(busy);
                                           ImGui::Text("目标地址:");
                                           gFloatingKeyboard->InputButton(buf_.ptrTarget, "点击输入Hex", {w, bh}, "目标地址(Hex)");
                                           ImGui::Dummy({0, S(4)});

                                           ImGui::Text("深度:");
                                           ImGui::SameLine();
                                           char dLbl[8];
                                           snprintf(dLbl, sizeof(dLbl), "%d层", ptrParams_.depth);
                                           if (ImGui::Button(dLbl, {S(70), bh})) state_.showDepth = true;
                                           ImGui::SameLine();
                                           ImGui::Text("偏移:");
                                           ImGui::SameLine();
                                           if (ImGui::Button(offsetLabels_[selectedOffsetIdx_].c_str(), {S(70), bh})) state_.showOffset = true;

                                           ImGui::Dummy({0, S(4)});
                                           ImGui::TextColored(Colors::LABEL, "指定模块 (可选):");
                                           gFloatingKeyboard->InputButton(buf_.filterModule, "全部模块", {w - S(60), bh}, "模块名(如il2cpp)");
                                           ImGui::SameLine();
                                           if (ImGui::Button("清##scanFilter", {S(50), bh})) buf_.filterModule[0] = 0;

                                           ImGui::Checkbox("手动基址##scan", &ptrParams_.useManual);
                                           if (ptrParams_.useManual)
                                           {
                                               ptrParams_.useArray = false;
                                               gFloatingKeyboard->InputButton(buf_.base, "基址(Hex)##scanBase", {w, bh}, "Hex基址");
                                           }
                                           ImGui::Checkbox("数组基址##scan", &ptrParams_.useArray);
                                           if (ptrParams_.useArray)
                                           {
                                               ptrParams_.useManual = false;
                                               float hw = (w - S(6)) / 2;
                                               gFloatingKeyboard->InputButton(buf_.arrayBase, "数组地址(Hex)", {hw, bh}, "数组首地址");
                                               ImGui::SameLine();
                                               gFloatingKeyboard->InputButton(buf_.arrayCount, "数量", {hw, bh}, "元素数量");
                                           }

                                           ImGui::Dummy({0, S(6)});
                                           if (UI::Btn("开始扫描", {w, S(48)}, Colors::BTN_GREEN))
                                           {
                                               if (const auto target = ParseHexAddress(buf_.ptrTarget))
                                               {
                                                   uintptr_t manualBase = 0;
                                                   uintptr_t arrayBase = 0;
                                                   size_t arrayCount = 0;
                                                   bool valid = true;
                                                   if (ptrParams_.useManual)
                                                   {
                                                       if (const auto parsed = ParseHexAddress(buf_.base)) manualBase = *parsed;
                                                       else valid = false;
                                                   }
                                                   else if (ptrParams_.useArray)
                                                   {
                                                       const auto parsedBase = ParseHexAddress(buf_.arrayBase);
                                                       const auto parsedCount = MemUtils::ParseUInt64(buf_.arrayCount, 10);
                                                       if (parsedBase && parsedCount && *parsedCount > 0 && *parsedCount <= 1000000)
                                                       {
                                                           arrayBase = *parsedBase;
                                                           arrayCount = static_cast<size_t>(*parsedCount);
                                                       }
                                                       else
                                                       {
                                                           valid = false;
                                                       }
                                                   }
                                                   if (valid) lastStatus_ = gPointerManager->startAsync(dr->GetGlobalPid(), *target, ptrParams_.depth, (selectedOffsetIdx_ + 1) * 500, ptrParams_.useManual, manualBase, ptrParams_.useArray, arrayBase, arrayCount, buf_.filterModule);
                                               }
                                           }

                                           ImGui::Dummy({0, S(12)});
                                           ImGui::Separator();
                                           ImGui::Dummy({0, S(8)});
                                           ImGui::TextColored({0.6f, 0.7f, 0.8f, 1}, "文件操作 (Pointer.bin)");
                                           ImGui::Dummy({0, S(4)});
                                           const float buttonWidth = (w - S(8)) / 2;
                                           if (UI::Btn("开始对比", {buttonWidth, S(40)}, Colors::BTN_PURPLE)) lastStatus_ = gPointerManager->MergeBins();
                                           ImGui::SameLine();
                                           if (UI::Btn("格式化输出", {buttonWidth, S(40)}, {0.45f, 0.35f, 0.2f, 1})) lastStatus_ = gPointerManager->ExportToTxt();
                                           ImGui::EndDisabled();

                                           ImGui::Dummy({0, S(8)});
                                           if (busy)
                                           {
                                               ImGui::TextColored(Colors::WARN, "%.*s...", static_cast<int>(PointerManager::OperationName(pointerState.operation).size()), PointerManager::OperationName(pointerState.operation).data());
                                               ImGui::ProgressBar(pointerState.progress, {w, S(22)});
                                           }
                                           else if (pointerState.completed && !pointerState.success)
                                           {
                                               ImGui::TextColored(Colors::ERR, "%s", pointerState.error.empty() ? "指针操作失败" : pointerState.error.c_str());
                                           }
                                           else if (pointerState.completed)
                                           {
                                               ImGui::TextColored(Colors::OK, "操作完成，指针链数量: %zu", pointerState.count);
                                           }
                                           else ImGui::TextColored(Colors::HINT, "扫描结果保存到 Pointer.bin");
                                           ImGui::PopID();
                                       }
                                       break;
                                   case 5:
                                       {
                                           // ================================================================
                                           // 特征码页
                                           // ================================================================
                                           float w = ImGui::GetContentRegionAvail().x, bh = S(45);

                                           // 扫描部分
                                           ImGui::TextColored(Colors::TITLE, "━━ 特征码扫描 ━━");
                                           ImGui::Dummy({0, S(4)});
                                           ImGui::Text("目标地址:");
                                           gFloatingKeyboard->InputButton(buf_.sigScanAddr, "点击输入Hex", {w, bh}, "目标地址(Hex)");
                                           ImGui::Dummy({0, S(4)});
                                           ImGui::Text("范围 (上下各N字节):");
                                           ImGui::SetNextItemWidth(w);
                                           ImGui::SliderInt("##sigRange", &sigParams_.range, 1, SignatureScanner::SIG_MAX_RANGE, "%d");

                                           // 快速范围按钮
                                           float qbw = (w - S(12)) / 4;
                                           for (int r : {10, 20, 50, 100})
                                           {
                                               char lb[8];
                                               snprintf(lb, sizeof(lb), "%d", r);
                                               if (ImGui::Button(lb, {qbw, S(30)})) sigParams_.range = r;
                                               if (r != 100) ImGui::SameLine();
                                           }

                                           ImGui::Dummy({0, S(8)});
                                           if (UI::Btn("扫描保存", {w, S(48)}, Colors::BTN_GREEN))
                                           {
                                               if (auto addr = ParseHexAddress(buf_.sigScanAddr)) lastStatus_ = SignatureScanner::ScanAddressSignature(*addr, sigParams_.range);
                                           }
                                           ImGui::TextColored(Colors::HINT, "保存到 Signature.txt");

                                           // 过滤部分
                                           ImGui::Dummy({0, S(20)});
                                           ImGui::Separator();
                                           ImGui::Dummy({0, S(10)});
                                           ImGui::TextColored(Colors::TITLE, "━━ 特征码过滤 ━━");
                                           ImGui::Dummy({0, S(4)});
                                           ImGui::Text("过滤地址:");
                                           gFloatingKeyboard->InputButton(buf_.sigVerifyAddr, "点击输入Hex", {w, bh}, "过滤地址(Hex)");
                                           ImGui::Dummy({0, S(8)});

                                           if (UI::Btn("过滤并更新", {w, S(48)}, {0.4f, 0.3f, 0.15f, 1}))
                                           {
                                               if (auto addr = ParseHexAddress(buf_.sigVerifyAddr))
                                               {
                                                   auto vr = SignatureScanner::FilterSignature(*addr);
                                                   sigParams_.lastChanged = vr.success ? vr.changedCount : -2;
                                                   if (vr.success) sigParams_.lastTotal = vr.totalCount;
                                                   sigParams_.lastScanCount = -1;
                                               }
                                           }
                                           if (sigParams_.lastChanged >= 0)
                                           {
                                               sigParams_.lastChanged == 0 ? ImGui::TextColored(Colors::OK, "完美! 无变动 (%d字节)", sigParams_.lastTotal) : ImGui::TextColored(Colors::WARN, "变动: %d/%d (已更新)", sigParams_.lastChanged, sigParams_.lastTotal);
                                           }
                                           else if (sigParams_.lastChanged == -2) ImGui::TextColored(Colors::ERR, "失败! 检查Signature.txt");

                                           ImGui::Dummy({0, S(10)});
                                           if (UI::Btn("扫描特征码", {w, S(48)}, Colors::BTN_PURPLE)) sigParams_.lastScanCount = (int)SignatureScanner::ScanSignatureFromFile().size();
                                           if (sigParams_.lastScanCount >= 0)
                                           {
                                               sigParams_.lastScanCount == 0 ? ImGui::TextColored(Colors::ERR, "未找到匹配地址") : ImGui::TextColored({0.5f, 0.9f, 1, 1}, "找到 %d 个地址", sigParams_.lastScanCount);
                                           }
                                           ImGui::TextColored(Colors::HINT, "结果保存到 Signature.txt");
                                       }
                                       break;
                                   case 6:
                                       {
                                           // ================================================================
                                           // 断点页
                                           // ================================================================
                                           float w = ImGui::GetContentRegionAvail().x, bh = S(45);

                                           ImGui::TextColored(Colors::TITLE, "━━ 断点 ━━");
                                           ImGui::Dummy({0, S(4)});

                                           // 硬件信息
                                           auto &snapshot = dr->GetBreakpointInfo();
                                           const auto activeMode = gApplicationRuntime->breakpointMode();
                                           const bool anyBpActive = !activeMode.empty();
                                           int activePointCount = 0;
                                           uintptr_t firstAddress = 0;
                                           for (const auto &point : snapshot.points)
                                           {
                                               if (!point.hit_addr) continue;
                                               if (!firstAddress) firstAddress = point.hit_addr;
                                               ++activePointCount;
                                           }
                                           UI::LabelValue(Colors::ADDR_CYAN, "执行断点寄存器: ", Colors::ADDR_GREEN, "%llu", (unsigned long long)snapshot.num_brps);
                                           ImGui::SameLine();
                                           UI::LabelValue(Colors::ADDR_CYAN, "  访问断点寄存器: ", Colors::ADDR_GREEN, "%llu", (unsigned long long)snapshot.num_wrps);
                                           ImGui::TextColored(activeMode == "ptebp" ? Colors::OK : Colors::HINT, activeMode == "ptebp" ? "PTEBP: 已激活" : "PTEBP: 未激活");
                                           ImGui::TextColored(activeMode == "dptbp" ? Colors::OK : Colors::HINT, activeMode == "dptbp" ? "DPTBP: 已激活" : "DPTBP: 未激活");
                                           ImGui::TextColored(activeMode == "stepbp" ? Colors::OK : Colors::HINT, activeMode == "stepbp" ? "STEPBP: 已激活" : "STEPBP: 未激活");

                                           ImGui::Dummy({0, S(6)});
                                           ImGui::Separator();
                                           ImGui::Dummy({0, S(6)});

                                           // 配置
                                           ImGui::Text("points:");
                                           float addrW = std::max(S(120), w - S(222));
                                           float typeW = S(58), scopeW = S(58), lenW = S(66);
                                           for (int i = 0; i < bpParams_.configPointCount; ++i)
                                           {
                                               auto &row = bpParams_.points[i];
                                               ImGui::PushID(i);
                                               ImGui::TextColored(Colors::LABEL, "P%d", i);
                                               ImGui::SameLine();
                                               gFloatingKeyboard->InputButton(row.addr, "地址", {addrW, bh}, "断点地址(Hex)");
                                               ImGui::SameLine();
                                               if (UI::Btn(bpTypeLabels[std::clamp(row.type, 0, 3)], {typeW, bh}, Colors::BTN_BLUE))
                                               {
                                                   state_.bpPopupPoint = i;
                                                   state_.showBpType = true;
                                               }
                                               ImGui::SameLine();
                                               if (UI::Btn(bpScopeLabels[std::clamp(row.scope, 0, 2)], {scopeW, bh}, Colors::BTN_TEAL))
                                               {
                                                   state_.bpPopupPoint = i;
                                                   state_.showBpScope = true;
                                               }
                                               ImGui::SameLine();
                                               if (UI::Btn(bpLenLabels[std::clamp(row.len, 1, 8) - 1], {lenW, bh}, Colors::BTN_ORANGE))
                                               {
                                                   state_.bpPopupPoint = i;
                                                   state_.showBpLen = true;
                                               }
                                               ImGui::Dummy({0, S(4)});
                                               ImGui::PopID();
                                           }
                                           float rowBtnW = (w - S(8)) / 2;
                                           ImGui::BeginDisabled(bpParams_.configPointCount >= 16 || anyBpActive);
                                           if (UI::Btn("添加point", {rowBtnW, S(38)}, Colors::BTN_BLUE)) ++bpParams_.configPointCount;
                                           ImGui::EndDisabled();
                                           ImGui::SameLine();
                                           ImGui::BeginDisabled(bpParams_.configPointCount <= 1 || anyBpActive);
                                           if (UI::Btn("删除point", {rowBtnW, S(38)}, Colors::BTN_RED)) --bpParams_.configPointCount;
                                           ImGui::EndDisabled();
                                           ImGui::Dummy({0, S(8)});

                                           // 操作按钮
                                           float halfW = (w - S(8)) / 2;
                                           auto setBp = [&](const char *label, ImVec4 color, std::string_view mode)
                                           {
                                               ImGui::BeginDisabled(anyBpActive);
                                               if (UI::Btn(label, {halfW, S(52)}, color))
                                               {
                                                   static constexpr Driver::bp_type typeValues[] = {Driver::BP_BREAKPOINT_R, Driver::BP_BREAKPOINT_W, Driver::BP_BREAKPOINT_RW, Driver::BP_BREAKPOINT_X};
                                                   static constexpr Driver::bp_scope scopeValues[] = {Driver::BP_SCOPE_MAIN_THREAD, Driver::BP_SCOPE_OTHER_THREADS, Driver::BP_SCOPE_ALL_THREADS};
                                                   std::vector<Driver::bp_point> points;
                                                   const int pointCount = std::clamp(bpParams_.configPointCount, 1, 16);
                                                   points.reserve(pointCount);
                                                   for (int index = 0; index < pointCount; ++index)
                                                   {
                                                       const auto address = ParseHexAddress(bpParams_.points[index].addr);
                                                       if (!address)
                                                       {
                                                           points.clear();
                                                           break;
                                                       }
                                                       Driver::bp_point point{};
                                                       point.hit_addr = *address;
                                                       point.bt = typeValues[std::clamp(bpParams_.points[index].type, 0, 3)];
                                                       point.bs = scopeValues[std::clamp(bpParams_.points[index].scope, 0, 2)];
                                                       point.bl = static_cast<Driver::bp_len>(std::clamp(bpParams_.points[index].len, 1, 8));
                                                       points.push_back(point);
                                                   }
                                                   if (!points.empty()) lastStatus_ = gApplicationRuntime->setBreakpoints(mode, points);
                                               }
                                               ImGui::EndDisabled();
                                           };
                                           auto removeBp = [&](const char *label, bool disabled)
                                           {
                                               ImGui::BeginDisabled(disabled);
                                               if (UI::Btn(label, {halfW, S(52)}, {0.5f, 0.15f, 0.15f, 1})) lastStatus_ = gApplicationRuntime->clearBreakpoints();
                                               ImGui::EndDisabled();
                                           };

                                           setBp("设置HWBP", Colors::BTN_GREEN, "hwbp");
                                           ImGui::SameLine();
                                           removeBp("移除HWBP", activeMode != "hwbp");
                                           ImGui::Dummy({0, S(6)});
                                           setBp("设置PTEBP", Colors::BTN_TEAL, "ptebp");
                                           ImGui::SameLine();
                                           removeBp("移除PTEBP", activeMode != "ptebp");
                                           ImGui::Dummy({0, S(6)});
                                           setBp("设置DPTBP", Colors::BTN_TEAL, "dptbp");
                                           ImGui::SameLine();
                                           removeBp("移除DPTBP", activeMode != "dptbp");
                                           ImGui::Dummy({0, S(6)});
                                           setBp("设置STEPBP", Colors::BTN_BLUE, "stepbp");
                                           ImGui::SameLine();
                                           removeBp("移除STEPBP", activeMode != "stepbp");

                                           ImGui::Dummy({0, S(8)});
                                           const char *activeModeLabel = "HWBP";
                                           if (activeMode == "ptebp") activeModeLabel = "PTEBP";
                                           else if (activeMode == "dptbp") activeModeLabel = "DPTBP";
                                           else if (activeMode == "stepbp") activeModeLabel = "STEPBP";
                                           anyBpActive ? ImGui::TextColored(Colors::OK, "● 断点已激活  地址数: %d  首地址: 0x%lX  模式: %s", activePointCount, firstAddress, activeModeLabel) : ImGui::TextColored(Colors::HINT, "○ 断点未激活");
                                           for (const auto &point : snapshot.points)
                                           {
                                               if (point.hit_addr) ImGui::TextColored(Colors::ADDR_CYAN, "监控地址: 0x%llX", (unsigned long long)point.hit_addr);
                                           }

                                           ImGui::Dummy({0, S(8)});
                                           ImGui::Separator();
                                           ImGui::Dummy({0, S(6)});
                                           ImGui::TextColored(Colors::TITLE, "━━ 命中信息 ━━");
                                           ImGui::Dummy({0, S(4)});

                                           if (activePointCount > 0)
                                           {
                                               uint64_t totalHits = 0;
                                               int totalPointCount = 0;
                                               int totalRecordCount = 0;
                                               std::array<uint64_t, BP_CONFIG_MAX> pointHitCounts{};
                                               for (auto &point : snapshot.points)
                                               {
                                                   const int recordCount = std::clamp(point.record_count, 0, BP_RECORD_MAX);
                                                   if (point.hit_addr) totalPointCount++;
                                                   for (int r = 0; r < recordCount; ++r)
                                                   {
                                                       auto &rec = point.records[r];
                                                       MemUtils::HwbpRequestAll(rec);
                                                       totalHits += rec.hit_count;
                                                       pointHitCounts[static_cast<size_t>(&point - snapshot.points)] += rec.hit_count;
                                                       totalRecordCount++;
                                                   }
                                               }
                                               ImGui::TextColored(Colors::WARN, "point数: %d  record数: %d  总命中: %llu", totalPointCount, totalRecordCount, (unsigned long long)totalHits);
                                               ImGui::Dummy({0, S(6)});

                                               int flatIndex = 0;

                                               for (int p = 0; p < BP_CONFIG_MAX; ++p)
                                               {
                                                   auto &point = snapshot.points[p];
                                                   const int recordCount = std::clamp(point.record_count, 0, BP_RECORD_MAX);
                                                   const int pointFlatStart = flatIndex;
                                                   if (!point.hit_addr)
                                                   {
                                                       flatIndex += recordCount;
                                                       continue;
                                                   }

                                                   ImGui::PushID(p);
                                                   const float expandPointW = S(55);
                                                   ImGui::TextColored(Colors::ADDR_CYAN, "hit_addr:0x%llX  point[%d]  records:%d  总命中:%llu", (unsigned long long)point.hit_addr, p, recordCount, (unsigned long long)pointHitCounts[p]);
                                                   ImGui::SameLine(w - expandPointW);
                                                   if (UI::Btn(bpParams_.pointExpandState[p] ? "收起" : "展开", {expandPointW, S(32)}, Colors::BTN_BLUE)) bpParams_.pointExpandState[p] = !bpParams_.pointExpandState[p];

                                                   if (bpParams_.pointExpandState[p])
                                                   {
                                                       ImGui::Indent(S(8));
                                                       ImGui::TextColored(Colors::TITLE, "records");
                                                       ImGui::SameLine();
                                                       if (UI::Btn(bpParams_.recordsExpandState[p] ? "收起##records" : "展开##records", {S(80), S(30)}, Colors::BTN_TEAL)) bpParams_.recordsExpandState[p] = !bpParams_.recordsExpandState[p];

                                                       if (bpParams_.recordsExpandState[p])
                                                       {
                                                           ImGui::Indent(S(8));
                                                           if (recordCount <= 0)
                                                           {
                                                               ImGui::TextColored(Colors::HINT, "暂无 record");
                                                           }
                                                           for (int r = 0; r < recordCount; ++r)
                                                           {
                                                               const int recordFlatIndex = pointFlatStart + r;
                                                               auto &rec = point.records[r];
                                                               ImGui::PushID(recordFlatIndex);
                                                               const float expandRecordW = S(55);

                                                               ImGui::TextColored({0.7f, 0.85f, 1, 1}, "record[%d:%d]  PC:0x%llX  命中:%llu", p, r, (unsigned long long)rec.pc, (unsigned long long)rec.hit_count);
                                                               ImGui::SameLine(w - expandRecordW);
                                                               if (UI::Btn(bpParams_.recordExpandState[recordFlatIndex] ? "收起" : "展开", {expandRecordW, S(32)}, {0.2f, 0.3f, 0.45f, 1})) bpParams_.recordExpandState[recordFlatIndex] = !bpParams_.recordExpandState[recordFlatIndex];

                                                               if (bpParams_.recordExpandState[recordFlatIndex])
                                                               {
                                                                   ImGui::Indent(S(8));
                                                                   {
                                                                       auto scalarLine = [&](const char *name, int regIndex, bool narrow32 = false)
                                                                       {
                                                                           const auto val = narrow32 ? static_cast<std::uint32_t>(MemUtils::HwbpReadRegisterValue(rec, regIndex)) : static_cast<std::uint64_t>(MemUtils::HwbpReadRegisterValue(rec, regIndex));
                                                                           const auto hex = std::format("{:X}", val);
                                                                           ImGui::TextColored({0.7f, 0.85f, 1, 1}, "%s: ", name);
                                                                           ImGui::SameLine();
                                                                           ImGui::TextColored(Colors::ADDR_GREEN, "0x%llX", (unsigned long long)val);
                                                                           ImGui::SameLine();

                                                                           char id[32];
                                                                           snprintf(id, sizeof(id), "复制##%s%d", name, recordFlatIndex);
                                                                           if (UI::Btn(id, {S(50), S(28)}, Colors::BTN_COPY)) ImGui::SetClipboardText(hex.c_str());

                                                                           ImGui::SameLine();
                                                                           snprintf(id, sizeof(id), "改##%s%d", name, recordFlatIndex);
                                                                           drawRegisterEditButton(id, recordFlatIndex, regIndex, name, hex, {S(40), S(28)});
                                                                       };

                                                                       scalarLine("PC", Driver::IDX_PC);
                                                                       scalarLine("LR", Driver::IDX_LR);
                                                                       scalarLine("SP", Driver::IDX_SP);
                                                                       ImGui::Dummy({0, S(4)});

                                                                       auto editableLine = [&](const char *label, const char *button, int regIndex, bool decimal = false)
                                                                       {
                                                                           const auto value = static_cast<std::uint64_t>(MemUtils::HwbpReadRegisterValue(rec, regIndex));
                                                                           decimal ? ImGui::TextColored(Colors::LABEL, "%s: %llu", label, (unsigned long long)value) : ImGui::TextColored(Colors::LABEL, "%s: 0x%llX", label, (unsigned long long)value);
                                                                           ImGui::SameLine();
                                                                           drawRegisterEditButton(button, recordFlatIndex, regIndex, label, std::format("{:X}", value), {S(40), S(28)});
                                                                       };

                                                                       editableLine("PSTATE", "改##pst", Driver::IDX_PSTATE);
                                                                       editableLine("SYSCALL", "改##syscall", Driver::IDX_SYSCALLNO, true);
                                                                       editableLine("ORIG_X0", "改##origx0", Driver::IDX_ORIG_X0);
                                                                       ImGui::TextColored(Colors::WARN, "命中次数: %llu", static_cast<unsigned long long>(MemUtils::HwbpReadRegisterValue(rec, Driver::IDX_HIT_COUNT)));
                                                                       ImGui::Dummy({0, S(6)});

                                                                       auto registerTable = [&](const char *title, const char *idPrefix, int count, int idOffset, auto &&drawRow)
                                                                       {
                                                                           ImGui::TextColored(Colors::TITLE, "%s", title);
                                                                           ImGui::Dummy({0, S(4)});
                                                                           ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {S(4), S(4)});
                                                                           if (ImGui::BeginTable(std::format("{}##{}", idPrefix, recordFlatIndex).c_str(), 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                                                                           {
                                                                               ImGui::TableSetupColumn("寄存器", ImGuiTableColumnFlags_WidthFixed, S(55));
                                                                               ImGui::TableSetupColumn("值", ImGuiTableColumnFlags_WidthStretch);
                                                                               ImGui::TableSetupColumn("复制", ImGuiTableColumnFlags_WidthFixed, S(50));
                                                                               ImGui::TableSetupColumn("改", ImGuiTableColumnFlags_WidthFixed, S(50));
                                                                               ImGui::TableHeadersRow();
                                                                               for (int i = 0; i < count; ++i)
                                                                               {
                                                                                   ImGui::TableNextRow();
                                                                                   ImGui::PushID(i + idOffset);
                                                                                   drawRow(i);
                                                                                   ImGui::PopID();
                                                                               }
                                                                               ImGui::EndTable();
                                                                           }
                                                                           ImGui::PopStyleVar();
                                                                       };

                                                                       registerTable("━━ 通用寄存器 ━━", "Regs", 30, 0,
                                                                                     [&](int i)
                                                                                     {
                                                                                         const int regIndex = Driver::IDX_X0 + i;
                                                                                         const auto value = static_cast<std::uint64_t>(MemUtils::HwbpReadRegisterValue(rec, regIndex));
                                                                                         const auto hex = std::format("{:X}", value);
                                                                                         ImGui::TableSetColumnIndex(0);
                                                                                         ImGui::TextColored({0.7f, 0.85f, 1, 1}, "X%d", i);
                                                                                         ImGui::TableSetColumnIndex(1);
                                                                                         ImGui::TextColored(Colors::ADDR_GREEN, "0x%llX", (unsigned long long)value);
                                                                                         ImGui::TableSetColumnIndex(2);
                                                                                         if (UI::Btn("复制", {S(42), S(28)}, Colors::BTN_COPY)) ImGui::SetClipboardText(hex.c_str());
                                                                                         ImGui::TableSetColumnIndex(3);
                                                                                         drawRegisterEditButton("改", recordFlatIndex, regIndex, std::format("X{}", i), hex, {S(42), S(28)});
                                                                                     });

                                                                       ImGui::Dummy({0, S(6)});
                                                                       scalarLine("FPSR", Driver::IDX_FPSR, true);
                                                                       scalarLine("FPCR", Driver::IDX_FPCR, true);
                                                                       ImGui::Dummy({0, S(4)});

                                                                       registerTable("━━ 浮点/SIMD 寄存器 ━━", "VRegs", 32, 32,
                                                                                     [&](int i)
                                                                                     {
                                                                                         const int regIndex = Driver::IDX_Q0 + i;
                                                                                         const auto value = MemUtils::HwbpReadRegisterValue(rec, regIndex);
                                                                                         const auto hex = std::format("{:016X}{:016X}", static_cast<unsigned long long>(value >> 64), static_cast<unsigned long long>(value));
                                                                                         ImGui::TableSetColumnIndex(0);
                                                                                         ImGui::TextColored({0.7f, 0.85f, 1, 1}, "V%d", i);
                                                                                         ImGui::TableSetColumnIndex(1);
                                                                                         ImGui::TextColored(Colors::ADDR_GREEN, "%016llX_%016llX", (unsigned long long)(value >> 64), (unsigned long long)value);
                                                                                         ImGui::TableSetColumnIndex(2);
                                                                                         if (UI::Btn("复制", {S(42), S(28)}, Colors::BTN_COPY)) ImGui::SetClipboardText(hex.c_str());
                                                                                         ImGui::TableSetColumnIndex(3);
                                                                                         drawRegisterEditButton("改", recordFlatIndex, regIndex, std::format("V{}", i), hex, {S(42), S(28)});
                                                                                     });
                                                                   }
                                                                   ImGui::Unindent(S(8));
                                                               }

                                                               ImGui::Dummy({0, S(4)});
                                                               ImGui::Separator();
                                                               ImGui::Dummy({0, S(4)});
                                                               ImGui::PopID();
                                                           }
                                                           ImGui::Unindent(S(8));
                                                       }

                                                       ImGui::Unindent(S(8));
                                                   }

                                                   ImGui::Dummy({0, S(4)});
                                                   ImGui::Separator();
                                                   ImGui::Dummy({0, S(4)});
                                                   ImGui::PopID();
                                                   flatIndex += recordCount;
                                               }
                                           }
                                           else ImGui::TextColored(Colors::HINT, "暂无命中记录");
                                       }
                                       break;
                                   case 7:
                                       drawMonitorTab("━━ 系统调用监控 ━━", "sysmon", "实时筛选包含 lsdriver 的内核日志", "SyscallLog", "暂无系统调用日志", ApplicationRuntime::Monitor::Syscall, syscallParams_);
                                       break;
                                   case 8:
                                       drawMonitorTab("━━ CNTVCT_EL0 读取监控 ━━", "cntvct", "实时筛选 cntvct 标签的内核日志", "CntvctLog", "暂无 CNTVCT 读取日志", ApplicationRuntime::Monitor::Cntvct, cntvctParams_);
                                       break;
                                   case 9:
                                       {
                                           // ================================================================
                                           // 环境参数页
                                           // ================================================================
                                           const float w = ImGui::GetContentRegionAvail().x;
                                           const int pid = dr->GetGlobalPid();
                                           ImGui::TextColored(Colors::TITLE, "━━ 环境参数 ━━");
                                           ImGui::Dummy({0, S(8)});
                                           UI::LabelValue(Colors::LABEL, "当前目标 PID: ", pid > 0 ? Colors::ADDR_GREEN : Colors::ERR, "%d", pid);
                                           ImGui::Dummy({0, S(10)});

                                           gFloatingKeyboard->InputButton(buf_.envThread, "可选线程名 task->comm", {w, S(48)}, "线程名(可选)");
                                           ImGui::Dummy({0, S(8)});
                                           ImGui::BeginDisabled(pid <= 0);
                                           if (UI::Btn("获取环境参数", {w, S(48)}, Colors::BTN_TEAL))
                                           {
                                               envParams_.pid = pid;
                                               envParams_.info = dr->GetEnvParams(buf_.envThread);
                                               envParams_.success = envParams_.info.tls_status == 0 || envParams_.info.pacga_status == 0;
                                               envParams_.hasResult = true;
                                           }
                                           ImGui::EndDisabled();

                                           ImGui::Dummy({0, S(14)});
                                           if (!envParams_.hasResult) ImGui::TextColored(Colors::HINT, "留空线程名可获取 PACGA；填写线程名时同时获取 TPIDR_EL0");
                                           else if (!envParams_.success) ImGui::TextColored(Colors::ERR, "环境参数获取失败");
                                           else
                                           {
                                               UI::LabelValue(Colors::LABEL, "PID: ", Colors::ADDR_GREEN, "%d", envParams_.pid);
                                               UI::LabelValue(Colors::LABEL, "线程名: ", Colors::VAL_YELLOW, "%s", buf_.envThread[0] ? buf_.envThread : "(未指定)");
                                               UI::LabelValue(Colors::LABEL, "TPIDR_EL0: ", Colors::ADDR_CYAN, "0x%llX", (unsigned long long)envParams_.info.tpidr_el0);
                                               UI::LabelValue(Colors::LABEL, "PACGA_LO: ", Colors::ADDR_CYAN, "0x%llX", (unsigned long long)envParams_.info.pacga_lo);
                                               UI::LabelValue(Colors::LABEL, "PACGA_HI: ", Colors::ADDR_CYAN, "0x%llX", (unsigned long long)envParams_.info.pacga_hi);
                                               UI::LabelValue(Colors::LABEL, "TLS 状态: ", envParams_.info.tls_status == 0 ? Colors::OK : Colors::ERR, "%d", envParams_.info.tls_status);
                                               UI::LabelValue(Colors::LABEL, "PACGA 状态: ", envParams_.info.pacga_status == 0 ? Colors::OK : Colors::ERR, "%d", envParams_.info.pacga_status);
                                           }
                                       }
                                       break;
                                   }
                               });
                ImGui::Dummy({0, S(4)});
                {
                    // ---- 标签栏 ----
                    UI::ColorChild(
                        "Tabs", {contentWidth, S(60)}, Colors::BG_PANEL,
                        [&]
                        {
                            constexpr int N = 10;
                            float bw = (contentWidth - S(36)) / N;
                            const char *labels[] = {"扫描", "保存", "浏览", "模块", "指针", "特征", "断点", "调用", "计时", "环境"};
                            for (int i = 0; i < N; ++i)
                            {
                                if (i > 0) ImGui::SameLine();
                                if (UI::Btn(labels[i], {bw, S(60) - S(14)}, state_.tab == i ? Colors::BTN_ACTIVE : Colors::BTN_INACTIVE))
                                {
                                    state_.tab = i;
                                    if (i == 2 && gMemViewer->base()) lastStatus_ = gMemViewer->refresh();
                                }
                            }
                        },
                        ImGuiWindowFlags_NoScrollbar);
                }
            }
            ImGui::End();
            ImGui::PopStyleColor();
            {
                // ================================================================
                // 弹窗统一管理
                // ================================================================
                const float popupMargin = margin;
                // 缩放弹窗
                if (state_.showScale)
                {
                    drawListPopup("缩放", &state_.showScale, popupMargin, popupMargin, w, h, S(180), S(210),
                                  [&](float fw)
                                  {
                                      ImGui::Text("不透明度: %.0f%%", opacity * 100);
                                      ImGui::SliderFloat("##o", &opacity, 0.2f, 1.0f, "");
                                      ImGui::Text("UI: %.0f%%", scale * 100);
                                      ImGui::SliderFloat("##s", &scale, 0.5f, 2.0f, "");
                                      float bw = fw / 3 - S(3);
                                      if (ImGui::Button("75%", {bw, S(28)})) scale = 0.75f;
                                      ImGui::SameLine();
                                      if (ImGui::Button("100%", {bw, S(28)})) scale = 1.0f;
                                      ImGui::SameLine();
                                      if (ImGui::Button("150%", {bw, S(28)})) scale = 1.5f;
                                      ImGui::Text("边距: %.0f", margin);
                                      ImGui::SliderFloat("##m", &margin, 0, 80, "");
                                  });
                }

                // 通用选择器
                auto doSelector = [&](const char *title, bool *show, auto items, int count, auto *sel)
                {
                    int s = static_cast<int>(*sel);
                    drawListPopup(title, show, popupMargin, popupMargin, w, h, w * 0.75f, std::min(count * (S(42) + S(4)) + S(50), h * 0.7f),
                                  [&](float fw)
                                  {
                                      for (int i = 0; i < count; ++i)
                                          if (UI::Btn(items[i], {fw, S(42)}, i == s ? ImVec4{0.2f, 0.35f, 0.25f, 1} : ImVec4{0.13f, 0.13f, 0.16f, 1}))
                                          {
                                              s = i;
                                              *show = false;
                                          }
                                  });
                    *sel = static_cast<std::remove_pointer_t<decltype(sel)>>(s);
                };

                if (state_.showType) doSelector("类型", &state_.showType, Types::Labels::TYPE.data(), (int)Types::Labels::TYPE.size(), &scanParams_.dataType);
                if (state_.showSavedType) doSelector("保存类型", &state_.showSavedType, Types::Labels::TYPE.data(), (int)Types::Labels::TYPE.size(), &savedParams_.dataType);
                if (state_.showMode) doSelector("模式", &state_.showMode, Types::Labels::FUZZY.data(), (int)Types::Labels::FUZZY.size(), &scanParams_.fuzzyMode);
                if (state_.showFormat)
                {
                    auto fmt = gMemViewer->format();
                    doSelector("格式", &state_.showFormat, Types::Labels::VIEW_FORMAT.data(), (int)Types::ViewFormat::Count, &fmt);
                    if (fmt != gMemViewer->format()) lastStatus_ = gMemViewer->setFormat(fmt);
                }
                if (state_.bpPopupPoint >= 0 && state_.bpPopupPoint < bpParams_.configPointCount)
                {
                    auto &row = bpParams_.points[state_.bpPopupPoint];
                    if (state_.showBpType)
                    {
                        int selected = std::clamp(row.type, 0, 3);
                        doSelector("断点类型", &state_.showBpType, bpTypeLabels, 4, &selected);
                        row.type = selected;
                    }
                    if (state_.showBpScope)
                    {
                        int selected = std::clamp(row.scope, 0, 2);
                        doSelector("线程范围", &state_.showBpScope, bpScopeLabels, 3, &selected);
                        row.scope = selected;
                    }
                    if (state_.showBpLen)
                    {
                        int selected = std::clamp(row.len, 1, 8) - 1;
                        doSelector("监控长度", &state_.showBpLen, bpLenLabels, 8, &selected);
                        row.len = selected + 1;
                    }
                }
                // 深度选择
                if (state_.showDepth)
                {
                    drawListPopup("深度", &state_.showDepth, popupMargin, popupMargin, w, h, S(160), S(320),
                                  [&](float fw)
                                  {
                                      for (int i = 1; i <= 16; ++i)
                                      {
                                          char lbl[8];
                                          snprintf(lbl, sizeof(lbl), "%d层", i);
                                          if (UI::Btn(lbl, {fw, S(28)}, i == ptrParams_.depth ? ImVec4{0.2f, 0.35f, 0.25f, 1} : ImVec4{0.13f, 0.13f, 0.16f, 1}))
                                          {
                                              ptrParams_.depth = i;
                                              state_.showDepth = false;
                                          }
                                      }
                                  });
                }

                // 偏移选择
                if (state_.showOffset)
                {
                    drawListPopup("偏移", &state_.showOffset, popupMargin, popupMargin, w, h, S(160), std::min((float)offsetLabels_.size() * S(32) + S(40), h * 0.6f),
                                  [&](float fw)
                                  {
                                      if (ImGui::BeginChild("List", {0, 0}, false))
                                      {
                                          for (size_t i = 0; i < offsetLabels_.size(); ++i)
                                              if (UI::Btn(offsetLabels_[i].c_str(), {fw, S(28)}, (int)i == selectedOffsetIdx_ ? ImVec4{0.2f, 0.35f, 0.25f, 1} : ImVec4{0.13f, 0.13f, 0.16f, 1}))
                                              {
                                                  selectedOffsetIdx_ = i;
                                                  state_.showOffset = false;
                                              }
                                      }
                                      ImGui::EndChild();
                                  });
                }

                // 修改弹窗
                if (state_.showModify)
                {
                    const auto result = gFloatingKeyboard->ConsumeResult(buf_.modify);
                    if (result == FloatingKeyboard::Result::Accepted && state_.modifyAddr && buf_.modify[0]) lastStatus_ = gSavedAddresses->write(state_.modifyAddr, buf_.modify);
                    if (result != FloatingKeyboard::Result::None)
                    {
                        state_.showModify = false;
                        state_.modifyAddr = 0;
                        buf_.modify[0] = 0;
                    }
                }

                if (state_.showSavedNote)
                {
                    const auto result = gFloatingKeyboard->ConsumeResult(buf_.savedNote);
                    if (result == FloatingKeyboard::Result::Accepted && state_.noteAddr) lastStatus_ = gSavedAddresses->setNote(state_.noteAddr, buf_.savedNote);
                    if (result != FloatingKeyboard::Result::None)
                    {
                        state_.showSavedNote = false;
                        state_.noteAddr = 0;
                        buf_.savedNote[0] = 0;
                    }
                }
            }
        }
        gFloatingKeyboard->Draw();
        return !state_.floating;
    }
};

inline std::unique_ptr<MainUI> gMainUI = nullptr;
