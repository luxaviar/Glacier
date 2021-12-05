#pragma once

namespace glacier {

template<typename T>
class Identifiable {
public:
    Identifiable() noexcept :
        id_(++id_counter_ != 0 ? id_counter_ : ++id_counter_)
    {
    }

    virtual ~Identifiable() {}

    uint32_t id() const { return id_; }

protected:
    static uint32_t id_counter_;
    uint32_t id_;
};

template<typename T>
uint32_t Identifiable<T>::id_counter_ = 0;

}
