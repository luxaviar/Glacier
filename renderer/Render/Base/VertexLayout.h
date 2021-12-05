#pragma once

#include <vector>
#include <type_traits>
#include <utility>
#include <array>
#include <d3d11_1.h>
#include "Math/Vec2.h"
#include "Math/Vec3.h"
#include "Math/Vec4.h"

#define DVTX_ELEMENT_AI_EXTRACTOR(member) 

#define LAYOUT_ELEMENT_TYPES \
    X( Position2D ) \
    X( Position3D ) \
    X( Texture2D ) \
    X( Normal ) \
    X( Tangent ) \
    X( Bitangent ) \
    X( Float3Color ) \
    X( Float4Color ) \
    //X( Count )

#define LAYOUT_ELEMENT_META(el, semantic, code, systy, hwtype, hwtypename, fld) \
struct Meta##el \
{ \
    using SysType = systy; \
    static constexpr DXGI_FORMAT GpuType = hwtype; \
    static constexpr size_t SysSize = sizeof(SysType); \
    static constexpr ElementType Type = ElementType::el; \
    static constexpr const char* Semantic = #semantic; \
    static constexpr const char* Code = #code; \
    static constexpr const char* GpuTypeName = #hwtypename; \
    DVTX_ELEMENT_AI_EXTRACTOR(fld) \
}; \
static constexpr Meta##el el = {}; \

namespace glacier {
namespace render {

class InputLayoutDesc {
public:
    enum class ElementType : uint8_t {
        #define X(el) el,
        LAYOUT_ELEMENT_TYPES
        #undef X
        Count
    };

    struct MetaDesc {
        const ElementType type;
        const char* semantic;
        const char* code;
        const char* type_name;
        const DXGI_FORMAT gfx_format;
        const D3D11_INPUT_ELEMENT_DESC gfx_desc;
        const size_t size;
    };

    LAYOUT_ELEMENT_META(Position2D, Position, P2, Vec2f, DXGI_FORMAT_R32G32_FLOAT, float2, mVertices)
    LAYOUT_ELEMENT_META(Position3D, Position, P3, Vec3f, DXGI_FORMAT_R32G32B32_FLOAT, float3, mVertices)
    LAYOUT_ELEMENT_META(Texture2D, Texcoord, T2, Vec2f, DXGI_FORMAT_R32G32_FLOAT, float2, mTextureCoords[0])
    LAYOUT_ELEMENT_META(Normal, Normal, N, Vec3f, DXGI_FORMAT_R32G32B32_FLOAT, float3, mNormals)
    LAYOUT_ELEMENT_META(Tangent, Tangent, T, Vec3f, DXGI_FORMAT_R32G32B32_FLOAT, float3, mTangents)
    LAYOUT_ELEMENT_META(Bitangent, Bitangent, B, Vec3f, DXGI_FORMAT_R32G32B32_FLOAT, float3, mBitangents)
    LAYOUT_ELEMENT_META(Float3Color, Color, C3, Vec3f, DXGI_FORMAT_R32G32B32_FLOAT, float3, mColors[0])
    LAYOUT_ELEMENT_META(Float4Color, Color, C4, Vec4f, DXGI_FORMAT_R32G32B32A32_FLOAT, float4, mColors[0])

    static constexpr std::array<MetaDesc, (int)ElementType::Count> kMetaDescArray = {{
    #define X(el) { \
        ElementType::el, \
        Meta##el::Semantic, \
        Meta##el::Code, \
        Meta##el::GpuTypeName, \
        Meta##el::GpuType, \
        {Meta##el::Semantic, 0, Meta##el::GpuType,0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0}, \
        Meta##el::SysSize \
    },
        LAYOUT_ELEMENT_TYPES
    #undef X
    }};

    struct Signature {
        Signature() : u(0) {
            static_assert(sizeof(Signature) == sizeof(uint32_t),
                "Signature size not what was intended");
        }

        union {
            struct {
                ElementType e0 : 4;
                ElementType e1 : 4;
                ElementType e2 : 4;
                ElementType e3 : 4;
                ElementType e4 : 4;
                ElementType e5 : 4;
                ElementType e6 : 4;
                ElementType e7 : 4;
            };
            uint32_t u;
        };
    };

    class Element
    {
    public:
        Element(ElementType type, size_t offset) : type_(type), offset_(offset) {}

        size_t offset() const { return offset_;  }
        ElementType type() const noexcept { return type_; }
        
        D3D11_INPUT_ELEMENT_DESC desc() const noexcept;
        size_t data_size() const noexcept;
        const char* code() const noexcept;
        const char* semantic() const noexcept;
        const char* type_name() const noexcept;

        template<typename V>
        void SetValue(char* addr, V&& val) const
        {
            switch (type_)
            {
#define X(el) case InputLayoutDesc::Meta##el::Type: { \
                    if constexpr (std::is_assignable<InputLayoutDesc::Meta##el::SysType, V>::value) { \
                        *reinterpret_cast<InputLayoutDesc::Meta##el::SysType*>(addr) = val; \
                    } else { \
                        assert("Parameter attribute type mismatch" && false); \
                    } \
                    return; \
                }
                LAYOUT_ELEMENT_TYPES
#undef X
            }
        }

    private:
        ElementType type_;
        size_t offset_;
    };

public:
    InputLayoutDesc() noexcept {}

    template<typename ...Args>
    explicit InputLayoutDesc(Args... args) {
        Append(args...);
    }

    size_t data_size() const;
    size_t size() const noexcept { return elements_.size(); }
    std::vector<D3D11_INPUT_ELEMENT_DESC> layout_desc() const;
    uint32_t signature() const { return sig_.u; }

    template<typename T>
    const Element& Get() const
    {
        for (auto& e : elements_)
        {
            if (e.type() == T::Type)
            {
                return e;
            }
        }
        assert("Could not resolve element type" && false);
        return elements_.front();
    }

    const Element& operator[](size_t i) const {
        return elements_[i];
    }

    template<typename T>
    bool Has() const noexcept
    {
        for (auto& e : elements_)
        {
            if (e.type() == T::Type)
            {
                return true;
            }
        }
        return false;
    }

    template<typename T>
    InputLayoutDesc& Append(T v) {
        if (!Has<T>())
        {
            auto pos = elements_.size();
            assert(pos <= (size_t)ElementType::Float4Color);
            elements_.emplace_back(T::Type, data_size());
            sig_.u |= ((pos * 4) << (uint32_t)T::Type);
        }
        return *this;
    }

    template<typename T, typename... Args>
    InputLayoutDesc& Append(T v, Args... args) {
        Append(v);
        Append(args...);
        return *this;
    }

private:
    std::vector<Element> elements_;
    Signature sig_;
};

class VertexStruct {
public:
    VertexStruct(char* pData, const InputLayoutDesc& layout);

    template<typename T>
    auto& operator[](T t) {
        auto pAttribute = data_ + layout_.Get<T>().offset();
        return *reinterpret_cast<typename T::SysType*>(pAttribute);
    }

    template<typename T>
    const auto& operator[](T t) const {
        auto pAttribute = data_ + layout_.Get<T>().offset();
        return *reinterpret_cast<const typename T::SysType*>(pAttribute);
    }

    template<typename T>
    void SetField(size_t i, T&& val)
    {
        const auto& element = layout_[i];
        auto pAttribute = data_ + element.offset();
        element.SetValue(pAttribute, std::forward<T>(val));
    }

    // enables parameter pack setting of multiple parameters by element index
    template<typename First,typename ...Rest>
    void SetField(size_t i, First&& first, Rest&&... rest)
    {
        SetField(i,std::forward<First>(first));
        SetField(i + 1,std::forward<Rest>(rest)...);
    }
private:
    char* data_ = nullptr;
    const InputLayoutDesc& layout_;
};

class VertexData {
public:
    VertexData(const InputLayoutDesc& layout,size_t size = 0u);
    //VertexData( VertexLayout layout,const aiMesh& mesh );

    const char* data() const noexcept { return buffer_.data(); }
    size_t data_size() const noexcept { return buffer_.size(); }
    const InputLayoutDesc& layout() const noexcept { return layout_; }
    size_t size() const noexcept { return buffer_.size() / layout_.data_size(); }
    size_t stride() const noexcept { return layout_.data_size(); }

    void Resize(size_t new_size);
    void Reserve(size_t new_size);

    template<typename ...Params>
    void EmplaceBack(Params&&... params)
    {
        assert(sizeof...(params) == layout_.size() && "Param count doesn't match number of vertex elements");
        buffer_.resize(buffer_.size() + layout_.data_size());
        back().SetField(0u, std::forward<Params>( params )...);
    }

    VertexStruct back();
    VertexStruct front();
    VertexStruct operator[](size_t i);

    const VertexStruct back() const;
    const VertexStruct front() const;
    const VertexStruct operator[]( size_t i ) const;
private:
    std::vector<char> buffer_;
    InputLayoutDesc layout_;
};

}
}

#undef DVTX_ELEMENT_AI_EXTRACTOR
#undef LAYOUT_ELEMENT_TYPES
#undef LAYOUT_ELEMENT_META