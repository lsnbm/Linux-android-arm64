#pragma once
#include "../base/memory_utils.h"
#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <tuple>

class PointerManager
{
public:
    struct PtrData
    {
        uintptr_t address, value;
        PtrData() : address(0), value(0)
        {
        }
        PtrData(uintptr_t a, uintptr_t v) : address(a), value(v)
        {
        }
    };

    struct PtrDir
    {
        uintptr_t address, value;
        uint32_t start, end;
        PtrDir() : address(0), value(0), start(0), end(0)
        {
        }
        PtrDir(uintptr_t a, uintptr_t v, uint32_t s = 0, uint32_t e = 0) : address(a), value(v), start(s), end(e)
        {
        }
    };

    enum class BaseMode : uint8_t
    {
        Module = 0,
        Manual,
        Array
    };

    struct BaseRange
    {
        BaseMode mode = BaseMode::Module;
        uintptr_t start = 0;
        uintptr_t end = 0;
        uint64_t sourceId = 0;
        std::string name;
        int segment = 0;
        bool isBss = false;
        uintptr_t arrayBase = 0;
        size_t arrayIndex = 0;
    };

    struct PtrRange
    {
        int level = 0;
        BaseRange base;
        std::vector<PtrDir> results;
    };

    struct BinHeader
    {
        char sign[32];
        int module_count;
        int version;
        int size;
        int level;
        uint8_t scanBaseMode;
        uint64_t scanManualBase;
        uint64_t scanArrayBase;
        uint64_t scanArrayCount;
        uint64_t scanTarget;
    };

    struct BinSym
    {
        uint64_t start;
        char name[128];
        int segment;
        int pointer_count;
        int level;
        bool isBss;
        uint8_t sourceMode;
        uint64_t sourceId;
        uint64_t manualBase;
        uint64_t arrayBase;
        uint64_t arrayIndex;
    };

    struct BinLevel
    {
        unsigned int count;
        int level;
    };

    static constexpr int kBinVersion = 103;

    enum class Operation : uint8_t
    {
        Idle = 0,
        Scanning,
        Merging,
        Exporting
    };

    struct State
    {
        Operation operation = Operation::Idle;
        Operation lastOperation = Operation::Idle;
        float progress = 0.0f;
        size_t count = 0;
        bool completed = false;
        bool success = false;
        std::string error;
    };

private:
    struct FileCloser
    {
        void operator()(FILE *file) const noexcept
        {
            if (file) fclose(file);
        }
    };

    using FilePtr = std::unique_ptr<FILE, FileCloser>;

    std::mutex block_mtx_;
    std::condition_variable block_cv_;
    std::vector<PtrData> pointers_;
    std::vector<std::pair<uintptr_t, uintptr_t>> regions_;
    std::vector<BaseRange> moduleBases_;
    std::atomic<Operation> operation_{Operation::Idle};
    std::atomic<Operation> lastOperation_{Operation::Idle};
    std::atomic<float> scanProgress_{0.0f};
    std::atomic<size_t> chainCount_{0};
    std::atomic<bool> completed_{false};
    std::atomic<bool> success_{false};
    mutable std::mutex statusMutex_;
    std::string lastError_;

    bool beginOperation(Operation operation)
    {
        std::lock_guard lock(statusMutex_);
        Operation expected = Operation::Idle;
        if (!operation_.compare_exchange_strong(expected, operation, std::memory_order_acq_rel)) return false;
        lastOperation_.store(operation, std::memory_order_release);
        scanProgress_.store(0.0f, std::memory_order_release);
        chainCount_.store(0, std::memory_order_release);
        completed_.store(false, std::memory_order_release);
        success_.store(false, std::memory_order_release);
        lastError_.clear();
        return true;
    }

    void finishOperation(Operation operation, bool success, std::string error = {}) noexcept
    {
        try
        {
            std::lock_guard lock(statusMutex_);
            lastError_ = success ? std::string() : std::move(error);
        }
        catch (...)
        {
            success = false;
        }

        success_.store(success, std::memory_order_release);
        completed_.store(true, std::memory_order_release);
        scanProgress_.store(1.0f, std::memory_order_release);
        Operation expected = operation;
        if (!operation_.compare_exchange_strong(expected, Operation::Idle, std::memory_order_acq_rel)) operation_.store(Operation::Idle, std::memory_order_release);
    }

    static uint64_t HashSource(std::string_view text)
    {
        uint64_t hash = 1469598103934665603ULL;
        for (const unsigned char ch : text)
        {
            hash ^= ch;
            hash *= 1099511628211ULL;
        }
        return hash;
    }

    static uintptr_t OffsetEnd(uintptr_t base, size_t offset)
    {
        const uintptr_t maxAddress = Config::Constants::ADDR_MAX - 1;
        const uintptr_t last = offset > maxAddress - base ? maxAddress : base + offset;
        return last + 1;
    }

    static size_t SaturatingAdd(size_t left, size_t right)
    {
        return right > std::numeric_limits<size_t>::max() - left ? std::numeric_limits<size_t>::max() : left + right;
    }

    static void WaitFutures(std::vector<std::future<void>> &futures)
    {
        std::exception_ptr firstError;
        for (auto &future : futures)
        {
            try
            {
                if (future.valid()) future.get();
            }
            catch (...)
            {
                if (!firstError) firstError = std::current_exception();
            }
        }
        futures.clear();
        if (firstError) std::rethrow_exception(firstError);
    }

    template <typename F> void RunOperation(Operation operation, std::string_view fallbackError, F &&task) noexcept
    {
        bool succeeded = false;
        std::string error;
        struct Guard
        {
            PointerManager &owner;
            Operation operation;
            bool &succeeded;
            std::string &error;
            ~Guard()
            {
                owner.finishOperation(operation, succeeded, std::move(error));
            }
        } guard{*this, operation, succeeded, error};

        try
        {
            error = fallbackError;
            task(succeeded, error);
        }
        catch (const std::exception &ex)
        {
            try
            {
                error = ex.what();
            }
            catch (...)
            {
            }
        }
        catch (...)
        {
        }
    }

    bool SnapshotLayout(const std::string &filterModule)
    {
        regions_.clear();
        moduleBases_.clear();

        const std::unique_ptr<Driver::virtual_memory> snapshot(new Driver::virtual_memory(dr->GetMemoryInfo()));
        const int regionCount = std::clamp(snapshot->region_count, 0, MAX_SCAN_REGIONS);
        const int moduleCount = std::clamp(snapshot->module_count, 0, MAX_MODULES);
        auto addRegion = [this](uintptr_t rawStart, uintptr_t rawEnd)
        {
            constexpr uintptr_t alignment = sizeof(uintptr_t);
            uintptr_t start = MemUtils::Normalize(rawStart);
            uintptr_t end = MemUtils::Normalize(rawEnd);
            start = std::max(start, Config::Constants::ADDR_MIN + 1);
            end = std::min(end, Config::Constants::ADDR_MAX);
            start = (start + alignment - 1) & ~(alignment - 1);
            end &= ~(alignment - 1);
            if (start < end) regions_.emplace_back(start, end);
        };

        for (int i = 0; i < regionCount; ++i) addRegion(snapshot->regions[i].start, snapshot->regions[i].end);

        for (int moduleIndex = 0; moduleIndex < moduleCount; ++moduleIndex)
        {
            const auto &module = snapshot->modules[moduleIndex];
            const size_t pathLength = strnlen(module.name, sizeof(module.name));
            const std::string fullPath(module.name, pathLength);
            const std::string name(MemUtils::BaseName(fullPath));
            const bool accepted = filterModule.empty() || name.find(filterModule) != std::string::npos;
            const uint64_t sourceId = HashSource(fullPath);
            const int segmentCount = std::clamp(module.seg_count, 0, MAX_SEGS_PER_MODULE);

            for (int segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex)
            {
                const auto &segment = module.segs[segmentIndex];
                const uintptr_t start = MemUtils::Normalize(segment.start);
                const uintptr_t end = MemUtils::Normalize(segment.end);
                addRegion(start, end);
                if (!accepted || !MemUtils::IsValidAddr(start) || end <= start || end > Config::Constants::ADDR_MAX) continue;

                BaseRange base;
                base.start = start;
                base.end = end;
                base.sourceId = sourceId;
                base.name = name;
                base.segment = segment.index;
                base.isBss = segment.index == -1;
                moduleBases_.push_back(std::move(base));
            }
        }

        std::sort(regions_.begin(), regions_.end());
        std::vector<std::pair<uintptr_t, uintptr_t>> merged;
        merged.reserve(regions_.size());
        for (const auto &region : regions_)
        {
            if (merged.empty() || region.first > merged.back().second) merged.push_back(region);
            else merged.back().second = std::max(merged.back().second, region.second);
        }
        regions_ = std::move(merged);

        std::sort(moduleBases_.begin(), moduleBases_.end(), [](const BaseRange &left, const BaseRange &right) { return std::tie(left.start, left.end, left.sourceId, left.segment) < std::tie(right.start, right.end, right.sourceId, right.segment); });
        moduleBases_.erase(std::unique(moduleBases_.begin(), moduleBases_.end(), [](const BaseRange &left, const BaseRange &right) { return left.start == right.start && left.end == right.end && left.sourceId == right.sourceId && left.segment == right.segment; }), moduleBases_.end());
        return !regions_.empty();
    }

    // 完整写入临时文件后原子发布为唯一结果文件。
    static bool SaveUniqueBin(FILE *source, std::string &path)
    {
        if (!source) return false;
        char tempPath[] = ".Pointer_scan_XXXXXX";
        int fd = mkstemp(tempPath);
        if (fd < 0) return false;

        FilePtr temp(fdopen(fd, "w+b"));
        if (!temp)
        {
            close(fd);
            remove(tempPath);
            return false;
        }

        rewind(source);
        std::array<char, 1 << 16> buffer{};
        bool ok = true;
        for (size_t size; (size = fread(buffer.data(), 1, buffer.size(), source)) > 0;)
        {
            if (fwrite(buffer.data(), 1, size, temp.get()) != size)
            {
                ok = false;
                break;
            }
        }
        if (ferror(source) != 0 || fflush(temp.get()) != 0 || fsync(fileno(temp.get())) != 0 || ferror(temp.get()) != 0) ok = false;
        FILE *raw = temp.release();
        if (fclose(raw) != 0) ok = false;

        char candidate[256];
        bool published = false;
        for (int i = 0; ok && i < 9999; ++i)
        {
            if (i == 0) snprintf(candidate, sizeof(candidate), "Pointer.bin");
            else snprintf(candidate, sizeof(candidate), "Pointer_%d.bin", i);

            if (link(tempPath, candidate) == 0)
            {
                path = candidate;
                published = true;
                break;
            }
            if (errno != EEXIST) break;
        }
        remove(tempPath);
        return published;
    }

    std::optional<std::vector<BaseRange>> SnapshotBases(BaseMode mode, uintptr_t manualBase, uintptr_t arrayBase, size_t arrayCount, size_t maxOffset)
    {
        if (mode == BaseMode::Module) return moduleBases_;

        std::vector<BaseRange> bases;
        if (mode == BaseMode::Manual)
        {
            BaseRange base;
            base.mode = mode;
            base.start = manualBase;
            base.end = OffsetEnd(manualBase, maxOffset);
            base.sourceId = manualBase;
            bases.push_back(std::move(base));
            return bases;
        }

        constexpr size_t kBatchEntries = PAGE_SIZE / sizeof(uintptr_t);
        std::array<uintptr_t, kBatchEntries> entries{};
        bases.reserve(arrayCount);
        auto addBase = [&](size_t index, uintptr_t objectBase)
        {
            objectBase = MemUtils::Normalize(objectBase);
            if (!MemUtils::IsValidAddr(objectBase)) return;
            BaseRange base;
            base.mode = mode;
            base.start = objectBase;
            base.end = OffsetEnd(objectBase, maxOffset);
            base.sourceId = index;
            base.arrayBase = arrayBase;
            base.arrayIndex = index;
            bases.push_back(std::move(base));
        };

        for (size_t first = 0; first < arrayCount; first += kBatchEntries)
        {
            const size_t count = std::min(kBatchEntries, arrayCount - first);
            const size_t bytes = count * sizeof(uintptr_t);
            const uintptr_t address = arrayBase + first * sizeof(uintptr_t);
            if (dr->Read(address, entries.data(), bytes) == static_cast<int>(bytes))
            {
                for (size_t i = 0; i < count; ++i) addBase(first + i, entries[i]);
                continue;
            }

            for (size_t i = 0; i < count; ++i)
            {
                uintptr_t objectBase = 0;
                if (dr->Read(address + i * sizeof(uintptr_t), &objectBase, sizeof(objectBase)) != static_cast<int>(sizeof(objectBase))) return std::nullopt;
                addBase(first + i, objectBase);
            }
        }

        std::sort(bases.begin(), bases.end(), [](const BaseRange &left, const BaseRange &right) { return std::tie(left.start, left.end, left.sourceId) < std::tie(right.start, right.end, right.sourceId); });
        return bases;
    }

    template <typename F>
    // 借用缓冲块执行任务并自动归还。
    void with_buffer_block(char **bufs, int &idx, uintptr_t start, size_t len, F &&call)
    {
        char *buf;
        {
            std::unique_lock<std::mutex> lk(block_mtx_);
            block_cv_.wait(lk, [&idx] { return idx >= 0; });
            buf = bufs[idx--];
        }
        struct BufGuard
        {
            char **b;
            int &i;
            char *p;
            std::mutex &m;
            std::condition_variable &cv;
            ~BufGuard()
            {
                std::lock_guard<std::mutex> lk(m);
                b[++i] = p;
                cv.notify_one();
            }
        } guard{bufs, idx, buf, block_mtx_, block_cv_};

        call(buf, start, len);
    }

    // 扫描缓冲块并提取候选指针。
    bool collect_pointers_block(char *buf, uintptr_t start, size_t len, std::vector<PtrData> &out)
    {
        out.clear();
        size_t cursor = 0;
        out.reserve(len / sizeof(uintptr_t));

        while (cursor < len)
        {
            const size_t request = std::min(static_cast<size_t>(PAGE_SIZE), len - cursor);
            if (dr->Read(start + cursor, buf, request) != static_cast<int>(request)) return false;
            const size_t pointerCount = request / sizeof(uintptr_t);
            auto *values = reinterpret_cast<uintptr_t *>(buf);

            for (size_t i = 0; i < pointerCount; ++i)
            {
                const uintptr_t value = MemUtils::Normalize(values[i]);
                const auto region = std::upper_bound(regions_.begin(), regions_.end(), value, [](uintptr_t address, const auto &range) { return address < range.second; });
                if (region == regions_.end() || value < region->first) continue;

                out.emplace_back(MemUtils::Normalize(start + cursor + i * sizeof(uintptr_t)), value);
            }
            cursor += request;
        }
        return true;
    }

    // 在候选指针中筛选可匹配项。
    void search_in_pointers(std::vector<PtrDir> &input, std::vector<PtrData *> &out, size_t offset)
    {
        if (input.empty() || pointers_.empty()) return;

        for (auto &pd : pointers_)
        {
            const uintptr_t v = MemUtils::Normalize(pd.value);
            const auto match = std::lower_bound(input.begin(), input.end(), v, [](const PtrDir &node, uintptr_t address) { return node.address < address; });
            if (match == input.end() || match->address - v > offset) continue;

            out.push_back(&pd);
        }
    }

    // 通过活动区间同时匹配模块、手动和数组基址。
    void filter_to_ranges(std::vector<std::vector<PtrDir>> &dirs, std::vector<PtrRange> &ranges, std::vector<PtrData *> &curr, int level, const std::vector<BaseRange> &bases)
    {
        std::set<size_t> active;
        std::priority_queue<std::pair<uintptr_t, size_t>, std::vector<std::pair<uintptr_t, size_t>>, std::greater<>> expiry;
        std::map<size_t, PtrRange> found;
        size_t nextBase = 0;

        for (auto *pointer : curr)
        {
            const uintptr_t address = MemUtils::Normalize(pointer->address);
            while (nextBase < bases.size() && bases[nextBase].start <= address)
            {
                active.insert(nextBase);
                expiry.emplace(bases[nextBase].end, nextBase);
                ++nextBase;
            }
            while (!expiry.empty() && expiry.top().first <= address)
            {
                active.erase(expiry.top().second);
                expiry.pop();
            }

            const uintptr_t value = MemUtils::Normalize(pointer->value);
            if (active.empty())
            {
                dirs[level].emplace_back(address, value, 0u, 1u);
                continue;
            }
            for (const size_t baseIndex : active)
            {
                auto [entry, inserted] = found.try_emplace(baseIndex);
                if (inserted)
                {
                    entry->second.level = level;
                    entry->second.base = bases[baseIndex];
                }
                entry->second.results.emplace_back(address, value, 0u, 1u);
            }
        }

        for (auto &[baseIndex, range] : found) ranges.push_back(std::move(range));
    }

    // 回填父子区间索引关系。
    void assoc_index(std::vector<PtrDir> &prev, PtrDir *start, size_t count, size_t offset)
    {
        for (size_t i = 0; i < count; i++)
        {
            uintptr_t normVal = MemUtils::Normalize(start[i].value);
            const uintptr_t upper = offset > std::numeric_limits<uintptr_t>::max() - normVal ? std::numeric_limits<uintptr_t>::max() : normVal + offset;
            const auto first = std::lower_bound(prev.begin(), prev.end(), normVal, [](const PtrDir &node, uintptr_t address) { return node.address < address; });
            const auto last = std::upper_bound(first, prev.end(), upper, [](uintptr_t address, const PtrDir &node) { return address < node.address; });
            start[i].start = static_cast<uint32_t>(first - prev.begin());
            start[i].end = static_cast<uint32_t>(last - prev.begin());
        }
    }

    // 并发建立各层索引关联。
    std::vector<std::future<void>> create_assoc_index(std::vector<PtrDir> &prev, std::vector<PtrDir> &curr, size_t offset)
    {
        std::vector<std::future<void>> futures;
        if (curr.empty()) return futures;
        if (prev.size() > std::numeric_limits<uint32_t>::max()) throw std::length_error("指针层节点数量超出格式上限");
        size_t total = curr.size(), pos = 0;
        try
        {
            while (pos < total)
            {
                size_t chunk = std::min(total - pos, static_cast<size_t>(10000));
                futures.push_back(Config::CpuThreadPool().submit_task([this, &prev, s = &curr[pos], chunk, offset] { assoc_index(prev, s, chunk, offset); }));
                pos += chunk;
            }
        }
        catch (...)
        {
            const auto submitError = std::current_exception();
            try
            {
                WaitFutures(futures);
            }
            catch (...)
            {
            }
            std::rethrow_exception(submitError);
        }
        return futures;
    }

    struct DirTree
    {
        std::vector<std::vector<size_t>> counts;
        std::vector<std::vector<PtrDir *>> contents;
        bool valid = false;
    };

    // 合并相邻且可并入的区间节点。
    void merge_dirs(const std::vector<PtrDir *> &sorted_ptrs, PtrDir *base_dir, std::vector<PtrDir *> &out)
    {
        size_t dist = 0;
        uint32_t right = 0;
        out.reserve(sorted_ptrs.size());

        for (auto *p : sorted_ptrs)
        {
            if (right <= p->start)
            {
                dist += p->start - right;
                for (uint32_t j = p->start; j < p->end; j++) out.push_back(&base_dir[j]);
                right = p->end;
            }
            else if (right < p->end)
            {
                for (uint32_t j = right; j < p->end; j++) out.push_back(&base_dir[j]);
                right = p->end;
            }
            p->start -= static_cast<uint32_t>(dist);
            p->end -= static_cast<uint32_t>(dist);
        }
    }

    // 构建层级化指针目录树结构。
    DirTree build_dir_tree(std::vector<std::vector<PtrDir>> &dirs, std::vector<PtrRange> &ranges)
    {
        DirTree tree;
        if (ranges.empty()) return tree;

        int max_level = 0;
        for (auto &r : ranges) max_level = std::max(max_level, r.level);

        std::vector<std::vector<PtrRange *>> level_ranges(dirs.size());
        for (auto &r : ranges) level_ranges[r.level].push_back(&r);

        tree.counts.resize(max_level);
        tree.contents.resize(max_level + 1);

        for (int i = max_level; i > 0; i--)
        {
            std::vector<PtrDir *> stn;
            for (auto *r : level_ranges[i])
                for (auto &v : r->results) stn.push_back(&v);
            for (auto *p : tree.contents[i]) stn.push_back(p);

            std::sort(stn.begin(), stn.end(), [](auto a, auto b) { return a->start < b->start; });

            std::vector<PtrDir *> merged_out;
            merge_dirs(stn, dirs[i - 1].data(), merged_out);

            if (merged_out.empty()) return tree;

            tree.contents[i - 1] = std::move(merged_out);
        }

        tree.counts[0].assign(tree.contents[0].size(), 1);
        for (int i = 1; i < max_level; i++)
        {
            auto &cc = tree.counts[i];
            cc.resize(tree.contents[i].size(), 0);
            for (size_t j = 0; j < tree.contents[i].size(); j++)
            {
                const auto &node = *tree.contents[i][j];
                for (uint32_t child = node.start; child < node.end; ++child) cc[j] = SaturatingAdd(cc[j], tree.counts[i - 1][child]);
            }
        }

        tree.valid = true;
        return tree;
    }

    // 将指针树结果序列化写入文件。
    bool write_bin_file(std::vector<std::vector<PtrDir *>> &contents, std::vector<PtrRange> &ranges, FILE *f, BaseMode scanMode, uintptr_t target, uintptr_t manualBase, uintptr_t arrayBase, size_t arrayCount)
    {
        if (!f || ranges.size() > static_cast<size_t>(std::numeric_limits<int>::max())) return false;
        if (!ranges.empty() && (contents.size() < 2 || contents.size() - 1 > static_cast<size_t>(std::numeric_limits<int>::max()))) return false;
        auto writeObject = [f](const auto &value) { return fwrite(&value, sizeof(value), 1, f) == 1; };
        auto writeArray = [f](const void *data, size_t size, size_t count) { return count == 0 || fwrite(data, size, count, f) == count; };

        BinHeader hdr{};
        strcpy(hdr.sign, ".bin pointer chain");
        hdr.size = sizeof(uintptr_t);
        hdr.version = kBinVersion;
        hdr.module_count = static_cast<int>(ranges.size());
        hdr.level = ranges.empty() ? 0 : static_cast<int>(contents.size()) - 1;
        hdr.scanBaseMode = static_cast<uint8_t>(scanMode);
        hdr.scanManualBase = scanMode == BaseMode::Manual ? MemUtils::Normalize(manualBase) : 0;
        hdr.scanArrayBase = scanMode == BaseMode::Array ? MemUtils::Normalize(arrayBase) : 0;
        hdr.scanArrayCount = scanMode == BaseMode::Array ? arrayCount : 0;
        hdr.scanTarget = MemUtils::Normalize(target);
        if (!writeObject(hdr)) return false;

        for (auto &r : ranges)
        {
            if (r.results.empty() || r.results.size() > static_cast<size_t>(std::numeric_limits<int>::max())) return false;
            BinSym sym{};
            sym.start = MemUtils::Normalize(r.base.start);
            sym.segment = r.base.segment;
            sym.isBss = r.base.isBss;
            sym.sourceMode = static_cast<uint8_t>(r.base.mode);
            sym.sourceId = r.base.sourceId;
            if (r.base.mode == BaseMode::Manual)
            {
                sym.manualBase = sym.start;
                strncpy(sym.name, "manual", sizeof(sym.name) - 1);
            }
            else if (r.base.mode == BaseMode::Array)
            {
                sym.arrayBase = MemUtils::Normalize(r.base.arrayBase);
                sym.arrayIndex = r.base.arrayIndex;
                snprintf(sym.name, sizeof(sym.name), "array[%zu]", r.base.arrayIndex);
            }
            else
            {
                strncpy(sym.name, r.base.name.data(), std::min(r.base.name.size(), sizeof(sym.name) - 1));
            }
            sym.level = r.level;
            sym.pointer_count = static_cast<int>(r.results.size());
            if (!writeObject(sym) || !writeArray(r.results.data(), sizeof(PtrDir), r.results.size())) return false;
        }

        for (size_t i = 0; !ranges.empty() && i + 1 < contents.size(); i++)
        {
            if (contents[i].empty() || contents[i].size() > std::numeric_limits<unsigned int>::max()) return false;
            BinLevel ll{};
            ll.level = static_cast<int>(i);
            ll.count = static_cast<unsigned int>(contents[i].size());
            if (!writeObject(ll)) return false;
            for (auto *p : contents[i])
                if (!writeObject(*p)) return false;
        }
        return fflush(f) == 0 && ferror(f) == 0;
    }

public:
    PointerManager() = default;
    ~PointerManager() = default;

    static std::string_view OperationName(Operation operation)
    {
        switch (operation)
        {
        case Operation::Scanning:
            return "scanning";
        case Operation::Merging:
            return "merging";
        case Operation::Exporting:
            return "exporting";
        default:
            return "idle";
        }
    }

    State state() const
    {
        std::lock_guard lock(statusMutex_);
        State result;
        result.operation = operation_.load(std::memory_order_acquire);
        result.lastOperation = lastOperation_.load(std::memory_order_acquire);
        result.progress = scanProgress_.load(std::memory_order_acquire);
        result.count = chainCount_.load(std::memory_order_acquire);
        result.completed = completed_.load(std::memory_order_acquire);
        result.success = success_.load(std::memory_order_acquire);
        result.error = lastError_;
        return result;
    }

    bool isBusy() const noexcept
    {
        return operation_.load(std::memory_order_acquire) != Operation::Idle;
    }

    void clear()
    {
        std::lock_guard lock(statusMutex_);
        if (operation_.load(std::memory_order_acquire) != Operation::Idle) return;
        pointers_.clear();
        regions_.clear();
        moduleBases_.clear();
        chainCount_ = 0;
        scanProgress_ = 0.0f;
        completed_ = false;
        success_ = false;
        lastOperation_ = Operation::Idle;
        lastError_.clear();
    }

    // 采集进程可用指针并建立初始集合。
    bool CollectPointers(size_t &pointerCount, int buf_count = 10, int buf_size = 1 << 20)
    {
        pointerCount = 0;
        pointers_.clear();
        if (regions_.empty() || buf_count <= 0 || buf_size < static_cast<int>(sizeof(uintptr_t))) return false;
        const size_t blockSize = static_cast<size_t>(buf_size) / sizeof(uintptr_t) * sizeof(uintptr_t);

        FilePtr merged(tmpfile());
        if (!merged)
        {
            LS_LOGE_TAG("Pointer", "无法创建合并临时文件");
            return false;
        }

        int idx = buf_count - 1;
        std::vector<std::unique_ptr<char[]>> bufferStorage;
        bufferStorage.reserve(buf_count);
        std::vector<char *> bufs(buf_count);
        for (int i = 0; i < buf_count; i++)
        {
            bufferStorage.push_back(std::make_unique<char[]>(blockSize + sizeof(uintptr_t)));
            bufs[i] = bufferStorage.back().get();
        }

        std::mutex mergedMtx;
        std::atomic<bool> collectionFailed{false};
        std::vector<std::future<void>> futures;
        const size_t maxPending = std::max<size_t>(1, std::min<size_t>(static_cast<size_t>(buf_count), Config::CpuThreadPool().get_thread_count()));
        try
        {
            for (auto &[rstart, rend] : regions_)
            {
                for (uintptr_t pos = rstart; pos < rend; pos += blockSize)
                {
                    futures.push_back(Config::CpuThreadPool().submit_task(
                        [this, &bufs, &idx, pos, chunk = std::min(static_cast<size_t>(rend - pos), blockSize), file = merged.get(), &mergedMtx, &collectionFailed]
                        {
                            std::vector<PtrData> blockPointers;
                            bool blockOk = true;
                            with_buffer_block(bufs.data(), idx, pos, chunk, [this, &blockPointers, &blockOk](char *buf, uintptr_t s, size_t l) { blockOk = collect_pointers_block(buf, s, l, blockPointers); });
                            if (!blockOk)
                            {
                                collectionFailed = true;
                                return;
                            }
                            if (!blockPointers.empty())
                            {
                                std::lock_guard<std::mutex> lk(mergedMtx);
                                if (!collectionFailed && fwrite(blockPointers.data(), sizeof(PtrData), blockPointers.size(), file) != blockPointers.size()) collectionFailed = true;
                            }
                        }));
                    if (futures.size() >= maxPending) WaitFutures(futures);
                }
            }
            WaitFutures(futures);
        }
        catch (...)
        {
            try
            {
                WaitFutures(futures);
            }
            catch (...)
            {
            }
            throw;
        }

        if (collectionFailed)
        {
            LS_LOGE_TAG("Pointer", "临时存储失败");
            return false;
        }

        if (fflush(merged.get()) != 0 || ferror(merged.get()) != 0)
        {
            LS_LOGE_TAG("Pointer", "刷新临时存储失败");
            return false;
        }

        struct stat st;
        if (fstat(fileno(merged.get()), &st) == 0)
        {
            if (st.st_size < 0 || static_cast<size_t>(st.st_size) % sizeof(PtrData) != 0)
            {
                LS_LOGE_TAG("Pointer", "临时存储大小无效");
                return false;
            }
            size_t total = static_cast<size_t>(st.st_size) / sizeof(PtrData);
            if (total > 0)
            {
                pointers_.resize(total);
                rewind(merged.get());
                if (fread(pointers_.data(), sizeof(PtrData), total, merged.get()) != total)
                {
                    pointers_.clear();
                    LS_LOGE_TAG("Pointer", "读取合并临时存储失败");
                    return false;
                }
            }
        }
        else
        {
            LS_LOGE_TAG("Pointer", "获取合并临时文件状态失败");
            return false;
        }

        std::sort(pointers_.begin(), pointers_.end(), [](const PtrData &left, const PtrData &right) { return std::tie(left.address, left.value) < std::tie(right.address, right.value); });
        pointers_.erase(std::unique(pointers_.begin(), pointers_.end(), [](const PtrData &left, const PtrData &right) { return left.address == right.address && left.value == right.value; }), pointers_.end());
        pointerCount = pointers_.size();
        return true;
    }

private:
    // 执行指针链扫描主流程。
    void runScan(pid_t pid, uintptr_t target, int depth, int maxOffset, bool useManual, uintptr_t manualBase, bool useArray, uintptr_t arrayBase, size_t arrayCount, const std::string &filterModule)
    {
        bool succeeded = false;
        std::string error;
        struct ScanGuard
        {
            PointerManager &owner;
            bool &succeeded;
            std::string &error;
            ~ScanGuard()
            {
                owner.finishOperation(PointerManager::Operation::Scanning, succeeded, std::move(error));
            }
        } guard{*this, succeeded, error};

        try
        {
            if (pid <= 0 || pid != dr->GetGlobalPid())
            {
                error = "目标进程已变化";
                return;
            }

            scanProgress_ = 0.0f;
            chainCount_ = 0;

            target = MemUtils::Normalize(target);
            manualBase = MemUtils::Normalize(manualBase);
            arrayBase = MemUtils::Normalize(arrayBase);

            LS_LOGI_TAG_FMT("Pointer", "开始扫描: 目标=0x{:X} 深度={} 偏移={}", target, depth, maxOffset);

            if (!SnapshotLayout(filterModule))
            {
                error = "无法获取有效内存布局";
                return;
            }

            const BaseMode scanMode = useManual ? BaseMode::Manual : (useArray ? BaseMode::Array : BaseMode::Module);
            auto baseSnapshot = SnapshotBases(scanMode, manualBase, arrayBase, arrayCount, static_cast<size_t>(maxOffset));
            if (!baseSnapshot)
            {
                error = "无法完整读取 Array 基址快照";
                return;
            }
            std::vector<BaseRange> bases = std::move(*baseSnapshot);
            if (scanMode == BaseMode::Module && bases.empty())
            {
                error = filterModule.empty() ? "内存布局中没有可用模块基址" : std::format("没有匹配模块过滤条件: {}", filterModule);
                return;
            }

            size_t pointerCount = 0;
            if (!CollectPointers(pointerCount))
            {
                LS_LOGE_TAG("Pointer", "扫描失败: 无法创建内存快照");
                error = "无法创建内存快照";
                return;
            }
            LS_LOGI_TAG_FMT("Pointer", "内存快照数量: {}", pointerCount);

            FilePtr outfile(tmpfile());
            if (!outfile)
            {
                LS_LOGE_TAG("Pointer", "无法创建结果临时文件");
                error = "无法创建结果临时文件";
                return;
            }

            std::vector<PtrRange> ranges;
            std::vector<std::vector<PtrDir>> dirs(depth + 1);
            size_t totalChains = 0;
            bool wroteResults = false;

            dirs[0].emplace_back(target, 0, 0, 1);
            LS_LOGI_TAG_FMT("Pointer", "Level 0 初始化完成，目标地址数量: {}", dirs[0].size());

            for (int level = 1; level <= depth; level++)
            {
                std::vector<PtrData *> curr;
                search_in_pointers(dirs[level - 1], curr, static_cast<size_t>(maxOffset));

                if (curr.empty())
                {
                    LS_LOGI_TAG_FMT("Pointer", "扫描在 Level {} 结束: 未找到指向上级的指针", level);
                    break;
                }

                LS_LOGI_TAG_FMT("Pointer", "Level {} 搜索结果: 找到 {} 个指针", level, curr.size());
                std::sort(curr.begin(), curr.end(), [](auto a, auto b) { return a->address < b->address; });

                filter_to_ranges(dirs, ranges, curr, level, bases);

                auto futures = create_assoc_index(dirs[level - 1], dirs[level], static_cast<size_t>(maxOffset));
                WaitFutures(futures);

                scanProgress_ = static_cast<float>(level + 1) / (depth + 2);
            }

            for (auto &range : ranges)
            {
                if (range.level > 0)
                {
                    auto futures = create_assoc_index(dirs[range.level - 1], range.results, static_cast<size_t>(maxOffset));
                    WaitFutures(futures);
                }
            }

            if (!ranges.empty())
            {
                auto tree = build_dir_tree(dirs, ranges);
                if (tree.valid)
                {
                    for (auto &r : ranges)
                    {
                        if (r.level > 0 && static_cast<size_t>(r.level - 1) < tree.counts.size())
                        {
                            for (auto &v : r.results)
                            {
                                const auto &counts = tree.counts[r.level - 1];
                                if (v.start >= v.end || v.end > counts.size()) continue;
                                for (uint32_t child = v.start; child < v.end; ++child) totalChains = SaturatingAdd(totalChains, counts[child]);
                            }
                        }
                    }

                    LS_LOGI_TAG_FMT("Pointer", "开始保存 {} 条链条", totalChains);
                    wroteResults = write_bin_file(tree.contents, ranges, outfile.get(), scanMode, target, manualBase, arrayBase, arrayCount);
                    if (wroteResults) LS_LOGI_TAG_FMT("Pointer", "文件写入完成，总链数: {}", totalChains);
                    else LS_LOGE_TAG("Pointer", "指针结果序列化失败");
                }
            }
            else
            {
                std::vector<std::vector<PtrDir *>> emptyContents;
                wroteResults = write_bin_file(emptyContents, ranges, outfile.get(), scanMode, target, manualBase, arrayBase, arrayCount);
                LS_LOGI_TAG("Pointer", "扫描结果为空，已生成空图");
            }

            if (!wroteResults)
            {
                chainCount_ = 0;
                error = "无法序列化指针结果";
                return;
            }

            if (pid != dr->GetGlobalPid())
            {
                error = "目标进程在扫描期间发生变化";
                return;
            }

            std::string autoName;
            const bool saved = SaveUniqueBin(outfile.get(), autoName);
            if (saved) LS_LOGI_TAG_FMT("Pointer", "结果已保存至: {}", autoName);
            else LS_LOGE_TAG("Pointer", "无法原子保存指针结果文件");

            chainCount_ = saved ? totalChains : 0;
            succeeded = saved;
            if (!saved) error = "无法完整保存指针结果";
        }
        catch (const std::exception &ex)
        {
            error = ex.what();
        }
        catch (...)
        {
            error = "指针扫描发生未知异常";
        }
    }

public:
    bool startAsync(pid_t pid, uintptr_t target, int depth, int maxOffset, bool useManual, uintptr_t manualBase, bool useArray, uintptr_t arrayBase, size_t arrayCount, std::string filterModule)
    {
        std::lock_guard targetLock(Config::TargetMutex);
        if (depth <= 0 || depth > 16 || maxOffset <= 0 || (useManual && useArray)) return false;
        target = MemUtils::Normalize(target);
        manualBase = MemUtils::Normalize(manualBase);
        arrayBase = MemUtils::Normalize(arrayBase);
        if (!MemUtils::IsValidAddr(target)) return false;
        if (useManual && !MemUtils::IsValidAddr(manualBase)) return false;
        if (useArray)
        {
            constexpr size_t kMaxArrayCount = 1000000;
            constexpr uintptr_t kMaxArrayBase = Config::Constants::ADDR_MAX - sizeof(uintptr_t);
            if (!MemUtils::IsValidAddr(arrayBase) || arrayCount == 0 || arrayCount > kMaxArrayCount) return false;
            if (arrayBase > kMaxArrayBase || arrayCount - 1 > (kMaxArrayBase - arrayBase) / sizeof(uintptr_t)) return false;
        }
        if (!beginOperation(Operation::Scanning)) return false;
        if (pid <= 0 || pid != dr->GetGlobalPid())
        {
            finishOperation(Operation::Scanning, false, "目标进程已变化");
            return false;
        }
        scanProgress_ = 0.0f;
        try
        {
            Config::IoThreadPool().detach_task([this, pid, target, depth, maxOffset, useManual, manualBase, useArray, arrayBase, arrayCount, filterModule = std::move(filterModule)] { runScan(pid, target, depth, maxOffset, useManual, manualBase, useArray, arrayBase, arrayCount, filterModule); });
        }
        catch (const std::exception &ex)
        {
            finishOperation(Operation::Scanning, false, ex.what());
            return false;
        }
        return true;
    }

    struct MemoryGraph
    {
        BinHeader hdr{};
        struct Block
        {
            BinSym sym;
            std::vector<PtrDir> roots;
        };
        std::vector<Block> blocks;
        std::vector<std::vector<PtrDir>> levels;

        static bool ReadBytes(const char *&cur, const char *eof, void *out, size_t size)
        {
            if (cur > eof || size > static_cast<size_t>(eof - cur)) return false;
            std::memcpy(out, cur, size);
            cur += size;
            return true;
        }

        template <typename T> static bool ReadObject(const char *&cur, const char *eof, T &out)
        {
            return ReadBytes(cur, eof, &out, sizeof(T));
        }

        template <typename T> static bool ReadVector(const char *&cur, const char *eof, size_t count, std::vector<T> &out)
        {
            if (cur > eof) return false;
            if (count > static_cast<size_t>(eof - cur) / sizeof(T)) return false;
            out.resize(count);
            if (count > 0 && !ReadBytes(cur, eof, out.data(), count * sizeof(T))) return false;
            return true;
        }

        static bool WriteBytes(FILE *file, const void *data, size_t size)
        {
            return size == 0 || fwrite(data, 1, size, file) == size;
        }

        template <typename T> static bool WriteObject(FILE *file, const T &value)
        {
            return WriteBytes(file, &value, sizeof(T));
        }

        template <typename T> static bool WriteVector(FILE *file, const std::vector<T> &values)
        {
            return values.empty() || WriteBytes(file, values.data(), values.size() * sizeof(T));
        }

        bool validate() const
        {
            constexpr std::string_view signature = ".bin pointer chain";
            if (std::memcmp(hdr.sign, signature.data(), signature.size()) != 0 || hdr.sign[signature.size()] != '\0') return false;
            if (hdr.version != kBinVersion || hdr.size != static_cast<int>(sizeof(uintptr_t))) return false;
            if (hdr.module_count < 0 || static_cast<size_t>(hdr.module_count) != blocks.size()) return false;
            if (hdr.level < 0 || hdr.level > 16 || levels.size() != static_cast<size_t>(hdr.level)) return false;
            if (hdr.scanBaseMode > static_cast<uint8_t>(BaseMode::Array) || !MemUtils::IsValidAddr(static_cast<uintptr_t>(hdr.scanTarget))) return false;

            const auto mode = static_cast<BaseMode>(hdr.scanBaseMode);
            if (mode == BaseMode::Module)
            {
                if (hdr.scanManualBase != 0 || hdr.scanArrayBase != 0 || hdr.scanArrayCount != 0) return false;
            }
            else if (mode == BaseMode::Manual)
            {
                if (!MemUtils::IsValidAddr(static_cast<uintptr_t>(hdr.scanManualBase)) || hdr.scanArrayBase != 0 || hdr.scanArrayCount != 0) return false;
            }
            else
            {
                constexpr uint64_t kMaxArrayCount = 1000000;
                constexpr uintptr_t kMaxArrayBase = Config::Constants::ADDR_MAX - sizeof(uintptr_t);
                const uintptr_t arrayBase = static_cast<uintptr_t>(hdr.scanArrayBase);
                if (hdr.scanManualBase != 0 || !MemUtils::IsValidAddr(arrayBase) || hdr.scanArrayCount == 0 || hdr.scanArrayCount > kMaxArrayCount) return false;
                if (arrayBase > kMaxArrayBase || hdr.scanArrayCount - 1 > (kMaxArrayBase - arrayBase) / sizeof(uintptr_t)) return false;
            }

            if (blocks.empty()) return hdr.level == 0 && levels.empty();
            if (hdr.level == 0) return false;

            for (int level = 0; level < hdr.level; ++level)
            {
                if (levels[level].empty()) return false;
                if (levels[level].size() > std::numeric_limits<uint32_t>::max()) return false;
                if (level == 0)
                {
                    for (const auto &node : levels[level])
                        if (node.address != hdr.scanTarget || node.value != 0 || node.start != 0 || node.end != 1) return false;
                    continue;
                }
                const size_t childCount = levels[level - 1].size();
                for (const auto &node : levels[level])
                {
                    if (!MemUtils::IsValidAddr(node.address) || !MemUtils::IsValidAddr(node.value) || node.start >= node.end || node.end > childCount) return false;
                    if (!std::is_sorted(levels[level - 1].begin() + node.start, levels[level - 1].begin() + node.end, [](const PtrDir &a, const PtrDir &b) { return a.address < b.address; })) return false;
                }
            }

            int maxLevel = 0;
            std::set<std::tuple<uint8_t, uint64_t, std::string_view, int, bool, uint64_t, int>> blockIdentities;
            for (const auto &block : blocks)
            {
                if (block.sym.pointer_count <= 0 || static_cast<size_t>(block.sym.pointer_count) != block.roots.size()) return false;
                if (block.sym.level <= 0 || block.sym.level > hdr.level || block.sym.sourceMode > static_cast<uint8_t>(BaseMode::Array)) return false;
                if (block.sym.sourceMode != hdr.scanBaseMode) return false;
                if (std::memchr(block.sym.name, '\0', sizeof(block.sym.name)) == nullptr) return false;
                if (!MemUtils::IsValidAddr(static_cast<uintptr_t>(block.sym.start))) return false;

                if (mode == BaseMode::Module)
                {
                    if (block.sym.name[0] == '\0' || block.sym.segment < -1 || block.sym.isBss != (block.sym.segment == -1)) return false;
                    if (block.sym.manualBase != 0 || block.sym.arrayBase != 0 || block.sym.arrayIndex != 0) return false;
                }
                else if (mode == BaseMode::Manual)
                {
                    if (block.sym.start != hdr.scanManualBase || block.sym.manualBase != hdr.scanManualBase || block.sym.sourceId != hdr.scanManualBase) return false;
                    if (block.sym.arrayBase != 0 || block.sym.arrayIndex != 0 || block.sym.segment != 0 || block.sym.isBss) return false;
                }
                else
                {
                    if (block.sym.arrayBase != hdr.scanArrayBase || block.sym.arrayIndex >= hdr.scanArrayCount || block.sym.sourceId != block.sym.arrayIndex) return false;
                    if (block.sym.manualBase != 0 || block.sym.segment != 0 || block.sym.isBss) return false;
                }

                const std::string_view name(block.sym.name);
                if (!blockIdentities.emplace(block.sym.sourceMode, block.sym.sourceId, name, block.sym.segment, block.sym.isBss, block.sym.arrayIndex, block.sym.level).second) return false;

                const size_t childCount = levels[block.sym.level - 1].size();
                std::set<std::tuple<uintptr_t, uintptr_t, uint32_t, uint32_t>> roots;
                for (const auto &root : block.roots)
                {
                    if (!MemUtils::IsValidAddr(root.address) || !MemUtils::IsValidAddr(root.value) || root.start >= root.end || root.end > childCount) return false;
                    if (!std::is_sorted(levels[block.sym.level - 1].begin() + root.start, levels[block.sym.level - 1].begin() + root.end, [](const PtrDir &a, const PtrDir &b) { return a.address < b.address; })) return false;
                    if (!roots.emplace(root.address, root.value, root.start, root.end).second) return false;
                }
                maxLevel = std::max(maxLevel, block.sym.level);
            }
            return maxLevel == hdr.level;
        }

        // 从二进制文件加载指针图数据。
        bool load(const std::string &path)
        {
            struct Mapping
            {
                int fd = -1;
                void *data = MAP_FAILED;
                size_t size = 0;
                ~Mapping()
                {
                    if (data != MAP_FAILED) munmap(data, size);
                    if (fd >= 0) close(fd);
                }
            } mapping;

            mapping.fd = open(path.c_str(), O_RDONLY);
            if (mapping.fd < 0) return false;
            struct stat st;
            if (fstat(mapping.fd, &st) != 0 || st.st_size < static_cast<off_t>(sizeof(BinHeader))) return false;
            if (static_cast<uintmax_t>(st.st_size) > std::numeric_limits<size_t>::max()) return false;

            mapping.size = static_cast<size_t>(st.st_size);
            mapping.data = mmap(nullptr, mapping.size, PROT_READ, MAP_PRIVATE, mapping.fd, 0);
            if (mapping.data == MAP_FAILED) return false;

            const char *cur = static_cast<const char *>(mapping.data);
            const char *eof = cur + mapping.size;
            blocks.clear();
            levels.clear();
            bool ok = false;
            try
            {
                ok = ReadObject(cur, eof, hdr);
                constexpr std::string_view signature = ".bin pointer chain";
                ok = ok && std::memcmp(hdr.sign, signature.data(), signature.size()) == 0 && hdr.sign[signature.size()] == '\0';
                ok = ok && hdr.version == kBinVersion && hdr.size == static_cast<int>(sizeof(uintptr_t));
                ok = ok && hdr.module_count >= 0 && hdr.level >= 0 && hdr.level <= 16 && hdr.scanBaseMode <= static_cast<uint8_t>(BaseMode::Array);
                ok = ok && static_cast<size_t>(hdr.module_count) <= static_cast<size_t>(eof - cur) / sizeof(BinSym);

                if (ok) blocks.reserve(static_cast<size_t>(hdr.module_count));
                for (int i = 0; ok && i < hdr.module_count; ++i)
                {
                    Block block;
                    ok = ReadObject(cur, eof, block.sym);
                    ok = ok && block.sym.pointer_count > 0 && block.sym.level > 0 && block.sym.level <= hdr.level;
                    ok = ok && block.sym.sourceMode <= static_cast<uint8_t>(BaseMode::Array);
                    ok = ok && std::memchr(block.sym.name, '\0', sizeof(block.sym.name)) != nullptr;
                    if (!ok || !ReadVector(cur, eof, static_cast<size_t>(block.sym.pointer_count), block.roots))
                    {
                        ok = false;
                        break;
                    }
                    blocks.push_back(std::move(block));
                }

                if (ok) levels.resize(static_cast<size_t>(hdr.level));
                for (int expectedLevel = 0; ok && expectedLevel < hdr.level; ++expectedLevel)
                {
                    BinLevel level{};
                    ok = ReadObject(cur, eof, level);
                    ok = ok && level.level == expectedLevel && level.count > 0;
                    if (!ok || !ReadVector(cur, eof, static_cast<size_t>(level.count), levels[expectedLevel]))
                    {
                        ok = false;
                        break;
                    }
                }

                if (ok) ok = cur == eof && validate();
            }
            catch (...)
            {
                ok = false;
            }

            if (!ok)
            {
                hdr = {};
                blocks.clear();
                levels.clear();
            }
            return ok;
        }

        // 将当前指针图保存到文件。
        bool save(const std::string &path) const
        {
            if (!validate()) return false;
            FilePtr file(fopen(path.c_str(), "wb"));
            if (!file) return false;

            bool ok = WriteObject(file.get(), hdr);
            for (const auto &blk : blocks)
            {
                ok = ok && WriteObject(file.get(), blk.sym) && WriteVector(file.get(), blk.roots);
                if (!ok) break;
            }
            for (int i = 0; ok && i < hdr.level; ++i)
            {
                if (levels[i].size() > std::numeric_limits<unsigned int>::max())
                {
                    ok = false;
                    break;
                }
                BinLevel bl{};
                bl.level = i;
                bl.count = static_cast<unsigned int>(levels[i].size());
                ok = WriteObject(file.get(), bl) && WriteVector(file.get(), levels[i]);
            }
            if (ok) ok = fflush(file.get()) == 0 && fsync(fileno(file.get())) == 0 && ferror(file.get()) == 0;
            FILE *raw = file.release();
            if (fclose(raw) != 0) ok = false;
            if (!ok) remove(path.c_str());
            return ok;
        }
    };

    static uint64_t ChainBaseAddr(const BinSym &sym)
    {
        return (sym.sourceMode == 1) ? sym.manualBase : sym.start;
    }

    using RootIdentity = std::tuple<uint8_t, uint64_t, std::string, int, bool, uint64_t, bool, uint64_t, int>;

    static RootIdentity MakeRootIdentity(const BinSym &sym, const PtrDir &root)
    {
        const uint64_t base = ChainBaseAddr(sym);
        const bool negative = root.address < base;
        const uint64_t offset = negative ? base - root.address : root.address - base;
        const std::string name = sym.sourceMode == 0 ? std::string(sym.name) : std::string();
        const uint64_t sourceId = sym.sourceMode == 0 ? sym.sourceId : 0;
        const int segment = sym.sourceMode == 0 ? sym.segment : 0;
        const bool isBss = sym.sourceMode == 0 && sym.isBss;
        const uint64_t arrayIndex = sym.sourceMode == 2 ? sym.arrayIndex : 0;
        return {sym.sourceMode, sourceId, name, segment, isBss, arrayIndex, negative, offset, sym.level};
    }

    struct PrunedNode
    {
        PtrDir node;
        std::vector<PrunedNode> children;
    };

    static std::optional<PrunedNode> IntersectNode(const PtrDir &nodeA, const std::vector<const PtrDir *> &nodesB, int childLevel, const MemoryGraph &graphA, const MemoryGraph &graphB)
    {
        if (nodesB.empty()) return std::nullopt;

        PrunedNode result{nodeA, {}};
        if (childLevel < 0)
        {
            result.node.start = 0;
            result.node.end = 1;
            return result;
        }
        if (static_cast<size_t>(childLevel) >= graphA.levels.size() || static_cast<size_t>(childLevel) >= graphB.levels.size()) return std::nullopt;

        const auto &layerA = graphA.levels[childLevel];
        const auto &layerB = graphB.levels[childLevel];
        const uint32_t startA = std::min(static_cast<uint32_t>(layerA.size()), nodeA.start);
        const uint32_t endA = std::min(static_cast<uint32_t>(layerA.size()), nodeA.end);
        if (startA >= endA) return std::nullopt;

        for (uint32_t i = startA; i < endA; ++i)
        {
            if (layerA[i].address < nodeA.value) continue;
            const uint64_t offset = layerA[i].address - nodeA.value;
            std::vector<const PtrDir *> matchingChildren;

            for (const PtrDir *nodeB : nodesB)
            {
                if (offset > std::numeric_limits<uint64_t>::max() - nodeB->value) continue;
                const uint64_t expectedAddress = nodeB->value + offset;
                const uint32_t startB = std::min(static_cast<uint32_t>(layerB.size()), nodeB->start);
                const uint32_t endB = std::min(static_cast<uint32_t>(layerB.size()), nodeB->end);
                if (startB >= endB) continue;

                auto it = std::lower_bound(layerB.begin() + startB, layerB.begin() + endB, expectedAddress, [](const PtrDir &node, uint64_t address) { return node.address < address; });
                while (it != layerB.begin() + endB && it->address == expectedAddress)
                {
                    matchingChildren.push_back(&*it);
                    ++it;
                }
            }

            auto child = IntersectNode(layerA[i], matchingChildren, childLevel - 1, graphA, graphB);
            if (child) result.children.push_back(std::move(*child));
        }

        if (result.children.empty()) return std::nullopt;
        return result;
    }

    static bool FlattenPrunedNode(const PrunedNode &source, int childLevel, std::vector<std::vector<PtrDir>> &levels, PtrDir &output)
    {
        output = source.node;
        if (childLevel < 0)
        {
            output.start = 0;
            output.end = 1;
            return true;
        }
        if (source.children.empty() || static_cast<size_t>(childLevel) >= levels.size()) return false;

        auto &layer = levels[childLevel];
        if (source.children.size() > std::numeric_limits<uint32_t>::max() - layer.size()) return false;
        output.start = static_cast<uint32_t>(layer.size());
        for (const auto &child : source.children)
        {
            PtrDir flattenedChild;
            if (!FlattenPrunedNode(child, childLevel - 1, levels, flattenedChild)) return false;
            layer.push_back(flattenedChild);
        }
        output.end = static_cast<uint32_t>(layer.size());
        return output.start < output.end;
    }

    static bool IntersectGraphs(const MemoryGraph &graphA, const MemoryGraph &graphB, MemoryGraph &output, std::string &error)
    {
        if (graphA.hdr.scanBaseMode != graphB.hdr.scanBaseMode)
        {
            error = "指针文件的基址模式不一致";
            return false;
        }

        std::map<RootIdentity, std::vector<const PtrDir *>> rootIndex;
        for (const auto &block : graphB.blocks)
            for (const auto &root : block.roots) rootIndex[MakeRootIdentity(block.sym, root)].push_back(&root);

        output = {};
        output.hdr = graphA.hdr;
        output.levels.resize(static_cast<size_t>(graphA.hdr.level));

        if (graphA.blocks.empty() || graphB.blocks.empty())
        {
            output.hdr.module_count = 0;
            output.hdr.level = 0;
            output.levels.clear();
            return output.validate();
        }

        for (const auto &blockA : graphA.blocks)
        {
            MemoryGraph::Block outputBlock;
            outputBlock.sym = blockA.sym;

            for (const auto &rootA : blockA.roots)
            {
                const auto candidates = rootIndex.find(MakeRootIdentity(blockA.sym, rootA));
                if (candidates == rootIndex.end()) continue;

                auto prunedRoot = IntersectNode(rootA, candidates->second, blockA.sym.level - 1, graphA, graphB);
                if (!prunedRoot) continue;

                PtrDir flattenedRoot;
                if (!FlattenPrunedNode(*prunedRoot, blockA.sym.level - 1, output.levels, flattenedRoot))
                {
                    error = "裁剪后的指针图过大或结构无效";
                    return false;
                }
                outputBlock.roots.push_back(flattenedRoot);
            }

            if (!outputBlock.roots.empty())
            {
                if (outputBlock.roots.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
                {
                    error = "裁剪后的根节点数量超出格式上限";
                    return false;
                }
                outputBlock.sym.pointer_count = static_cast<int>(outputBlock.roots.size());
                output.blocks.push_back(std::move(outputBlock));
            }
        }

        if (output.blocks.empty())
        {
            output.hdr.module_count = 0;
            output.hdr.level = 0;
            output.levels.clear();
            return output.validate();
        }
        if (output.blocks.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
        {
            error = "裁剪后的基址块数量超出格式上限";
            return false;
        }

        int maxLevel = 0;
        for (const auto &block : output.blocks) maxLevel = std::max(maxLevel, block.sym.level);
        output.hdr.level = maxLevel;
        output.hdr.module_count = static_cast<int>(output.blocks.size());
        output.levels.resize(static_cast<size_t>(maxLevel));
        if (!output.validate())
        {
            error = "裁剪后的指针图校验失败";
            return false;
        }
        return true;
    }

    static size_t CountGraphChains(const MemoryGraph &graph)
    {
        if (graph.blocks.empty()) return 0;

        std::vector<std::vector<size_t>> counts(static_cast<size_t>(graph.hdr.level));
        counts[0].assign(graph.levels[0].size(), 1);
        for (int level = 1; level < graph.hdr.level; ++level)
        {
            counts[level].resize(graph.levels[level].size(), 0);
            for (size_t i = 0; i < graph.levels[level].size(); ++i)
            {
                const auto &node = graph.levels[level][i];
                for (uint32_t child = node.start; child < node.end; ++child) counts[level][i] = SaturatingAdd(counts[level][i], counts[level - 1][child]);
            }
        }

        size_t total = 0;
        for (const auto &block : graph.blocks)
        {
            const auto &childCounts = counts[block.sym.level - 1];
            for (const auto &root : block.roots)
                for (uint32_t child = root.start; child < root.end; ++child) total = SaturatingAdd(total, childCounts[child]);
        }
        return total;
    }

    // 合并多轮扫描结果并裁剪失效链。
    bool MergeBins()
    {
        {
            std::lock_guard targetLock(Config::TargetMutex);
            if (!beginOperation(Operation::Merging)) return false;
        }

        try
        {
            Config::CpuThreadPool().detach_task(
                [this]()
                {
                    RunOperation(Operation::Merging, "指针结果合并失败",
                                 [this](bool &succeeded, std::string &error)
                                 {
                                     LS_LOGI_TAG("Pointer", "开始基于图裁剪算法合并结果");

                                     std::vector<std::string> files;
                                     if (access("Pointer.bin", F_OK) == 0) files.push_back("Pointer.bin");
                                     for (int i = 1; i < 9999; ++i)
                                     {
                                         char buf[64];
                                         snprintf(buf, 64, "Pointer_%d.bin", i);
                                         if (access(buf, F_OK) == 0) files.push_back(buf);
                                     }

                                     if (files.size() < 2)
                                     {
                                         error = "至少需要两个指针结果文件";
                                         LS_LOGE_TAG_FMT("Pointer", "合并失败: {}", error);
                                         return;
                                     }

                                     MemoryGraph GA;
                                     LS_LOGI_TAG_FMT("Pointer", "加载基准指针图: {}", files[0]);
                                     if (!GA.load(files[0]))
                                     {
                                         error = std::format("无法加载或校验 {}", files[0]);
                                         return;
                                     }

                                     for (size_t f_idx = 1; f_idx < files.size(); ++f_idx)
                                     {
                                         LS_LOGI_TAG_FMT("Pointer", "正在比对并裁剪: {}", files[f_idx]);
                                         MemoryGraph GB;
                                         if (!GB.load(files[f_idx]))
                                         {
                                             error = std::format("无法加载或校验 {}", files[f_idx]);
                                             return;
                                         }
                                         MemoryGraph nextGraph;
                                         std::string intersectError;
                                         if (!IntersectGraphs(GA, GB, nextGraph, intersectError))
                                         {
                                             error = std::format("{}: {}", files[f_idx], intersectError);
                                             return;
                                         }
                                         GA = std::move(nextGraph);
                                         scanProgress_ = static_cast<float>(f_idx + 1) / static_cast<float>(files.size() + 1);

                                         size_t remaining_roots = 0;
                                         for (auto &blk : GA.blocks) remaining_roots += blk.roots.size();
                                         LS_LOGI_TAG_FMT("Pointer", "该轮裁剪完毕，剩余有效起始节点: {}", remaining_roots);
                                     }

                                     const size_t mergedChainCount = CountGraphChains(GA);
                                     remove("Pointer_Merged.tmp");
                                     if (!GA.save("Pointer_Merged.tmp"))
                                     {
                                         error = "无法完整写入合并临时文件";
                                         return;
                                     }
                                     if (rename("Pointer_Merged.tmp", "Pointer.bin") != 0)
                                     {
                                         error = std::format("无法替换 Pointer.bin: {}", std::strerror(errno));
                                         remove("Pointer_Merged.tmp");
                                         return;
                                     }
                                     for (const auto &fn : files)
                                     {
                                         if (fn != "Pointer.bin" && remove(fn.c_str()) != 0) LS_LOGE_TAG_FMT("Pointer", "无法删除旧文件 {}: {}", fn, std::strerror(errno));
                                     }

                                     chainCount_ = mergedChainCount;
                                     succeeded = true;
                                     LS_LOGI_TAG("Pointer", "图层合并结束，已生成 Pointer.bin");
                                 });
                });
        }
        catch (const std::exception &ex)
        {
            finishOperation(Operation::Merging, false, ex.what());
            return false;
        }
        catch (...)
        {
            finishOperation(Operation::Merging, false, "无法启动指针合并任务");
            return false;
        }
        return true;
    }

    // 将指针链导出为可读文本。
    bool ExportToTxt()
    {
        {
            std::lock_guard targetLock(Config::TargetMutex);
            if (!beginOperation(Operation::Exporting)) return false;
        }

        try
        {
            Config::CpuThreadPool().detach_task(
                [this]()
                {
                    RunOperation(Operation::Exporting, "指针链导出失败",
                                 [this](bool &succeeded, std::string &error)
                                 {
                                     LS_LOGI_TAG("Pointer", "开始导出文本链条");

                                     MemoryGraph graph;
                                     if (!graph.load("Pointer.bin"))
                                     {
                                         error = "无法加载或校验 Pointer.bin";
                                         return;
                                     }

                                     constexpr const char *tempPath = "Pointer_Export.tmp";
                                     constexpr const char *outputPath = "Pointer_Export.txt";
                                     remove(tempPath);
                                     FilePtr output(fopen(tempPath, "wb"));
                                     if (!output)
                                     {
                                         error = std::format("无法创建导出临时文件: {}", std::strerror(errno));
                                         return;
                                     }
                                     struct TempGuard
                                     {
                                         const char *path;
                                         bool committed = false;
                                         ~TempGuard()
                                         {
                                             if (!committed) remove(path);
                                         }
                                     } tempGuard{tempPath};

                                     bool writeOk = true;
                                     auto writeText = [&](const char *text)
                                     {
                                         if (writeOk && fputs(text, output.get()) == EOF) writeOk = false;
                                     };
                                     auto writeFormat = [&](const char *format, auto... args)
                                     {
                                         if (writeOk && fprintf(output.get(), format, args...) < 0) writeOk = false;
                                     };

                                     writeText("// Pointer Scan Export\n");
                                     writeFormat("// Version: %d, Depth: %d\n", graph.hdr.version, graph.hdr.level);
                                     writeFormat("// Target: 0x%llX\n", static_cast<unsigned long long>(graph.hdr.scanTarget));
                                     writeFormat("// Base Mode: %d (0=Module, 1=Manual, 2=Array)\n", graph.hdr.scanBaseMode);
                                     writeText("// ========================================\n\n");

                                     size_t exportedChains = 0;
                                     struct SignedOffset
                                     {
                                         bool negative;
                                         uint64_t magnitude;
                                     };
                                     std::array<SignedOffset, 16> offsets{};
                                     size_t offsetCount = 0;
                                     std::string currentBasePrefix;

                                     std::function<void(int, const PtrDir &)> dfs = [&](int currentLevel, const PtrDir &node)
                                     {
                                         if (!writeOk) return;
                                         if (currentLevel < 0)
                                         {
                                             writeFormat("%s", currentBasePrefix.c_str());
                                             for (size_t i = 0; i < offsetCount; ++i) writeFormat(offsets[i].negative ? " - 0x%llX" : " + 0x%llX", static_cast<unsigned long long>(offsets[i].magnitude));
                                             writeText("\n");
                                             if (exportedChains < std::numeric_limits<size_t>::max()) ++exportedChains;
                                             return;
                                         }

                                         if (static_cast<size_t>(currentLevel) >= graph.levels.size() || node.start >= node.end || node.end > graph.levels[currentLevel].size())
                                         {
                                             writeOk = false;
                                             return;
                                         }

                                         for (uint32_t i = node.start; i < node.end; ++i)
                                         {
                                             if (offsetCount >= offsets.size())
                                             {
                                                 writeOk = false;
                                                 return;
                                             }
                                             const auto &child = graph.levels[currentLevel][i];
                                             offsets[offsetCount++] = child.address < node.value ? SignedOffset{true, node.value - child.address} : SignedOffset{false, child.address - node.value};
                                             dfs(currentLevel - 1, child);
                                             --offsetCount;
                                         }
                                     };

                                     for (const auto &block : graph.blocks)
                                     {
                                         char baseText[256];
                                         uint64_t baseAddress = 0;
                                         switch (block.sym.sourceMode)
                                         {
                                         case 1:
                                             snprintf(baseText, sizeof(baseText), "\"Manual_0x%llX\"", static_cast<unsigned long long>(block.sym.manualBase));
                                             baseAddress = block.sym.manualBase;
                                             break;
                                         case 2:
                                             snprintf(baseText, sizeof(baseText), "\"Array[%llu]\"", static_cast<unsigned long long>(block.sym.arrayIndex));
                                             baseAddress = block.sym.start;
                                             break;
                                         default:
                                             snprintf(baseText, sizeof(baseText), "\"%s[%d]\"", block.sym.name, block.sym.segment);
                                             baseAddress = block.sym.start;
                                             break;
                                         }

                                         for (const auto &root : block.roots)
                                         {
                                             const bool negative = root.address < baseAddress;
                                             const uint64_t magnitude = negative ? baseAddress - root.address : root.address - baseAddress;
                                             currentBasePrefix = std::format("[{} {} 0x{:X}]", baseText, negative ? "-" : "+", magnitude);
                                             offsetCount = 0;
                                             dfs(block.sym.level - 1, root);
                                         }
                                     }

                                     if (writeOk) writeOk = fflush(output.get()) == 0 && fsync(fileno(output.get())) == 0 && ferror(output.get()) == 0;
                                     FILE *rawOutput = output.release();
                                     if (fclose(rawOutput) != 0) writeOk = false;
                                     if (!writeOk)
                                     {
                                         error = "导出文件写入不完整";
                                         return;
                                     }
                                     if (rename(tempPath, outputPath) != 0)
                                     {
                                         error = std::format("无法替换 {}: {}", outputPath, std::strerror(errno));
                                         return;
                                     }
                                     tempGuard.committed = true;

                                     chainCount_ = exportedChains;
                                     succeeded = true;
                                     LS_LOGI_TAG_FMT("Pointer", "导出完成: 成功输出 {} 条链条", exportedChains);
                                 });
                });
        }
        catch (const std::exception &ex)
        {
            finishOperation(Operation::Exporting, false, ex.what());
            return false;
        }
        catch (...)
        {
            finishOperation(Operation::Exporting, false, "无法启动指针导出任务");
            return false;
        }
        return true;
    }
};
