#pragma once

#include <functional>

namespace glacier {
namespace lux {

template<typename T>
class Dtor {
public:
    using DtorCallback = std::function<void(T*)>;

    Dtor(DtorCallback&& cb) : cb_(std::move(cb)) {}

    void destroy(T* o) const {
        cb_(o);
    }

private:
    DtorCallback cb_;
};

}
}
