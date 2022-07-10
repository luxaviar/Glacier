#pragma once

#include <queue>
#include <mutex>

namespace glacier {
namespace concurrent {

template<typename T>
class ThreadSafeQueue {
public:
    ThreadSafeQueue();
    ThreadSafeQueue(const ThreadSafeQueue& copy);

    void Push(T&& value);
    void Push(const T& value);
    bool Pop();

    bool TryPop(T& value);
    bool TryPeek(T& value);

    bool Empty() const;
    size_t Size() const;

private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
};

template<typename T>
ThreadSafeQueue<T>::ThreadSafeQueue()
{}

template<typename T>
ThreadSafeQueue<T>::ThreadSafeQueue(const ThreadSafeQueue<T>& copy)
{
    std::lock_guard<std::mutex> lock(copy.mutex_);
    queue_ = copy.queue_;
}

template<typename T>
void ThreadSafeQueue<T>::Push(T&& value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(std::move(value));
}

template<typename T>
void ThreadSafeQueue<T>::Push(const T& value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(value);
}

template<typename T>
bool ThreadSafeQueue<T>::TryPop(T& value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty())
        return false;

    value = queue_.front();
    queue_.pop();

    return true;
}


template<typename T>
bool ThreadSafeQueue<T>::Pop()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty())
        return false;

    queue_.pop();

    return true;
}

template<typename T>
bool ThreadSafeQueue<T>::TryPeek(T& value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty())
        return false;

    value = queue_.front();

    return true;
}

template<typename T>
bool ThreadSafeQueue<T>::Empty() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}

template<typename T>
size_t ThreadSafeQueue<T>::Size() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

}
}
