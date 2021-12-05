#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "Common/Singleton.h"
#include "common/list.h"

namespace glacier {

class Renderable;

template<typename S, typename T>
class BaseManager : public Singleton<S> {
public:
    using BaseType = BaseManager<S, T>;

    void Add(ListNode<T>& node) {
        objects_.push_back(node);
        lookup_.emplace(node->id(), node.data);
    }

    void Remove(ListNode<T>& node) {
        node.RemoveFromList();
        lookup_.erase(node->id());
    }

    T* Find(uint32_t id) {
        auto it = lookup_.find(id);
        if (it != lookup_.end()) {
            return it->second;
        }

        return nullptr;
    }

    const List<T>& GetList() { return objects_; }

protected:
    List<T> objects_;
    std::unordered_map<uint32_t, T*> lookup_;
};

}
