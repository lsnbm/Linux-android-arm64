#pragma once
#include <memory>
#include "../base/memory_utils.h"
#include <atomic>
#include <chrono>
#include <future>
#include <list>
#include <mutex>
#include <span>
#include <thread>

class SavedAddressManager
{
public:
    struct Item
    {
        uintptr_t address;
        Types::DataType type;
        Types::SavedValueKind kind;
        size_t textLength;
        std::string note;
    };

    struct ItemState
    {
        Item item;
        std::string value;
        bool locked;
        std::string lockValue;
    };

    using Items = std::vector<Item>;
    using ItemStates = std::vector<ItemState>;

private:
    struct LockItem
    {
        uintptr_t addr;
        Types::DataType type;
        Types::SavedValueKind kind;
        std::string value;
    };

    Items items_;
    std::list<LockItem> locks_;
    mutable std::mutex mutex_;
    std::future<void> writeTask_;
    std::atomic<bool> writeStop_{false};

    // 按地址查找锁定项。
    auto find(uintptr_t addr)
    {
        return std::ranges::find_if(locks_, [addr](auto &i) { return i.addr == addr; });
    }

    auto findSaved(uintptr_t addr)
    {
        return std::ranges::find_if(items_, [addr](const auto &item) { return item.address == addr; });
    }

    static std::string readValue(const Item &item)
    {
        if (item.kind == Types::SavedValueKind::Pointer) return MemUtils::ReadAsPointerString(item.address);
        if (item.kind == Types::SavedValueKind::Text) return MemUtils::ReadAsText(item.address, item.textLength);
        return MemUtils::ReadAsString(item.address, item.type);
    }

    static int writeValue(uintptr_t addr, Types::DataType type, Types::SavedValueKind kind, std::string_view value)
    {
        int status = 0;
        if (kind == Types::SavedValueKind::Pointer) status = MemUtils::WritePointerFromString(addr, value);
        else if (kind == Types::SavedValueKind::Text) status = MemUtils::WriteText(addr, value);
        else status = MemUtils::WriteFromString(addr, type, value);
        return status;
    }

public:
    SavedAddressManager()
    {
        writeTask_ = std::async(std::launch::async, [this]
        {
            while (!writeStop_.load(std::memory_order_acquire))
            {
                {
                    std::lock_guard lock(mutex_);
                    // clear/remove 返回后，不应再按旧地址提交后台写入。
                    for (auto item = locks_.begin(); item != locks_.end();)
                    {
                        int status = 0;
                        try { status = writeValue(item->addr, item->type, item->kind, item->value); }
                        catch (const std::bad_alloc &) { status = -ENOMEM; }
                        catch (...) { status = -EIO; }
                        if (status != 0)
                        {
                            LS_LOGE_TAG("Saved", "锁定写入失败: addr=%llx status=%d", static_cast<unsigned long long>(item->addr), status);
                            item = locks_.erase(item);
                        }
                        else ++item;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        });
    }

    ~SavedAddressManager()
    {
        writeStop_.store(true, std::memory_order_release);
        writeTask_.wait();
    }

    bool contains(uintptr_t addr) const
    {
        std::lock_guard lock(mutex_);
        return std::ranges::any_of(items_, [addr](const auto &item) { return item.address == addr; });
    }

    int add(uintptr_t addr, Types::DataType type, Types::SavedValueKind kind = Types::SavedValueKind::Numeric, size_t textLength = 64)
    {
        int status = 0;
        std::lock_guard targetLock(Config::TargetMutex);
        std::lock_guard lock(mutex_);
        if (!addr)
        {
            status = -EINVAL;
            return status;
        }
        if (findSaved(addr) != items_.end())
        {
            status = -EEXIST;
            return status;
        }
        items_.push_back({addr, type, kind, std::clamp<size_t>(textLength, 1, 256), {}});
        return status;
    }

    int setNote(uintptr_t addr, std::string note)
    {
        int status = 0;
        std::lock_guard lock(mutex_);
        const auto saved = findSaved(addr);
        if (saved == items_.end())
        {
            status = -ENOENT;
            return status;
        }
        if (note.size() > 256) note.resize(256);
        saved->note = std::move(note);
        return status;
    }

    int remove(uintptr_t addr)
    {
        int status = 0;
        std::lock_guard lock(mutex_);
        const auto item = findSaved(addr);
        if (item == items_.end())
        {
            status = -ENOENT;
            return status;
        }
        items_.erase(item);
        if (auto locked = find(addr); locked != locks_.end()) locks_.erase(locked);
        return status;
    }

    int write(uintptr_t addr, std::string_view value)
    {
        int status = 0;
        std::lock_guard targetLock(Config::TargetMutex);
        std::lock_guard lock(mutex_);
        const auto saved = findSaved(addr);
        if (saved == items_.end())
        {
            status = -ENOENT;
            return status;
        }
        status = writeValue(saved->address, saved->type, saved->kind, value);
        if (status != 0) return status;
        if (auto locked = find(addr); locked != locks_.end()) locked->value = std::string(value);
        return status;
    }

    int setLocked(uintptr_t addr, bool locked, std::string_view value = {})
    {
        int status = 0;
        std::lock_guard targetLock(Config::TargetMutex);
        std::lock_guard lock(mutex_);
        const auto saved = findSaved(addr);
        if (saved == items_.end())
        {
            status = -ENOENT;
            return status;
        }
        if (!locked)
        {
            if (auto entry = find(addr); entry != locks_.end()) locks_.erase(entry);
        }
        else
        {
            std::string lockValue = value.empty() ? readValue(*saved) : std::string(value);
            if (lockValue.empty() || lockValue == "??")
            {
                status = -EIO;
                return status;
            }
            status = writeValue(addr, saved->type, saved->kind, lockValue);
            if (status != 0) return status;
            if (auto entry = find(addr); entry != locks_.end())
            {
                entry->type = saved->type;
                entry->kind = saved->kind;
                entry->value = std::move(lockValue);
            }
            else locks_.push_back({addr, saved->type, saved->kind, std::move(lockValue)});
        }
        return status;
    }

    int togglePage(std::span<const ItemState> states, bool lockItems)
    {
        int status = 0;
        std::lock_guard targetLock(Config::TargetMutex);
        std::lock_guard lk(mutex_);
        std::list<LockItem> pending;
        for (const auto &state : states)
        {
            if (lockItems)
            {
                const auto saved = findSaved(state.item.address);
                if (saved == items_.end()) return status = -ENOENT;
                if (find(state.item.address) != locks_.end()) continue;
                std::string value = readValue(*saved);
                if (value.empty() || value == "??") return status = -EIO;
                status = writeValue(saved->address, saved->type, saved->kind, value);
                if (status != 0) return status;
                pending.push_back({saved->address, saved->type, saved->kind, std::move(value)});
            }
            else
            {
                std::erase_if(locks_, [&](const auto &locked) { return locked.addr == state.item.address; });
            }
        }
        locks_.splice(locks_.end(), pending);
        return status;
    }

    int applyOffset(uintptr_t offset, bool negative)
    {
        int status = 0;
        std::lock_guard targetLock(Config::TargetMutex);
        std::lock_guard lock(mutex_);
        Items shifted;
        shifted.reserve(items_.size());
        std::list<LockItem> shiftedLocks;
        for (const auto &item : items_)
        {
            if (negative ? item.address < offset : item.address > std::numeric_limits<uintptr_t>::max() - offset) continue;
            const uintptr_t address = negative ? item.address - offset : item.address + offset;
            Item shiftedItem = item;
            shiftedItem.address = address;
            shifted.push_back(shiftedItem);
            if (find(item.address) != locks_.end())
            {
                std::string value = readValue(shiftedItem);
                if (value.empty() || value == "??") return status = -EIO;
                shiftedLocks.push_back({address, item.type, item.kind, std::move(value)});
            }
        }
        items_.swap(shifted);
        locks_.swap(shiftedLocks);
        return status;
    }

    ItemStates snapshotStates() const
    {
        std::lock_guard targetLock(Config::TargetMutex);
        ItemStates states;
        {
            std::lock_guard lock(mutex_);
            states.reserve(items_.size());
            for (const auto &item : items_)
            {
                const auto locked = std::ranges::find_if(locks_, [&](const auto &entry) { return entry.addr == item.address; });
                states.push_back({item, {}, locked != locks_.end(), locked != locks_.end() ? locked->value : std::string{}});
            }
        }
        for (auto &state : states) state.value = readValue(state.item);
        return states;
    }

    // 清空当前模块维护的全部数据。
    void clear()
    {
        std::lock_guard lk(mutex_);
        items_.clear();
        locks_.clear();
    }
};

inline std::unique_ptr<SavedAddressManager> gSavedAddresses = nullptr;
