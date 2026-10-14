#pragma once
#include "../base/memory_utils.h"
#include <atomic>
#include <list>
#include <mutex>

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

    static bool writeValue(uintptr_t addr, Types::DataType type, Types::SavedValueKind kind, std::string_view value)
    {
        if (kind == Types::SavedValueKind::Pointer) return MemUtils::WritePointerFromString(addr, value);
        if (kind == Types::SavedValueKind::Text) return MemUtils::WriteText(addr, value);
        return MemUtils::WriteFromString(addr, type, value);
    }

    // 后台循环写入被锁定的内存项。
    void writeLoop()
    {
        while (!writeStop_.load(std::memory_order_acquire) && Config::g_Running)
        {
            std::vector<LockItem> snapshot;
            {
                std::lock_guard lock(mutex_);
                snapshot.assign(locks_.begin(), locks_.end());
            }
            for (const auto &item : snapshot) writeValue(item.addr, item.type, item.kind, item.value);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

public:
    SavedAddressManager()
    {
        writeTask_ = Config::IoThreadPool().submit_task([this] { writeLoop(); });
    }

    ~SavedAddressManager()
    {
        writeStop_.store(true, std::memory_order_release);
        if (writeTask_.valid()) writeTask_.wait();
    }

    bool contains(uintptr_t addr) const
    {
        std::lock_guard lock(mutex_);
        return std::ranges::any_of(items_, [addr](const auto &item) { return item.address == addr; });
    }

    bool add(uintptr_t addr, Types::DataType type, Types::SavedValueKind kind = Types::SavedValueKind::Numeric, size_t textLength = 64)
    {
        std::lock_guard lock(mutex_);
        if (!addr || findSaved(addr) != items_.end()) return false;
        items_.push_back({addr, type, kind, std::clamp<size_t>(textLength, 1, 256), {}});
        return true;
    }

    bool setNote(uintptr_t addr, std::string note)
    {
        std::lock_guard lock(mutex_);
        const auto saved = findSaved(addr);
        if (saved == items_.end()) return false;
        if (note.size() > 256) note.resize(256);
        saved->note = std::move(note);
        return true;
    }

    bool remove(uintptr_t addr)
    {
        std::lock_guard lock(mutex_);
        const auto item = findSaved(addr);
        if (item == items_.end()) return false;
        items_.erase(item);
        if (auto locked = find(addr); locked != locks_.end()) locks_.erase(locked);
        return true;
    }

    bool write(uintptr_t addr, std::string_view value)
    {
        Item item{};
        {
            std::lock_guard lock(mutex_);
            const auto saved = findSaved(addr);
            if (saved == items_.end()) return false;
            item = *saved;
        }
        if (!writeValue(item.address, item.type, item.kind, value)) return false;
        std::lock_guard lock(mutex_);
        if (auto locked = find(addr); locked != locks_.end()) locked->value = std::string(value);
        return true;
    }

    bool toggle(uintptr_t addr)
    {
        std::lock_guard lock(mutex_);
        if (auto locked = find(addr); locked != locks_.end())
        {
            locks_.erase(locked);
            return false;
        }
        const auto saved = findSaved(addr);
        if (saved == items_.end()) return false;
        locks_.push_back({addr, saved->type, saved->kind, readValue(*saved)});
        return true;
    }

    bool setLocked(uintptr_t addr, bool locked, std::string_view value = {})
    {
        Item item{};
        {
            std::lock_guard lock(mutex_);
            const auto saved = findSaved(addr);
            if (saved == items_.end()) return false;
            if (!locked)
            {
                std::erase_if(locks_, [addr](const auto &entry) { return entry.addr == addr; });
                return true;
            }
            item = *saved;
        }

        std::string lockValue = value.empty() ? readValue(item) : std::string(value);
        if (lockValue.empty() || lockValue == "??" || !writeValue(item.address, item.type, item.kind, lockValue)) return false;

        std::lock_guard lock(mutex_);
        if (findSaved(addr) == items_.end()) return false;
        if (auto entry = find(addr); entry != locks_.end())
        {
            entry->type = item.type;
            entry->kind = item.kind;
            entry->value = std::move(lockValue);
        }
        else
        {
            locks_.push_back({addr, item.type, item.kind, std::move(lockValue)});
        }
        return true;
    }

    void togglePage(std::span<const ItemState> states, bool lockItems)
    {
        std::lock_guard lk(mutex_);
        for (const auto &state : states)
        {
            const auto &item = state.item;
            if (lockItems)
            {
                const auto saved = findSaved(item.address);
                if (saved != items_.end() && find(item.address) == locks_.end()) locks_.push_back({saved->address, saved->type, saved->kind, readValue(*saved)});
            }
            else
            {
                std::erase_if(locks_, [&](const auto &locked) { return locked.addr == item.address; });
            }
        }
    }

    void applyOffset(uintptr_t offset, bool negative)
    {
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
            if (find(item.address) != locks_.end()) shiftedLocks.push_back({address, item.type, item.kind, readValue(shiftedItem)});
        }
        items_.swap(shifted);
        locks_.swap(shiftedLocks);
    }

    ItemStates snapshotStates() const
    {
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
