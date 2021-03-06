#pragma once

#include <vector>
#include <queue>
#include <optional>
#include <memory>
#include <d3d12.h>
#include "Common/Alignment.h"
#include "Common/Uncopyable.h"

namespace glacier {
namespace render {

enum class LinearAllocatorType : uint8_t {
    kDefault,
    kUpload,
};

class LinearAllocPage : private Uncopyable {
public:
    LinearAllocPage(size_t size, LinearAllocatorType type = LinearAllocatorType::kUpload);
    ~LinearAllocPage();

    size_t size() const { return size_; }
    size_t offset() const { return offset_; }

    void* GetMappedAddress() const { return mapped_address_; }
    D3D12_GPU_VIRTUAL_ADDRESS GetGpuAddress() const { return gpu_address_; }

    void Reset() { offset_ = 0; }

    std::optional<size_t> Allocate(size_t aligned_size, size_t alignment) {
        auto aligned_offset = AlignUp(offset_, alignment);
        if (aligned_offset + aligned_size > size_) {
            return std::nullopt;
        }

        offset_ = aligned_offset + aligned_size;
        return { aligned_offset };
    }

private:
    ComPtr<ID3D12Resource> resource_;
    void* mapped_address_;
    D3D12_GPU_VIRTUAL_ADDRESS gpu_address_;

    LinearAllocatorType type_;
    size_t size_;
    size_t offset_;
};

struct LinearAllocBlock {
    size_t offset;
    size_t size;
    LinearAllocPage* page;

    void* GetMappedAddress() const { return (uint8_t*)page->GetMappedAddress() + offset; }
    D3D12_GPU_VIRTUAL_ADDRESS GetGpuAddress() const { return page->GetGpuAddress() + offset; }
};

class LinearAllocator : private Uncopyable {
public:
    constexpr static size_t kDefaultPageSize = 0x10000;    // 64K
    constexpr static size_t kUploadPageSize = 0x200000;   // 2MB

    LinearAllocator(size_t page_size, LinearAllocatorType type = LinearAllocatorType::kUpload);

    size_t page_size() const { return page_size_; }

    LinearAllocBlock Allocate(size_t size, size_t alignment = 256);
    void Cleanup(uint64_t fence_value);
    void Clear();

private:
    LinearAllocPage* AcquirePage();

    LinearAllocatorType type_;
    size_t page_size_;

    LinearAllocPage* page_ = nullptr;
    std::vector<LinearAllocPage*> inflight_pages_;
    std::queue<std::pair<uint64_t, LinearAllocPage*>> retired_pages_;

    std::queue<LinearAllocPage*> available_pages_;
    std::vector<std::unique_ptr<LinearAllocPage>> page_pool_;

    std::vector<std::unique_ptr<LinearAllocPage>> large_pages_;
    std::queue<std::pair<uint64_t, std::unique_ptr<LinearAllocPage>>> dying_pages_;
};

}
}
