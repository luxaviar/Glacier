#pragma once

#include <type_traits>

namespace glacier {

template <class T>
class move_wrapper {
public:
    move_wrapper() = default;

    /// Move a value in.
    explicit move_wrapper(T&& t) : value(std::move(t)) {}

    /// copy is move
    move_wrapper(const move_wrapper& other) : value(std::move(other.value)) {}

    /// move is also move
    move_wrapper(move_wrapper&& other) : value(std::move(other.value)) {}

    T& operator*() { return value; }
    const T& operator*() const { return value; }

    T* operator->() { return &value; }
    const T* operator->() const { return &value; }

    /// move the value out (sugar for std::move(*moveWrapper))
    T&& move() { return std::move(value); }

    // If you want these you're probably doing it wrong, though they'd be
    // easy enough to implement
    move_wrapper& operator=(move_wrapper const&) = delete;
    move_wrapper& operator=(move_wrapper&&) = delete;

private:
    mutable T value;
};

/// Make a MoveWrapper from the argument. Because the name "makeMoveWrapper"
/// is already quite transparent in its intent, this will work for lvalues as
/// if you had wrapped them in std::move.
template <class T, class T0 = typename std::remove_reference<T>::type>
move_wrapper<T0> make_move_wrapper(T&& t) {
    return move_wrapper<T0>(std::forward<T0>(t));
}

}
