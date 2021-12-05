#pragma once

namespace glacier {

template <class T>
class Singleton {
public:
    static T* Instance() {
        static T_Instance instance;
        T* pt = &instance;
        return pt;
    }

    Singleton& operator = (const Singleton&) = delete;
    Singleton& operator = (Singleton&&) = delete;

protected:
    Singleton() {}

private:
    struct T_Instance : public T {
        T_Instance() : T() {}
    };
};

}
