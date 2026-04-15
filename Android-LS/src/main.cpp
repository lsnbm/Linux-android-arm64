
#include <imgui.h>
#include <Android_touch/ImGuiFloatingKeyboard.h>
#include <cstdarg>
#include <cstring>
#include <functional>
#include <future>
#include <mutex>
#include <vector>
#include <span>

#include "MemoryTool.h"
#include "DriverMemory.h"
#include "Disassembler.h"
#include "PerformanceTestMain.h"
#include "tcp_server.h"
#include "Android_touch/TouchHelperA.h"
#include "Android_draw/draw.h"

// ============================================================================
// UI 构建器
// ============================================================================
class UIStyle
{
public:
    float scale = 2.0f, margin = 40.0f;
    constexpr float S(float v) const noexcept { return v * scale; }
    void apply() const
    {
        auto &s = ImGui::GetStyle();
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
    inline void Space(float y) { ImGui::Dummy({0, y}); }

    inline void Text(ImVec4 col, const char *fmt, ...)
    {
        va_list a;
        va_start(a, fmt);
        ImGui::TextColoredV(col, fmt, a);
        va_end(a);
    }

    inline bool Btn(const char *label, ImVec2 size, ImVec4 col = {})
    {
        if (col.w > 0)
            ImGui::PushStyleColor(ImGuiCol_Button, col);
        bool r = ImGui::Button(label, size);
        if (col.w > 0)
            ImGui::PopStyleColor();
        return r;
    }

    inline bool KbBtn(const char *text, const char *empty, ImVec2 size,
                      char *buf, int maxLen, const char *title)
    {
        ImGui::PushID((const void *)buf);
        bool r = ImGui::Button(strlen(text) ? text : empty, size) &&
                 (ImGuiFloatingKeyboard::Open(buf, maxLen, title), true);
        ImGui::PopID();
        return r;
    }

    // ---- 高级布局组件 ----

    // 带颜色的子窗口块
    template <typename F>
    void ColorChild(const char *id, ImVec2 size, ImVec4 bg, F &&body,
                    ImGuiWindowFlags flags = 0)
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
        if (ImGui::BeginChild(id, size, true, flags))
            body();
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
    inline void ButtonRow(float totalW, float h, std::initializer_list<BtnDef> btns,
                          float gap = 0)
    {
        float bw = (totalW - gap * (btns.size() - 1)) / btns.size();
        int i = 0;
        for (auto &b : btns)
        {
            if (i++ > 0)
                ImGui::SameLine();
            if (Btn(b.label, {bw, h}, b.col) && b.action)
                b.action();
        }
    }

    // 标签 + 值 行
    inline void LabelValue(ImVec4 labelCol, const char *label,
                           ImVec4 valCol, const char *fmt, ...)
    {
        Text(labelCol, "%s", label);
        ImGui::SameLine();
        va_list a;
        va_start(a, fmt);
        ImGui::TextColoredV(valCol, fmt, a);
        va_end(a);
    }

    // 输入行：KB按钮 + 动作按钮
    inline bool InputAction(char *buf, const char *hint, float inputW, float btnW,
                            float h, const char *btnLabel, ImVec4 btnCol,
                            const char *kbTitle)
    {
        KbBtn(buf, hint, {inputW - btnW - 6, h}, buf, 31, kbTitle);
        ImGui::SameLine();
        return Btn(btnLabel, {btnW, h}, btnCol);
    }

    // 上下箭头滚动条
    inline void ArrowScroll(const char *id, float w, float h,
                            int &idx, int minIdx, int maxIdx)
    {
        if (ImGui::BeginChild(id, {w, h}, false, ImGuiWindowFlags_NoScrollbar))
        {
            ImGui::PushStyleColor(ImGuiCol_Button, {0.2f, 0.3f, 0.4f, 1.0f});
            ImGui::BeginDisabled(idx <= minIdx);
            if (ImGui::Button("▲", {w, h / 2 - 3}))
                --idx;
            ImGui::EndDisabled();
            ImGui::BeginDisabled(idx >= maxIdx);
            if (ImGui::Button("▼", {w, h / 2 - 3}))
                ++idx;
            ImGui::EndDisabled();
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();
    }

    // 通用选择器弹窗
    template <typename T>
    bool SelectorPopup(const char *title, bool *show, float sx, float sy, float sw, float sh,
                       const char *const *items, int count, T *sel)
    {
        float S = 2.0f; // 或从外部传入
        float pw = sw * 0.75f;
        float ph = std::min(count * 46.0f + 50.0f, sh * 0.7f);
        ImGui::SetNextWindowPos({sx + (sw - pw) / 2, sy + (sh - ph) / 2});
        ImGui::SetNextWindowSize({pw, ph});
        ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.1f, 0.1f, 0.13f, 0.98f});
        bool changed = false;
        if (ImGui::Begin(title, show, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
        {
            float fw = ImGui::GetContentRegionAvail().x;
            for (int i = 0; i < count; ++i)
            {
                ImVec4 c = (i == static_cast<int>(*sel))
                               ? ImVec4{0.2f, 0.35f, 0.25f, 1}
                               : ImVec4{0.13f, 0.13f, 0.16f, 1};
                if (Btn(items[i], {fw, 42}, c))
                {
                    *sel = static_cast<T>(i);
                    *show = false;
                    changed = true;
                }
            }
        }
        ImGui::End();
        ImGui::PopStyleColor();
        return changed;
    }
}
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
}

// ============================================================================
// 主界面
// ============================================================================
class MainUI
{
private:
    MemScanner scanner_;
    PointerManager ptrManager_;
    LockManager lockManager_;
    MemViewer memViewer_;

    struct ScanParams
    {
        Types::DataType dataType = Types::DataType::I32;
        Types::FuzzyMode fuzzyMode = Types::FuzzyMode::Unknown;
        int page = 0;
        std::string lastStringPattern;
    } scanParams_;

    struct PtrParams
    {
        uintptr_t target = 0;
        int depth = 3, maxOffset = 1000;
        bool useManual = false, useArray = false;
        uintptr_t manualBase = 0, arrayBase = 0;
        size_t arrayCount = 0;
        std::string filterModule;
    } ptrParams_;

    struct SigParams
    {
        uintptr_t scanAddr = 0, verifyAddr = 0;
        int range = 20, lastChanged = -1, lastTotal = 0, lastScanCount = -1;
    } sigParams_;

    struct BpParams
    {
        uintptr_t address = 0;
        int bpType = 1, bpScope = 2, lenBytes = 4;
        bool active = false;

        int editingRecordIdx = -1;    // 正在编辑哪条记录
        Driver::hwbp_record editCopy; // 副本
        char regEditBuf[64] = {};
        int editingField = -1; // 正在编辑哪个字段
    } bpParams_;

    std::vector<std::string> offsetLabels_;
    std::vector<int> offsetValues_;
    int selectedOffsetIdx_ = 1;
    UIStyle style_;
    std::vector<std::future<void>> backgroundTasks_;
    std::mutex backgroundTasksMutex_;

    struct Buf
    {
        char pid[32] = {}, value[64] = {}, addAddr[32] = {}, base[32] = {}, page[16] = "20";
        char modify[64] = {}, memOffset[32] = {}, resultOffset[32] = {}, moduleSearch[64] = {};
        char ptrTarget[32] = {}, arrayBase[32] = {}, arrayCount[16] = "100", filterModule[64] = {};
        char sigScanAddr[32] = {}, sigVerifyAddr[32] = {};
        char viewAddr[32] = {}, bpAddr[32] = {}, bpLen[16] = "4";
    } buf_;

    struct State
    {
        int tab = 0, resultScrollIdx = 0;
        uintptr_t modifyAddr = 0;
        bool showModify = false, floating = false, dragging = false;
        ImVec2 floatPos = {50, 200}, dragOffset = {};
        bool showType = false, showMode = false, showDepth = false,
             showOffset = false, showScale = false, showFormat = false;
        bool showBpType = false, showBpScope = false;
    } state_;

    float S(float v) const { return style_.S(v); }

    void collectFinishedTasks()
    {
        std::lock_guard lock(backgroundTasksMutex_);
        auto it = backgroundTasks_.begin();
        while (it != backgroundTasks_.end())
        {
            if (it->valid() &&
                it->wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
            {
                it->wait();
                it = backgroundTasks_.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    template <typename F>
    void enqueueBackgroundTask(F &&task)
    {
        collectFinishedTasks();
        auto fut = Utils::GlobalPool.push(std::forward<F>(task));
        std::lock_guard lock(backgroundTasksMutex_);
        backgroundTasks_.push_back(std::move(fut));
    }

    void waitForBackgroundTasks()
    {
        std::vector<std::future<void>> tasks;
        {
            std::lock_guard lock(backgroundTasksMutex_);
            tasks.swap(backgroundTasks_);
        }
        for (auto &task : tasks)
        {
            if (task.valid())
                task.wait();
        }
    }

    // ---- 扫描逻辑 ----
    void startScan(std::string_view valueStr, bool isFirst)
    {
        scanParams_.page = 0;
        auto type = scanParams_.dataType;
        auto mode = scanParams_.fuzzyMode;
        auto pid = dr.GetGlobalPid();
        std::string valCopy(valueStr);
        double rangeMax = 0.0;

        if (mode == Types::FuzzyMode::Pointer)
        {
            type = Types::DataType::I64;
            enqueueBackgroundTask([=, this]
                                  {
                try {
                    auto addr = MemUtils::Normalize(std::strtoull(valCopy.c_str(), nullptr, 16));
                    scanner_.scan<int64_t>(pid, static_cast<int64_t>(addr), mode, isFirst, 0.0);
                } catch (...) {} });
            return;
        }
        if (mode == Types::FuzzyMode::String)
        {
            if (valCopy.empty())
                return;
            scanParams_.lastStringPattern = valCopy;
            enqueueBackgroundTask([=, this]
                                  { scanner_.scanString(pid, valCopy, isFirst); });
            return;
        }
        if (mode == Types::FuzzyMode::Range)
        {
            auto pos = valCopy.find('~');
            if (pos == std::string::npos)
                return;
            try
            {
                rangeMax = std::stod(valCopy.substr(pos + 1));
                valCopy = valCopy.substr(0, pos);
            }
            catch (...)
            {
                return;
            }
        }
        enqueueBackgroundTask([=, this]
                              {
            try {
                MemUtils::DispatchType(type, [&]<typename T>() {
                    T val;
                    if constexpr (std::is_floating_point_v<T>) val = static_cast<T>(std::stod(valCopy));
                    else if constexpr (sizeof(T) <= 4) val = static_cast<T>(std::stoi(valCopy));
                    else val = static_cast<T>(std::stoll(valCopy));
                    scanner_.scan<T>(pid, val, mode, isFirst, rangeMax);
                });
            } catch (...) {} });
    }

    void startPtrScan()
    {
        auto p = ptrParams_;
        p.maxOffset = offsetValues_[selectedOffsetIdx_];
        auto pid = dr.GetGlobalPid();
        enqueueBackgroundTask([=, this]
                              { ptrManager_.scan(pid, p.target, p.depth, p.maxOffset, p.useManual,
                                                 p.manualBase, p.useArray, p.arrayBase,
                                                 p.arrayCount, p.filterModule); });
    }

    void copyAddress(uintptr_t addr)
    {
        ImGui::SetClipboardText(std::format("{:X}", addr).c_str());
    }

    static void uiHwbpRequestRead(Driver::hwbp_record &record, int reg)
    {
        if (HWBP_GET_MASK(&record, reg) != HWBP_OP_WRITE)
        {
            HWBP_SET_MASK(&record, reg, HWBP_OP_READ);
        }
    }

    static std::uint64_t uiHwbpGetXField(const Driver::hwbp_record &record, int index)
    {
        switch (index)
        {
        case 0: return record.x0;
        case 1: return record.x1;
        case 2: return record.x2;
        case 3: return record.x3;
        case 4: return record.x4;
        case 5: return record.x5;
        case 6: return record.x6;
        case 7: return record.x7;
        case 8: return record.x8;
        case 9: return record.x9;
        case 10: return record.x10;
        case 11: return record.x11;
        case 12: return record.x12;
        case 13: return record.x13;
        case 14: return record.x14;
        case 15: return record.x15;
        case 16: return record.x16;
        case 17: return record.x17;
        case 18: return record.x18;
        case 19: return record.x19;
        case 20: return record.x20;
        case 21: return record.x21;
        case 22: return record.x22;
        case 23: return record.x23;
        case 24: return record.x24;
        case 25: return record.x25;
        case 26: return record.x26;
        case 27: return record.x27;
        case 28: return record.x28;
        case 29: return record.x29;
        default: return 0;
        }
    }

    static void uiHwbpSetXField(Driver::hwbp_record &record, int index, std::uint64_t value)
    {
        switch (index)
        {
        case 0: record.x0 = value; break;
        case 1: record.x1 = value; break;
        case 2: record.x2 = value; break;
        case 3: record.x3 = value; break;
        case 4: record.x4 = value; break;
        case 5: record.x5 = value; break;
        case 6: record.x6 = value; break;
        case 7: record.x7 = value; break;
        case 8: record.x8 = value; break;
        case 9: record.x9 = value; break;
        case 10: record.x10 = value; break;
        case 11: record.x11 = value; break;
        case 12: record.x12 = value; break;
        case 13: record.x13 = value; break;
        case 14: record.x14 = value; break;
        case 15: record.x15 = value; break;
        case 16: record.x16 = value; break;
        case 17: record.x17 = value; break;
        case 18: record.x18 = value; break;
        case 19: record.x19 = value; break;
        case 20: record.x20 = value; break;
        case 21: record.x21 = value; break;
        case 22: record.x22 = value; break;
        case 23: record.x23 = value; break;
        case 24: record.x24 = value; break;
        case 25: record.x25 = value; break;
        case 26: record.x26 = value; break;
        case 27: record.x27 = value; break;
        case 28: record.x28 = value; break;
        case 29: record.x29 = value; break;
        default: break;
        }
    }

    static __uint128_t uiHwbpGetQField(const Driver::hwbp_record &record, int index)
    {
        switch (index)
        {
        case 0: return record.q0;
        case 1: return record.q1;
        case 2: return record.q2;
        case 3: return record.q3;
        case 4: return record.q4;
        case 5: return record.q5;
        case 6: return record.q6;
        case 7: return record.q7;
        case 8: return record.q8;
        case 9: return record.q9;
        case 10: return record.q10;
        case 11: return record.q11;
        case 12: return record.q12;
        case 13: return record.q13;
        case 14: return record.q14;
        case 15: return record.q15;
        case 16: return record.q16;
        case 17: return record.q17;
        case 18: return record.q18;
        case 19: return record.q19;
        case 20: return record.q20;
        case 21: return record.q21;
        case 22: return record.q22;
        case 23: return record.q23;
        case 24: return record.q24;
        case 25: return record.q25;
        case 26: return record.q26;
        case 27: return record.q27;
        case 28: return record.q28;
        case 29: return record.q29;
        case 30: return record.q30;
        case 31: return record.q31;
        default: return 0;
        }
    }

    static void uiHwbpSetQField(Driver::hwbp_record &record, int index, __uint128_t value)
    {
        switch (index)
        {
        case 0: record.q0 = value; break;
        case 1: record.q1 = value; break;
        case 2: record.q2 = value; break;
        case 3: record.q3 = value; break;
        case 4: record.q4 = value; break;
        case 5: record.q5 = value; break;
        case 6: record.q6 = value; break;
        case 7: record.q7 = value; break;
        case 8: record.q8 = value; break;
        case 9: record.q9 = value; break;
        case 10: record.q10 = value; break;
        case 11: record.q11 = value; break;
        case 12: record.q12 = value; break;
        case 13: record.q13 = value; break;
        case 14: record.q14 = value; break;
        case 15: record.q15 = value; break;
        case 16: record.q16 = value; break;
        case 17: record.q17 = value; break;
        case 18: record.q18 = value; break;
        case 19: record.q19 = value; break;
        case 20: record.q20 = value; break;
        case 21: record.q21 = value; break;
        case 22: record.q22 = value; break;
        case 23: record.q23 = value; break;
        case 24: record.q24 = value; break;
        case 25: record.q25 = value; break;
        case 26: record.q26 = value; break;
        case 27: record.q27 = value; break;
        case 28: record.q28 = value; break;
        case 29: record.q29 = value; break;
        case 30: record.q30 = value; break;
        case 31: record.q31 = value; break;
        default: break;
        }
    }

    static std::uint64_t uiHwbpReadRegisterValue(Driver::hwbp_record &record, int reg)
    {
        uiHwbpRequestRead(record, reg);
        switch (reg)
        {
        case Driver::IDX_PC:
            return record.pc;
        case Driver::IDX_HIT_COUNT:
            return record.hit_count;
        case Driver::IDX_LR:
            return record.lr;
        case Driver::IDX_SP:
            return record.sp;
        case Driver::IDX_ORIG_X0:
            return record.orig_x0;
        case Driver::IDX_SYSCALLNO:
            return record.syscallno;
        case Driver::IDX_PSTATE:
            return record.pstate;
        case Driver::IDX_FPSR:
            return record.fpsr;
        case Driver::IDX_FPCR:
            return record.fpcr;
        default:
            if (reg >= Driver::IDX_X0 && reg <= Driver::IDX_X29)
            {
                return uiHwbpGetXField(record, reg - Driver::IDX_X0);
            }
            return 0;
        }
    }

    static bool uiHwbpWriteRegisterValue(Driver::hwbp_record &record, int reg, std::uint64_t value)
    {
        HWBP_SET_MASK(&record, reg, HWBP_OP_WRITE);
        switch (reg)
        {
        case Driver::IDX_PC:
            record.pc = value;
            return true;
        case Driver::IDX_HIT_COUNT:
            record.hit_count = value;
            return true;
        case Driver::IDX_LR:
            record.lr = value;
            return true;
        case Driver::IDX_SP:
            record.sp = value;
            return true;
        case Driver::IDX_ORIG_X0:
            record.orig_x0 = value;
            return true;
        case Driver::IDX_SYSCALLNO:
            record.syscallno = value;
            return true;
        case Driver::IDX_PSTATE:
            record.pstate = value;
            return true;
        case Driver::IDX_FPSR:
            record.fpsr = static_cast<std::uint32_t>(value);
            return true;
        case Driver::IDX_FPCR:
            record.fpcr = static_cast<std::uint32_t>(value);
            return true;
        default:
            if (reg >= Driver::IDX_X0 && reg <= Driver::IDX_X29)
            {
                uiHwbpSetXField(record, reg - Driver::IDX_X0, value);
                return true;
            }
            if (reg >= Driver::IDX_Q0 && reg <= Driver::IDX_Q31)
            {
                uiHwbpSetQField(record, reg - Driver::IDX_Q0, static_cast<__uint128_t>(value));
                return true;
            }
            return false;
        }
    }

    static std::uint64_t uiHwbpReadXField(Driver::hwbp_record &record, int index)
    {
        uiHwbpRequestRead(record, Driver::IDX_X0 + index);
        return uiHwbpGetXField(record, index);
    }

    static __uint128_t uiHwbpReadQField(Driver::hwbp_record &record, int index)
    {
        uiHwbpRequestRead(record, Driver::IDX_Q0 + index);
        return uiHwbpGetQField(record, index);
    }

    static void uiHwbpWriteQField(Driver::hwbp_record &record, int index, __uint128_t value)
    {
        HWBP_SET_MASK(&record, Driver::IDX_Q0 + index, HWBP_OP_WRITE);
        uiHwbpSetQField(record, index, value);
    }

public:
    MainUI()
    {
        for (int i = 500; i <= 100000; i += 500)
        {
            offsetLabels_.push_back(std::to_string(i));
            offsetValues_.push_back(i);
        }
        snprintf(buf_.page, sizeof(buf_.page), "%d", Config::g_ItemsPerPage.load());
        if (int pid = dr.GetGlobalPid(); pid > 0)
            snprintf(buf_.pid, sizeof(buf_.pid), "%d", pid);
        SetInputBlocking(true);
    }

    ~MainUI()
    {
        Config::g_Running = false;
        waitForBackgroundTasks();
    }

    void draw()
    {
        collectFinishedTasks();
        style_.apply();
        if (state_.floating)
            drawFloatButton();
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
            if (ImGui::IsWindowHovered() && io.MouseDown[0] && !state_.dragging)
            {
                state_.dragging = true;
                state_.dragOffset = {io.MousePos.x - ImGui::GetWindowPos().x,
                                     io.MousePos.y - ImGui::GetWindowPos().y};
            }
            if (state_.dragging)
            {
                if (io.MouseDown[0])
                    state_.floatPos = {io.MousePos.x - state_.dragOffset.x,
                                       io.MousePos.y - state_.dragOffset.y};
                else
                    state_.dragging = false;
            }
            if (ImGui::Button("M", {sz, sz}) && !state_.dragging)
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
        UI::ColorChild("Top", {w, h}, Colors::BG_PANEL, [&]
                       {
            float bh = h - S(12);
            if (UI::Btn("收起", {S(55), bh}, Colors::BTN_MINIMIZE)) {
                state_.floating = true; SetInputBlocking(false);
            }
            ImGui::SameLine();
            ImGui::SetCursorPosX((w - ImGui::CalcTextSize("内存扫描").x) / 2);
            ImGui::SetCursorPosY((h - ImGui::GetTextLineHeight()) / 2);
            ImGui::Text("内存扫描");
            ImGui::SameLine(w - (S(50) + S(85) + S(50) + S(18)));
            ImGui::SetCursorPosY(S(6));
            char sc[16]; snprintf(sc, sizeof(sc), "%.0f%%", style_.scale * 100);
            if (ImGui::Button(sc, {S(50), bh})) state_.showScale = !state_.showScale;
            ImGui::SameLine();
            UI::KbBtn(buf_.pid, "PID", {S(85), bh}, buf_.pid, 31, "PID");
            ImGui::SameLine();
            if (!ImGuiFloatingKeyboard::IsVisible() && buf_.pid[0]) {
                int pid = atoi(buf_.pid);
                if (pid > 0 && pid != dr.GetGlobalPid())
                    dr.SetGlobalPid(pid);
            }
            ImGui::SameLine();
            if (UI::Btn("退出", {S(50), bh}, Colors::BTN_EXIT)) Config::g_Running = false; }, ImGuiWindowFlags_NoScrollbar);
    }

    // ---- 内容区 ----
    void drawContent(float w, float h)
    {
        using DrawFn = void (MainUI::*)();
        static constexpr int TAB_COUNT = 7;
        DrawFn tabs[] = {
            &MainUI::drawScanTab, &MainUI::drawResultTab, &MainUI::drawViewerTab,
            &MainUI::drawModuleTab, &MainUI::drawPointerTab,
            &MainUI::drawSignatureTab, &MainUI::drawBreakpointTab};
        UI::ColorChild("Content", {w, h}, Colors::BG_MID, [&]
                       { (this->*tabs[state_.tab])(); });
    }

    // ---- 标签栏 ----
    void drawTabs(float w, float h)
    {
        UI::ColorChild("Tabs", {w, h}, Colors::BG_PANEL, [&]
                       {
            constexpr int N = 7;
            float bw = (w - S(36)) / N;
            const char* labels[] = {"扫描", "结果", "浏览", "模块", "指针", "特征", "断点"};
            for (int i = 0; i < N; ++i) {
                if (i > 0) ImGui::SameLine();
                ImVec4 c = state_.tab == i ? Colors::BTN_ACTIVE : Colors::BTN_INACTIVE;
                if (UI::Btn(labels[i], {bw, h - S(14)}, c)) {
                    state_.tab = i;
                    if (i == 3 || i == 5) dr.GetMemoryInformation();
                    if (i == 2 && memViewer_.base()) memViewer_.refresh();
                }
            } }, ImGuiWindowFlags_NoScrollbar);
    }

    // ================================================================
    // 扫描页
    // ================================================================
    void drawScanTab()
    {
        float w = ImGui::GetContentRegionAvail().x;
        bool isPtrMode = scanParams_.fuzzyMode == Types::FuzzyMode::Pointer;
        bool isStringMode = scanParams_.fuzzyMode == Types::FuzzyMode::String;

        // 数据类型
        UI::Text(Colors::LABEL, "数据类型:");
        if (isPtrMode || isStringMode)
        {
            ImGui::BeginDisabled();
            ImGui::Button(isPtrMode ? "Int64 (pointer mode)" : "String mode ignores type", {w, S(45)});
            ImGui::EndDisabled();
        }
        else
        {
            if (ImGui::Button(Types::Labels::TYPE[static_cast<int>(scanParams_.dataType)], {w, S(45)}))
                state_.showType = true;
        }

        UI::Space(S(6));
        UI::Text(Colors::LABEL, "搜索模式:");
        if (ImGui::Button(Types::Labels::FUZZY[static_cast<int>(scanParams_.fuzzyMode)], {w, S(45)}))
            state_.showMode = true;

        UI::Space(S(6));
        UI::Text(Colors::LABEL, isPtrMode ? "目标地址(Hex):" : "搜索数值:");
        UI::KbBtn(buf_.value, isPtrMode ? "输入Hex地址..." : "点击输入...",
                  {w, S(52)}, buf_.value, 63, isPtrMode ? "目标地址(Hex)" : "数值");

        if (isPtrMode)
            UI::Text(Colors::INFO_CYAN, "输入16进制地址，搜索指向该地址的指针");
        else if (isStringMode)
            UI::Text(Colors::INFO_CYAN, "按原始字节匹配，区分大小写；再次扫描会在当前结果中继续过滤");
        else if (scanParams_.fuzzyMode == Types::FuzzyMode::Range)
            UI::Text(Colors::INFO_CYAN, "格式: 最小值~最大值  例: 0~45  -2~2  0.1~6.5");

        UI::Space(S(10));
        ImGui::BeginDisabled(scanner_.isScanning());
        UI::ButtonRow(w, S(52), {{"首次扫描", Colors::BTN_GREEN, [&]
                                  { startScan(buf_.value, true); }},
                                 {"再次扫描", Colors::BTN_BLUE, [&]
                                  { startScan(buf_.value, false); }},
                                 {"清空", Colors::BTN_RED, [&]
                                  { scanner_.clear(); }}},
                      S(6));
        ImGui::EndDisabled();

        UI::Space(S(6));
        if (scanner_.isScanning())
        {
            UI::Text(Colors::WARN, "扫描中...");
            ImGui::ProgressBar(scanner_.progress(), {w, S(18)});
        }
        else
        {
            scanner_.count() ? UI::Text(Colors::OK, "找到 %zu 个", scanner_.count())
                             : UI::Text(Colors::HINT, "暂无结果");
        }
    }

    // ================================================================
    // 结果页
    // ================================================================
    void drawResultTab()
    {
        size_t total = scanner_.count();
        float w = ImGui::GetContentRegionAvail().x, bh = S(40);

        // 添加地址行
        UI::KbBtn(buf_.addAddr, "Hex地址...", {w - S(76), bh}, buf_.addAddr, 31, "Hex地址");
        ImGui::SameLine();
        if (UI::Btn("添加", {S(70), bh}, Colors::BTN_GREEN))
        {
            uintptr_t addr = 0;
            if (sscanf(buf_.addAddr, "%lx", &addr) == 1 && addr)
            {
                scanner_.add(addr);
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
        drawPagination(w, bh, maxPage);
        UI::Space(S(4));
        drawResultToolbar(w, data);
        ImGui::Separator();

        // 结果列表 + 箭头
        float listH = ImGui::GetContentRegionAvail().y;
        float contentW = w - S(56);
        int maxIdx = std::max(0, (int)data.size() - (int)(listH / S(93)));
        state_.resultScrollIdx = std::clamp(state_.resultScrollIdx, 0, maxIdx);

        if (ImGui::BeginChild("ListContent", {contentW, listH}, false, ImGuiWindowFlags_NoScrollbar))
        {
            int endIdx = state_.resultScrollIdx + (int)(listH / S(93)) + 1;
            for (int i = state_.resultScrollIdx; i < (int)data.size() && i < endIdx; ++i)
                drawCard(data[i], contentW - S(10));
        }
        ImGui::EndChild();
        ImGui::SameLine();
        UI::ArrowScroll("ListArrows", S(50), listH, state_.resultScrollIdx, 0, maxIdx);
    }

    void drawPagination(float w, float bh, int maxPage)
    {
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
        UI::ColorChild("PageInfo", {infoW, bh}, Colors::BG_PANEL, [&]
                       {
            ImGui::SetCursorPos({(infoW - ImGui::CalcTextSize(info).x) / 2,
                                 (bh - ImGui::GetTextLineHeight()) / 2 - S(4)});
            ImGui::Text("%s", info); }, ImGuiWindowFlags_NoScrollbar);
        ImGui::SameLine();

        ImGui::BeginDisabled(scanParams_.page >= maxPage);
        if (ImGui::Button("下页", {pgW, bh}))
        {
            ++scanParams_.page;
            state_.resultScrollIdx = 0;
        }
        ImGui::EndDisabled();
    }

    void drawResultToolbar(float w, const std::vector<uintptr_t> &data)
    {
        ImGui::Text("每页:");
        ImGui::SameLine();
        UI::KbBtn(buf_.page, buf_.page, {S(55), S(36)}, buf_.page, 10, "每页数量");
        if (buf_.page[0] && !ImGuiFloatingKeyboard::IsVisible())
        {
            int v = atoi(buf_.page);
            if (v >= 1 && v <= 500)
            {
                if (v != Config::g_ItemsPerPage.load())
                {
                    Config::g_ItemsPerPage = v;
                    scanParams_.page = state_.resultScrollIdx = 0;
                }
            }
            else
                snprintf(buf_.page, sizeof(buf_.page), "%d", Config::g_ItemsPerPage.load());
        }
        ImGui::SameLine();

        bool anyLocked = std::ranges::any_of(data, [&](auto a)
                                             { return lockManager_.isLocked(a); });
        bool isStringMode = scanParams_.fuzzyMode == Types::FuzzyMode::String;
        if (anyLocked)
        {
            if (UI::Btn("解锁页", {S(70), S(36)}, {0.2f, 0.25f, 0.42f, 1}))
                lockManager_.unlockBatch(data);
        }
        else if (isStringMode)
        {
            ImGui::BeginDisabled();
            UI::Btn("Lock", {S(70), S(36)}, {0.2f, 0.2f, 0.2f, 1});
            ImGui::EndDisabled();
        }
        else
        {
            if (UI::Btn("锁定页", {S(70), S(36)}, {0.42f, 0.28f, 0.1f, 1}))
                lockManager_.lockBatch(data, scanParams_.fuzzyMode == Types::FuzzyMode::Pointer
                                                 ? Types::DataType::I64
                                                 : scanParams_.dataType);
        }
        ImGui::SameLine();

        if (UI::Btn("偏移", {S(55), S(36)}, Colors::BTN_ORANGE))
        {
            buf_.resultOffset[0] = 0;
            ImGuiFloatingKeyboard::Open(buf_.resultOffset, 31, "偏移量(Hex,可负)");
        }
        if (buf_.resultOffset[0] && !ImGuiFloatingKeyboard::IsVisible())
        {
            if (auto r = MemUtils::ParseHexOffset(buf_.resultOffset))
                scanner_.applyOffset(r->negative ? -(int64_t)r->offset : (int64_t)r->offset);
            buf_.resultOffset[0] = 0;
        }
    }

    void drawCard(uintptr_t addr, float w)
    {
        bool locked = lockManager_.isLocked(addr);
        bool isPtrMode = scanParams_.fuzzyMode == Types::FuzzyMode::Pointer;
        bool isStringMode = scanParams_.fuzzyMode == Types::FuzzyMode::String;
        size_t previewLen = std::clamp(scanParams_.lastStringPattern.size(), size_t(16), size_t(64));

        ImGui::PushID((void *)addr);
        UI::ColorChild("Card", {w, S(85)}, locked ? Colors::LOCKED : Colors::BG_PANEL, [&]
                       {
            float cw = ImGui::GetContentRegionAvail().x;

            // 地址 + 值
            UI::LabelValue({0.5f,0.6f,0.7f,1}, "地址:",
                locked ? ImVec4{1,0.5f,0.5f,1} : Colors::ADDR_GREEN, "%lX", addr);
            ImGui::SameLine(cw * 0.45f);
            if (isPtrMode)
                UI::LabelValue({0.5f,0.6f,0.7f,1}, "指向:", Colors::VAL_YELLOW, "%s",
                               MemUtils::ReadAsPointerString(addr).c_str());
            else if (isStringMode)
                UI::LabelValue({0.5f,0.6f,0.7f,1}, "字符串:", Colors::VAL_YELLOW, "%s",
                               MemUtils::ReadAsText(addr, previewLen).c_str());
            else
                UI::LabelValue({0.5f,0.6f,0.7f,1}, "数值:", Colors::VAL_YELLOW, "%s",
                               MemUtils::ReadAsString(addr, scanParams_.dataType).c_str());
            if (locked) { ImGui::SameLine(); UI::Text({1,0.3f,0.3f,1}, "[锁定]"); }

            // 操作按钮
            UI::Space(S(4));
            float bw = (cw - S(15)) / 4;
            if (ImGui::Button("改", {bw, S(36)})) {
                state_.modifyAddr = addr;
                std::string current = isPtrMode ? MemUtils::ReadAsPointerString(addr)
                                                : isStringMode ? MemUtils::ReadAsText(addr, previewLen)
                                                               : MemUtils::ReadAsString(addr, scanParams_.dataType);
                std::snprintf(buf_.modify, sizeof(buf_.modify), "%s", current.c_str());
                state_.showModify = true;
                ImGuiFloatingKeyboard::Open(buf_.modify, 63, isPtrMode ? "新地址(Hex)"
                                                                        : isStringMode ? "新字符串"
                                                                                       : "新数值");
            }
            ImGui::SameLine();
            if (UI::Btn(locked ? "解锁" : "锁定", {bw, S(36)},
                        locked ? Colors::BTN_UNLOCK : Colors::BTN_LOCK))
                if (!(isStringMode && !locked))
                    lockManager_.toggle(addr, isPtrMode ? Types::DataType::I64 : scanParams_.dataType);
            ImGui::SameLine();
            if (UI::Btn("复制", {bw, S(36)}, Colors::BTN_COPY)) copyAddress(addr);
            ImGui::SameLine();
            if (UI::Btn("删除", {bw, S(36)}, Colors::BTN_DEL)) {
                if (locked) lockManager_.unlock(addr);
                scanner_.remove(addr);
            } }, ImGuiWindowFlags_NoScrollbar);
        ImGui::PopID();
        UI::Space(S(4));
    }

    // ================================================================
    // 内存浏览页
    // ================================================================
    void drawViewerTab()
    {
        float w = ImGui::GetContentRegionAvail().x, bh = S(42);
        float goW = S(55), ofsW = S(55), fmtW = S(85), refW = S(55);
        float inputW = w - goW - ofsW - fmtW - refW - S(24);

        // 工具栏：一行五按钮
        UI::KbBtn(buf_.viewAddr, "输入Hex地址...", {inputW, bh}, buf_.viewAddr, 31, "Hex地址");
        ImGui::SameLine();
        if (UI::Btn("跳转", {goW, bh}, {0.15f, 0.4f, 0.25f, 1}))
        {
            uintptr_t addr = 0;
            if (sscanf(buf_.viewAddr, "%lx", &addr) == 1 && addr)
                memViewer_.open(addr);
        }
        ImGui::SameLine();
        if (UI::Btn("偏移", {ofsW, bh}, Colors::BTN_ORANGE))
        {
            buf_.memOffset[0] = 0;
            ImGuiFloatingKeyboard::Open(buf_.memOffset, 31, "偏移量(Hex,可负)");
        }
        if (buf_.memOffset[0] && !ImGuiFloatingKeyboard::IsVisible())
        {
            memViewer_.applyOffset(buf_.memOffset);
            buf_.memOffset[0] = 0;
        }
        ImGui::SameLine();
        if (UI::Btn(Types::Labels::VIEW_FORMAT[static_cast<size_t>(memViewer_.format())], {fmtW, bh}, {0.18f, 0.25f, 0.35f, 1}))
            state_.showFormat = true;
        ImGui::SameLine();
        if (UI::Btn("刷新", {refW, bh}, Colors::BTN_TEAL))
            memViewer_.refresh();

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
        if (!memViewer_.base())
            return;

        // 读取失败提示
        if (!memViewer_.readSuccess())
        {
            UI::Space(S(20));
            ImGui::PushStyleColor(ImGuiCol_Text, {1, 0.5f, 0.5f, 1});
            ImGui::TextWrapped("无法读取内存，请检查：\n\n1. PID 是否正确并已同步\n"
                               "2. 目标地址是否有效\n3. 驱动是否正常工作\n4. 目标进程是否仍在运行");
            ImGui::PopStyleColor();
            UI::Space(S(10));
            if (ImGui::Button("重试", {S(80), S(36)}))
                memViewer_.refresh();
            return;
        }

        // 数据显示 + 箭头
        auto fmt = memViewer_.format();
        size_t step = fmt == Types::ViewFormat::Disasm ? 1
                                                       : (fmt == Types::ViewFormat::Hex ? 4 : Types::GetViewSize(fmt));
        float cH = ImGui::GetContentRegionAvail().y, aW = S(50);
        float cW = ImGui::GetContentRegionAvail().x - aW - S(6);
        float rH = ImGui::GetTextLineHeight() +
                   (fmt == Types::ViewFormat::Disasm ? S(14)
                    : fmt == Types::ViewFormat::Hex  ? S(8)
                                                     : S(12));
        int rows = (int)(cH / rH) + 2;

        if (ImGui::BeginChild("MemContent", {cW, cH}, false, ImGuiWindowFlags_NoScrollbar))
        {
            if (fmt == Types::ViewFormat::Disasm)
                drawDisasmView(memViewer_.base(), memViewer_.getDisasm(), rows, memViewer_.disasmScrollIdx());
            else if (fmt == Types::ViewFormat::Hex)
                drawHexDump(memViewer_.base(), memViewer_.buffer(), rows);
            else
                drawTypedView(fmt, memViewer_.base(), memViewer_.buffer(), rows);
        }
        ImGui::EndChild();
        ImGui::SameLine();

        if (ImGui::BeginChild("MemArrows", {aW, cH}, false, ImGuiWindowFlags_NoScrollbar))
        {
            ImGui::PushStyleColor(ImGuiCol_Button, {0.2f, 0.3f, 0.4f, 1});
            if (ImGui::Button("▲##view_up", {aW, cH / 2 - S(3)}))
                memViewer_.move(-1, step);
            if (ImGui::Button("▼##view_down", {aW, cH / 2 - S(3)}))
                memViewer_.move(1, step);
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();
    }

    // ================================================================
    // 模块页
    // ================================================================
    void drawModuleTab()
    {
        float w = ImGui::GetContentRegionAvail().x;
        UI::KbBtn(buf_.moduleSearch, "搜索模块名和dump模块", {w, S(42)},
                  buf_.moduleSearch, 63, "输入模块名进行搜索或Dump");
        UI::Space(S(4));
        if (UI::Btn("刷新模块", {w, S(48)}, Colors::BTN_TEAL))
            dr.GetMemoryInformation();
        UI::Space(S(6));
        if (UI::Btn("Dump 模块 (保存至 /sdcard/dump/)", {w, S(48)}, Colors::BTN_PURPLE))
        {
            if (strlen(buf_.moduleSearch) > 0)
            {
                std::string mod = buf_.moduleSearch;
                Utils::GlobalPool.push([mod]
                                       { dr.DumpModule(mod); });
            }
        }
        UI::Space(S(6));

        if (ImGui::BeginChild("ModList", {0, 0}, false))
        {
            const auto &info = dr.GetMemoryInfoRef();
            if (info.module_count == 0)
            {
                UI::Text(Colors::HINT, "暂无模块");
            }
            else
            {
                int displayCount = 0;
                for (int i = 0; i < info.module_count; ++i)
                {
                    const auto &mod = info.modules[i];
                    std::string_view name = mod.name;
                    if (auto s = name.rfind('/'); s != std::string_view::npos)
                        name = name.substr(s + 1);
                    if (buf_.moduleSearch[0] && name.find(buf_.moduleSearch) == std::string_view::npos)
                        continue;
                    for (int j = 0; j < mod.seg_count; ++j)
                    {
                        const auto &seg = mod.segs[j];
                        displayCount++;
                        ImGui::PushID(i * 1000 + j);
                        UI::ColorChild("Mod", {w - S(20), 0}, Colors::BG_CARD, [&]
                                       {
                            UI::Text({0.7f,0.85f,1,1}, "%.*s", (int)name.size(), name.data());
                            seg.index == -1
                                ? UI::Text({0.9f,0.6f,0.3f,1}, "Segment: BSS")
                                : UI::Text(Colors::ADDR_GREEN, "Segment: %d", seg.index);
                            UI::Text(Colors::HINT, "Range: "); ImGui::SameLine();
                            UI::Text({0.4f,1,0.4f,1}, "%llX - ", (unsigned long long)seg.start);
                            ImGui::SameLine();
                            UI::Text({1,0.6f,0.4f,1}, "%llX", (unsigned long long)seg.end); }, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize);
                        ImGui::PopID();
                        UI::Space(S(4));
                    }
                }
                if (!displayCount)
                    UI::Text({0.6f, 0.4f, 0.4f, 1}, "未找到匹配 \"%s\" 的模块", buf_.moduleSearch);
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
        ImGui::PushID("PtrScan");
        UI::Text(Colors::TITLE, "━━ 指针扫描 ━━");
        UI::Space(S(4));

        if (!ptrManager_.isScanning())
        {
            ImGui::Text("目标地址:");
            UI::KbBtn(buf_.ptrTarget, "点击输入Hex", {w, bh}, buf_.ptrTarget, 31, "目标地址(Hex)");
            UI::Space(S(4));

            // 深度和偏移
            ImGui::Text("深度:");
            ImGui::SameLine();
            char dLbl[8];
            snprintf(dLbl, sizeof(dLbl), "%d层", ptrParams_.depth);
            if (ImGui::Button(dLbl, {S(70), bh}))
                state_.showDepth = true;
            ImGui::SameLine();
            ImGui::Text("偏移:");
            ImGui::SameLine();
            if (ImGui::Button(offsetLabels_[selectedOffsetIdx_].c_str(), {S(70), bh}))
                state_.showOffset = true;

            UI::Space(S(4));
            UI::Text(Colors::LABEL, "指定模块 (可选):");
            UI::KbBtn(buf_.filterModule, "全部模块", {w - S(60), bh}, buf_.filterModule, 63, "模块名(如il2cpp)");
            ImGui::SameLine();
            if (ImGui::Button("清##scanFilter", {S(50), bh}))
                buf_.filterModule[0] = 0;

            // 手动/数组基址
            ImGui::Checkbox("手动基址##scan", &ptrParams_.useManual);
            if (ptrParams_.useManual)
            {
                ptrParams_.useArray = false;
                UI::KbBtn(buf_.base, "基址(Hex)##scanBase", {w, bh}, buf_.base, 30, "Hex基址");
            }
            ImGui::Checkbox("数组基址##scan", &ptrParams_.useArray);
            if (ptrParams_.useArray)
            {
                ptrParams_.useManual = false;
                float hw = (w - S(6)) / 2;
                UI::KbBtn(buf_.arrayBase, "数组地址(Hex)", {hw, bh}, buf_.arrayBase, 30, "数组首地址");
                ImGui::SameLine();
                UI::KbBtn(buf_.arrayCount, "数量", {hw, bh}, buf_.arrayCount, 15, "元素数量");
            }

            UI::Space(S(6));
            if (UI::Btn("开始扫描", {w, S(48)}, Colors::BTN_GREEN))
            {
                if (sscanf(buf_.ptrTarget, "%lx", &ptrParams_.target) == 1 && ptrParams_.target)
                {
                    ptrParams_.filterModule = buf_.filterModule;
                    if (ptrParams_.useManual && buf_.base[0])
                        ptrParams_.manualBase = strtoull(buf_.base, nullptr, 16);
                    if (ptrParams_.useArray)
                    {
                        if (buf_.arrayBase[0])
                            ptrParams_.arrayBase = strtoull(buf_.arrayBase, nullptr, 16);
                        if (buf_.arrayCount[0])
                            ptrParams_.arrayCount = strtoull(buf_.arrayCount, nullptr, 10);
                    }
                    startPtrScan();
                }
            }

            // 文件操作
            UI::Space(S(12));
            ImGui::Separator();
            UI::Space(S(8));
            UI::Text({0.6f, 0.7f, 0.8f, 1}, "文件操作 (Pointer.bin)");
            UI::Space(S(4));
            UI::ButtonRow(w, S(40), {{"开始对比", Colors::BTN_PURPLE, [&]
                                      { ptrManager_.MergeBins(); }},
                                     {"格式化输出", {0.45f, 0.35f, 0.2f, 1}, [&]
                                      { ptrManager_.ExportToTxt(); }}},
                          S(8));

            if (auto cnt = ptrManager_.count(); cnt > 0)
            {
                UI::Space(S(6));
                UI::Text({0.4f, 1, 0.4f, 1}, "扫描完成！找到 %zu 条指针链", cnt);
            }
            else if (ptrManager_.scanProgress() >= 1.0f)
            {
                UI::Space(S(6));
                UI::Text(Colors::ERR, "扫描完成，未找到结果");
            }
            UI::Text(Colors::HINT, "保存到 Pointer.bin");
        }
        else
        {
            UI::Text(Colors::WARN, "扫描中...");
            ImGui::ProgressBar(ptrManager_.scanProgress(), {w, S(22)});
        }
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
        UI::KbBtn(buf_.sigScanAddr, "点击输入Hex", {w, bh}, buf_.sigScanAddr, 31, "目标地址(Hex)");
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
            if (ImGui::Button(lb, {qbw, S(30)}))
                sigParams_.range = r;
            if (r != 100)
                ImGui::SameLine();
        }

        UI::Space(S(8));
        if (UI::Btn("扫描保存", {w, S(48)}, Colors::BTN_GREEN))
        {
            uintptr_t addr = 0;
            if (sscanf(buf_.sigScanAddr, "%lx", &addr) == 1 && addr)
                SignatureScanner::ScanAddressSignature(addr, sigParams_.range);
        }
        UI::Text(Colors::HINT, "保存到 Signature.txt");

        // 过滤部分
        UI::Space(S(20));
        ImGui::Separator();
        UI::Space(S(10));
        UI::Text(Colors::TITLE, "━━ 特征码过滤 ━━");
        UI::Space(S(4));
        ImGui::Text("过滤地址:");
        UI::KbBtn(buf_.sigVerifyAddr, "点击输入Hex", {w, bh}, buf_.sigVerifyAddr, 31, "过滤地址(Hex)");
        UI::Space(S(8));

        if (UI::Btn("过滤并更新", {w, S(48)}, {0.4f, 0.3f, 0.15f, 1}))
        {
            if (sscanf(buf_.sigVerifyAddr, "%lx", &sigParams_.verifyAddr) == 1 && sigParams_.verifyAddr)
            {
                auto vr = SignatureScanner::FilterSignature(sigParams_.verifyAddr);
                sigParams_.lastChanged = vr.success ? vr.changedCount : -2;
                if (vr.success)
                    sigParams_.lastTotal = vr.totalCount;
                sigParams_.lastScanCount = -1;
            }
        }
        if (sigParams_.lastChanged >= 0)
        {
            sigParams_.lastChanged == 0
                ? UI::Text(Colors::OK, "完美! 无变动 (%d字节)", sigParams_.lastTotal)
                : UI::Text(Colors::WARN, "变动: %d/%d (已更新)", sigParams_.lastChanged, sigParams_.lastTotal);
        }
        else if (sigParams_.lastChanged == -2)
            UI::Text(Colors::ERR, "失败! 检查Signature.txt");

        UI::Space(S(10));
        if (UI::Btn("扫描特征码", {w, S(48)}, Colors::BTN_PURPLE))
            sigParams_.lastScanCount = (int)SignatureScanner::ScanSignatureFromFile().size();
        if (sigParams_.lastScanCount >= 0)
        {
            sigParams_.lastScanCount == 0
                ? UI::Text(Colors::ERR, "未找到匹配地址")
                : UI::Text({0.5f, 0.9f, 1, 1}, "找到 %d 个地址", sigParams_.lastScanCount);
        }
        UI::Text(Colors::HINT, "结果保存到 Signature.txt");
    }

    // ================================================================
    // 断点页
    // ================================================================
    void drawBreakpointTab()
    {
        float w = ImGui::GetContentRegionAvail().x, bh = S(45);
        static const char *bpTypeLabels[] = {"读取", "写入", "读写", "执行"};
        static const char *bpScopeLabels[] = {"仅主线程", "仅子线程", "全部线程"};

        UI::Text(Colors::TITLE, "━━ 硬件断点 ━━");
        UI::Space(S(4));

        // 硬件信息
        const auto &info = dr.GetHwbpInfoRef();
        UI::LabelValue(Colors::ADDR_CYAN, "执行断点寄存器: ", Colors::ADDR_GREEN,
                       "%llu", (unsigned long long)info.num_brps);
        ImGui::SameLine();
        UI::LabelValue(Colors::ADDR_CYAN, "  访问断点寄存器: ", Colors::ADDR_GREEN,
                       "%llu", (unsigned long long)info.num_wrps);

        UI::Space(S(6));
        ImGui::Separator();
        UI::Space(S(6));

        // 配置
        ImGui::Text("断点地址:");
        UI::KbBtn(buf_.bpAddr, "点击输入Hex地址", {w, bh}, buf_.bpAddr, 31, "断点地址(Hex)");
        UI::Space(S(4));

        ImGui::Text("断点类型:");
        if (ImGui::Button(bpTypeLabels[bpParams_.bpType], {w, bh}))
            state_.showBpType = true;
        UI::Space(S(4));

        ImGui::Text("线程范围:");
        if (ImGui::Button(bpScopeLabels[bpParams_.bpScope], {w, bh}))
            state_.showBpScope = true;
        UI::Space(S(4));

        ImGui::Text("监控长度(字节):");
        UI::KbBtn(buf_.bpLen, "4", {w, bh}, buf_.bpLen, 15, "监控字节数");
        UI::Space(S(8));

        // 操作按钮
        float halfW = (w - S(8)) / 2;
        ImGui::BeginDisabled(bpParams_.active);
        if (UI::Btn("设置断点", {halfW, S(52)}, Colors::BTN_GREEN))
        {
            uintptr_t addr = 0;
            if (sscanf(buf_.bpAddr, "%lx", &addr) == 1 && addr)
            {
                int len = std::max(atoi(buf_.bpLen), 1);
                bpParams_.address = addr;
                bpParams_.lenBytes = len;
                if (dr.SetProcessHwbpRef(addr,
                                         static_cast<decltype(dr)::bp_type>(bpParams_.bpType),
                                         static_cast<decltype(dr)::bp_scope>(bpParams_.bpScope), len) == 0)
                    bpParams_.active = true;
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!bpParams_.active);
        if (UI::Btn("移除断点", {halfW, S(52)}, {0.5f, 0.15f, 0.15f, 1}))
        {
            dr.RemoveProcessHwbpRef();
            bpParams_.active = false;
        }
        ImGui::EndDisabled();

        UI::Space(S(8));
        bpParams_.active
            ? UI::Text(Colors::OK, "● 断点已激活  地址: 0x%lX", bpParams_.address)
            : UI::Text(Colors::HINT, "○ 断点未激活");
        if (info.hit_addr)
            UI::Text(Colors::ADDR_CYAN, "监控地址: 0x%llX", (unsigned long long)info.hit_addr);

        UI::Space(S(8));
        ImGui::Separator();
        UI::Space(S(6));
        UI::Text(Colors::TITLE, "━━ 命中信息 ━━");
        UI::Space(S(4));

        if (info.record_count > 0)
            drawBpRecords(info, w);
        else
            UI::Text(Colors::HINT, "暂无命中记录");
    }

    void drawBpRecords(const Driver::hwbp_info &info, float w)
    {
        uint64_t totalHits = 0;
        for (int r = 0; r < info.record_count; ++r)
        {
            auto &rec = const_cast<Driver::hwbp_record &>(info.records[r]);
            totalHits += uiHwbpReadRegisterValue(rec, Driver::IDX_HIT_COUNT);
        }
        UI::Text(Colors::WARN, "不同PC数: %d  总命中: %llu", info.record_count, (unsigned long long)totalHits);
        UI::Space(S(6));

        static bool expandState[0x100] = {};
        int deleteIdx = -1;

        for (int r = 0; r < info.record_count; ++r)
        {
            auto &rec = const_cast<Driver::hwbp_record &>(info.records[r]);
            const auto pc = uiHwbpReadRegisterValue(rec, Driver::IDX_PC);
            const auto hitCount = uiHwbpReadRegisterValue(rec, Driver::IDX_HIT_COUNT);
            ImGui::PushID(r);
            float btnW = S(55), expandW = S(45);

            // 摘要行
            UI::Text({0.7f, 0.85f, 1, 1}, "[%d]", r);
            ImGui::SameLine();
            UI::Text(Colors::ADDR_GREEN, "PC:0x%llX", (unsigned long long)pc);
            ImGui::SameLine();
            UI::Text(Colors::WARN, "x%llu", (unsigned long long)hitCount);

            ImGui::SameLine(w - btnW);
            if (UI::Btn("删除", {btnW, S(32)}, {0.6f, 0.15f, 0.15f, 1}))
                deleteIdx = r;
            ImGui::SameLine(w - btnW - expandW - S(4));
            if (UI::Btn(expandState[r] ? "收起" : "展开", {expandW, S(32)}, {0.2f, 0.3f, 0.45f, 1}))
                expandState[r] = !expandState[r];

            if (expandState[r])
            {
                ImGui::Indent(S(8));
                drawBpRecordDetail(rec, r);
                ImGui::Unindent(S(8));
            }

            UI::Space(S(4));
            ImGui::Separator();
            UI::Space(S(4));
            ImGui::PopID();
        }
        if (deleteIdx >= 0)
            dr.RemoveHwbpRecord(deleteIdx);
    }

    void drawBpRecordDetail(const Driver::hwbp_record &rec, int r)
    {
        bool isEditing = (bpParams_.editingRecordIdx == r);
        // 编辑模式下显示副本，否则显示原始
        auto &show = isEditing ? bpParams_.editCopy : const_cast<Driver::hwbp_record &>(rec);

        // 编辑/应用/取消
        if (!isEditing)
        {
            if (UI::Btn("编辑寄存器", {S(120), S(32)}, {0.3f, 0.4f, 0.2f, 1}))
                beginEditRecord(r);
        }
        else
        {
            if (UI::Btn("应用", {S(70), S(32)}, Colors::BTN_GREEN))
                applyRecordEdits(r);
            ImGui::SameLine();
            if (UI::Btn("取消", {S(60), S(32)}, Colors::BTN_RED))
            {
                bpParams_.editingRecordIdx = -1;
                bpParams_.editingField = -1;
            }
        }
        UI::Space(S(4));

        // 通用：显示一行寄存器，编辑模式下多一个"改"按钮
        // fieldId: 0~29=X0~X29, 30=LR, 31=SP, 32=PC, 33=PSTATE, 34=ORIG_X0, 35=SYSCALLNO
        auto regLine = [&](const char *name, int regIndex)
        {
            const auto val = uiHwbpReadRegisterValue(show, regIndex);
            UI::Text({0.7f, 0.85f, 1, 1}, "%s: ", name);
            ImGui::SameLine();
            UI::Text(Colors::ADDR_GREEN, "0x%llX", (unsigned long long)val);
            ImGui::SameLine();

            char id[32];
            snprintf(id, sizeof(id), "复制##%s%d", name, r);
            if (UI::Btn(id, {S(50), S(28)}, Colors::BTN_COPY))
            {
                char tmp[32];
                snprintf(tmp, sizeof(tmp), "%llX", (unsigned long long)val);
                ImGui::SetClipboardText(tmp);
            }

            if (isEditing)
            {
                ImGui::SameLine();
                snprintf(id, sizeof(id), "改##%s%d", name, r);
                if (UI::Btn(id, {S(40), S(28)}, {0.4f, 0.3f, 0.15f, 1}))
                {
                    bpParams_.editingField = regIndex;
                    snprintf(bpParams_.regEditBuf, sizeof(bpParams_.regEditBuf),
                             "%llX", (unsigned long long)val);
                    char title[48];
                    snprintf(title, sizeof(title), "修改 %s (Hex)", name);
                    ImGuiFloatingKeyboard::Open(bpParams_.regEditBuf, 63, title);
                }
                // 键盘关闭，写入副本
                if (bpParams_.editingField == regIndex && !ImGuiFloatingKeyboard::IsVisible() && bpParams_.regEditBuf[0])
                {
                    uiHwbpWriteRegisterValue(bpParams_.editCopy, regIndex, strtoull(bpParams_.regEditBuf, nullptr, 16));
                    bpParams_.editingField = -1;
                    bpParams_.regEditBuf[0] = 0;
                }
            }
        };

        regLine("PC", Driver::IDX_PC);
        regLine("LR", Driver::IDX_LR);
        regLine("SP", Driver::IDX_SP);
        UI::Space(S(4));

        // PSTATE / SYSCALL / ORIG_X0 同理
        const auto pstate = uiHwbpReadRegisterValue(show, Driver::IDX_PSTATE);
        const auto syscallno = uiHwbpReadRegisterValue(show, Driver::IDX_SYSCALLNO);
        const auto origX0 = uiHwbpReadRegisterValue(show, Driver::IDX_ORIG_X0);
        const auto hitCount = uiHwbpReadRegisterValue(show, Driver::IDX_HIT_COUNT);
        UI::Text(Colors::LABEL, "PSTATE:  0x%llX", (unsigned long long)pstate);
        if (isEditing)
        {
            ImGui::SameLine();
            if (UI::Btn("改##pst", {S(40), S(28)}, {0.4f, 0.3f, 0.15f, 1}))
            {
                bpParams_.editingField = Driver::IDX_PSTATE;
                snprintf(bpParams_.regEditBuf, sizeof(bpParams_.regEditBuf),
                         "%llX", (unsigned long long)pstate);
                ImGuiFloatingKeyboard::Open(bpParams_.regEditBuf, 63, "修改 PSTATE (Hex)");
            }
            if (bpParams_.editingField == Driver::IDX_PSTATE && !ImGuiFloatingKeyboard::IsVisible() && bpParams_.regEditBuf[0])
            {
                uiHwbpWriteRegisterValue(bpParams_.editCopy, Driver::IDX_PSTATE, strtoull(bpParams_.regEditBuf, nullptr, 16));
                bpParams_.editingField = -1;
                bpParams_.regEditBuf[0] = 0;
            }
        }

        UI::Text(Colors::LABEL, "SYSCALL: %llu", (unsigned long long)syscallno);
        UI::Text(Colors::LABEL, "ORIG_X0: 0x%llX", (unsigned long long)origX0);
        UI::Text(Colors::WARN, "命中次数: %llu", (unsigned long long)hitCount);
        UI::Space(S(6));

        // ━━ 通用寄存器表格 ━━
        UI::Text(Colors::TITLE, "━━ 通用寄存器 ━━");
        UI::Space(S(4));
        char tableId[32];
        snprintf(tableId, sizeof(tableId), "Regs##%d", r);
        int cols = isEditing ? 4 : 3;
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {S(4), S(4)});
        if (ImGui::BeginTable(tableId, cols, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("寄存器", ImGuiTableColumnFlags_WidthFixed, S(55));
            ImGui::TableSetupColumn("值", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("复制", ImGuiTableColumnFlags_WidthFixed, S(50));
            if (isEditing)
                ImGui::TableSetupColumn("改", ImGuiTableColumnFlags_WidthFixed, S(50));
            ImGui::TableHeadersRow();

            for (int i = 0; i < 30; ++i)
            {
                const int regIndex = Driver::IDX_X0 + i;
                const auto regValue = uiHwbpReadXField(show, i);
                ImGui::TableNextRow();
                ImGui::PushID(i);

                ImGui::TableSetColumnIndex(0);
                UI::Text({0.7f, 0.85f, 1, 1}, "X%d", i);

                ImGui::TableSetColumnIndex(1);
                UI::Text(Colors::ADDR_GREEN, "0x%llX", (unsigned long long)regValue);

                ImGui::TableSetColumnIndex(2);
                if (UI::Btn("复制", {S(42), S(28)}, Colors::BTN_COPY))
                {
                    char tmp[32];
                    snprintf(tmp, sizeof(tmp), "%llX", (unsigned long long)regValue);
                    ImGui::SetClipboardText(tmp);
                }

                if (isEditing)
                {
                    ImGui::TableSetColumnIndex(3);
                    char bid[16];
                    snprintf(bid, sizeof(bid), "改##x%d", i);
                    if (UI::Btn(bid, {S(42), S(28)}, {0.4f, 0.3f, 0.15f, 1}))
                    {
                        bpParams_.editingField = regIndex;
                        snprintf(bpParams_.regEditBuf, sizeof(bpParams_.regEditBuf),
                                 "%llX", (unsigned long long)regValue);
                        char title[32];
                        snprintf(title, sizeof(title), "修改 X%d (Hex)", i);
                        ImGuiFloatingKeyboard::Open(bpParams_.regEditBuf, 63, title);
                    }
                    if (bpParams_.editingField == regIndex && !ImGuiFloatingKeyboard::IsVisible() && bpParams_.regEditBuf[0])
                    {
                        uiHwbpWriteRegisterValue(bpParams_.editCopy, regIndex, strtoull(bpParams_.regEditBuf, nullptr, 16));
                        bpParams_.editingField = -1;
                        bpParams_.regEditBuf[0] = 0;
                    }
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();

        // ━━ 浮点/SIMD 寄存器 ━━
        UI::Space(S(6));
        UI::Text(Colors::TITLE, "━━ 浮点/SIMD 寄存器 ━━");
        UI::Space(S(4));

        // FPSR / FPCR 显示与编辑
        auto fpCtrlLine = [&](const char *name, int regIndex)
        {
            const auto val = static_cast<uint32_t>(uiHwbpReadRegisterValue(show, regIndex));
            UI::Text({0.7f, 0.85f, 1, 1}, "%s: ", name);
            ImGui::SameLine();
            UI::Text(Colors::ADDR_GREEN, "0x%X", (unsigned int)val);
            ImGui::SameLine();

            char id[32];
            snprintf(id, sizeof(id), "复制##%s%d", name, r);
            if (UI::Btn(id, {S(50), S(28)}, Colors::BTN_COPY))
            {
                char tmp[32];
                snprintf(tmp, sizeof(tmp), "%X", (unsigned int)val);
                ImGui::SetClipboardText(tmp);
            }

            if (isEditing)
            {
                ImGui::SameLine();
                snprintf(id, sizeof(id), "改##%s%d", name, r);
                if (UI::Btn(id, {S(40), S(28)}, {0.4f, 0.3f, 0.15f, 1}))
                {
                    bpParams_.editingField = regIndex;
                    snprintf(bpParams_.regEditBuf, sizeof(bpParams_.regEditBuf),
                             "%X", (unsigned int)val);
                    char title[48];
                    snprintf(title, sizeof(title), "修改 %s (Hex)", name);
                    ImGuiFloatingKeyboard::Open(bpParams_.regEditBuf, 63, title);
                }
                if (bpParams_.editingField == regIndex && !ImGuiFloatingKeyboard::IsVisible() && bpParams_.regEditBuf[0])
                {
                    uiHwbpWriteRegisterValue(bpParams_.editCopy, regIndex, strtoul(bpParams_.regEditBuf, nullptr, 16));
                    bpParams_.editingField = -1;
                    bpParams_.regEditBuf[0] = 0;
                }
            }
        };

        fpCtrlLine("FPSR", Driver::IDX_FPSR);
        fpCtrlLine("FPCR", Driver::IDX_FPCR);
        UI::Space(S(4));

        // V0~V31 表格
        char vtblId[32];
        snprintf(vtblId, sizeof(vtblId), "VRegs##%d", r);
        int vcols = isEditing ? 4 : 3;
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {S(4), S(4)});
        if (ImGui::BeginTable(vtblId, vcols, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("寄存器", ImGuiTableColumnFlags_WidthFixed, S(55));
            ImGui::TableSetupColumn("值", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("复制", ImGuiTableColumnFlags_WidthFixed, S(50));
            if (isEditing)
                ImGui::TableSetupColumn("改", ImGuiTableColumnFlags_WidthFixed, S(50));
            ImGui::TableHeadersRow();

            for (int i = 0; i < 32; ++i)
            {
                const int regIndex = Driver::IDX_Q0 + i;
                const auto qValue = uiHwbpReadQField(show, i);
                const auto qLo = static_cast<uint64_t>(qValue);
                const auto qHi = static_cast<uint64_t>(qValue >> 64);
                ImGui::TableNextRow();
                ImGui::PushID(i + 32); // offset to avoid ID clash with X regs

                ImGui::TableSetColumnIndex(0);
                UI::Text({0.7f, 0.85f, 1, 1}, "V%d", i);

                // Vn 寄存器是 128 位，拆成 高64位:低64位 显示
                ImGui::TableSetColumnIndex(1);
                UI::Text(Colors::ADDR_GREEN, "%016llX_%016llX", (unsigned long long)qHi, (unsigned long long)qLo);

                ImGui::TableSetColumnIndex(2);
                if (UI::Btn("复制", {S(42), S(28)}, Colors::BTN_COPY))
                {
                    char tmp[64];
                    snprintf(tmp, sizeof(tmp), "%016llX%016llX", (unsigned long long)qHi, (unsigned long long)qLo);
                    ImGui::SetClipboardText(tmp);
                }

                if (isEditing)
                {
                    ImGui::TableSetColumnIndex(3);
                    char bid[16];
                    snprintf(bid, sizeof(bid), "改##v%d", i);
                    if (UI::Btn(bid, {S(42), S(28)}, {0.4f, 0.3f, 0.15f, 1}))
                    {
                        bpParams_.editingField = regIndex;
                        snprintf(bpParams_.regEditBuf, sizeof(bpParams_.regEditBuf),
                                 "%016llX%016llX", (unsigned long long)qHi, (unsigned long long)qLo);
                        char title[32];
                        snprintf(title, sizeof(title), "修改 V%d (Hex)", i);
                        ImGuiFloatingKeyboard::Open(bpParams_.regEditBuf, 63, title);
                    }
                    if (bpParams_.editingField == regIndex && !ImGuiFloatingKeyboard::IsVisible() && bpParams_.regEditBuf[0])
                    {
                        // 解析128位hex回写 (高16位_低16位 或 低16位)
                        int len = static_cast<int>(strlen(bpParams_.regEditBuf));
                        char hiBuf[17] = {}, loBuf[17] = {};
                        if (len > 16)
                        {
                            strncpy(hiBuf, bpParams_.regEditBuf, len - 16);
                            hiBuf[len - 16] = '\0';
                            strncpy(loBuf, bpParams_.regEditBuf + len - 16, 16);
                            loBuf[16] = '\0';
                        }
                        else
                        {
                            strncpy(loBuf, bpParams_.regEditBuf, 16);
                            loBuf[16] = '\0';
                        }
                        uint64_t hi = strtoull(hiBuf, nullptr, 16);
                        uint64_t lo = strtoull(loBuf, nullptr, 16);
                        uiHwbpWriteQField(bpParams_.editCopy, i, (static_cast<__uint128_t>(hi) << 64) | lo);
                        bpParams_.editingField = -1;
                        bpParams_.regEditBuf[0] = 0;
                    }
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
    }

    // 拷贝副本
    void beginEditRecord(int idx)
    {
        const auto &info = dr.GetHwbpInfoRef();
        if (idx < 0 || idx >= info.record_count)
            return;
        bpParams_.editingRecordIdx = idx;
        bpParams_.editCopy = info.records[idx]; // 完整拷贝
        bpParams_.editingField = -1;
    }
    // 写回副本
    void applyRecordEdits(int idx)
    {
        const auto &info = dr.GetHwbpInfoRef();
        if (idx < 0 || idx >= info.record_count)
            return;
        const_cast<Driver::hwbp_record &>(info.records[idx]) = bpParams_.editCopy;
        bpParams_.editingRecordIdx = -1;
        bpParams_.editingField = -1;
    }

    // ================================================================
    // 弹窗统一管理
    // ================================================================
    void drawPopups(float sx, float sy, float sw, float sh)
    {
        // 缩放弹窗
        if (state_.showScale)
        {
            drawListPopup("缩放", &state_.showScale, sx, sy, sw, sh, S(180), S(160), [&](float fw)
                          {
                ImGui::Text("UI: %.0f%%", style_.scale * 100);
                ImGui::SliderFloat("##s", &style_.scale, 0.5f, 2.0f, "");
                float bw = fw / 3 - S(3);
                if (ImGui::Button("75%", {bw, S(28)})) style_.scale = 0.75f; ImGui::SameLine();
                if (ImGui::Button("100%", {bw, S(28)})) style_.scale = 1.0f; ImGui::SameLine();
                if (ImGui::Button("150%", {bw, S(28)})) style_.scale = 1.5f;
                ImGui::Text("边距: %.0f", style_.margin);
                ImGui::SliderFloat("##m", &style_.margin, 0, 80, ""); });
        }

        // 通用选择器
        auto doSelector = [&](const char *title, bool *show, auto items, int count, auto *sel)
        {
            int s = static_cast<int>(*sel);
            drawListPopup(title, show, sx, sy, sw, sh, sw * 0.75f,
                          std::min(count * (S(42) + S(4)) + S(50), sh * 0.7f), [&](float fw)
                          {
                for (int i = 0; i < count; ++i)
                    if (UI::Btn(items[i], {fw, S(42)},
                        i == s ? ImVec4{0.2f,0.35f,0.25f,1} : ImVec4{0.13f,0.13f,0.16f,1}))
                    { s = i; *show = false; } });
            *sel = static_cast<std::remove_pointer_t<decltype(sel)>>(s);
        };

        if (state_.showType)
            doSelector("类型", &state_.showType, Types::Labels::TYPE.data(),
                       (int)Types::Labels::TYPE.size(), &scanParams_.dataType);
        if (state_.showMode)
            doSelector("模式", &state_.showMode, Types::Labels::FUZZY.data(),
                       (int)Types::Labels::FUZZY.size(), &scanParams_.fuzzyMode);
        if (state_.showFormat)
        {
            auto fmt = memViewer_.format();
            auto oldFmt = fmt;
            doSelector("格式", &state_.showFormat, Types::Labels::VIEW_FORMAT.data(),
                       (int)Types::ViewFormat::Count, &fmt);
            if (fmt != oldFmt)
                memViewer_.setFormat(fmt);
        }
        if (state_.showBpType)
        {
            static const char *items[] = {"读取", "写入", "读写", "执行"};
            doSelector("断点类型", &state_.showBpType, items, 4, &bpParams_.bpType);
        }
        if (state_.showBpScope)
        {
            static const char *items[] = {"仅主线程", "仅子线程", "全部线程"};
            doSelector("线程范围", &state_.showBpScope, items, 3, &bpParams_.bpScope);
        }

        // 深度选择
        if (state_.showDepth)
        {
            drawListPopup("深度", &state_.showDepth, sx, sy, sw, sh, S(160), S(320), [&](float fw)
                          {
                for (int i = 1; i <= 20; ++i) {
                    char lbl[8]; snprintf(lbl, sizeof(lbl), "%d层", i);
                    if (UI::Btn(lbl, {fw, S(28)}, i == ptrParams_.depth
                        ? ImVec4{0.2f,0.35f,0.25f,1} : ImVec4{0.13f,0.13f,0.16f,1}))
                    { ptrParams_.depth = i; state_.showDepth = false; }
                } });
        }

        // 偏移选择
        if (state_.showOffset)
        {
            drawListPopup("偏移", &state_.showOffset, sx, sy, sw, sh, S(160),
                          std::min((float)offsetLabels_.size() * S(32) + S(40), sh * 0.6f), [&](float fw)
                          {
                if (ImGui::BeginChild("List", {0, 0}, false)) {
                    for (size_t i = 0; i < offsetLabels_.size(); ++i)
                        if (UI::Btn(offsetLabels_[i].c_str(), {fw, S(28)},
                            (int)i == selectedOffsetIdx_
                                ? ImVec4{0.2f,0.35f,0.25f,1} : ImVec4{0.13f,0.13f,0.16f,1}))
                        { selectedOffsetIdx_ = i; state_.showOffset = false; }
                }
                ImGui::EndChild(); });
        }

        // 修改弹窗
        if (state_.showModify && !ImGuiFloatingKeyboard::IsVisible())
        {
            if (state_.modifyAddr && strlen(buf_.modify))
            {
                if (scanParams_.fuzzyMode == Types::FuzzyMode::Pointer)
                    MemUtils::WritePointerFromString(state_.modifyAddr, buf_.modify);
                else if (scanParams_.fuzzyMode == Types::FuzzyMode::String)
                    MemUtils::WriteText(state_.modifyAddr, buf_.modify);
                else
                    MemUtils::WriteFromString(state_.modifyAddr, scanParams_.dataType, buf_.modify);
            }
            state_.showModify = false;
            state_.modifyAddr = 0;
            buf_.modify[0] = 0;
        }
    }

    template <typename F>
    void drawListPopup(const char *title, bool *show, float sx, float sy, float sw, float sh,
                       float pw, float ph, F &&drawItems)
    {
        ImGui::SetNextWindowPos({sx + (sw - pw) / 2, sy + (sh - ph) / 2});
        ImGui::SetNextWindowSize({pw, ph});
        ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.1f, 0.1f, 0.13f, 0.98f});
        if (ImGui::Begin(title, show, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
            drawItems(ImGui::GetContentRegionAvail().x);
        ImGui::End();
        ImGui::PopStyleColor();
    }

    // ================================================================
    // 内存视图渲染 (保持不变，已经很紧凑)
    // ================================================================
    void drawTypedView(Types::ViewFormat format, uintptr_t base,
                       std::span<const uint8_t> buffer, int rows)
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
                if (off + step > buffer.size())
                    break;
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
                case Types::ViewFormat::Hex64:
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
                if (UI::Btn("存", {S(42), S(28)}, {0.2f, 0.4f, 0.25f, 1}))
                    scanner_.add(addr);
                ImGui::TableSetColumnIndex(3);
                uintptr_t jump = MemUtils::Normalize(ptrVal);
                bool canJump = (format == Types::ViewFormat::I32 || format == Types::ViewFormat::I64 || format == Types::ViewFormat::Hex64) && MemUtils::IsValidAddr(jump);
                if (canJump)
                {
                    if (UI::Btn("->", {S(42), S(28)}, Colors::BTN_PURPLE))
                        memViewer_.open(jump);
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
                if (off >= buffer.size())
                    break;
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
                if (UI::Btn("存", {S(32), S(22)}, {0.2f, 0.4f, 0.25f, 1}))
                    scanner_.add(rowAddr);
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
                    if (UI::Btn("->", {S(32), S(22)}, Colors::BTN_PURPLE))
                        memViewer_.open(ptrVal);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("跳转到: %lX", ptrVal);
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

    void drawDisasmView(uintptr_t base, std::span<const Disasm::DisasmLine> lines, int rows, int scrollIdx)
    {
        if (lines.empty())
        {
            UI::Text(Colors::ERR, "无法反汇编 (无效地址或非代码段)");
            return;
        }
        if (scrollIdx >= (int)lines.size())
            scrollIdx = 0;
        auto visible = lines.subspan(scrollIdx);
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {S(4), S(4)});
        if (ImGui::BeginTable("Disasm", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("地址", ImGuiTableColumnFlags_WidthFixed, S(110));
            ImGui::TableSetupColumn("字节码", ImGuiTableColumnFlags_WidthFixed, S(90));
            ImGui::TableSetupColumn("指令", ImGuiTableColumnFlags_WidthFixed, S(60));
            ImGui::TableSetupColumn("操作数", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("操作", ImGuiTableColumnFlags_WidthFixed, S(80));
            ImGui::TableHeadersRow();
            for (int i = 0; i < std::min((int)visible.size(), rows); ++i)
            {
                const auto &line = visible[i];
                if (!line.valid)
                    continue;
                ImGui::TableNextRow();
                ImGui::PushID((void *)line.address);
                ImGui::TableSetColumnIndex(0);
                UI::Text(line.address == base ? ImVec4{0.4f, 1, 0.4f, 1} : ImVec4{0.5f, 0.85f, 0.9f, 1},
                         "%llX", (unsigned long long)line.address);
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
                        if (UI::Btn("跳", {S(35), S(24)}, Colors::BTN_PURPLE))
                            memViewer_.open(t);
                    ImGui::SameLine();
                }
                if (UI::Btn("存", {S(35), S(24)}, {0.2f, 0.4f, 0.25f, 1}))
                    scanner_.add(line.address);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
    }

    static ImVec4 getMnemonicColor(const char *m)
    {
        if (!m)
            return {1, 1, 1, 1};
        if (m[0] == 'b' || !strncmp(m, "cb", 2) || !strncmp(m, "tb", 2) || !strcmp(m, "ret"))
            return {0.8f, 0.5f, 1, 1};
        if (!strncmp(m, "ld", 2) || !strncmp(m, "st", 2))
            return {0.5f, 0.7f, 1, 1};
        if (!strncmp(m, "add", 3) || !strncmp(m, "sub", 3) || !strncmp(m, "mul", 3) || !strncmp(m, "div", 3))
            return {0.5f, 1, 0.5f, 1};
        if (!strncmp(m, "cmp", 3) || !strncmp(m, "tst", 3))
            return {1, 1, 0.5f, 1};
        if (!strncmp(m, "mov", 3))
            return {0.5f, 1, 1, 1};
        if (!strcmp(m, "nop"))
            return {0.5f, 0.5f, 0.5f, 1};
        return {1, 1, 1, 1};
    }
    static bool isJumpInstruction(const char *m)
    {
        return m && (m[0] == 'B' || !strncmp(m, "CB", 2) || !strncmp(m, "TB", 2) || !strcmp(m, "BL") || !strcmp(m, "BLR"));
    }
    static uintptr_t parseJumpTarget(const char *op)
    {
        if (!op)
            return 0;
        auto p = strstr(op, "#0X");
        if (p)
            return strtoull(p + 1, nullptr, 16);
        p = strstr(op, "0X");
        return p ? strtoull(p, nullptr, 16) : 0;
    }
};

// ============================================================================
// 主函数
// ============================================================================
int RunMemoryTool()
{
    Config::g_Running = true;

    if (!RenderVK::init())
    {
        std::println(stderr, "[错误] 初始化图形引擎失败。");
        return 1;
    }

    if (!Touch_Init())
    {
        std::println(stderr, "[错误] 初始化触摸失败。");
        RenderVK::shutdown();
        return 1;
    }

    int rc = 0;
    try
    {
        MainUI ui;
        while (Config::g_Running)
        {
            Touch_UpdateImGui();
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

    Touch_Shutdown();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    RenderVK::shutdown();

    return rc;
}

int main()
{

    std::println(stdout, "请选择启动模式：");
    std::println(stdout, "  1) 性能测试");
    std::println(stdout, "  2) 内存工具");
    std::println(stdout, "  3) TCP服务器");
    std::print(stdout, "请输入 [1/2/3]: ");

    int rc = 1;
    int mode = 0;
    if (!(std::cin >> mode))
    {
        std::println(stderr, "[错误] 输入无效。");
    }
    else if (mode == 1)
    {
        rc = mainno();
    }
    else if (mode == 2)
    {
        rc = RunMemoryTool();
    }
    else if (mode == 3)
    {
        rc = tcp_server();
    }
    else
    {
        std::println(stderr, "[错误] 未知选项: {}", mode);
    }

    // 仅在 main 函数统一清理全局线程池。
    Utils::GlobalPool.force_stop();
    return rc;
}
