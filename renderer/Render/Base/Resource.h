#pragma once

#include "Common/Uncopyable.h"
#include "core/identifiable.h"
#include "enums.h"

namespace glacier {
namespace render {

class Resource : private Uncopyable, public Identifiable<Resource> {
public:
    virtual ~Resource() = default;

    virtual void* underlying_resource() const { return nullptr; };
};

template<typename T>
struct ResourceGuard {
    ResourceGuard(T* t) : res(t) {
        if (res) res->Bind();
    }

    ~ResourceGuard() {
        if (res) res->UnBind();
    }

    T* res;
};

template<typename T>
struct ResourceGuardEx {
    ResourceGuardEx(T* t, ShaderType ty, uint16_t idx) : res(t), type(ty), slot(idx) {
        if (res) res->Bind(type, slot);
    }

    ~ResourceGuardEx() {
        if (res) res->UnBind(type, slot);
    }

    T* res;
    ShaderType type;
    uint16_t slot;
};

}
}
