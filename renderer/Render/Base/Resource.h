#pragma once

#include "Common/Uncopyable.h"
#include "core/identifiable.h"
#include "enums.h"

namespace glacier {
namespace render {

class Resource : private Uncopyable, public Identifiable<Resource> {
public:
    virtual ~Resource() = default;
    virtual void SetName(const TCHAR* name) {}
    virtual const TCHAR* GetName(const TCHAR* name) const { return TEXT(""); }

    virtual void* underlying_resource() const { return nullptr; };
};

class GfxDriver;

template<typename T>
struct BindingGuard {
    BindingGuard(GfxDriver* gfx, T* t) : gfx(gfx), res(t) {
        if (res) res->Bind(gfx);
    }

    ~BindingGuard() {
        if (res) {
            res->UnBind(gfx);
        }
    }

    GfxDriver* gfx;
    T* res;
};

}
}
