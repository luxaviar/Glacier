#pragma once

#include <stdint.h>
#include <string>
#include <memory>
#include "render/base/resource.h"

namespace glacier {
namespace render {

struct ResourceID {
    ResourceID(uint32_t id) : id(id) {}
    virtual ~ResourceID() = default;

    operator uint32_t() {
        return id;
    }

    bool operator<(ResourceID other) {
        return id < other.id;
    }

    bool operator==(ResourceID other) {
        return id == other.id;
    }

    bool operator!=(ResourceID other) {
        return !(*this == other);
    }

    uint32_t id;
};

template<typename T>
struct ResourceHandle : public ResourceID {
    ResourceHandle(ResourceID id) : ResourceID(id) {}
    ResourceHandle(uint32_t id) : ResourceID(id) {}
};

class ResourceEntryBase {
public:
    ResourceEntryBase(uint32_t id, const char* name, std::shared_ptr<Resource>&& res) :
        id_(id),
        name_(name),
        resource_(std::move(res))
    {}
    virtual ~ResourceEntryBase() = default;

    const auto id() const { return id_; }
    const char* name() const { return name_; }

    template<typename T>
    std::shared_ptr<T> As() {
        auto p = std::dynamic_pointer_cast<T>(resource_);
        return p;
    }

protected:
    uint32_t id_;
    const char* const name_;
    std::shared_ptr<Resource> resource_;
};

template<typename T>
class ResourceEntry : public ResourceEntryBase {
public:
    ResourceEntry(uint32_t id, const char* name, std::shared_ptr<T>&& res) :
        ResourceEntryBase(id, name, std::move(res))
    { }

    auto Get() {
        auto p = std::dynamic_pointer_cast<T>(resource_);
        return p;
    }
};

//template<typename T>
//class RenderResourceEntry : public ResourceEntry {
//public:
//    RenderResourceEntry(uint32_t id, const char* name, std::unique_ptr<T>&& res) : ResourceEntry(id, name) {}
//};

}
}
