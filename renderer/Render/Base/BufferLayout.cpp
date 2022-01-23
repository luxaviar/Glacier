#include "BufferLayout.h"
#include <string>
#include <algorithm>
#include <cctype>
#include "common/alignment.h"

namespace glacier {
namespace render {

using Element = BufferLayoutDesc::Element;

Element::Element(ElementType ty) : type_(ty)
{
    assert("Wrong ctor for array or empty" && ty != ElementType::Empty && ty != ElementType::Array);
    if (ty == ElementType::Struct)
    {
        struct_ = std::make_unique<StructType>();
    }
}

Element::Element(ElementType ty, size_t size) : type_(ElementType::Array)
{
    assert("Don't support array of array" && ty != ElementType::Array);
    array_ = std::make_unique<ArrayType>(new Element(ty), size);
}

Element& Element::Add(ElementType ty, const char* key)
{
    assert("Modify baked uniform layout" && !baked());
    assert("Add to non-struct in layout" && type_ == ElementType::Struct);
    for (auto& mem : *struct_)
    {
        if (mem.first == key)
        {
            assert("Adding duplicate name to struct" && false);
        }
    }
    struct_->emplace_back(key, Element{ ty });
    return *this;
}

Element& Element::Add(ElementType ty, size_t size, const char* key) {
    assert("Modify baked uniform layout" && !baked());
    assert("Add to non-struct in layout" && type_ == ElementType::Struct);
    for (auto& mem : *struct_)
    {
        if (mem.first == key)
        {
            assert("Adding duplicate key to struct" && false);
        }
    }
    struct_->emplace_back(key, Element{ ty, size });
    return *this;
}

Element& Element::operator[]( const std::string& key )
{
    assert( "Keying into non-struct" && type_ == ElementType::Struct );
    for( auto& mem : *struct_)
    {
        if( mem.first == key )
        {
            return mem.second;
        }
    }
    
    static Element empty{};
    return empty;
}

const Element& Element::operator[]( const std::string& key ) const
{
    return const_cast<Element&>(*this)[key];
}

size_t Element::Bake( size_t offsetIn )
{
    if (baked()) return offset_ + size_;

    if (struct_) {
        assert(struct_->size() != 0u);
        offset_ = AlignBoundary(offsetIn);
        auto offset_end = offset_;
        for (auto& el : *struct_)
        {
            offset_end = el.second.Bake(offset_end);
        }
        size_ = offset_end - offset_;
        return offset_end;
    }
    else if (array_) {
        assert(array_->size != 0u);
        offset_ = AlignBoundary(offsetIn);
        array_->element->Bake(offset_);
        array_->element_size = AlignBoundary(array_->element->size_);
        size_ = array_->element_size * array_->size;
        return offset_ + size_;
    }
    else {
        assert(type_ < ElementType::Struct);
        size_ = kMetaDescArray[(int)type_].size;
        offset_ = AlignOffset(offsetIn, size_);
        return offset_ + size_;
    }
}

void BufferField::OnUpdate() const {
    uniform_->Update();
}

BufferData::BufferData(BufferLayoutDesc&& layout) :
    layout_(std::move(layout.root())),
    data_(AlignBoundary(layout_->Bake(0)))
{}

BufferData::BufferData( const BufferData& buf ) noexcept :
    layout_( buf.layout_ ),
    data_( buf.data_ )
{}

BufferData::BufferData( BufferData&& buf ) noexcept :
    layout_( std::move( buf.layout_ ) ),
    data_( std::move( buf.data_ ) )
{}

void BufferData::CopyFrom( const BufferData& other )
{
    assert( &layout() == &other.layout() );
    std::copy( other.data_.begin(),other.data_.end(),data_.begin() );
}

}
}
