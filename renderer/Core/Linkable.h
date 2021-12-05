#pragma once

#include "common/list.h"

namespace glacier {

template<typename S, typename T>
class Linkable {
public:
    Linkable() noexcept :
        node_(static_cast<T*>(this)) 
    {
        S::Instance()->Add(node_);
    }

    virtual ~Linkable() {
        S::Instance()->Remove(node_);
    }

protected:
    ListNode<T> node_;
};

//template<typename S, typename T>
//uint32_t Linkable<S, T>::id_counter_ = 0;

}
