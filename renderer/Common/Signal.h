#pragma once

#include <functional>
#include <vector>
#include <assert.h>
#include "uncopyable.h"

namespace glacier {

template<typename ...Args>
class Signal : private Uncopyable {
public:
    using CallType = std::function<void(Args...)>;

    struct Slot {
        static uint32_t id_counter_;

        Slot(size_t ver_, const CallType& cb, bool oneshot_) : 
            id(++id_counter_), 
            valid(true), 
            oneshot(oneshot_), 
            ver(ver_), 
            func(cb) 
        {
        }

        Slot(size_t ver_, CallType&& cb, bool oneshot_) : 
            id(++id_counter_), 
            valid(true), 
            oneshot(oneshot_), 
            ver(ver_), 
            func(std::move(cb)) 
        {
        }

        Slot(Slot&& s) noexcept = default;
        Slot& operator=(Slot&& s) noexcept = default;

        uint32_t id;
        bool valid;
        bool oneshot;
        size_t ver;
        CallType func;
    };

    Signal() : cnt_(0), ver_(0) {}
    ~Signal() { Clear(); }

    uint32_t Connect(const CallType& cb, bool oneshot=false) {
        ++cnt_;
        ++ver_;
        if (free_list_.size() > 0) {
            size_t idx = free_list_.back();
            free_list_.pop_back();
            Slot s(ver_, cb, oneshot);
            slots_[idx] = std::move(s);
            return s.id;
        }

        slots_.emplace_back(ver_, cb, oneshot);
        auto& s = slots_.back();
        return s.id;
    }

    uint32_t Connect(CallType&& cb, bool oneshot=false) {
        ++cnt_;
        ++ver_;
        if (free_list_.size() > 0) {
            size_t idx = free_list_.back();
            free_list_.pop_back();

            Slot s(ver_, std::move(cb), oneshot);
            slots_[idx] = std::move(s);
            return s.id;
        }

        slots_.emplace_back(ver_, std::move(cb), oneshot);
        auto& s = slots_.back();
        return s.id;
    }

    bool Disconnect(uint32_t id) {
        if (id == 0) return false;

        for (size_t i = 0; i < slots_.size(); ++i) {
            auto& s = slots_[i];
            if (s.id == id) {
                if (!s.valid) return false;

                s.valid = false;
                free_list_.push_back(i);
                --cnt_;
                return true;
            }
        }

        return false;
    }

    void Clear() {
        slots_.clear();
        free_list_.clear();
        cnt_ = 0;
        ver_ = 0;
    }

    void Emit(Args... args) {
        size_t cur_ver = ver_;
        for (auto i = slots_.size(); i > 0; --i) {
            auto& s = slots_[i - 1];
            if (s.valid && s.ver <= cur_ver) {
                s.func(args...);
                if (s.oneshot) {
                    s.valid = false;
                    s.func = {};
                    free_list_.push_back(i);
                }
            }
        }
    }

    size_t size() const { return cnt_; }
    bool empty() const { return cnt_ == 0; }

private:
    size_t cnt_;
    uint64_t ver_;

    std::vector<Slot> slots_;
    std::vector<size_t> free_list_;
};

template<typename ...Args>
uint32_t Signal<Args...>::Slot::id_counter_ = 0;

}
