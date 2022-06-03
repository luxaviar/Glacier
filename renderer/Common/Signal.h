#pragma once

#include <functional>
#include <vector>
#include <assert.h>
#include "uncopyable.h"

namespace glacier {

template<typename ...Args>
class Signal : private Uncopyable {
public:
    using Delegate = std::function<void(Args...)>;
    using Handle = uint32_t;

    struct Slot {
        static Handle handle_counter_;

        Slot(size_t ver, const Delegate& cb, bool oneshot) :
            handle(++handle_counter_),
            valid(true), 
            oneshot(oneshot), 
            ver(ver), 
            func(cb) 
        {
        }

        Slot(size_t ver, Delegate&& cb, bool oneshot) :
            handle(++handle_counter_),
            valid(true), 
            oneshot(oneshot), 
            ver(ver), 
            func(std::move(cb)) 
        {
        }

        Slot(Slot&& s) noexcept = default;
        Slot& operator=(Slot&& s) noexcept = default;

        Handle handle;
        bool valid;
        bool oneshot;
        size_t ver;
        Delegate func;
    };

    Signal() : cnt_(0), ver_(0) {}
    ~Signal() { Clear(); }

    uint32_t Connect(const Delegate& cb, bool oneshot=false) {
        ++cnt_;
        ++ver_;
        if (free_list_.size() > 0) {
            size_t idx = free_list_.back();
            free_list_.pop_back();
            Slot s(ver_, cb, oneshot);
            slots_[idx] = std::move(s);
            return s.handle;
        }

        slots_.emplace_back(ver_, cb, oneshot);
        auto& s = slots_.back();
        return s.handle;
    }

    uint32_t Connect(Delegate&& cb, bool oneshot=false) {
        ++cnt_;
        ++ver_;
        if (free_list_.size() > 0) {
            size_t idx = free_list_.back();
            free_list_.pop_back();

            Slot s(ver_, std::move(cb), oneshot);
            slots_[idx] = std::move(s);
            return s.handle;
        }

        slots_.emplace_back(ver_, std::move(cb), oneshot);
        auto& s = slots_.back();
        return s.handle;
    }

    bool Disconnect(Handle handle) {
        if (handle == 0) return false;

        for (size_t i = 0; i < slots_.size(); ++i) {
            auto& s = slots_[i];
            if (s.handle == handle) {
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
uint32_t Signal<Args...>::Slot::handle_counter_ = 0;

}
