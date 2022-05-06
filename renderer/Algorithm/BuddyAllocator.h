#pragma  once

#include <vector>
#include <list>
#include <type_traits>
#include <optional>
#include <functional>
#include "Common/Alignment.h"
#include "Common/BitUtil.h"

namespace glacier {

class BuddyAllocatorBase;

struct BuddyAllocBlock {
    uint32_t order;
    size_t page_offset;
    size_t align_offset;
    size_t alloc_size;
    size_t size;
    BuddyAllocatorBase* allocator = nullptr;
};

class BuddyAllocatorBase {
public:
    virtual ~BuddyAllocatorBase() = default;
    virtual void Deallocate(const BuddyAllocBlock& block) = 0;
};

template<typename T, uint32_t MaxOrder, uint32_t MinOrder = 4>
class BuddyAllocator : public BuddyAllocatorBase {
public:
    BuddyAllocator(T&& res) : underlying_resource_(std::move(res)) {
        static_assert(MaxOrder > MinOrder);
        free_list_[OrderToIndex(MaxOrder)].emplace_back(0);
    }

    ~BuddyAllocator() = default;

    constexpr static uint32_t kHeapSize = (1 << MaxOrder);
    constexpr static uint32_t kMinPageSize = (1 << MinOrder);

    constexpr uint32_t PageSize(uint32_t order) const noexcept { return 1 << order; }
    constexpr size_t BuddyOfPage(size_t page_offset, uint32_t order) const noexcept { return page_offset ^ PageSize(order); }

    constexpr uint32_t OrderToIndex(uint32_t order) const noexcept { return order - MinOrder; }
    constexpr uint32_t IndexToOrder(uint32_t index) const noexcept { return index + MinOrder; }

    constexpr size_t CalcAllocSize(size_t size, size_t alignment) const noexcept {
        if (alignment != 0 && (kMinPageSize % alignment) != 0) {
            return size + alignment;
        }
        return size;
    }

    size_t TotalAllocSize() const noexcept { return total_alloc_; }
    T& GetUnderlyingResource() { return underlying_resource_; }

    std::optional<BuddyAllocBlock> Allocate(size_t size, size_t alignment) {
        if (total_alloc_ == kHeapSize) return std::nullopt;

        auto alloc_size = CalcAllocSize(size, alignment);

        uint32_t order = MinOrder;
        while (PageSize(order) < alloc_size) order++;

        // level up until available list found
        uint32_t i = order;
        for (;; i++) {
            if (i > MaxOrder)
                return std::nullopt;
            
            if (!free_list_[OrderToIndex(i)].empty())
                break;
        }

        uint32_t index = OrderToIndex(i);
        auto page_offset = free_list_[index].back();
        free_list_[index].pop_back();

        auto align_offset = (size_t)AlignBoundary(page_offset, alignment);
        assert(align_offset - page_offset + size <= PageSize(order));

        // split until i == order
        while (i-- > order) {
            size_t buddy_offset = BuddyOfPage(page_offset, i);
            free_list_[OrderToIndex(i)].emplace_back(buddy_offset);
        }

        total_alloc_ += PageSize(order);

        return BuddyAllocBlock{ order, page_offset, align_offset, alloc_size, size, this };
    }

    void Deallocate(const BuddyAllocBlock& block) override {
        total_alloc_ -= PageSize(block.order);

        uint32_t i = block.order;
        size_t buddy_offset;
        size_t page_offset = block.page_offset;

        for (;; i++) {
            buddy_offset = BuddyOfPage(page_offset, i);
            auto& list = free_list_[OrderToIndex(i)];

            bool hit = false;
            for (auto it = list.begin(); it != list.end(); ++it) {
                // find buddy in free list
                if (*it == buddy_offset) {
                    hit = true;
                    // remove buddy out of list
                    list.erase(it);
                    break;
                }
            }

            if (!hit) {
                list.emplace_back(page_offset);
                return;
            }

            // found, merged block starts from the lower one
            page_offset = page_offset < buddy_offset ? page_offset : buddy_offset;
        }
    }

protected:
    T underlying_resource_ = {};
    std::vector<size_t> free_list_[MaxOrder - MinOrder + 1];
    size_t total_alloc_ = 0;
};

template<typename T, uint32_t MaxOrder, uint32_t MinOrder = 4>
class BuddyAllocatorPool {
public:
    //using AllocPage = typename BuddyAllocator<T, MaxOrder, MinOrder>::BuddyAllocPage;
    using BuddyResourceInitializeFunction = std::function<T()>;
    using AllocatorType = BuddyAllocator<T, MaxOrder, MinOrder>;

    constexpr static uint32_t kHeapSize = (1 << MaxOrder);
    constexpr static uint32_t kMinPageSize = (1 << MinOrder);

    BuddyAllocatorPool(BuddyResourceInitializeFunction&& func) : func_(std::move(func)) {

    }

    BuddyAllocBlock Allocate(size_t size, size_t alignment) {
        for (auto& allocator : pool_) {
            auto result = allocator.Allocate(size, alignment);
            if (result) return result.value();
        }

        auto res = func_();
        pool_.emplace_back(std::move(res));

        auto& allocator = pool_.back();
        auto result = allocator.Allocate(size, alignment);
        assert(result);
        return result.value();
    }

    void Deallocate(const BuddyAllocBlock& block) {
        block.allocator->Deallocate(block);
    }

protected:
    std::list<AllocatorType> pool_;
    BuddyResourceInitializeFunction func_;
};

}
