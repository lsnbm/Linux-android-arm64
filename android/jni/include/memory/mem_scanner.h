#pragma once
#include "../base/config.h"
#include "../base/scan_value.h"
#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <numeric>
#include <optional>
#include <shared_mutex>
#include <span>
#include <unistd.h>
#include <vector>

template <typename MemoryReader> class MemScanner
{
public:
    using Results = std::vector<uintptr_t>;

private:
    MemoryReader &memory_;
    size_t alignment_ = 1;

    template <typename T> struct ValueWindow
    {
        std::vector<uint8_t> combined;
        std::array<uint8_t, sizeof(T) - 1> tail{};
        size_t tailSize = 0;
        uintptr_t previousEnd = 0;

        template <typename Process> void process(uintptr_t address, const uint8_t *buffer, size_t size, size_t alignment, Process &&processValue)
        {
            if (address != previousEnd) tailSize = 0;
            combined.resize(tailSize + size);
            if (tailSize != 0) std::memcpy(combined.data(), tail.data(), tailSize);
            std::memcpy(combined.data() + tailSize, buffer, size);
            const uintptr_t base = address - tailSize;
            for (size_t offset = (alignment - base % alignment) % alignment; offset + sizeof(T) <= combined.size(); offset += alignment)
            {
                T value{};
                std::memcpy(&value, combined.data() + offset, sizeof(T));
                processValue(base + offset, value);
            }
            tailSize = std::min(sizeof(T) - 1, combined.size());
            if (tailSize != 0) std::memcpy(tail.data(), combined.data() + combined.size() - tailSize, tailSize);
            previousEnd = address + size;
        }
    };

    struct CandidateRecord
    {
        uintptr_t address;
        std::uint64_t value;
    };

    struct SnapshotSpan
    {
        uintptr_t address;
        std::uint64_t fileOffset;
        size_t size;
    };

    struct FileCloser
    {
        void operator()(FILE *file) const noexcept
        {
            if (file) fclose(file);
        }
    };

    using FilePtr = std::unique_ptr<FILE, FileCloser>;

    FilePtr candidates_;
    FilePtr snapshot_;
    std::vector<SnapshotSpan> snapshotSpans_;

    size_t candidateCount_ = 0;
    size_t snapshotCount_ = 0;
    std::optional<Types::DataType> dataType_;
    Types::FuzzyMode scanMode_ = Types::FuzzyMode::Unknown;
    bool stringScan_ = false;

    mutable std::mutex operationMutex_;
    mutable std::shared_mutex mutex_;
    std::atomic<float> progress_{0.0f};
    std::atomic<bool> scanning_{false};
    std::atomic<size_t> liveCount_{0};

    struct ScanRunGuard
    {
        std::atomic<bool> &scanning;
        std::atomic<float> &progress;
        ~ScanRunGuard()
        {
            progress = 1.0f;
            scanning = false;
        }
    };

    static FilePtr createCandidateFile()
    {
        FilePtr file(tmpfile());
        if (!file) throw std::runtime_error("无法创建扫描候选临时文件");
        return file;
    }

    static void writeCandidates(FILE *file, std::span<const CandidateRecord> records)
    {
        if (!records.empty() && fwrite(records.data(), sizeof(CandidateRecord), records.size(), file) != records.size()) throw std::runtime_error("写入扫描候选临时文件失败");
    }

    static void finishFile(FILE *file)
    {
        if (fflush(file) != 0 || ferror(file) != 0) throw std::runtime_error("刷新扫描候选临时文件失败");
        rewind(file);
    }

    static bool readExactAt(FILE *file, void *buffer, size_t size, std::uint64_t offset)
    {
        if (!file || size > static_cast<size_t>(std::numeric_limits<ssize_t>::max())) return false;
        return pread(fileno(file), buffer, size, static_cast<off_t>(offset)) == static_cast<ssize_t>(size);
    }

    static std::vector<std::pair<uintptr_t, uintptr_t>> normalizeScanRegions(std::vector<std::pair<uintptr_t, uintptr_t>> regions)
    {
        std::erase_if(regions, [](const auto &region) { return region.second <= region.first; });
        std::sort(regions.begin(), regions.end());
        size_t write = 0;
        for (const auto &[start, end] : regions)
        {
            if (write == 0 || start > regions[write - 1].second) regions[write++] = {start, end};
            else regions[write - 1].second = std::max(regions[write - 1].second, end);
        }
        regions.resize(write);
        return regions;
    }

    void updateByteProgress(size_t completed, size_t total)
    {
        if (total != 0) progress_ = std::min(1.0f, static_cast<float>(completed) / static_cast<float>(total));
    }

    template <typename T> static std::uint64_t storeValue(T value) noexcept
    {
        static_assert(sizeof(T) <= sizeof(std::uint64_t));
        std::uint64_t stored = 0;
        std::memcpy(&stored, &value, sizeof(T));
        return stored;
    }

    template <typename T> static T loadValue(std::uint64_t stored) noexcept
    {
        static_assert(sizeof(T) <= sizeof(std::uint64_t));
        T value{};
        std::memcpy(&value, &stored, sizeof(T));
        return value;
    }

    bool scanReadExact(uintptr_t addr, void *buffer, size_t size)
    {
        auto *bytes = static_cast<uint8_t *>(buffer);
        size_t done = 0;
        while (done < size)
        {
            const size_t pageOffset = static_cast<size_t>((addr + done) % Config::Constants::SCAN_BUFFER);
            const size_t request = std::min(size - done, Config::Constants::SCAN_BUFFER - pageOffset);
            if (memory_.Read(addr + done, bytes + done, request) != static_cast<int>(request)) return false;
            done += request;
        }
        return true;
    }

    void clearStorage()
    {
        candidates_.reset();
        snapshot_.reset();
        snapshotSpans_.clear();
        candidateCount_ = 0;
        snapshotCount_ = 0;
        liveCount_ = 0;
    }

    void publishCandidates(FilePtr candidates, size_t count, Types::FuzzyMode mode)
    {
        std::unique_lock lock(mutex_);
        candidates_ = std::move(candidates);
        snapshot_.reset();
        snapshotSpans_.clear();
        candidateCount_ = count;
        snapshotCount_ = 0;
        liveCount_ = count;
        scanMode_ = mode;
    }

    template <typename Process> void scanMemoryRegions(const std::vector<std::pair<uintptr_t, uintptr_t>> &regions, Process &&process)
    {
        size_t totalBytes = 0;
        for (const auto &[start, end] : regions)
        {
            const size_t bytes = static_cast<size_t>(end - start);
            totalBytes = bytes > std::numeric_limits<size_t>::max() - totalBytes ? std::numeric_limits<size_t>::max() : totalBytes + bytes;
        }
        size_t completedBytes = 0;
        std::vector<uint8_t> batch(Config::Constants::BATCH_SIZE);
        std::vector<uint8_t> page(Config::Constants::SCAN_BUFFER);

        for (const auto &[start, end] : regions)
        {
            for (uintptr_t address = start; address < end && Config::g_Running;)
            {
                const size_t request = std::min(static_cast<size_t>(end - address), batch.size());
                if (memory_.Read(address, batch.data(), request) == static_cast<int>(request))
                {
                    process(address, batch.data(), request);
                }
                else
                {
                    for (size_t offset = 0; offset < request;)
                    {
                        const size_t pageOffset = static_cast<size_t>((address + offset) % Config::Constants::SCAN_BUFFER);
                        const size_t chunk = std::min(request - offset, Config::Constants::SCAN_BUFFER - pageOffset);
                        if (scanReadExact(address + offset, page.data(), chunk)) process(address + offset, page.data(), chunk);
                        offset += chunk;
                    }
                }
                address += request;
                completedBytes += request;
                updateByteProgress(completedBytes, totalBytes);
            }
        }
    }

    static void appendCandidate(FILE *file, std::vector<CandidateRecord> &buffer, CandidateRecord record)
    {
        buffer.push_back(record);
        if (buffer.size() >= 4096)
        {
            writeCandidates(file, buffer);
            buffer.clear();
        }
    }

    static void finishCandidates(FILE *file, std::vector<CandidateRecord> &buffer)
    {
        writeCandidates(file, buffer);
        buffer.clear();
        finishFile(file);
    }

    template <typename T> void scanFirstUnknown()
    {
        auto regions = normalizeScanRegions(memory_.GetScanRegions(true));
        auto snapshot = createCandidateFile();
        std::vector<SnapshotSpan> spans;
        std::uint64_t fileOffset = 0;
        size_t count = 0;
        ValueWindow<T> window;

        scanMemoryRegions(regions,
                          [&](uintptr_t address, const uint8_t *buffer, size_t size)
                          {
                              if (fwrite(buffer, 1, size, snapshot.get()) != size) throw std::runtime_error("写入 Unknown 快照失败");
                              if (!spans.empty() && spans.back().address + spans.back().size == address && spans.back().fileOffset + spans.back().size == fileOffset)
                                  spans.back().size += size;
                              else
                                  spans.push_back({address, fileOffset, size});
                              fileOffset += size;
                              window.process(address, buffer, size, alignment_, [&](uintptr_t, T value) { if (MemUtils::IsValidFloat(value)) ++count; });
                              liveCount_ = count;
                          });

        finishFile(snapshot.get());
        std::unique_lock lock(mutex_);
        candidates_.reset();
        snapshot_ = std::move(snapshot);
        snapshotSpans_ = std::move(spans);
        candidateCount_ = 0;
        snapshotCount_ = count;
        liveCount_ = count;
        scanMode_ = Types::FuzzyMode::Unknown;
    }

    template <typename T> void scanFirst(T target, Types::FuzzyMode mode, T rangeMax, MemUtils::FloatScanRange floatRange)
    {
        auto regions = normalizeScanRegions(memory_.GetScanRegions(true));
        auto output = createCandidateFile();
        std::vector<CandidateRecord> outputBuffer;
        outputBuffer.reserve(4096);
        size_t count = 0;
        ValueWindow<T> window;

        scanMemoryRegions(regions,
                          [&](uintptr_t address, const uint8_t *buffer, size_t size)
                          {
                              window.process(address, buffer, size, alignment_,
                                             [&](uintptr_t valueAddress, T value)
                                             {
                                                 if (!MemUtils::Compare(value, target, mode, T{}, rangeMax, floatRange)) return;
                                                 appendCandidate(output.get(), outputBuffer, {valueAddress, storeValue(value)});
                                                 liveCount_ = ++count;
                                             });
                          });

        finishCandidates(output.get(), outputBuffer);
        publishCandidates(std::move(output), count, mode);
    }

    template <typename T> void scanSnapshot(T target, Types::FuzzyMode mode, T rangeMax, MemUtils::FloatScanRange floatRange)
    {
        auto output = createCandidateFile();
        std::vector<CandidateRecord> outputBuffer;
        outputBuffer.reserve(4096);
        std::vector<uint8_t> oldValues(Config::Constants::BATCH_SIZE + sizeof(T) - 1);
        std::vector<uint8_t> currentValues(oldValues.size());
        const size_t totalBytes = std::accumulate(snapshotSpans_.begin(), snapshotSpans_.end(), size_t{}, [](size_t total, const SnapshotSpan &span) { return total + span.size; });
        size_t completedBytes = 0;
        size_t count = 0;

        auto compareBlock = [&](uintptr_t address, const uint8_t *oldBuffer, const uint8_t *currentBuffer, size_t size, size_t advance)
        {
            for (size_t offset = (alignment_ - address % alignment_) % alignment_; offset < advance && offset + sizeof(T) <= size; offset += alignment_)
            {
                T oldValue{};
                T currentValue{};
                std::memcpy(&oldValue, oldBuffer + offset, sizeof(oldValue));
                std::memcpy(&currentValue, currentBuffer + offset, sizeof(currentValue));
                if (!MemUtils::Compare(currentValue, target, mode, oldValue, rangeMax, floatRange)) continue;
                appendCandidate(output.get(), outputBuffer, {address + offset, storeValue(currentValue)});
                liveCount_ = ++count;
            }
        };

        liveCount_ = 0;
        for (const auto &span : snapshotSpans_)
        {
            for (size_t offset = 0; offset < span.size && Config::g_Running;)
            {
                const size_t advance = std::min(span.size - offset, Config::Constants::BATCH_SIZE);
                const size_t request = std::min(span.size - offset, oldValues.size());
                if (!readExactAt(snapshot_.get(), oldValues.data(), request, span.fileOffset + offset)) throw std::runtime_error("读取 Unknown 快照失败");
                const uintptr_t address = span.address + offset;
                if (memory_.Read(address, currentValues.data(), request) == static_cast<int>(request))
                {
                    compareBlock(address, oldValues.data(), currentValues.data(), request, advance);
                }
                else
                {
                    for (size_t pageOffset = 0; pageOffset < advance;)
                    {
                        const size_t inPage = static_cast<size_t>((address + pageOffset) % Config::Constants::SCAN_BUFFER);
                        const size_t chunk = std::min(advance - pageOffset, Config::Constants::SCAN_BUFFER - inPage);
                        const size_t withOverlap = std::min(request - pageOffset, chunk + sizeof(T) - 1);
                        if (scanReadExact(address + pageOffset, currentValues.data() + pageOffset, withOverlap))
                            compareBlock(address + pageOffset, oldValues.data() + pageOffset, currentValues.data() + pageOffset, withOverlap, chunk);
                        else if (withOverlap != chunk && scanReadExact(address + pageOffset, currentValues.data() + pageOffset, chunk))
                            compareBlock(address + pageOffset, oldValues.data() + pageOffset, currentValues.data() + pageOffset, chunk, chunk);
                        pageOffset += chunk;
                    }
                }
                offset += advance;
                completedBytes += advance;
                updateByteProgress(completedBytes, totalBytes);
            }
        }

        finishCandidates(output.get(), outputBuffer);
        publishCandidates(std::move(output), count, mode);
    }

    template <typename T> void scanCandidates(T target, Types::FuzzyMode mode, T rangeMax, MemUtils::FloatScanRange floatRange)
    {
        auto output = createCandidateFile();
        std::vector<CandidateRecord> outputBuffer;
        outputBuffer.reserve(4096);
        std::vector<CandidateRecord> pageCandidates;
        std::array<uint8_t, Config::Constants::SCAN_BUFFER> page{};
        size_t processed = 0;
        size_t count = 0;

        auto processPage = [&](uintptr_t pageBase)
        {
            if (pageCandidates.empty()) return;
            const bool pageReadable = scanReadExact(pageBase, page.data(), page.size());
            for (const auto &record : pageCandidates)
            {
                T value{};
                const size_t offset = static_cast<size_t>(record.address - pageBase);
                const bool readable = offset + sizeof(T) <= page.size() && pageReadable ? (std::memcpy(&value, page.data() + offset, sizeof(value)), true) : scanReadExact(record.address, &value, sizeof(value));
                if (readable && MemUtils::Compare(value, target, mode, loadValue<T>(record.value), rangeMax, floatRange))
                {
                    appendCandidate(output.get(), outputBuffer, {record.address, storeValue(value)});
                    liveCount_ = ++count;
                }
                ++processed;
            }
            pageCandidates.clear();
            updateByteProgress(processed, candidateCount_);
        };

        liveCount_ = 0;
        rewind(candidates_.get());
        CandidateRecord record{};
        uintptr_t pageBase = 0;
        while (Config::g_Running && fread(&record, sizeof(record), 1, candidates_.get()) == 1)
        {
            const uintptr_t recordPage = record.address & ~static_cast<uintptr_t>(Config::Constants::SCAN_BUFFER - 1);
            if (!pageCandidates.empty() && recordPage != pageBase) processPage(pageBase);
            if (pageCandidates.empty()) pageBase = recordPage;
            pageCandidates.push_back(record);
        }
        if (ferror(candidates_.get()) != 0) throw std::runtime_error("读取扫描候选临时文件失败");
        processPage(pageBase);

        finishCandidates(output.get(), outputBuffer);
        publishCandidates(std::move(output), count, mode);
    }

    void scanFirstString(const std::string &needle)
    {
        if (needle.empty()) return;
        auto regions = normalizeScanRegions(memory_.GetScanRegions(true));
        auto output = createCandidateFile();
        std::vector<CandidateRecord> outputBuffer;
        outputBuffer.reserve(4096);
        std::vector<uint8_t> carry;
        std::vector<uint8_t> combined;
        uintptr_t previousEnd = 0;
        size_t count = 0;

        scanMemoryRegions(regions,
                          [&](uintptr_t address, const uint8_t *buffer, size_t size)
                          {
                              if (address != previousEnd) carry.clear();
                              combined.clear();
                              combined.reserve(carry.size() + size);
                              combined.insert(combined.end(), carry.begin(), carry.end());
                              combined.insert(combined.end(), buffer, buffer + size);
                              const uintptr_t base = address - carry.size();
                              for (size_t offset = 0; offset + needle.size() <= combined.size(); ++offset)
                              {
                                  if (std::memcmp(combined.data() + offset, needle.data(), needle.size()) != 0) continue;
                                  appendCandidate(output.get(), outputBuffer, {base + offset, 0});
                                  liveCount_ = ++count;
                              }
                              const size_t carrySize = std::min(needle.size() - 1, combined.size());
                              carry.assign(combined.end() - carrySize, combined.end());
                              previousEnd = address + size;
                          });

        finishCandidates(output.get(), outputBuffer);
        publishCandidates(std::move(output), count, Types::FuzzyMode::String);
    }

    size_t storedCount() const noexcept
    {
        return candidateCount_ + snapshotCount_;
    }

    template <typename T> void appendSnapshotPage(size_t start, size_t count, Results &results) const
    {
        std::vector<uint8_t> buffer(Config::Constants::BATCH_SIZE + sizeof(T) - 1);
        size_t skipped = 0;
        for (const auto &span : snapshotSpans_)
        {
            for (size_t spanOffset = 0; spanOffset < span.size && results.size() < count;)
            {
                const size_t advance = std::min(span.size - spanOffset, Config::Constants::BATCH_SIZE);
                const size_t request = std::min(span.size - spanOffset, buffer.size());
                if (!readExactAt(snapshot_.get(), buffer.data(), request, span.fileOffset + spanOffset)) return;
                const uintptr_t address = span.address + spanOffset;
                for (size_t offset = (alignment_ - address % alignment_) % alignment_; offset < advance && offset + sizeof(T) <= request && results.size() < count; offset += alignment_)
                {
                    if constexpr (std::is_floating_point_v<T>)
                    {
                        T value{};
                        std::memcpy(&value, buffer.data() + offset, sizeof(value));
                        if (!MemUtils::IsValidFloat(value)) continue;
                    }
                    if (skipped++ < start) continue;
                    results.push_back(span.address + spanOffset + offset);
                }
                spanOffset += advance;
            }
            if (results.size() >= count) break;
        }
    }

    void appendResultPage(size_t start, size_t count, Results &results) const
    {
        if (count == 0 || start >= storedCount()) return;
        results.reserve(count);
        if (snapshot_ && dataType_)
        {
            MemUtils::DispatchType(*dataType_, [&]<typename T> { appendSnapshotPage<T>(start, count, results); });
            return;
        }

        const size_t end = std::min(candidateCount_, start + count);
        std::vector<CandidateRecord> records(end - start);
        if (records.empty() || !readExactAt(candidates_.get(), records.data(), records.size() * sizeof(CandidateRecord), start * sizeof(CandidateRecord))) return;
        for (const auto &record : records)
        {
            results.push_back(record.address);
        }
    }

    void scanNextString(const std::string &needle)
    {
        auto output = createCandidateFile();
        std::vector<CandidateRecord> outputBuffer;
        outputBuffer.reserve(4096);
        std::vector<CandidateRecord> pageCandidates;
        std::vector<uint8_t> window(Config::Constants::SCAN_BUFFER + needle.size() - 1);
        std::vector<uint8_t> fallback;
        size_t processed = 0;
        size_t count = 0;

        auto processPage = [&](uintptr_t pageBase)
        {
            if (pageCandidates.empty()) return;
            const bool windowReadable = scanReadExact(pageBase, window.data(), window.size());
            for (const auto &record : pageCandidates)
            {
                const size_t offset = static_cast<size_t>(record.address - pageBase);
                bool matches = offset + needle.size() <= window.size() && windowReadable && std::memcmp(window.data() + offset, needle.data(), needle.size()) == 0;
                if (!windowReadable)
                {
                    fallback.resize(needle.size());
                    matches = scanReadExact(record.address, fallback.data(), fallback.size()) && std::memcmp(fallback.data(), needle.data(), needle.size()) == 0;
                }
                if (matches)
                {
                    appendCandidate(output.get(), outputBuffer, record);
                    liveCount_ = ++count;
                }
                ++processed;
            }
            pageCandidates.clear();
            updateByteProgress(processed, candidateCount_);
        };

        liveCount_ = 0;
        rewind(candidates_.get());
        CandidateRecord record{};
        uintptr_t pageBase = 0;
        while (Config::g_Running && fread(&record, sizeof(record), 1, candidates_.get()) == 1)
        {
            const uintptr_t recordPage = record.address & ~static_cast<uintptr_t>(Config::Constants::SCAN_BUFFER - 1);
            if (!pageCandidates.empty() && recordPage != pageBase) processPage(pageBase);
            if (pageCandidates.empty()) pageBase = recordPage;
            pageCandidates.push_back(record);
        }
        if (ferror(candidates_.get()) != 0) throw std::runtime_error("读取字符串候选临时文件失败");
        processPage(pageBase);

        finishCandidates(output.get(), outputBuffer);
        publishCandidates(std::move(output), count, Types::FuzzyMode::String);
    }

    template <typename Scan> void run(Scan &&scan, std::string_view errorPrefix)
    {
        std::lock_guard operationLock(operationMutex_);
        ScanRunGuard guard{scanning_, progress_};
        progress_ = 0.0f;
        try
        {
            scan();
        }
        catch (const std::exception &error)
        {
            liveCount_ = storedCount();
            LS_LOGE_TAG_FMT("MemoryScan", "{}失败: {}", errorPrefix, error.what());
        }
        catch (...)
        {
            liveCount_ = storedCount();
            LS_LOGE_TAG_FMT("MemoryScan", "{}失败: 未知异常", errorPrefix);
        }
    }

public:
    struct State
    {
        bool scanning;
        float progress;
        size_t count;
        std::optional<Types::DataType> dataType;
        Types::FuzzyMode mode;
        bool stringScan;
        size_t alignment;
    };

    struct PageState
    {
        State state;
        Results results;
    };

    explicit MemScanner(MemoryReader &memory) : memory_(memory) {}
    ~MemScanner() = default;
    MemScanner(const MemScanner &) = delete;
    MemScanner &operator=(const MemScanner &) = delete;

    // 返回扫描线程当前是否在运行。
    bool isScanning() const noexcept
    {
        return scanning_;
    }
    // 返回当前扫描进度百分比(0~1)。
    float progress() const noexcept
    {
        return progress_;
    }

    State state() const
    {
        std::shared_lock lock(mutex_);
        return {scanning_.load(), progress_.load(), liveCount_.load(), dataType_, scanMode_, stringScan_, alignment_};
    }

    PageState pageState(size_t start, size_t cnt) const
    {
        if (scanning_)
        {
            std::shared_lock lock(mutex_);
            return {{true, progress_.load(), liveCount_.load(), dataType_, scanMode_, stringScan_, alignment_}, {}};
        }

        std::lock_guard operationLock(operationMutex_);
        std::shared_lock lock(mutex_);

        PageState snapshot{{scanning_.load(), progress_.load(), liveCount_.load(), dataType_, scanMode_, stringScan_, alignment_}, {}};
        if (snapshot.state.scanning || snapshot.state.count == 0) return snapshot;
        appendResultPage(start, cnt, snapshot.results);
        return snapshot;
    }

    std::optional<Types::DataType> dataType() const
    {
        std::shared_lock lock(mutex_);
        return dataType_;
    }

    Types::FuzzyMode scanMode() const
    {
        std::shared_lock lock(mutex_);
        return scanMode_;
    }

    bool isStringScan() const
    {
        std::shared_lock lock(mutex_);
        return stringScan_;
    }

    // 返回当前结果数量。
    size_t count() const
    {
        return liveCount_.load();
    }

    // 结果分页获取
    Results getPage(size_t start, size_t cnt) const
    {
        if (scanning_) return {};
        std::lock_guard operationLock(operationMutex_);
        if (scanning_) return {};
        std::shared_lock lock(mutex_);
        Results results;
        appendResultPage(start, cnt, results);
        return results;
    }

    // 清除
    void clear()
    {
        if (scanning_) return;
        std::lock_guard operationLock(operationMutex_);
        if (scanning_) return;
        std::unique_lock lock(mutex_);
        clearStorage();
        dataType_.reset();
        scanMode_ = Types::FuzzyMode::Unknown;
        stringScan_ = false;
        alignment_ = 1;
    }

    template <typename T> bool startAsync(pid_t pid, T target, Types::DataType dataType, Types::FuzzyMode mode, bool isFirst, T rangeMax = T{}, std::string_view valueText = {}, MemUtils::FloatRounding rounding = MemUtils::FloatRounding::Rounded, size_t alignment = 0)
    {
        MemUtils::FloatScanRange floatRange;
        if constexpr (std::is_floating_point_v<T>)
        {
            if (mode == Types::FuzzyMode::Equal && !valueText.empty())
            {
                const auto parsedRange = MemUtils::ParseFloatScanRange<T>(valueText, rounding);
                if (!parsedRange) return false;
                floatRange = *parsedRange;
            }
        }
        std::lock_guard targetLock(Config::TargetMutex);
        std::unique_lock lock(mutex_);
        if (scanning_.exchange(true)) return false;

        Types::DataType actualType;
        if constexpr (std::is_same_v<T, int8_t>) actualType = Types::DataType::I8;
        else if constexpr (std::is_same_v<T, int16_t>) actualType = Types::DataType::I16;
        else if constexpr (std::is_same_v<T, int32_t>) actualType = Types::DataType::I32;
        else if constexpr (std::is_same_v<T, int64_t>) actualType = Types::DataType::I64;
        else if constexpr (std::is_same_v<T, float>) actualType = Types::DataType::Float;
        else actualType = Types::DataType::Double;

        bool accepted = pid > 0 && pid == memory_.GetGlobalPid() && dataType == actualType && mode != Types::FuzzyMode::String && (mode != Types::FuzzyMode::Pointer || dataType == Types::DataType::I64);
        if (isFirst)
        {
            accepted = accepted && (alignment == 0 || alignment == 1 || alignment == 2 || alignment == 4 || alignment == 8) && !(mode >= Types::FuzzyMode::Increased && mode <= Types::FuzzyMode::Unchanged);
        }
        else
        {
            accepted = accepted && dataType_.has_value() && *dataType_ == dataType && !stringScan_ && mode != Types::FuzzyMode::Unknown && (alignment == 0 || alignment == alignment_) && (candidates_ || snapshot_);
        }
        if (!accepted)
        {
            scanning_ = false;
            return false;
        }

        if (isFirst)
        {
            clearStorage();
            alignment_ = alignment == 0 ? std::min(sizeof(T), size_t{4}) : alignment;
        }
        dataType_ = dataType;
        scanMode_ = mode;
        stringScan_ = false;
        liveCount_ = 0;

        progress_ = 0.0f;
        lock.unlock();
        try
        {
            Config::IoThreadPool().detach_task([this, target, mode, isFirst, rangeMax, floatRange]
            {
                run([&]
                {
                    if (isFirst && mode == Types::FuzzyMode::Unknown) scanFirstUnknown<T>();
                    else if (isFirst) scanFirst<T>(target, mode, rangeMax, floatRange);
                    else if (snapshot_) scanSnapshot(target, mode, rangeMax, floatRange);
                    else scanCandidates(target, mode, rangeMax, floatRange);
                }, "扫描");
            });
        }
        catch (...)
        {
            scanning_ = false;
            return false;
        }
        return true;
    }

    bool startStringAsync(pid_t pid, std::string needle, bool isFirst)
    {
        std::lock_guard targetLock(Config::TargetMutex);
        if (pid <= 0 || pid != memory_.GetGlobalPid() || needle.empty() || needle.size() > Config::Constants::SCAN_BUFFER) return false;
        std::unique_lock lock(mutex_);
        if (scanning_.exchange(true)) return false;

        if (!isFirst && (!stringScan_ || !candidates_))
        {
            scanning_ = false;
            return false;
        }

        if (isFirst)
        {
            clearStorage();
        }
        dataType_.reset();
        scanMode_ = Types::FuzzyMode::String;
        stringScan_ = true;
        alignment_ = 1;
        liveCount_ = 0;

        progress_ = 0.0f;
        lock.unlock();
        try
        {
            Config::IoThreadPool().detach_task([this, needle = std::move(needle), isFirst]
            {
                run([&] { isFirst ? scanFirstString(needle) : scanNextString(needle); }, "字符串扫描");
            });
        }
        catch (...)
        {
            scanning_ = false;
            return false;
        }
        return true;
    }
};
