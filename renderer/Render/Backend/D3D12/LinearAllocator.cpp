#include "LinearAllocator.h"
#include <d3d12.h>
#include "GfxDriver.h"
#include "Common/Log.h"

namespace glacier {
namespace render {

LinearAllocPage::LinearAllocPage(size_t size, LinearAllocatorType type) :
    type_(type),
    size_(size),
    offset_(0)
{
    D3D12_HEAP_PROPERTIES HeapProps;
    HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    HeapProps.CreationNodeMask = 1;
    HeapProps.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC ResourceDesc;
    ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    ResourceDesc.Alignment = 0;
    ResourceDesc.Height = 1;
    ResourceDesc.DepthOrArraySize = 1;
    ResourceDesc.MipLevels = 1;
    ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    ResourceDesc.SampleDesc.Count = 1;
    ResourceDesc.SampleDesc.Quality = 0;
    ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ResourceDesc.Width = size_;

    D3D12_RESOURCE_STATES DefaultUsage;

    if (type_ == LinearAllocatorType::kDefault) {
        HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
        ResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        DefaultUsage = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }
    else {
        HeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        ResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        DefaultUsage = D3D12_RESOURCE_STATE_GENERIC_READ;
    }

    auto gfx = D3D12GfxDriver::Instance();
    gfx->GetDevice()->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE,
        &ResourceDesc, DefaultUsage, nullptr, IID_PPV_ARGS(&resource_));

    resource_->SetName(TEXT("LinearAllocator Page"));
    resource_->Map(0, nullptr, &mapped_address_);
    gpu_address_ = resource_->GetGPUVirtualAddress();
}

LinearAllocPage::~LinearAllocPage() {
    resource_->Unmap(0, nullptr);
}

LinearAllocator::LinearAllocator(size_t page_size, LinearAllocatorType type) :
    type_(type),
    page_size_(page_size)
{

}

void LinearAllocator::Clear() {
    page_ = nullptr;
    inflight_pages_.clear();
    page_pool_.clear();
    large_pages_.clear();

    while (!retired_pages_.empty()) {
        retired_pages_.pop();
    }

    while (!available_pages_.empty()) {
        available_pages_.pop();
    }

    while (!dying_pages_.empty()) {
        dying_pages_.pop();
    }
}

void LinearAllocator::BeginFrame(uint64_t completed_fence) {
    // a page that a frame retired is free once that frame completed, which <=
    // says as plainly as it can be said
    while (!retired_pages_.empty() && retired_pages_.front().first <= completed_fence) {
        available_pages_.push(retired_pages_.front().second);
        retired_pages_.pop();
    }

    // for large pages
    while (!dying_pages_.empty() && dying_pages_.front().first <= completed_fence) {
        dying_pages_.pop();
    }
}

void LinearAllocator::EndFrame(uint64_t frame_fence) {
    // the page being filled is done with as well: the frame is over, and its
    // contents belong to that frame from here on
    if (page_) {
        retired_pages_.emplace(std::pair{ frame_fence, page_ });
        page_ = nullptr;
    }

    for (auto page : inflight_pages_) {
        retired_pages_.emplace(std::pair{ frame_fence, page });
    }
    inflight_pages_.clear();

    for (auto& page : large_pages_) {
        dying_pages_.emplace(std::pair{ frame_fence, std::move(page) });
    }

    large_pages_.clear();
}

LinearAllocPage* LinearAllocator::AcquirePage() {
    LinearAllocPage* ptr = nullptr;
    if (!available_pages_.empty()) {
        ptr = available_pages_.front();
        available_pages_.pop();
        ptr->Reset();
    }
    else {
        auto page = std::make_unique<LinearAllocPage>(page_size_, type_);
        ptr = page.get();
        page_pool_.emplace_back(std::move(page));

        // a page that has to be created rather than recycled means the frames in
        // flight needed more than the pool held; the count should settle, and
        // seeing it climb means the pages are not coming back (see BeginFrame)
        LOG_LOG("linear allocator: created page #{} of {} bytes", page_pool_.size(), page_size_);
    }

    return ptr;
}

LinearAllocBlock LinearAllocator::Allocate(size_t size, size_t alignment) {
    size_t aligned_size = AlignUp(size, alignment);
    if (aligned_size > page_size_) {
        auto page = std::make_unique<LinearAllocPage>(aligned_size, type_);
        LinearAllocBlock block{ 0, aligned_size, page.get() };

        large_pages_.emplace_back(std::move(page));
        return block;
    }

    if (page_) {
        auto result = page_->Allocate(aligned_size, alignment);
        if (result) {
            return LinearAllocBlock{ result.value(), aligned_size, page_ };
        }
        inflight_pages_.push_back(page_);
        page_ = nullptr;
    }

    if (!page_) {
        page_ = AcquirePage();
    }

    auto result = page_->Allocate(aligned_size, alignment);
    assert(result);

    return { result.value(), aligned_size, page_ };
}

}
}
