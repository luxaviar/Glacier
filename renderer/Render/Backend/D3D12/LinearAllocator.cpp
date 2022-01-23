#include "LinearAllocator.h"
#include <d3d12.h>
#include "GfxDriver.h"

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

    ComPtr<ID3D12Resource> buffer;
    auto gfx = D3D12GfxDriver::Instance();
    gfx->GetDevice()->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE,
        &ResourceDesc, DefaultUsage, nullptr, IID_PPV_ARGS(&buffer));

    buffer->SetName(L"LinearAllocator Page");

    resource_ = D3D12Resource{ buffer, DefaultUsage };

    resource_.Map();
}

LinearAllocPage::~LinearAllocPage() {
    resource_.Unmap();
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

void LinearAllocator::Cleanup(uint64_t fence_value) {
    while (!retired_pages_.empty() && retired_pages_.front().first < fence_value) {
        available_pages_.push(retired_pages_.front().second);
        retired_pages_.pop();
    }

    if (page_) {
        retired_pages_.emplace(std::pair{ fence_value, page_ });
        page_ = nullptr;
    }

    for (auto page : inflight_pages_) {
        retired_pages_.emplace(std::pair{ fence_value, page });
    }
    inflight_pages_.clear();

    // for large pages
    while (!dying_pages_.empty() && dying_pages_.front().first < fence_value) {
        dying_pages_.pop();
    }

    for (auto& page : large_pages_) {
        dying_pages_.emplace(std::pair{ fence_value, std::move(page) });
    }
    large_pages_.clear();
}

LinearAllocPage* LinearAllocator::AcquirePage()
{
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
    }

    return ptr;
}

LinearAllocBlock LinearAllocator::Allocate(size_t size, size_t alignment) {
    size_t aligned_size = AlignUp(size, alignment);
    if (aligned_size > page_size_) {
        auto page = std::make_unique<LinearAllocPage>(aligned_size, type_);
        LinearAllocBlock block{ 0, aligned_size, page->GetUnderlyingResource() };

        large_pages_.emplace_back(std::move(page));
        return block;
    }

    if (page_) {
        auto result = page_->Allocate(aligned_size, alignment);
        if (result) {
            return LinearAllocBlock{ result.value(), aligned_size, page_->GetUnderlyingResource() };
        }
        inflight_pages_.push_back(page_);
        page_ = nullptr;
    }

    if (!page_) {
        page_ = AcquirePage();
    }

    auto result = page_->Allocate(aligned_size, alignment);
    assert(result);

    return { result.value(), aligned_size, page_->GetUnderlyingResource() };
}

}
}