#pragma once

#include <queue>
#include <mutex>
#include <memory>
#include <cassert>

namespace dronecode_sdk {

template<class T> class LockedQueue {
public:
    LockedQueue(){};
    ~LockedQueue(){};

    void push_back(T item)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _queue.push_back(std::make_shared<T>(item));
    }

    // This allows to get access to the front and keep others
    // from using it. This blocks if the front is already borrowed.
    std::shared_ptr<T> borrow_front(std::unique_lock<std::mutex> &queue_lock)
    {
        assert(queue_lock.owns_lock() == false); // double borrow
        queue_lock = std::unique_lock<std::mutex>(_mutex);
        if (_queue.size() == 0) {
            // We couldn't borrow anything, therefore don't keep the lock.
            queue_lock.unlock();
            return nullptr;
        }
        return _queue.front();
    }

    // This allows to return a borrowed queue.
    void return_front(std::unique_lock<std::mutex> &queue_lock)
    {
        assert(queue_lock.owns_lock() == true); // return without borrow
        queue_lock.unlock();
    }

    void pop_front(std::unique_lock<std::mutex> &queue_lock)
    {
        assert(queue_lock.owns_lock() == true); // pop without borrow
        _queue.pop_front();
        queue_lock.unlock();
    }

    size_t size()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _queue.size();
    }

private:
    std::deque<std::shared_ptr<T>> _queue{};
    std::mutex _mutex{};
};

} // namespace dronecode_sdk
