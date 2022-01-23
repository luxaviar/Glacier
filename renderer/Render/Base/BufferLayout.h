#pragma once

#include <cassert>
#include <vector>
#include <memory>
#include <string>
#include <array>
#include <type_traits>
#include "Math/Vec2.h"
#include "Math/Vec3.h"
#include "Math/Vec4.h"
#include "Math/Mat4.h"

#define LEAF_ELEMENT_TYPES \
    X( Float ) \
    X( Float2 ) \
    X( Float3 ) \
    X( Float4 ) \
    X( Matrix ) \
    X( Bool ) \
    X( Integer )

#define LAYOUT_ELEMENT_META(el, code, systy) \
struct Meta##el \
{ \
    using SysType = systy; \
    static constexpr size_t SysSize = sizeof(SysType); \
    static constexpr ElementType Type = el; \
    static constexpr const char* Code = #code; \
    static constexpr bool valid = true; \
};
//static constexpr Meta##el el = {};

namespace glacier {
namespace render {

class BufferLayoutDesc {
public:
    enum ElementType {
        #define X(el) el,
        LEAF_ELEMENT_TYPES
        #undef X
        Struct,
        Array,
        Empty,
    };

    struct MetaDesc {
        const ElementType type;
        const char* code;
        const size_t size;
    };

    LAYOUT_ELEMENT_META(Float, F1, float)
    LAYOUT_ELEMENT_META(Float2, F2, Vec2f)
    LAYOUT_ELEMENT_META(Float3, F3, Vec3f)
    LAYOUT_ELEMENT_META(Float4, F4, Vec4f)
    LAYOUT_ELEMENT_META(Matrix, M4, Matrix4x4)
    LAYOUT_ELEMENT_META(Bool, BL, int32_t)
    LAYOUT_ELEMENT_META(Integer, IN, int32_t)

    static constexpr std::array<MetaDesc, (int)Struct> kMetaDescArray = { {
    #define X(el) { \
        el, \
        Meta##el::Code, \
        Meta##el::SysSize \
    },
        LEAF_ELEMENT_TYPES
    #undef X
    } };

    class Element
    {
        friend class BufferField;
    public:
        Element() noexcept = default;
        Element(ElementType ty);
        Element(ElementType ty, size_t size);

        Element& Add(ElementType ty, const char* key);
        Element& Add(ElementType ty, size_t size, const char* key);
        size_t Bake(size_t offsetIn);

        ElementType type() const { return type_; }
        bool baked() const { return size_ > 0; }

        Element& operator[](const std::string& key);
        const Element& operator[](const std::string& key) const;

        template<typename T>
        size_t Resolve() const
        {
            switch( type_ )
            {
            #define X(el) case el: { \
                    if constexpr (std::is_same<Meta##el::SysType, T>::value) { \
                        return offset_; \
                    } \
                    else { \
                        assert("Tried to resolve wrong type element "#el && false);\
                    } \
                }
            LEAF_ELEMENT_TYPES
            #undef X
            default:
                assert( "Tried to resolve non-leaf element" && false );
                return 0u;
            }
        }

    private:
        using StructType = std::vector<std::pair<std::string, Element>>;
        struct ArrayType {
            ArrayType(Element* e, size_t n) : element(e), size(n), element_size(0) {}
            std::unique_ptr<Element> element;
            size_t size;
            size_t element_size;
        };

        size_t offset_ = 0;
        size_t size_ = 0;
        ElementType type_ = ElementType::Empty;

        std::unique_ptr<StructType> struct_;
        std::unique_ptr<ArrayType> array_;
    };
    
    BufferLayoutDesc() noexcept : root_(new Element(ElementType::Struct)) 
    {}

    BufferLayoutDesc(std::shared_ptr<BufferLayoutDesc::Element>&& root) noexcept :
        root_{ std::move(root) }
    {
        assert(root_->type() == ElementType::Struct);
    }

    Element& operator[](const std::string& key) {
        return (*root_)[key];
    }

    const Element& operator[](const std::string& key) const {
        return (*root_)[key];
    }

    template<ElementType type, size_t size=1>
    Element& Add(const char* key)
    {
        static_assert(size >= 1);
        if (size > 1) {
            return root_->Add(type, size, key);
        }
        else {
            return root_->Add(type, key);
        }
    }

    std::shared_ptr<Element> root() const noexcept
    {
        return root_;
    }

private:
    std::shared_ptr<Element> root_;
};

class BufferData;

class BufferField
{
public:
    BufferField(BufferData* uniform, const BufferLayoutDesc::Element* layout, char* data, size_t offset) noexcept :
        uniform_(uniform),
        layout_(layout),
        data_(data),
        offset_(offset)
    {
        assert(uniform_);
        assert(layout->baked());
    }

    BufferField operator[](const std::string& key)
    {
        return { uniform_, &(*layout_)[key], data_, offset_ };
    }

    BufferField operator[](size_t index)
    {
        assert("Indexing into non-array" && layout_->type() == BufferLayoutDesc::ElementType::Array);
        auto& arr = layout_->array_;
        assert("Indexing uniform array out of bound" && index < arr->size);
        return { uniform_, arr->element.get(), data_, offset_ + arr->element_size * index };
    }

    const BufferField operator[](const std::string& key) const
    {
        return const_cast<BufferField&>(*this)[key];
    }

    const BufferField operator[](size_t index) const
    {
        return const_cast<BufferField&>(*this)[index];
    }

    template<typename T>
    operator T& () const
    {
        return *reinterpret_cast<T*>(data_ + offset_ + layout_->Resolve<T>());
    }

    template<typename T>
    T& operator=(const T& rhs) const
    {
        OnUpdate();
        return static_cast<T&>(*this) = rhs;
    }

private:
    void OnUpdate() const;

    BufferData* uniform_;
    const BufferLayoutDesc::Element* layout_;
    char* data_;
    size_t offset_; //for array
};

class BufferData
{
public:
    BufferData(BufferLayoutDesc&& lay);
    BufferData(const BufferData&) noexcept;
    BufferData(BufferData&&) noexcept;

    void CopyFrom(const BufferData&);

    BufferField operator[](const std::string& key) {
        return {this, &(*layout_)[key], data_.data(), 0u};
    }

    const BufferField operator[](const std::string& key) const {
        return const_cast<BufferData&>(*this)[key];
    }

    const char* data() const noexcept {
        return data_.data();
    }

    char* data() noexcept {
        return data_.data();
    }

    size_t data_size() const noexcept {
        return data_.size();
    }

    const BufferLayoutDesc::Element& layout() const noexcept {
        return *layout_;
    }

    void Update() const { ++version_; }
    uint32_t version() const { return version_;  }

private:
    std::shared_ptr<BufferLayoutDesc::Element> layout_;
    std::vector<char> data_;
    mutable uint32_t version_ = 0;
};
}
}

#undef LEAF_ELEMENT_TYPES
#undef LAYOUT_ELEMENT_META

