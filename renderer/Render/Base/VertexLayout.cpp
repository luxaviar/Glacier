#include "vertexlayout.h"

namespace glacier {
namespace render {

size_t InputLayoutDesc::data_size() const
{
    return elements_.empty() ? 0u : elements_.back().offset() + elements_.back().data_size();
}

std::vector<D3D11_INPUT_ELEMENT_DESC> InputLayoutDesc::layout_desc() const
{
    std::vector<D3D11_INPUT_ELEMENT_DESC> desc;
    desc.reserve(elements_.size());
    for( const auto& e : elements_ )
    {
        desc.push_back( e.desc() );
    }
    return desc;
}

D3D11_INPUT_ELEMENT_DESC InputLayoutDesc::Element::desc() const noexcept {
    D3D11_INPUT_ELEMENT_DESC desc = kMetaDescArray[(int)type_].gfx_desc;
    desc.AlignedByteOffset = (UINT)offset_;	
    return desc;
}

size_t InputLayoutDesc::Element::data_size() const noexcept {
    return kMetaDescArray[(int)type_].size;
}

const char* InputLayoutDesc::Element::code() const noexcept {
    return kMetaDescArray[(int)type_].code;
}

const char* InputLayoutDesc::Element::semantic() const noexcept {
    return kMetaDescArray[(int)type_].semantic;
}

const char* InputLayoutDesc::Element::type_name() const noexcept {
    return kMetaDescArray[(int)type_].type_name;
}

// Vertex
VertexStruct::VertexStruct( char* pData,const InputLayoutDesc& layout ) :
    data_( pData ),
    layout_( layout )
{
    assert( pData != nullptr );
}

// VertexData
VertexData::VertexData(const InputLayoutDesc& layout, size_t size) :
    layout_(layout)
{
    Resize(size);
}

void VertexData::Resize(size_t new_size)
{
    const auto cur_size = size();
    if(cur_size < new_size)
    {
        buffer_.resize( buffer_.size() + layout_.data_size() * (new_size - cur_size) );
    }
}


void VertexData::Reserve(size_t new_size) {
    buffer_.reserve(layout_.data_size() * new_size);
}

VertexStruct VertexData::back()
{
    assert( buffer_.size() != 0u );
    return VertexStruct{ buffer_.data() + buffer_.size() - layout_.data_size(),layout_ };
}

VertexStruct VertexData::front()
{
    assert( buffer_.size() != 0u );
    return VertexStruct{ buffer_.data(),layout_ };
}

VertexStruct VertexData::operator[](size_t i)
{
    assert( i < size() );
    return VertexStruct{ buffer_.data() + layout_.data_size() * i,layout_ };
}

const VertexStruct VertexData::back() const
{
    return const_cast<VertexData*>(this)->back();
}

const VertexStruct VertexData::front() const
{
    return const_cast<VertexData*>(this)->front();
}

const VertexStruct VertexData::operator[](size_t i) const
{
    return const_cast<VertexData&>(*this)[i];
}

}
}