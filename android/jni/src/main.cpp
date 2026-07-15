
#include <imgui.h>
#include <android_imgui_input/imgui_floating_keyboard.h>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <functional>
#include <optional>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>
#include <span>

#include "memory_tool.h"
#include "driver.h"
#include "disassembler.h"
#include "read_write_test.h"
#include "http_server.h"
#include "touch_test.h"
#include "gyro_test.h"
#include "gnss_test.h"
#include "android_imgui_input/imgui_touch_input.h"
#include "android_draw/opengl_imgui_renderer.h"
#include "android_draw/vulkan_imgui_renderer.h"

// ============================================================================
// UI 构建器
// ============================================================================
class UIStyle
{
  public:
    float scale = 2.0f, margin = 40.0f, opacity = 1.0f;
    constexpr float S(float v) const noexcept
    {
        return v * scale;
    }
    void apply() const
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
    }
};

// ============================================================================
// 布局构建器
// ============================================================================
namespace UI
{
    inline void Space(float y)
    {
        ImGui::Dummy({0, y});
    }

    inline void Text(ImVec4 col, const char *fmt, ...)
    {
        va_list a;
        va_start(a, fmt);
        ImGui::TextColoredV(col, fmt, a);
        va_end(a);
    }

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

    // 一行多按钮，自动 SameLine
    struct BtnDef
    {
        const char *label;
        ImVec4 col;
        std::function<void()> action;
    };
    inline void ButtonRow(float totalW, float h, std::initializer_list<BtnDef> btns, float gap = 0)
    {
        float bw = (totalW - gap * (btns.size() - 1)) / btns.size();
        int i = 0;
        for (auto &b : btns)
        {
            if (i++ > 0) ImGui::SameLine();
            if (Btn(b.label, {bw, h}, b.col) && b.action) b.action();
        }
    }

    // 标签 + 值 行
    inline void LabelValue(ImVec4 labelCol, const char *label, ImVec4 valCol, const char *fmt, ...)
    {
        Text(labelCol, "%s", label);
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
  private:
    MemScanner &scanner_ = MemoryTool::Scanner();
    PointerManager &ptrManager_ = MemoryTool::Pointer();
    LockManager &lockManager_ = MemoryTool::Locks();
    MemViewer &memViewer_ = MemoryTool::Viewer();

    struct ScanParams
    {
        Types::DataType dataType = Types::DataType::I32;
        Types::FuzzyMode fuzzyMode = Types::FuzzyMode::Unknown;
        int page = 0;
        std::string lastStringPattern;
    } scanParams_;

    struct PtrParams
    {
        int depth = 3;
        bool useManual = false, useArray = false;
    } ptrParams_;

    struct SigParams
    {
        int range = 20, lastChanged = -1, lastTotal = 0, lastScanCount = -1;
    } sigParams_;

    struct BpParams
    {
        struct PointRow
        {
            char addr[32] = {};
            int type = 3;
            int scope = 2;
            int len = 4;
        };

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

    struct SyscallParams
    {
        int lastStatus = 0;
        bool hasResult = false;
        std::string log;
        std::chrono::steady_clock::time_point nextLogRefresh{};
    } syscallParams_;

    struct EnvParams
    {
        bool hasResult = false;
        bool success = false;
        int pid = 0;
        Driver::env_params info{};
    } envParams_;

    std::vector<std::string> offsetLabels_;
    int selectedOffsetIdx_ = 1;
    UIStyle style_;

    struct Buf
    {
        char pid[32] = {}, value[64] = {}, addAddr[32] = {}, base[32] = {}, page[16] = "20";
        char modify[64] = {}, memOffset[32] = {}, resultOffset[32] = {}, moduleSearch[64] = {}, envThread[16] = {};
        char ptrTarget[32] = {}, arrayBase[32] = {}, arrayCount[16] = "100", filterModule[64] = {};
        char sigScanAddr[32] = {}, sigVerifyAddr[32] = {};
        char viewAddr[32] = {};
    } buf_;

    struct State
    {
        int tab = 0, resultScrollIdx = 0;
        uintptr_t modifyAddr = 0;
        bool showModify = false, floating = false, dragging = false, dragMoved = false;
        ImVec2 floatPos = {50, 200}, dragOffset = {}, dragStartMouse = {};
        bool showType = false, showMode = false, showDepth = false, showOffset = false, showScale = false, showFormat = false;
        bool showBpType = false, showBpScope = false, showBpLen = false;
        int bpPopupPoint = -1;
    } state_;

    float S(float v) const
    {
        return style_.S(v);
    }

    static std::optional<uintptr_t> ParseHexAddress(const char *buf)
    {
        const auto addr = MemUtils::ParseUInt64(buf, 16);
        return addr.has_value() && *addr ? std::optional<uintptr_t>(static_cast<uintptr_t>(*addr)) : std::nullopt;
    }

    static int ParseIntOr(const char *buf, int fallback = 0)
    {
        char *end = nullptr;
        const long value = std::strtol(buf, &end, 10);
        return end != buf && *end == '\0' ? static_cast<int>(value) : fallback;
    }

    static std::string Hexadecimal(std::uint64_t value)
    {
        return std::format("{:X}", value);
    }

    static std::string Hex128(__uint128_t value)
    {
        return std::format("{:016X}{:016X}", static_cast<unsigned long long>(value >> 64), static_cast<unsigned long long>(value));
    }

    template <typename T> static T HwbpRead(Driver::bp_record &record, int regIndex)
    {
        return static_cast<T>(MemUtils::HwbpReadRegisterValue(record, regIndex));
    }

    static void CopyText(std::string_view text)
    {
        std::string temp(text);
        ImGui::SetClipboardText(temp.c_str());
    }

    Driver::bp_record *findHwbpRecordByFlatIndex(int recordIndex)
    {
        if (recordIndex < 0) return nullptr;

        auto &info = const_cast<Driver::break_point &>(dr->GetHwbpInfoRef());
        int flatIndex = 0;
        for (auto &point : info.points)
        {
            const int recordCount = std::clamp(point.record_count, 0, 0x100);
            if (recordIndex >= flatIndex && recordIndex < flatIndex + recordCount) return &point.records[recordIndex - flatIndex];
            flatIndex += recordCount;
        }
        return nullptr;
    }

    void openRegisterEdit(int recordIndex, int regIndex, std::string_view name, std::string_view hexValue)
    {
        bpParams_.editingRecordIdx = recordIndex;
        bpParams_.editingField = regIndex;
        std::snprintf(bpParams_.regEditBuf, sizeof(bpParams_.regEditBuf), "%.*s", static_cast<int>(std::min(hexValue.size(), sizeof(bpParams_.regEditBuf) - 1)), hexValue.data());
        const std::string title = std::format("修改 {} (Hex)", name);
        ImGuiFloatingKeyboard::Open(bpParams_.regEditBuf, title.c_str());
    }

    bool commitRegisterEdit(int recordIndex, int regIndex)
    {
        if (bpParams_.editingRecordIdx != recordIndex || bpParams_.editingField != regIndex) return false;
        const auto result = ImGuiFloatingKeyboard::ConsumeResult(bpParams_.regEditBuf);
        if (result == ImGuiFloatingKeyboard::Result::None) return false;

        if (result == ImGuiFloatingKeyboard::Result::Accepted)
        {
            if (const auto value = MemUtils::ParseUInt128(bpParams_.regEditBuf, 16); value.has_value())
            {
                if (auto *record = findHwbpRecordByFlatIndex(recordIndex)) MemUtils::HwbpWriteRegisterValue(*record, regIndex, *value);
            }
        }
        bpParams_.editingRecordIdx = -1;
        bpParams_.editingField = -1;
        bpParams_.regEditBuf[0] = 0;
        return true;
    }

    std::vector<Driver::bp_point> buildHwbpPointsFromRows()
    {
        static constexpr Driver::bp_type typeValues[] = {
            Driver::BP_BREAKPOINT_R,
            Driver::BP_BREAKPOINT_W,
            Driver::BP_BREAKPOINT_RW,
            Driver::BP_BREAKPOINT_X,
        };
        static constexpr Driver::bp_scope scopeValues[] = {
            Driver::BP_SCOPE_MAIN_THREAD,
            Driver::BP_SCOPE_OTHER_THREADS,
            Driver::BP_SCOPE_ALL_THREADS,
        };

        std::vector<Driver::bp_point> points;
        const int count = std::clamp(bpParams_.configPointCount, 1, 16);
        points.reserve(count);
        for (int i = 0; i < count; ++i)
        {
            const auto addr = ParseHexAddress(bpParams_.points[i].addr);
            if (!addr.has_value()) return {};

            const int typeIndex = std::clamp(bpParams_.points[i].type, 0, 3);
            const int scopeIndex = std::clamp(bpParams_.points[i].scope, 0, 2);
            const int len = std::clamp(bpParams_.points[i].len, 1, 8);

            Driver::bp_point point{};
            point.hit_addr = *addr;
            point.bt = typeValues[typeIndex];
            point.bs = scopeValues[scopeIndex];
            point.bl = static_cast<Driver::bp_len>(len);
            points.push_back(point);
        }
        return points;
    }

    void drawRegisterEditButton(const char *buttonId, int recordIndex, int regIndex, std::string_view name, std::string_view hexValue, ImVec2 size)
    {
        if (UI::Btn(buttonId, size, {0.4f, 0.3f, 0.15f, 1})) openRegisterEdit(recordIndex, regIndex, name, hexValue);
        commitRegisterEdit(recordIndex, regIndex);
    }

    // ---- 扫描逻辑 ----
    void startScan(std::string_view valueStr, bool isFirst)
    {
        scanParams_.page = 0;
        auto type = scanParams_.dataType;
        auto mode = scanParams_.fuzzyMode;
        auto pid = dr->GetGlobalPid();
        std::string valCopy(valueStr);

        if (mode == Types::FuzzyMode::Pointer)
        {
            type = Types::DataType::I64;
            auto parsed = MemUtils::ParseUInt64(valCopy, 16);
            if (!parsed) return;
            auto addr = MemUtils::Normalize(static_cast<uintptr_t>(*parsed));
            scanner_.startAsync<int64_t>(pid, static_cast<int64_t>(addr), type, mode, isFirst);
            return;
        }
        if (mode == Types::FuzzyMode::String)
        {
            if (valCopy.empty()) return;
            scanParams_.lastStringPattern = valCopy;
            scanner_.startStringAsync(pid, std::move(valCopy), isFirst);
            return;
        }

        MemUtils::DispatchType(type,
                               [&]<typename T>()
                               {
                                   if (mode == Types::FuzzyMode::Unknown)
                                   {
                                       scanner_.startAsync<T>(pid, T{}, type, mode, isFirst);
                                       return;
                                   }

                                   if (mode == Types::FuzzyMode::Increased || mode == Types::FuzzyMode::Decreased || mode == Types::FuzzyMode::Changed || mode == Types::FuzzyMode::Unchanged)
                                   {
                                       scanner_.startAsync<T>(pid, T{}, type, mode, isFirst);
                                       return;
                                   }

                                   if (mode == Types::FuzzyMode::Range)
                                   {
                                       const auto pos = valCopy.find('~');
                                       if (pos == std::string::npos) return;
                                       const auto minValue = MemUtils::ParseScanValue<T>(std::string_view(valCopy).substr(0, pos));
                                       const auto maxValue = MemUtils::ParseScanValue<T>(std::string_view(valCopy).substr(pos + 1));
                                       if (!minValue || !maxValue) return;
                                       scanner_.startAsync<T>(pid, *minValue, type, mode, isFirst, *maxValue);
                                       return;
                                   }

                                   const auto value = MemUtils::ParseScanValue<T>(valCopy);
                                   if (value) scanner_.startAsync<T>(pid, *value, type, mode, isFirst);
                               });
    }

    bool startPtrScan()
    {
        const auto target = ParseHexAddress(buf_.ptrTarget);
        if (!target) return false;

        uintptr_t manualBase = 0, arrayBase = 0;
        size_t arrayCount = 0;
        if (ptrParams_.useManual)
        {
            const auto parsed = ParseHexAddress(buf_.base);
            if (!parsed) return false;
            manualBase = *parsed;
        }
        else if (ptrParams_.useArray)
        {
            const auto parsedBase = ParseHexAddress(buf_.arrayBase);
            const auto parsedCount = MemUtils::ParseUInt64(buf_.arrayCount, 10);
            if (!parsedBase || !parsedCount || *parsedCount == 0 || *parsedCount > 1000000 || *parsedCount > std::numeric_limits<size_t>::max()) return false;
            arrayBase = *parsedBase;
            arrayCount = static_cast<size_t>(*parsedCount);
        }

        return ptrManager_.startAsync(dr->GetGlobalPid(), *target, ptrParams_.depth, (selectedOffsetIdx_ + 1) * 500, ptrParams_.useManual, manualBase, ptrParams_.useArray, arrayBase, arrayCount, buf_.filterModule);
    }

    void copyAddress(uintptr_t addr)
    {
        CopyText(Hexadecimal(addr));
    }

  public:
    MainUI()
    {
        for (int i = 500; i <= 100000; i += 500)
        {
            offsetLabels_.push_back(std::to_string(i));
        }
        snprintf(buf_.page, sizeof(buf_.page), "%d", Config::g_ItemsPerPage.load());
        if (int pid = dr->GetGlobalPid(); pid > 0) snprintf(buf_.pid, sizeof(buf_.pid), "%d", pid);
        SetInputBlocking(true);
    }

    ~MainUI()
    {
        Config::g_Running = false;
        MemoryTool::StopSyscallMonitor();
    }

    void draw()
    {
        style_.apply();
        if (state_.floating) drawFloatButton();
        else
        {
            float m = style_.margin;
            float w = RenderVK::displayInfo.width - 2 * m;
            float h = RenderVK::displayInfo.height - 2 * m;
            drawMainWindow(m, m, w, h);
            drawPopups(m, m, w, h);
        }
        ImGuiFloatingKeyboard::Draw();
    }

  private:
    // ---- 悬浮按钮 ----
    void drawFloatButton()
    {
        float sw = RenderVK::displayInfo.width, sh = RenderVK::displayInfo.height;
        float sz = S(65), m = style_.margin;
        state_.floatPos.x = std::clamp(state_.floatPos.x, m, sw - sz - m);
        state_.floatPos.y = std::clamp(state_.floatPos.y, m, sh - sz - m);
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
            if (ImGui::Button("M", {sz, sz}) && !state_.dragging && !releasedAfterDrag)
            {
                state_.floating = false;
                SetInputBlocking(true);
            }
        }
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }

    // ---- 主窗口 ----
    void drawMainWindow(float x, float y, float w, float h)
    {
        ImGui::SetNextWindowPos({x, y});
        ImGui::SetNextWindowSize({w, h});
        ImGui::PushStyleColor(ImGuiCol_WindowBg, Colors::BG_DARK);
        if (ImGui::Begin("##Main", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove))
        {
            float cw = ImGui::GetContentRegionAvail().x;
            drawTopBar(cw, S(55));
            UI::Space(S(4));
            float contentH = ImGui::GetContentRegionAvail().y - S(60) - S(4);
            drawContent(cw, contentH);
            UI::Space(S(4));
            drawTabs(cw, S(60));
        }
        ImGui::End();
        ImGui::PopStyleColor();
    }

    // ---- 顶栏 ----
    void drawTopBar(float w, float h)
    {
        UI::ColorChild(
            "Top", {w, h}, Colors::BG_PANEL,
            [&]
            {
                float bh = h - S(12);
                if (UI::Btn("收起", {S(55), bh}, Colors::BTN_MINIMIZE))
                {
                    state_.floating = true;
                    SetInputBlocking(false);
                }
                ImGui::SameLine();
                ImGui::SetCursorPosX((w - ImGui::CalcTextSize("内存扫描").x) / 2);
                ImGui::SetCursorPosY((h - ImGui::GetTextLineHeight()) / 2);
                ImGui::Text("内存扫描");
                ImGui::SameLine(w - (S(50) + S(85) + S(50) + S(18)));
                ImGui::SetCursorPosY(S(6));
                char sc[16];
                snprintf(sc, sizeof(sc), "%.0f%%", style_.scale * 100);
                if (ImGui::Button(sc, {S(50), bh})) state_.showScale = !state_.showScale;
                ImGui::SameLine();
                const bool pidAccepted = ImGuiFloatingKeyboard::InputButton(buf_.pid, "PID", {S(85), bh});
                ImGui::SameLine();
                if (pidAccepted && buf_.pid[0])
                {
                    int pid = ParseIntOr(buf_.pid);
                    if (pid > 0 && pid != dr->GetGlobalPid())
                    {
                        if (MemoryTool::SelectTarget(pid))
                        {
                            moduleRows_.clear();
                            envParams_ = {};
                            syscallParams_.log.clear();
                            syscallParams_.hasResult = false;
                        }
                    }
                }
                ImGui::SameLine();
                if (UI::Btn("退出", {S(50), bh}, Colors::BTN_EXIT)) Config::g_Running = false;
            },
            ImGuiWindowFlags_NoScrollbar);
    }

    // ---- 内容区 ----
    void drawContent(float w, float h)
    {
        using DrawFn = void (MainUI::*)();
        DrawFn tabs[] = {&MainUI::drawScanTab, &MainUI::drawResultTab, &MainUI::drawViewerTab, &MainUI::drawModuleTab, &MainUI::drawPointerTab, &MainUI::drawSignatureTab, &MainUI::drawBreakpointTab, &MainUI::drawSyscallTab, &MainUI::drawEnvTab};
        UI::ColorChild("Content", {w, h}, Colors::BG_MID, [&] { (this->*tabs[state_.tab])(); });
    }

    // ---- 标签栏 ----
    void drawTabs(float w, float h)
    {
        UI::ColorChild(
            "Tabs", {w, h}, Colors::BG_PANEL,
            [&]
            {
                constexpr int N = 9;
                float bw = (w - S(36)) / N;
                const char *labels[] = {"扫描", "结果", "浏览", "模块", "指针", "特征", "断点", "调用", "环境"};
                for (int i = 0; i < N; ++i)
                {
                    if (i > 0) ImGui::SameLine();
                    ImVec4 c = state_.tab == i ? Colors::BTN_ACTIVE : Colors::BTN_INACTIVE;
                    if (UI::Btn(labels[i], {bw, h - S(14)}, c))
                    {
                        state_.tab = i;
                        if (i == 5) dr->GetMemoryInfoRef();
                        if (i == 6) dr->GetHwbpInfoRef();
                        if (i == 2 && memViewer_.base()) memViewer_.refresh();
                    }
                }
            },
            ImGuiWindowFlags_NoScrollbar);
    }

    // ================================================================
    // 环境参数页
    // ================================================================
    void drawEnvTab()
    {
        const float w = ImGui::GetContentRegionAvail().x;
        const int pid = dr->GetGlobalPid();
        UI::Text(Colors::TITLE, "━━ 环境参数 ━━");
        UI::Space(S(8));
        UI::LabelValue(Colors::LABEL, "当前目标 PID: ", pid > 0 ? Colors::ADDR_GREEN : Colors::ERR, "%d", pid);
        UI::Space(S(10));

        ImGuiFloatingKeyboard::InputButton(buf_.envThread, "可选线程名 task->comm", {w, S(48)}, "线程名(可选)");
        UI::Space(S(8));
        ImGui::BeginDisabled(pid <= 0);
        if (UI::Btn("获取环境参数", {w, S(48)}, Colors::BTN_TEAL))
        {
            envParams_.pid = pid;
            envParams_.success = dr->GetEnvParams(buf_.envThread);
            envParams_.hasResult = true;
            if (envParams_.success) envParams_.info = dr->GetEnvParamsRef();
        }
        ImGui::EndDisabled();

        UI::Space(S(14));
        if (!envParams_.hasResult) UI::Text(Colors::HINT, "留空线程名可获取 PACGA；填写线程名时同时获取 TPIDR_EL0");
        else if (!envParams_.success) UI::Text(Colors::ERR, "环境参数获取失败");
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

    // ================================================================
    // 系统调用监控页
    // ================================================================
    void drawSyscallTab()
    {
        float w = ImGui::GetContentRegionAvail().x;
        const int targetPid = dr->GetGlobalPid();
        const int monitoredPid = MemoryTool::SyscallMonitorPid().load(std::memory_order_acquire);
        const bool active = monitoredPid > 0;
        const auto now = std::chrono::steady_clock::now();
        if (now >= syscallParams_.nextLogRefresh)
        {
            syscallParams_.log = SyscallLog::ReadDmesg();
            syscallParams_.nextLogRefresh = now + std::chrono::seconds(1);
        }

        UI::Text(Colors::TITLE, "━━ 系统调用监控 ━━");
        UI::Space(S(8));
        UI::LabelValue(Colors::LABEL, "当前目标 PID: ", targetPid > 0 ? Colors::ADDR_GREEN : Colors::ERR, "%d", targetPid);
        UI::LabelValue(Colors::LABEL, "监听状态: ", active ? Colors::OK : Colors::HINT, "%s", active ? "已监听" : "未监听");
        if (active) UI::LabelValue(Colors::LABEL, "监听 PID: ", Colors::ADDR_CYAN, "%d", monitoredPid);

        UI::Space(S(12));
        ImGui::BeginDisabled(targetPid <= 0 || active);
        if (UI::Btn("开始监听", {w, S(54)}, Colors::BTN_GREEN))
        {
            syscallParams_.lastStatus = MemoryTool::StartSyscallMonitor();
            syscallParams_.hasResult = true;
        }
        ImGui::EndDisabled();

        UI::Space(S(8));
        ImGui::BeginDisabled(!active);
        if (UI::Btn("取消监听", {w, S(54)}, Colors::BTN_RED))
        {
            syscallParams_.lastStatus = MemoryTool::StopSyscallMonitor();
            syscallParams_.hasResult = true;
        }
        ImGui::EndDisabled();

        UI::Space(S(12));
        if (syscallParams_.hasResult)
        {
            if (syscallParams_.lastStatus == 0) UI::Text(Colors::OK, "请求执行成功");
            else UI::Text(Colors::ERR, "请求执行失败，状态: %d", syscallParams_.lastStatus);
        }

        UI::Space(S(8));
        if (UI::Btn("刷新日志", {w, S(42)}, Colors::BTN_TEAL)) syscallParams_.nextLogRefresh = {};
        UI::Text(Colors::HINT, "实时筛选包含 lsdriver 的内核日志");
        if (ImGui::BeginChild("SyscallLog", {w, 0}, true, ImGuiWindowFlags_HorizontalScrollbar)) ImGui::TextUnformatted(syscallParams_.log.empty() ? "暂无系统调用日志" : syscallParams_.log.c_str());
        ImGui::EndChild();
    }

    // ================================================================
    // 扫描页
    // ================================================================
    void drawScanTab()
    {
        float w = ImGui::GetContentRegionAvail().x;
        bool isPtrMode = scanParams_.fuzzyMode == Types::FuzzyMode::Pointer;
        bool isStringMode = scanParams_.fuzzyMode == Types::FuzzyMode::String;
        const auto lockedType = scanner_.dataType();
        bool typeLocked = lockedType.has_value();
        bool firstModeValid = scanParams_.fuzzyMode != Types::FuzzyMode::Increased && scanParams_.fuzzyMode != Types::FuzzyMode::Decreased && scanParams_.fuzzyMode != Types::FuzzyMode::Changed && scanParams_.fuzzyMode != Types::FuzzyMode::Unchanged;
        if (lockedType) scanParams_.dataType = *lockedType;
        if (typeLocked || scanner_.isScanning()) state_.showType = false;

        // 数据类型
        UI::Text(Colors::LABEL, "数据类型:");
        if (isPtrMode || isStringMode)
        {
            ImGui::BeginDisabled();
            ImGui::Button(isPtrMode ? "Int64（指针模式）" : "字符串模式忽略类型", {w, S(45)});
            ImGui::EndDisabled();
        }
        else
        {
            ImGui::BeginDisabled(typeLocked || scanner_.isScanning());
            if (ImGui::Button(Types::Labels::TYPE[static_cast<int>(scanParams_.dataType)], {w, S(45)})) state_.showType = true;
            ImGui::EndDisabled();
        }

        UI::Space(S(6));
        UI::Text(Colors::LABEL, "搜索模式:");
        if (ImGui::Button(Types::Labels::FUZZY[static_cast<int>(scanParams_.fuzzyMode)], {w, S(45)})) state_.showMode = true;

        UI::Space(S(6));
        UI::Text(Colors::LABEL, isPtrMode ? "目标地址(Hex):" : "搜索数值:");
        ImGuiFloatingKeyboard::InputButton(buf_.value, isPtrMode ? "输入Hex地址..." : "点击输入...", {w, S(52)}, isPtrMode ? "目标地址(Hex)" : "数值");

        if (isPtrMode) UI::Text(Colors::INFO_CYAN, "输入16进制地址，搜索指向该地址的指针");
        else if (isStringMode) UI::Text(Colors::INFO_CYAN, "按原始字节匹配，区分大小写；再次扫描会在当前结果中继续过滤");
        else if (scanParams_.fuzzyMode == Types::FuzzyMode::Range) UI::Text(Colors::INFO_CYAN, "格式: 最小值~最大值  例: 0~45  -2~2  0.1~6.5");

        UI::Space(S(10));
        ImGui::BeginDisabled(scanner_.isScanning());
        float bw = (w - S(12)) / 3;
        ImGui::BeginDisabled(!firstModeValid);
        if (UI::Btn("首次扫描", {bw, S(52)}, Colors::BTN_GREEN)) startScan(buf_.value, true);
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (UI::Btn("再次扫描", {bw, S(52)}, Colors::BTN_BLUE)) startScan(buf_.value, false);
        ImGui::SameLine();
        if (UI::Btn("清空", {bw, S(52)}, Colors::BTN_RED)) scanner_.clear();
        ImGui::EndDisabled();

        UI::Space(S(6));
        if (scanner_.isScanning())
        {
            UI::Text(Colors::WARN, "扫描中...");
            ImGui::ProgressBar(scanner_.progress(), {w, S(18)});
        }
        else
        {
            scanner_.count() ? UI::Text(Colors::OK, "找到 %zu 个", scanner_.count()) : UI::Text(Colors::HINT, "暂无结果");
        }
    }

    // ================================================================
    // 结果页
    // ================================================================
    void drawResultTab()
    {
        if (scanner_.isScanning())
        {
            float w = ImGui::GetContentRegionAvail().x;
            UI::Text(Colors::WARN, "扫描中，结果页暂不可操作");
            ImGui::ProgressBar(scanner_.progress(), {w, S(18)});
            return;
        }

        size_t total = scanner_.count();
        float w = ImGui::GetContentRegionAvail().x, bh = S(40);
        const auto resultType = scanner_.dataType().value_or(scanParams_.dataType);
        const auto resultMode = scanner_.scanMode();
        const bool isPtrMode = resultMode == Types::FuzzyMode::Pointer;
        const bool isStringMode = scanner_.isStringScan();

        // 添加地址行
        ImGuiFloatingKeyboard::InputButton(buf_.addAddr, "Hex地址...", {w - S(76), bh}, "Hex地址");
        ImGui::SameLine();
        if (UI::Btn("添加", {S(70), bh}, Colors::BTN_GREEN))
        {
            if (auto addr = ParseHexAddress(buf_.addAddr))
            {
                scanner_.add(*addr);
                buf_.addAddr[0] = 0;
            }
        }
        if (!total)
        {
            UI::Text(Colors::HINT, "暂无结果");
            return;
        }

        int perPage = Config::g_ItemsPerPage.load();
        int maxPage = static_cast<int>((total - 1) / perPage);
        scanParams_.page = std::clamp(scanParams_.page, 0, maxPage);
        auto data = scanner_.getPage(scanParams_.page * perPage, perPage);

        // 翻页行
        UI::Space(S(4));
        float pgW = S(65);
        ImGui::BeginDisabled(scanParams_.page <= 0);
        if (ImGui::Button("上页", {pgW, bh}))
        {
            --scanParams_.page;
            state_.resultScrollIdx = 0;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();

        char info[64];
        snprintf(info, sizeof(info), "%d/%d (共%zu)", scanParams_.page + 1, maxPage + 1, scanner_.count());
        float infoW = w - pgW * 2 - S(12);
        UI::ColorChild(
            "PageInfo", {infoW, bh}, Colors::BG_PANEL,
            [&]
            {
                ImGui::SetCursorPos({(infoW - ImGui::CalcTextSize(info).x) / 2, (bh - ImGui::GetTextLineHeight()) / 2 - S(4)});
                ImGui::Text("%s", info);
            },
            ImGuiWindowFlags_NoScrollbar);
        ImGui::SameLine();

        ImGui::BeginDisabled(scanParams_.page >= maxPage);
        if (ImGui::Button("下页", {pgW, bh}))
        {
            ++scanParams_.page;
            state_.resultScrollIdx = 0;
        }
        ImGui::EndDisabled();

        UI::Space(S(4));
        ImGui::Text("每页:");
        ImGui::SameLine();
        if (ImGuiFloatingKeyboard::InputButton(buf_.page, "每页数量", {S(55), S(36)}) && buf_.page[0])
        {
            int v = ParseIntOr(buf_.page);
            if (v >= 1 && v <= 500)
            {
                if (v != Config::g_ItemsPerPage.load())
                {
                    Config::g_ItemsPerPage = v;
                    scanParams_.page = state_.resultScrollIdx = 0;
                }
            }
            else snprintf(buf_.page, sizeof(buf_.page), "%d", Config::g_ItemsPerPage.load());
        }
        ImGui::SameLine();

        bool anyLocked = std::ranges::any_of(data, [&](auto a) { return lockManager_.isLocked(a); });
        if (anyLocked)
        {
            if (UI::Btn("解锁页", {S(70), S(36)}, {0.2f, 0.25f, 0.42f, 1})) lockManager_.unlockBatch(data);
        }
        else if (isStringMode)
        {
            ImGui::BeginDisabled();
            UI::Btn("Lock", {S(70), S(36)}, {0.2f, 0.2f, 0.2f, 1});
            ImGui::EndDisabled();
        }
        else
        {
            if (UI::Btn("锁定页", {S(70), S(36)}, {0.42f, 0.28f, 0.1f, 1})) lockManager_.lockBatch(data, isPtrMode ? Types::DataType::I64 : resultType);
        }
        ImGui::SameLine();

        if (ImGuiFloatingKeyboard::ActionButton("偏移", {S(55), S(36)}, buf_.resultOffset, "偏移量(Hex,可负)", Colors::BTN_ORANGE) && buf_.resultOffset[0])
        {
            if (auto r = MemUtils::ParseHexOffset(buf_.resultOffset)) scanner_.applyOffset(r->offset, r->negative);
            buf_.resultOffset[0] = 0;
        }

        ImGui::Separator();
        float listH = ImGui::GetContentRegionAvail().y, contentW = w - S(56);
        int maxIdx = std::max(0, (int)data.size() - (int)(listH / S(93)));
        state_.resultScrollIdx = std::clamp(state_.resultScrollIdx, 0, maxIdx);
        if (ImGui::BeginChild("ListContent", {contentW, listH}, false, ImGuiWindowFlags_NoScrollbar))
        {
            int endIdx = state_.resultScrollIdx + (int)(listH / S(93)) + 1;
            for (int i = state_.resultScrollIdx; i < (int)data.size() && i < endIdx; ++i)
            {
                const auto addr = data[i];
                const bool locked = lockManager_.isLocked(addr), isPtr = isPtrMode, isString = isStringMode;
                const size_t previewLen = std::clamp(scanParams_.lastStringPattern.size(), size_t(16), size_t(64));
                ImGui::PushID((void *)addr);
                UI::ColorChild(
                    "Card", {contentW - S(10), S(85)}, locked ? Colors::LOCKED : Colors::BG_PANEL,
                    [&]
                    {
                        float cw = ImGui::GetContentRegionAvail().x;
                        UI::LabelValue({0.5f, 0.6f, 0.7f, 1}, "地址:", locked ? ImVec4{1, 0.5f, 0.5f, 1} : Colors::ADDR_GREEN, "%lX", addr);
                        ImGui::SameLine(cw * 0.45f);
                        if (isPtr) UI::LabelValue({0.5f, 0.6f, 0.7f, 1}, "指向:", Colors::VAL_YELLOW, "%s", MemUtils::ReadAsPointerString(addr).c_str());
                        else if (isString) UI::LabelValue({0.5f, 0.6f, 0.7f, 1}, "字符串:", Colors::VAL_YELLOW, "%s", MemUtils::ReadAsText(addr, previewLen).c_str());
                        else UI::LabelValue({0.5f, 0.6f, 0.7f, 1}, "数值:", Colors::VAL_YELLOW, "%s", MemUtils::ReadAsString(addr, resultType).c_str());
                        if (locked)
                        {
                            ImGui::SameLine();
                            UI::Text({1, 0.3f, 0.3f, 1}, "[锁定]");
                        }

                        UI::Space(S(4));
                        float bw = (cw - S(15)) / 4;
                        if (ImGui::Button("改", {bw, S(36)}))
                        {
                            state_.modifyAddr = addr;
                            const auto current = isPtr ? MemUtils::ReadAsPointerString(addr) : isString ? MemUtils::ReadAsText(addr, previewLen) : MemUtils::ReadAsString(addr, resultType);
                            std::snprintf(buf_.modify, sizeof(buf_.modify), "%s", current.c_str());
                            state_.showModify = true;
                            ImGuiFloatingKeyboard::Open(buf_.modify, isPtr ? "新地址(Hex)" : isString ? "新字符串" : "新数值");
                        }
                        ImGui::SameLine();
                        if (UI::Btn(locked ? "解锁" : "锁定", {bw, S(36)}, locked ? Colors::BTN_UNLOCK : Colors::BTN_LOCK) && !(isString && !locked)) lockManager_.toggle(addr, isPtr ? Types::DataType::I64 : resultType);
                        ImGui::SameLine();
                        if (UI::Btn("复制", {bw, S(36)}, Colors::BTN_COPY)) copyAddress(addr);
                        ImGui::SameLine();
                        if (UI::Btn("删除", {bw, S(36)}, Colors::BTN_DEL))
                        {
                            if (locked) lockManager_.unlock(addr);
                            scanner_.remove(addr);
                        }
                    },
                    ImGuiWindowFlags_NoScrollbar);
                ImGui::PopID();
                UI::Space(S(4));
            }
        }
        ImGui::EndChild();
        ImGui::SameLine();
        UI::ArrowScroll("ListArrows", S(50), listH, state_.resultScrollIdx, 0, maxIdx);
    }

    // ================================================================
    // 内存浏览页
    // ================================================================
    void drawViewerTab()
    {
        memViewer_.pollDisasm();

        float w = ImGui::GetContentRegionAvail().x, bh = S(42);
        float goW = S(55), ofsW = S(55), fmtW = S(110), refW = S(55);
        float inputW = w - goW - ofsW - fmtW - refW - S(24);

        // 工具栏：一行五按钮
        ImGuiFloatingKeyboard::InputButton(buf_.viewAddr, "输入Hex地址...", {inputW, bh}, "Hex地址");
        ImGui::SameLine();
        if (UI::Btn("跳转", {goW, bh}, {0.15f, 0.4f, 0.25f, 1}))
        {
            if (auto addr = ParseHexAddress(buf_.viewAddr)) memViewer_.open(*addr);
        }
        ImGui::SameLine();
        if (ImGuiFloatingKeyboard::ActionButton("偏移", {ofsW, bh}, buf_.memOffset, "偏移量(Hex,可负)", Colors::BTN_ORANGE) && buf_.memOffset[0])
        {
            memViewer_.applyOffset(buf_.memOffset);
            buf_.memOffset[0] = 0;
        }
        ImGui::SameLine();
        if (UI::Btn(Types::Labels::VIEW_FORMAT[static_cast<size_t>(memViewer_.format())], {fmtW, bh}, {0.18f, 0.25f, 0.35f, 1})) state_.showFormat = true;
        ImGui::SameLine();
        if (UI::Btn("刷新", {refW, bh}, Colors::BTN_TEAL)) memViewer_.refresh();

        // 基址信息
        UI::Space(S(2));
        if (memViewer_.base())
        {
            UI::LabelValue(Colors::ADDR_CYAN, "基址: ", Colors::ADDR_GREEN, "%lX", memViewer_.base());
            if (!memViewer_.readSuccess())
            {
                ImGui::SameLine();
                UI::Text(Colors::ERR, "[读取失败]");
            }
        }
        else
        {
            UI::Text(Colors::HINT, "输入地址后点击跳转开始浏览");
        }
        ImGui::Separator();
        if (!memViewer_.base()) return;

        // 读取失败提示
        if (!memViewer_.readSuccess())
        {
            UI::Space(S(20));
            ImGui::PushStyleColor(ImGuiCol_Text, {1, 0.5f, 0.5f, 1});
            ImGui::TextWrapped("无法读取内存，请检查：\n\n1. PID 是否正确并已同步\n"
                               "2. 目标地址是否有效\n3. 驱动是否正常工作\n4. 目标进程是否仍在运行");
            ImGui::PopStyleColor();
            UI::Space(S(10));
            if (ImGui::Button("重试", {S(80), S(36)})) memViewer_.refresh();
            return;
        }

        // 数据显示 + 箭头
        auto fmt = memViewer_.format();
        int64_t byteOffset = fmt == Types::ViewFormat::Disasm ? 4 : (fmt == Types::ViewFormat::Hex ? 16 : Types::GetViewSize(fmt));
        float cH = ImGui::GetContentRegionAvail().y, aW = S(50);
        float cW = ImGui::GetContentRegionAvail().x - aW - S(6);
        float rH = ImGui::GetTextLineHeight() + (fmt == Types::ViewFormat::Disasm ? S(14) : fmt == Types::ViewFormat::Hex ? S(8) : S(12));
        int rows = (int)(cH / rH) + 2;

        if (ImGui::BeginChild("MemContent", {cW, cH}, false, ImGuiWindowFlags_NoScrollbar))
        {
            if (fmt == Types::ViewFormat::Disasm)
            {
                if (memViewer_.disasmBusy()) UI::Text(Colors::HINT, "反汇编中...");
                else drawDisasmView(memViewer_.base(), memViewer_.getDisasm(), rows);
            }
            else if (fmt == Types::ViewFormat::Hex) drawHexDump(memViewer_.base(), memViewer_.buffer(), rows);
            else drawTypedView(fmt, memViewer_.base(), memViewer_.buffer(), rows);
        }
        ImGui::EndChild();
        ImGui::SameLine();

        if (ImGui::BeginChild("MemArrows", {aW, cH}, false, ImGuiWindowFlags_NoScrollbar))
        {
            ImGui::PushStyleColor(ImGuiCol_Button, {0.2f, 0.3f, 0.4f, 1});
            bool disableDisasmMove = fmt == Types::ViewFormat::Disasm && memViewer_.disasmBusy();
            if (disableDisasmMove) ImGui::BeginDisabled();
            if (ImGui::Button("▲##view_up", {aW, cH / 2 - S(3)})) memViewer_.applyOffset(-byteOffset);
            if (ImGui::Button("▼##view_down", {aW, cH / 2 - S(3)})) memViewer_.applyOffset(byteOffset);
            if (disableDisasmMove) ImGui::EndDisabled();
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();
    }

    // ================================================================
    // 模块页
    // ================================================================
    void refreshModuleRows()
    {
        const auto &info = dr->GetMemoryInfoRef();
        moduleRows_.clear();
        for (int i = 0; i < info.module_count; ++i)
        {
            const auto &mod = info.modules[i];
            const std::string name(MemUtils::BaseName(mod.name));
            for (int j = 0; j < mod.seg_count; ++j)
            {
                const auto &seg = mod.segs[j];
                moduleRows_.push_back({name, seg.index, seg.prot, seg.start, seg.end});
            }
        }
    }

    void drawModuleTab()
    {
        float w = ImGui::GetContentRegionAvail().x;
        ImGuiFloatingKeyboard::InputButton(buf_.moduleSearch, "模块名或Dump范围", {w, S(42)}, "模块名，或地址范围如 0x5000-0x6000");
        UI::Space(S(4));
        if (UI::Btn("刷新模块", {w, S(48)}, Colors::BTN_TEAL)) refreshModuleRows();
        UI::Space(S(6));
        if (UI::Btn("Dump 模块/内存范围 (保存至 /sdcard/dump/)", {w, S(48)}, Colors::BTN_PURPLE))
        {
            if (strlen(buf_.moduleSearch) > 0)
            {
                std::string target = buf_.moduleSearch;
                Config::CpuThreadPool().detach_task([target] { dr->DumpMemory(target); });
            }
        }
        UI::Space(S(6));

        if (ImGui::BeginChild("ModList", {0, 0}, false))
        {
            if (moduleRows_.empty())
            {
                UI::Text(Colors::HINT, "暂无模块，请手动刷新");
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
                            UI::Text({0.7f, 0.85f, 1, 1}, "%.*s", (int)name.size(), name.data());
                            seg.index == -1 ? UI::Text({0.9f, 0.6f, 0.3f, 1}, "Segment: BSS  %c%c%c", (seg.prot & 1) ? 'R' : '-', (seg.prot & 2) ? 'W' : '-', (seg.prot & 4) ? 'X' : '-') : UI::Text(Colors::ADDR_GREEN, "Segment: %d  %c%c%c", seg.index, (seg.prot & 1) ? 'R' : '-', (seg.prot & 2) ? 'W' : '-', (seg.prot & 4) ? 'X' : '-');
                            UI::Text(Colors::HINT, "范围: ");
                            ImGui::SameLine();
                            UI::Text({0.4f, 1, 0.4f, 1}, "%llX - ", (unsigned long long)seg.start);
                            ImGui::SameLine();
                            UI::Text({1, 0.6f, 0.4f, 1}, "%llX", (unsigned long long)seg.end);
                        },
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize);
                    ImGui::PopID();
                    UI::Space(S(4));
                }
                if (!displayCount) UI::Text({0.6f, 0.4f, 0.4f, 1}, "未找到匹配 \"%s\" 的模块", buf_.moduleSearch);
            }
        }
        ImGui::EndChild();
    }

    // ================================================================
    // 指针扫描页
    // ================================================================
    void drawPointerTab()
    {
        float w = ImGui::GetContentRegionAvail().x, bh = S(45);
        const auto pointerState = ptrManager_.state();
        const bool busy = pointerState.operation != PointerManager::Operation::Idle;
        ImGui::PushID("PtrScan");
        UI::Text(Colors::TITLE, "━━ 指针扫描 ━━");
        UI::Space(S(4));

        ImGui::BeginDisabled(busy);
        ImGui::Text("目标地址:");
        ImGuiFloatingKeyboard::InputButton(buf_.ptrTarget, "点击输入Hex", {w, bh}, "目标地址(Hex)");
        UI::Space(S(4));

        ImGui::Text("深度:");
        ImGui::SameLine();
        char dLbl[8];
        snprintf(dLbl, sizeof(dLbl), "%d层", ptrParams_.depth);
        if (ImGui::Button(dLbl, {S(70), bh})) state_.showDepth = true;
        ImGui::SameLine();
        ImGui::Text("偏移:");
        ImGui::SameLine();
        if (ImGui::Button(offsetLabels_[selectedOffsetIdx_].c_str(), {S(70), bh})) state_.showOffset = true;

        UI::Space(S(4));
        UI::Text(Colors::LABEL, "指定模块 (可选):");
        ImGuiFloatingKeyboard::InputButton(buf_.filterModule, "全部模块", {w - S(60), bh}, "模块名(如il2cpp)");
        ImGui::SameLine();
        if (ImGui::Button("清##scanFilter", {S(50), bh})) buf_.filterModule[0] = 0;

        ImGui::Checkbox("手动基址##scan", &ptrParams_.useManual);
        if (ptrParams_.useManual)
        {
            ptrParams_.useArray = false;
            ImGuiFloatingKeyboard::InputButton(buf_.base, "基址(Hex)##scanBase", {w, bh}, "Hex基址");
        }
        ImGui::Checkbox("数组基址##scan", &ptrParams_.useArray);
        if (ptrParams_.useArray)
        {
            ptrParams_.useManual = false;
            float hw = (w - S(6)) / 2;
            ImGuiFloatingKeyboard::InputButton(buf_.arrayBase, "数组地址(Hex)", {hw, bh}, "数组首地址");
            ImGui::SameLine();
            ImGuiFloatingKeyboard::InputButton(buf_.arrayCount, "数量", {hw, bh}, "元素数量");
        }

        UI::Space(S(6));
        UI::Btn("开始扫描", {w, S(48)}, Colors::BTN_GREEN) && startPtrScan();

        UI::Space(S(12));
        ImGui::Separator();
        UI::Space(S(8));
        UI::Text({0.6f, 0.7f, 0.8f, 1}, "文件操作 (Pointer.bin)");
        UI::Space(S(4));
        UI::ButtonRow(w, S(40), {{"开始对比", Colors::BTN_PURPLE, [&] { ptrManager_.MergeBins(); }}, {"格式化输出", {0.45f, 0.35f, 0.2f, 1}, [&] { ptrManager_.ExportToTxt(); }}}, S(8));
        ImGui::EndDisabled();

        UI::Space(S(8));
        if (busy)
        {
            UI::Text(Colors::WARN, "%.*s...", static_cast<int>(PointerManager::OperationName(pointerState.operation).size()), PointerManager::OperationName(pointerState.operation).data());
            ImGui::ProgressBar(pointerState.progress, {w, S(22)});
        }
        else if (pointerState.completed && !pointerState.success)
        {
            UI::Text(Colors::ERR, "%s", pointerState.error.empty() ? "指针操作失败" : pointerState.error.c_str());
        }
        else if (pointerState.completed)
        {
            UI::Text(Colors::OK, "操作完成，指针链数量: %zu", pointerState.count);
        }
        else UI::Text(Colors::HINT, "扫描结果保存到 Pointer.bin");
        ImGui::PopID();
    }

    // ================================================================
    // 特征码页
    // ================================================================
    void drawSignatureTab()
    {
        float w = ImGui::GetContentRegionAvail().x, bh = S(45);

        // 扫描部分
        UI::Text(Colors::TITLE, "━━ 特征码扫描 ━━");
        UI::Space(S(4));
        ImGui::Text("目标地址:");
        ImGuiFloatingKeyboard::InputButton(buf_.sigScanAddr, "点击输入Hex", {w, bh}, "目标地址(Hex)");
        UI::Space(S(4));
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

        UI::Space(S(8));
        if (UI::Btn("扫描保存", {w, S(48)}, Colors::BTN_GREEN))
        {
            if (auto addr = ParseHexAddress(buf_.sigScanAddr)) SignatureScanner::ScanAddressSignature(*addr, sigParams_.range);
        }
        UI::Text(Colors::HINT, "保存到 Signature.txt");

        // 过滤部分
        UI::Space(S(20));
        ImGui::Separator();
        UI::Space(S(10));
        UI::Text(Colors::TITLE, "━━ 特征码过滤 ━━");
        UI::Space(S(4));
        ImGui::Text("过滤地址:");
        ImGuiFloatingKeyboard::InputButton(buf_.sigVerifyAddr, "点击输入Hex", {w, bh}, "过滤地址(Hex)");
        UI::Space(S(8));

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
            sigParams_.lastChanged == 0 ? UI::Text(Colors::OK, "完美! 无变动 (%d字节)", sigParams_.lastTotal) : UI::Text(Colors::WARN, "变动: %d/%d (已更新)", sigParams_.lastChanged, sigParams_.lastTotal);
        }
        else if (sigParams_.lastChanged == -2) UI::Text(Colors::ERR, "失败! 检查Signature.txt");

        UI::Space(S(10));
        if (UI::Btn("扫描特征码", {w, S(48)}, Colors::BTN_PURPLE)) sigParams_.lastScanCount = (int)SignatureScanner::ScanSignatureFromFile().size();
        if (sigParams_.lastScanCount >= 0)
        {
            sigParams_.lastScanCount == 0 ? UI::Text(Colors::ERR, "未找到匹配地址") : UI::Text({0.5f, 0.9f, 1, 1}, "找到 %d 个地址", sigParams_.lastScanCount);
        }
        UI::Text(Colors::HINT, "结果保存到 Signature.txt");
    }

    // ================================================================
    // 断点页
    // ================================================================
    void drawBreakpointTab()
    {
        float w = ImGui::GetContentRegionAvail().x, bh = S(45);

        UI::Text(Colors::TITLE, "━━ 断点 ━━");
        UI::Space(S(4));

        // 硬件信息
        const auto &info = dr->GetHwbpInfoRef();
        const auto &activeMode = MemoryTool::HwbpMode();
        const bool anyBpActive = !activeMode.empty();
        int activePointCount = 0;
        uintptr_t firstAddress = 0;
        for (const auto &point : info.points)
        {
            if (!point.hit_addr) continue;
            if (!firstAddress) firstAddress = point.hit_addr;
            ++activePointCount;
        }
        UI::LabelValue(Colors::ADDR_CYAN, "执行断点寄存器: ", Colors::ADDR_GREEN, "%llu", (unsigned long long)info.num_brps);
        ImGui::SameLine();
        UI::LabelValue(Colors::ADDR_CYAN, "  访问断点寄存器: ", Colors::ADDR_GREEN, "%llu", (unsigned long long)info.num_wrps);
        UI::Text(activeMode == "ptebp" ? Colors::OK : Colors::HINT, activeMode == "ptebp" ? "PTEBP: 已激活" : "PTEBP: 未激活");
        UI::Text(activeMode == "stepbp" ? Colors::OK : Colors::HINT, activeMode == "stepbp" ? "STEPBP: 已激活" : "STEPBP: 未激活");

        UI::Space(S(6));
        ImGui::Separator();
        UI::Space(S(6));

        // 配置
        static const char *bpTypeLabels[] = {"读取", "写入", "读写", "执行"};
        static const char *bpScopeLabels[] = {"主线程", "子线程", "全部"};
        static const char *bpLenLabels[] = {"1字节", "2字节", "3字节", "4字节", "5字节", "6字节", "7字节", "8字节"};
        ImGui::Text("points:");
        float addrW = std::max(S(120), w - S(222));
        float typeW = S(58), scopeW = S(58), lenW = S(66);
        for (int i = 0; i < bpParams_.configPointCount; ++i)
        {
            auto &row = bpParams_.points[i];
            ImGui::PushID(i);
            UI::Text(Colors::LABEL, "P%d", i);
            ImGui::SameLine();
            ImGuiFloatingKeyboard::InputButton(row.addr, "地址", {addrW, bh}, "断点地址(Hex)");
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
            UI::Space(S(4));
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
        UI::Space(S(8));

        // 操作按钮
        float halfW = (w - S(8)) / 2;
        auto setBp = [&](const char *label, ImVec4 color, const char *mode, auto set)
        {
            ImGui::BeginDisabled(anyBpActive);
            if (UI::Btn(label, {halfW, S(52)}, color))
            {
                auto points = buildHwbpPointsFromRows();
                if (!points.empty() && set(std::span<const Driver::bp_point>(points)) == 0)
                {
                    MemoryTool::HwbpMode() = mode;
                }
            }
            ImGui::EndDisabled();
        };
        auto removeBp = [&](const char *label, bool disabled, auto remove)
        {
            ImGui::BeginDisabled(disabled);
            if (UI::Btn(label, {halfW, S(52)}, {0.5f, 0.15f, 0.15f, 1}))
            {
                remove();
                MemoryTool::HwbpMode().clear();
            }
            ImGui::EndDisabled();
        };

        setBp("设置HWBP", Colors::BTN_GREEN, "hwbp", [&](auto points) { return dr->SetProcessHwbpRef(points); });
        ImGui::SameLine();
        removeBp("移除HWBP", activeMode != "hwbp", [&] { dr->RemoveProcessHwbpRef(); });
        UI::Space(S(6));
        setBp("设置PTEBP", Colors::BTN_TEAL, "ptebp", [&](auto points) { return dr->SetProcessPtebpRef(points); });
        ImGui::SameLine();
        removeBp("移除PTEBP", activeMode != "ptebp", [&] { dr->RemoveProcessPtebpRef(); });
        UI::Space(S(6));
        setBp("设置STEPBP", Colors::BTN_BLUE, "stepbp", [&](auto points) { return dr->SetProcessStepbpRef(points); });
        ImGui::SameLine();
        removeBp("移除STEPBP", activeMode != "stepbp", [&] { dr->RemoveProcessStepbpRef(); });

        UI::Space(S(8));
        const char *activeModeLabel = activeMode == "stepbp" ? "STEPBP" : (activeMode == "ptebp" ? "PTEBP" : "HWBP");
        anyBpActive ? UI::Text(Colors::OK, "● 断点已激活  地址数: %d  首地址: 0x%lX  模式: %s", activePointCount, firstAddress, activeModeLabel) : UI::Text(Colors::HINT, "○ 断点未激活");
        for (const auto &point : info.points)
        {
            if (point.hit_addr) UI::Text(Colors::ADDR_CYAN, "监控地址: 0x%llX", (unsigned long long)point.hit_addr);
        }

        UI::Space(S(8));
        ImGui::Separator();
        UI::Space(S(6));
        UI::Text(Colors::TITLE, "━━ 命中信息 ━━");
        UI::Space(S(4));

        bool hasHitAddrPoint = false;
        for (const auto &point : info.points)
        {
            if (point.hit_addr) hasHitAddrPoint = true;
        }

        if (hasHitAddrPoint) drawBpRecords(info, w);
        else UI::Text(Colors::HINT, "暂无命中记录");
    }

    void drawBpRecords(const Driver::break_point &info, float w)
    {
        uint64_t totalHits = 0;
        int totalPointCount = 0;
        int totalRecordCount = 0;
        for (const auto &point : info.points)
        {
            const int recordCount = std::clamp(point.record_count, 0, 0x100);
            if (point.hit_addr) totalPointCount++;
            for (int r = 0; r < recordCount; ++r)
            {
                auto &rec = const_cast<Driver::bp_record &>(point.records[r]);
                MemUtils::HwbpRequestAll(rec);
                totalHits += HwbpRead<std::uint64_t>(rec, Driver::IDX_HIT_COUNT);
                totalRecordCount++;
            }
        }
        UI::Text(Colors::WARN, "point数: %d  record数: %d  总命中: %llu", totalPointCount, totalRecordCount, (unsigned long long)totalHits);
        UI::Space(S(6));

        static bool pointExpandState[16] = {};
        static bool recordsExpandState[16] = {};
        static bool recordExpandState[16 * 0x100] = {};
        int deleteRecordIdx = -1;
        int deletePointStart = -1;
        int deletePointCount = 0;
        int flatIndex = 0;

        for (int p = 0; p < 16; ++p)
        {
            const auto &point = info.points[p];
            const int recordCount = std::clamp(point.record_count, 0, 0x100);
            const int pointFlatStart = flatIndex;
            if (!point.hit_addr)
            {
                flatIndex += recordCount;
                continue;
            }

            uint64_t pointHits = 0;
            for (int r = 0; r < recordCount; ++r)
            {
                auto &rec = const_cast<Driver::bp_record &>(point.records[r]);
                pointHits += HwbpRead<std::uint64_t>(rec, Driver::IDX_HIT_COUNT);
            }

            ImGui::PushID(p);
            const float deletePointW = S(78);
            const float expandPointW = S(55);
            UI::Text(Colors::ADDR_CYAN, "hit_addr:0x%llX  point[%d]  records:%d  总命中:%llu", (unsigned long long)point.hit_addr, p, recordCount, (unsigned long long)pointHits);
            ImGui::SameLine(w - deletePointW);
            if (UI::Btn("删point", {deletePointW, S(32)}, Colors::BTN_DEL))
            {
                deletePointStart = pointFlatStart;
                deletePointCount = recordCount;
            }
            ImGui::SameLine(w - deletePointW - expandPointW - S(4));
            if (UI::Btn(pointExpandState[p] ? "收起" : "展开", {expandPointW, S(32)}, Colors::BTN_BLUE)) pointExpandState[p] = !pointExpandState[p];

            if (pointExpandState[p])
            {
                ImGui::Indent(S(8));
                UI::Text(Colors::TITLE, "records");
                ImGui::SameLine();
                if (UI::Btn(recordsExpandState[p] ? "收起##records" : "展开##records", {S(80), S(30)}, Colors::BTN_TEAL)) recordsExpandState[p] = !recordsExpandState[p];

                if (recordsExpandState[p])
                {
                    ImGui::Indent(S(8));
                    if (recordCount <= 0)
                    {
                        UI::Text(Colors::HINT, "暂无 record");
                    }
                    for (int r = 0; r < recordCount; ++r)
                    {
                        const int recordFlatIndex = pointFlatStart + r;
                        auto &rec = const_cast<Driver::bp_record &>(point.records[r]);
                        const auto pc = HwbpRead<std::uint64_t>(rec, Driver::IDX_PC);
                        const auto hitCount = HwbpRead<std::uint64_t>(rec, Driver::IDX_HIT_COUNT);
                        ImGui::PushID(recordFlatIndex);
                        const float deleteRecordW = S(72);
                        const float expandRecordW = S(55);

                        UI::Text({0.7f, 0.85f, 1, 1}, "record[%d:%d]  PC:0x%llX  命中:%llu", p, r, (unsigned long long)pc, (unsigned long long)hitCount);
                        ImGui::SameLine(w - deleteRecordW);
                        if (UI::Btn("删record", {deleteRecordW, S(32)}, Colors::BTN_DEL)) deleteRecordIdx = recordFlatIndex;
                        ImGui::SameLine(w - deleteRecordW - expandRecordW - S(4));
                        if (UI::Btn(recordExpandState[recordFlatIndex] ? "收起" : "展开", {expandRecordW, S(32)}, {0.2f, 0.3f, 0.45f, 1})) recordExpandState[recordFlatIndex] = !recordExpandState[recordFlatIndex];

                        if (recordExpandState[recordFlatIndex])
                        {
                            ImGui::Indent(S(8));
                            drawBpRecordDetail(rec, recordFlatIndex);
                            ImGui::Unindent(S(8));
                        }

                        UI::Space(S(4));
                        ImGui::Separator();
                        UI::Space(S(4));
                        ImGui::PopID();
                    }
                    ImGui::Unindent(S(8));
                }

                ImGui::Unindent(S(8));
            }

            UI::Space(S(4));
            ImGui::Separator();
            UI::Space(S(4));
            ImGui::PopID();
            flatIndex += recordCount;
        }
        if (deleteRecordIdx >= 0)
        {
            dr->RemoveHwbpRecord(deleteRecordIdx);
            bpParams_.editingRecordIdx = -1;
            bpParams_.editingField = -1;
        }
        if (deletePointStart >= 0 && deletePointCount > 0)
        {
            for (int i = deletePointCount - 1; i >= 0; --i) dr->RemoveHwbpRecord(deletePointStart + i);
            bpParams_.editingRecordIdx = -1;
            bpParams_.editingField = -1;
        }
    }

    void drawBpRecordDetail(const Driver::bp_record &rec, int r)
    {
        auto &show = const_cast<Driver::bp_record &>(rec);

        auto scalarLine = [&](const char *name, int regIndex, bool narrow32 = false)
        {
            const auto val = narrow32 ? HwbpRead<std::uint32_t>(show, regIndex) : HwbpRead<std::uint64_t>(show, regIndex);
            const auto hex = Hexadecimal(val);
            UI::Text({0.7f, 0.85f, 1, 1}, "%s: ", name);
            ImGui::SameLine();
            UI::Text(Colors::ADDR_GREEN, "0x%llX", (unsigned long long)val);
            ImGui::SameLine();

            char id[32];
            snprintf(id, sizeof(id), "复制##%s%d", name, r);
            if (UI::Btn(id, {S(50), S(28)}, Colors::BTN_COPY)) CopyText(hex);

            ImGui::SameLine();
            snprintf(id, sizeof(id), "改##%s%d", name, r);
            drawRegisterEditButton(id, r, regIndex, name, hex, {S(40), S(28)});
        };

        scalarLine("PC", Driver::IDX_PC);
        scalarLine("LR", Driver::IDX_LR);
        scalarLine("SP", Driver::IDX_SP);
        UI::Space(S(4));

        auto editableLine = [&](const char *label, const char *button, int regIndex, bool decimal = false)
        {
            const auto value = HwbpRead<std::uint64_t>(show, regIndex);
            decimal ? UI::Text(Colors::LABEL, "%s: %llu", label, (unsigned long long)value) : UI::Text(Colors::LABEL, "%s: 0x%llX", label, (unsigned long long)value);
            ImGui::SameLine();
            drawRegisterEditButton(button, r, regIndex, label, Hexadecimal(value), {S(40), S(28)});
        };

        editableLine("PSTATE", "改##pst", Driver::IDX_PSTATE);
        editableLine("SYSCALL", "改##syscall", Driver::IDX_SYSCALLNO, true);
        editableLine("ORIG_X0", "改##origx0", Driver::IDX_ORIG_X0);
        const auto hitCount = HwbpRead<std::uint64_t>(show, Driver::IDX_HIT_COUNT);
        UI::Text(Colors::WARN, "命中次数: %llu", (unsigned long long)hitCount);
        UI::Space(S(6));

        auto registerTable = [&](const char *title, const char *idPrefix, int count, int idOffset, auto &&drawRow)
        {
            UI::Text(Colors::TITLE, "%s", title);
            UI::Space(S(4));
            const auto tableId = std::format("{}##{}", idPrefix, r);
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {S(4), S(4)});
            if (ImGui::BeginTable(tableId.c_str(), 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
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
                          const auto value = HwbpRead<std::uint64_t>(show, regIndex);
                          const auto hex = Hexadecimal(value);
                          ImGui::TableSetColumnIndex(0);
                          UI::Text({0.7f, 0.85f, 1, 1}, "X%d", i);
                          ImGui::TableSetColumnIndex(1);
                          UI::Text(Colors::ADDR_GREEN, "0x%llX", (unsigned long long)value);
                          ImGui::TableSetColumnIndex(2);
                          if (UI::Btn("复制", {S(42), S(28)}, Colors::BTN_COPY)) CopyText(hex);
                          ImGui::TableSetColumnIndex(3);
                          drawRegisterEditButton("改", r, regIndex, std::format("X{}", i), hex, {S(42), S(28)});
                      });

        UI::Space(S(6));
        scalarLine("FPSR", Driver::IDX_FPSR, true);
        scalarLine("FPCR", Driver::IDX_FPCR, true);
        UI::Space(S(4));

        registerTable("━━ 浮点/SIMD 寄存器 ━━", "VRegs", 32, 32,
                      [&](int i)
                      {
                          const int regIndex = Driver::IDX_Q0 + i;
                          const auto value = MemUtils::HwbpReadRegisterValue(show, regIndex);
                          const auto hex = Hex128(value);
                          ImGui::TableSetColumnIndex(0);
                          UI::Text({0.7f, 0.85f, 1, 1}, "V%d", i);
                          ImGui::TableSetColumnIndex(1);
                          UI::Text(Colors::ADDR_GREEN, "%016llX_%016llX", (unsigned long long)(value >> 64), (unsigned long long)value);
                          ImGui::TableSetColumnIndex(2);
                          if (UI::Btn("复制", {S(42), S(28)}, Colors::BTN_COPY)) CopyText(hex);
                          ImGui::TableSetColumnIndex(3);
                          drawRegisterEditButton("改", r, regIndex, std::format("V{}", i), hex, {S(42), S(28)});
                      });
    }

    // ================================================================
    // 弹窗统一管理
    // ================================================================
    void drawPopups(float sx, float sy, float sw, float sh)
    {
        // 缩放弹窗
        if (state_.showScale)
        {
            drawListPopup("缩放", &state_.showScale, sx, sy, sw, sh, S(180), S(210),
                          [&](float fw)
                          {
                              ImGui::Text("不透明度: %.0f%%", style_.opacity * 100);
                              ImGui::SliderFloat("##o", &style_.opacity, 0.2f, 1.0f, "");
                              ImGui::Text("UI: %.0f%%", style_.scale * 100);
                              ImGui::SliderFloat("##s", &style_.scale, 0.5f, 2.0f, "");
                              float bw = fw / 3 - S(3);
                              if (ImGui::Button("75%", {bw, S(28)})) style_.scale = 0.75f;
                              ImGui::SameLine();
                              if (ImGui::Button("100%", {bw, S(28)})) style_.scale = 1.0f;
                              ImGui::SameLine();
                              if (ImGui::Button("150%", {bw, S(28)})) style_.scale = 1.5f;
                              ImGui::Text("边距: %.0f", style_.margin);
                              ImGui::SliderFloat("##m", &style_.margin, 0, 80, "");
                          });
        }

        // 通用选择器
        auto doSelector = [&](const char *title, bool *show, auto items, int count, auto *sel)
        {
            int s = static_cast<int>(*sel);
            drawListPopup(title, show, sx, sy, sw, sh, sw * 0.75f, std::min(count * (S(42) + S(4)) + S(50), sh * 0.7f),
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
        if (state_.showMode) doSelector("模式", &state_.showMode, Types::Labels::FUZZY.data(), (int)Types::Labels::FUZZY.size(), &scanParams_.fuzzyMode);
        if (state_.showFormat)
        {
            auto fmt = memViewer_.format();
            auto oldFmt = fmt;
            doSelector("格式", &state_.showFormat, Types::Labels::VIEW_FORMAT.data(), (int)Types::ViewFormat::Count, &fmt);
            if (fmt != oldFmt) memViewer_.setFormat(fmt);
        }
        if (state_.bpPopupPoint >= 0 && state_.bpPopupPoint < bpParams_.configPointCount)
        {
            auto &row = bpParams_.points[state_.bpPopupPoint];
            if (state_.showBpType)
            {
                static const char *items[] = {"读取", "写入", "读写", "执行"};
                int selected = std::clamp(row.type, 0, 3);
                doSelector("断点类型", &state_.showBpType, items, 4, &selected);
                row.type = selected;
            }
            if (state_.showBpScope)
            {
                static const char *items[] = {"主线程", "子线程", "全部"};
                int selected = std::clamp(row.scope, 0, 2);
                doSelector("线程范围", &state_.showBpScope, items, 3, &selected);
                row.scope = selected;
            }
            if (state_.showBpLen)
            {
                static const char *items[] = {"1字节", "2字节", "3字节", "4字节", "5字节", "6字节", "7字节", "8字节"};
                int selected = std::clamp(row.len, 1, 8) - 1;
                doSelector("监控长度", &state_.showBpLen, items, 8, &selected);
                row.len = selected + 1;
            }
        }
        // 深度选择
        if (state_.showDepth)
        {
            drawListPopup("深度", &state_.showDepth, sx, sy, sw, sh, S(160), S(320),
                          [&](float fw)
                          {
                              for (int i = 1; i <= 20; ++i)
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
            drawListPopup("偏移", &state_.showOffset, sx, sy, sw, sh, S(160), std::min((float)offsetLabels_.size() * S(32) + S(40), sh * 0.6f),
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
            const auto result = ImGuiFloatingKeyboard::ConsumeResult(buf_.modify);
            if (result == ImGuiFloatingKeyboard::Result::Accepted && state_.modifyAddr && strlen(buf_.modify) && !scanner_.isScanning())
            {
                if (scanner_.scanMode() == Types::FuzzyMode::Pointer) MemUtils::WritePointerFromString(state_.modifyAddr, buf_.modify);
                else if (scanner_.isStringScan()) MemUtils::WriteText(state_.modifyAddr, buf_.modify);
                else MemUtils::WriteFromString(state_.modifyAddr, scanner_.dataType().value_or(scanParams_.dataType), buf_.modify);
            }
            if (result != ImGuiFloatingKeyboard::Result::None)
            {
                state_.showModify = false;
                state_.modifyAddr = 0;
                buf_.modify[0] = 0;
            }
        }
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

    // ================================================================
    // 内存视图渲染 (保持不变，已经很紧凑)
    // ================================================================
    void drawTypedView(Types::ViewFormat format, uintptr_t base, std::span<const uint8_t> buffer, int rows)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {S(6), S(6)});
        if (ImGui::BeginTable("Typed", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("地址", ImGuiTableColumnFlags_WidthFixed, S(100));
            ImGui::TableSetupColumn("数值", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("存", ImGuiTableColumnFlags_WidthFixed, S(50));
            ImGui::TableSetupColumn("跳", ImGuiTableColumnFlags_WidthFixed, S(50));
            ImGui::TableHeadersRow();
            size_t step = Types::GetViewSize(format);
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
                UI::Text(i == 0 ? ImVec4{0.4f, 1, 0.4f, 1} : Colors::ADDR_CYAN, "%lX", addr);
                ImGui::TableSetColumnIndex(1);
                switch (format)
                {
                case Types::ViewFormat::Hexadecimal:
                    ptrVal = *(const uint64_t *)p;
                    UI::Text({0.6f, 1, 0.6f, 1}, "%lX", ptrVal);
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
                if (UI::Btn("存", {S(42), S(28)}, {0.2f, 0.4f, 0.25f, 1})) scanner_.add(addr);
                ImGui::TableSetColumnIndex(3);
                uintptr_t jump = MemUtils::Normalize(ptrVal);
                bool canJump = (format == Types::ViewFormat::I32 || format == Types::ViewFormat::I64 || format == Types::ViewFormat::Hexadecimal) && MemUtils::IsValidAddr(jump);
                if (canJump)
                {
                    if (UI::Btn("->", {S(42), S(28)}, Colors::BTN_PURPLE)) memViewer_.open(jump);
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

    void drawHexDump(uintptr_t base, std::span<const uint8_t> buffer, int rows)
    {
        if (buffer.empty())
        {
            UI::Text(Colors::HINT, "无数据");
            return;
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
                UI::Text(i == 0 ? ImVec4{0.4f, 1, 0.4f, 1} : ImVec4{0.5f, 0.75f, 0.85f, 1}, "%lX", rowAddr);
                char ascii[5] = "....";
                for (int c = 0; c < 4; ++c)
                {
                    ImGui::TableSetColumnIndex(c + 1);
                    if (off + c < buffer.size())
                    {
                        uint8_t b = buffer[off + c];
                        b == 0 ? UI::Text({0.4f, 0.4f, 0.4f, 1}, ".") : ImGui::Text("%02X", b);
                        ascii[c] = (b >= 32 && b < 127) ? (char)b : '.';
                    }
                    else
                    {
                        UI::Text({0.3f, 0.3f, 0.3f, 1}, "??");
                        ascii[c] = ' ';
                    }
                }
                ImGui::TableSetColumnIndex(5);
                UI::Text({0.65f, 0.65f, 0.5f, 1}, "%s", ascii);
                ImGui::TableSetColumnIndex(6);
                if (UI::Btn("存", {S(32), S(22)}, {0.2f, 0.4f, 0.25f, 1})) scanner_.add(rowAddr);
                ImGui::TableSetColumnIndex(7);
                // 跳转逻辑
                uintptr_t ptrVal = 0;
                bool canJump = false;
                size_t avail = off < buffer.size() ? buffer.size() - off : 0;
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
                    if (UI::Btn("->", {S(32), S(22)}, Colors::BTN_PURPLE)) memViewer_.open(ptrVal);
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
    }

    void drawDisasmView(uintptr_t base, std::span<const Disasm::DisasmLine> lines, int rows)
    {
        if (lines.empty())
        {
            UI::Text(Colors::ERR, "无法反汇编 (无效地址或非代码段)");
            return;
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
                UI::Text(line.address == base ? ImVec4{0.4f, 1, 0.4f, 1} : ImVec4{0.5f, 0.85f, 0.9f, 1}, "%llX", (unsigned long long)line.address);
                ImGui::TableSetColumnIndex(1);
                char bytes[48] = {};
                for (size_t j = 0; j < line.size && j < 8; ++j)
                {
                    char tmp[4];
                    snprintf(tmp, sizeof(tmp), "%02X ", line.bytes[j]);
                    strcat(bytes, tmp);
                }
                UI::Text({0.6f, 0.6f, 0.6f, 1}, "%s", bytes);
                ImGui::TableSetColumnIndex(2);
                UI::Text(getMnemonicColor(line.mnemonic), "%s", line.mnemonic);
                ImGui::TableSetColumnIndex(3);
                UI::Text({0.9f, 0.9f, 0.7f, 1}, "%s", line.op_str);
                ImGui::TableSetColumnIndex(4);
                if (isJumpInstruction(line.mnemonic))
                {
                    if (auto t = parseJumpTarget(line.op_str))
                        if (UI::Btn("跳", {S(35), S(24)}, Colors::BTN_PURPLE)) memViewer_.open(t);
                    ImGui::SameLine();
                }
                if (UI::Btn("存", {S(35), S(24)}, {0.2f, 0.4f, 0.25f, 1})) scanner_.add(line.address);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
    }

    static ImVec4 getMnemonicColor(const char *m)
    {
        if (!m) return {1, 1, 1, 1};
        if (m[0] == 'B' || !strncmp(m, "CB", 2) || !strncmp(m, "TB", 2) || !strcmp(m, "RET")) return {0.8f, 0.5f, 1, 1};
        if (!strncmp(m, "LD", 2) || !strncmp(m, "ST", 2)) return {0.5f, 0.7f, 1, 1};
        if (!strncmp(m, "ADD", 3) || !strncmp(m, "SUB", 3) || !strncmp(m, "MUL", 3) || !strncmp(m, "DIV", 3)) return {0.5f, 1, 0.5f, 1};
        if (!strncmp(m, "CMP", 3) || !strncmp(m, "TST", 3)) return {1, 1, 0.5f, 1};
        if (!strncmp(m, "MOV", 3)) return {0.5f, 1, 1, 1};
        if (!strcmp(m, "NOP")) return {0.5f, 0.5f, 0.5f, 1};
        return {1, 1, 1, 1};
    }
    static bool isJumpInstruction(const char *m)
    {
        return m && (m[0] == 'B' || !strncmp(m, "CB", 2) || !strncmp(m, "TB", 2) || !strcmp(m, "BL") || !strcmp(m, "BLR"));
    }
    static uintptr_t parseJumpTarget(const char *op)
    {
        if (!op) return 0;
        auto p = strstr(op, "#0X");
        if (p) return ParseHexAddress(p + 1).value_or(0);
        p = strstr(op, "0X");
        return p ? ParseHexAddress(p).value_or(0) : 0;
    }
};

// ============================================================================
// 主函数
// ============================================================================
int RunMemoryTool()
{
    Config::g_Running = true;
    constexpr bool kPreventCapture = false;

    if (!RenderVK::init(kPreventCapture))
    {
        std::println(stderr, "[错误] 初始化图形引擎失败。");
        return 1;
    }

    int rc = 0;
    try
    {
        MainUI ui;
        while (Config::g_Running)
        {
            RenderVK::drawBegin();
            ui.draw();
            RenderVK::drawEnd();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    catch (const std::exception &ex)
    {
        std::println(stderr, "[错误] 模式2运行异常: {}", ex.what());
        rc = 1;
    }
    catch (...)
    {
        std::println(stderr, "[错误] 模式2运行异常: unknown exception");
        rc = 1;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    RenderVK::shutdown();

    return rc;
}

// 脱离终端后台运行；调用前不得创建工作线程，fork() 后仅保留调用线程。
static bool daemonize(const char *log_path)
{
    pid_t pid;

    fflush(nullptr);

    //  第一次 fork：让父进程退出，使子进程在后台运行
    pid = fork();
    if (pid < 0) return false;
    if (pid > 0) _exit(EXIT_SUCCESS); // 父进程退出，不执行C++析构/atexit

    //  创建新会话：子进程成为新会话的首进程，完全脱离控制终端
    if (setsid() < 0) _exit(EXIT_FAILURE);

    //  忽略 SIGHUP 信号（可选，防止终端关闭时进程退出）
    signal(SIGHUP, SIG_IGN);

    // 第二次 fork：确保进程不是会话首进程，从而无法再次自动打开终端
    pid = fork();
    if (pid < 0) _exit(EXIT_FAILURE);
    if (pid > 0) _exit(EXIT_SUCCESS);

    //  修改文件权限掩码 (umask)
    umask(0);

    //  切换工作目录（防止占用卸载的分区）
    chdir("/");

    //  重定向标准输入、输出、错误
    int devnull = open("/dev/null", O_RDONLY);
    if (devnull != -1)
    {
        dup2(devnull, STDIN_FILENO);
        if (devnull > STDERR_FILENO) close(devnull);
    }

    int fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd != -1)
    {
        // 将标准输出 (1) 定向到 fd
        dup2(fd, STDOUT_FILENO);
        // 将标准错误 (2) 定向到 fd
        dup2(fd, STDERR_FILENO);

        if (fd > STDERR_FILENO) close(fd);
    }
    return true;
}

int main()
{

    std::println(stdout, "请选择启动模式：");
    std::println(stdout, "  0) 停止驱动线程");
    std::println(stdout, "  1) 读写测试");
    std::println(stdout, "  2) 触摸测试");
    std::println(stdout, "  3) 内存工具");
    std::println(stdout, "  4) HTTP服务器");
    std::println(stdout, "  5) 陀螺仪测试");
    std::println(stdout, "  6) 定位测试");
    std::print(stdout, "请输入 [0/1/2/3/4/5/6]: ");

    int rc = 1;
    int mode = 0;
    bool c = (bool)(std::cin >> mode);

    if (!c)
    {
        std::println(stderr, "[错误] 输入无效。");
        return rc;
    }
    if (mode < 0 || mode > 6)
    {
        std::println(stderr, "[错误] 未知选项: {}", mode);
        return rc;
    }

    if (!daemonize("/sdcard/log.txt"))
    {
        std::println(stderr, "[错误] 后台化失败。");
        return rc;
    }

    dr = new Driver((mode == 2 || mode == 3) ? 5 : 0, mode == 5, mode == 6);

    const std::function<int()> run[] = {[]
                                        {
                                            dr->ExitKernel();
                                            return 0;
                                        },
                                        RunReadWriteTest,
                                        RunTouchTest,
                                        RunMemoryTool,
                                        http_server,
                                        RunGyroTest,
                                        RunGnssTest};
    rc = run[mode]();
    Config::CpuThreadPool().purge();
    Config::IoThreadPool().purge();
    Config::CpuThreadPool().wait();
    Config::IoThreadPool().wait();

    delete dr;
    dr = nullptr;
    return rc;
}
