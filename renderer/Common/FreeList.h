#pragma once

#include <list>
#include <vector>
#include <assert.h>
#include <type_traits>
#include "Math/Util.h"
#include "uncopyable.h"

namespace glacier {

template<typename T>
class FreeList : private Uncopyable {
public:
    struct alignas(alignof(T)) Element {
        T object;
        bool active;
        int next;
        int id;
                
        template<typename... Args>
        Element(int next_, int list_id, int vec_id, bool active_, Args&&... args) :
            object(std::forward<Args>(args)...),
            active(active_),
            next(next_),
            id((list_id << 16) | vec_id)
        {
        }
    };

    FreeList(size_t capacity) : free_(-1), count_(0), capacity_(math::Clamp<size_t>(capacity, 32, 65535)) {
    }

    ~FreeList() {
        
    }

    template<typename... Args>
    T* Acquire(Args&&... args) {
        if (free_ < 0) {
            std::vector<Element>* vec = Last();
            if (!vec || vec->size() == capacity_) {
                list_.emplace_back();
                vec = Last();
                vec->reserve(capacity_);
            }

            int vec_id = static_cast<int>(vec->size());
            vec->emplace_back(-1, static_cast<int>(list_.size()) - 1, vec_id, true, std::forward<Args>(args)...);

            Element& e = (*vec)[vec_id];
            ++count_;
            return &e.object;
        }

        T* obj = Get(free_);
        Element* e = (Element*)obj;
        e->active = true;
        free_ = e->next;
        ++count_;
        obj->Reset(std::forward<Args>(args)...);
        return obj;
    }

    void Release(T* object) {
        Element* e = (Element*)object;
        assert(e->active);

        e->next = free_;
        e->active = false;
        free_ = e->id;
        --count_;
    }

    T* Get(int id) {
        int list_id = id >> 16;
        int vec_id = id & 0xFFFF;

        assert(list_id >= 0 && list_id < (int)list_.size());
        assert(vec_id >= 0 && vec_id < (int)capacity_);

        auto it = list_.begin();
        std::advance(it, list_id);
        auto& vec = *it;

        Element& e = vec[vec_id];
        assert(e.id == id);

        return &e.object;
    }

    int Index(T* object) const {
        Element* e = (Element*)object;
        return e->id;
    }

    void Reset() {
        free_ = -1;
        count_ = 0;
        size_t list_size = list_.size();
        if (list_size == 0) return;

        free_ = 0;
        for (size_t i = 0; i < list_size; ++i) {
            auto it = list_.begin();
            std::advance(it, i);
            auto& vec = *it;

            size_t vec_size = vec.size();
            for (size_t j = 0; j < vec_size; ++j) {
                Element& e = vec[j];
                if (e.active) {
                    //(&e.object)->~T();
                }
                e.active = false;
                e.next = static_cast<int>((i << 16) | (j + 1));
                if (j == vec_size - 1) {
                    if (i < list_size - 1) {
                        e.next = static_cast<int>(((i + 1) << 16));
                    } else {
                        e.next = -1;
                    }
                }
            }
        }
    }

    // free empty vector
    void Shrink() {
        std::vector<size_t> dels;
        for (size_t i = 0; i < list_.size(); ++i) {
            auto it = list_.begin();
            std::advance(it, i);
            auto& vec = *it;
            bool empty = true;
            for (size_t j = 0; j < vec.size(); ++j) {
                if (vec[j].active) {
                    empty = false;
                    break;
                }
            }

            if (empty) {
                dels.push_back(i);
            }
        }

        if (dels.size() == 0) return;

        for (int i = (int)dels.size() - 1; i >= 0; --i) {
            size_t del_idx = dels[i];
            auto it = list_.begin();
            std::advance(it, del_idx);
            list_.erase(it);
        }

        Element* last = nullptr;
        for (size_t i = 0; i < list_.size(); ++i) {
            auto it = list_.begin();
            std::advance(it, i);
            auto& vec = *it;
            for (size_t j = 0; j < vec.size(); ++j) {
                auto& e = vec[j];
                int id = (i << 16) | j;
                e.id = id;
                if (!e.active) {
                    if (!last) {
                        free_ = id;
                    } else {
                        last->next = id;
                    }
                    last = &e;
                    last->next = -1;
                }
            }
        }
    }

    size_t count() const { return count_; }
    size_t list_count() const { return list_.size(); }

private:
    std::vector<Element>* Last() {
        if (list_.size() == 0) {
            return nullptr;
        }

        int list_id = (int)(list_.size() - 1);
        auto it = list_.begin();
        std::advance(it, list_id);
        auto& vec = *it;
        return &vec;
    }

    int free_;
    size_t count_;
    size_t capacity_;
    std::list<std::vector<Element>> list_;
};

}
