
#include "locked_queue.h"

#include <string>
#include <future>
#include <gtest/gtest.h>

using namespace dronecode_sdk;

TEST(LockedQueue, FillAndEmpty)
{
    int one = 1;
    int two = 2;
    int three = 3;

    LockedQueue<int> locked_queue{};

    locked_queue.push_back(one);
    EXPECT_EQ(locked_queue.size(), 1);
    locked_queue.push_back(two);
    locked_queue.push_back(three);
    EXPECT_EQ(locked_queue.size(), 3);

    std::unique_lock<std::mutex> queue_lock;
    auto tmp = locked_queue.borrow_front(queue_lock);
    locked_queue.pop_front(queue_lock);
    EXPECT_EQ(locked_queue.size(), 2);
    tmp = locked_queue.borrow_front(queue_lock);
    locked_queue.pop_front(queue_lock);
    tmp = locked_queue.borrow_front(queue_lock);
    locked_queue.pop_front(queue_lock);
    EXPECT_EQ(locked_queue.size(), 0);
}

TEST(LockedQueue, BorrowAndReturn)
{
    int one = 1;
    int two = 2;
    int three = 3;

    LockedQueue<int> locked_queue{};

    locked_queue.push_back(one);
    locked_queue.push_back(two);
    locked_queue.push_back(three);

    std::unique_lock<std::mutex> queue_lock;
    auto borrowed_item = locked_queue.borrow_front(queue_lock);
    EXPECT_EQ(*borrowed_item, 1);
    locked_queue.pop_front(queue_lock);

    borrowed_item = locked_queue.borrow_front(queue_lock);
    EXPECT_EQ(*borrowed_item, 2);
    locked_queue.pop_front(queue_lock);

    borrowed_item = locked_queue.borrow_front(queue_lock);
    EXPECT_EQ(*borrowed_item, 3);
    // Popping without returning should automatically return it.
    locked_queue.pop_front(queue_lock);
    EXPECT_EQ(locked_queue.size(), 0);

    borrowed_item = locked_queue.borrow_front(queue_lock);
    EXPECT_EQ(borrowed_item, nullptr);
}

TEST(LockedQueue, ConcurrantAccess)
{
    int one = 1;
    int two = 2;

    LockedQueue<int> locked_queue{};

    locked_queue.push_back(one);
    locked_queue.push_back(two);

    std::unique_lock<std::mutex> queue_lock;
    auto borrowed_item = locked_queue.borrow_front(queue_lock);
    EXPECT_EQ(*borrowed_item, 1);

    auto prom = std::make_shared<std::promise<void>>();
    auto fut = prom->get_future();

    auto some_future = std::async(std::launch::async, [&prom, &locked_queue]() {
        // This will wait in the lock until the first item is returned.
        std::unique_lock<std::mutex> queue_lock_future;
        auto second_borrowed_item = locked_queue.borrow_front(queue_lock_future);
        locked_queue.return_front(queue_lock_future);
        prom->set_value();
    });

    // The promise should not be fulfilled yet because we have not
    // returned the borrowed item.
    auto status = fut.wait_for(std::chrono::milliseconds(20));
    EXPECT_EQ(status, std::future_status::timeout);

    locked_queue.return_front(queue_lock);
    status = fut.wait_for(std::chrono::milliseconds(20));
    EXPECT_EQ(status, std::future_status::ready);
}

TEST(LockedQueue, ChangeValue)
{
    struct Item {
        int value{42};
    };

    LockedQueue<Item> locked_queue{};

    Item one;

    locked_queue.push_back(one);

    {
        std::unique_lock<std::mutex> queue_lock;
        auto borrowed_item = locked_queue.borrow_front(queue_lock);
        EXPECT_EQ(borrowed_item->value, 42);
        borrowed_item->value = 43;
        locked_queue.return_front(queue_lock);
    }

    {
        std::unique_lock<std::mutex> queue_lock;
        auto borrowed_item = locked_queue.borrow_front(queue_lock);
        EXPECT_EQ(borrowed_item->value, 43);
        locked_queue.pop_front(queue_lock);
    }
}

TEST(LockedQueue, MissedReturn)
{
    struct Item {
        int value{42};
    };

    LockedQueue<Item> locked_queue{};

    Item one;

    locked_queue.push_back(one);

    {
        std::unique_lock<std::mutex> queue_lock;
        auto borrowed_item = locked_queue.borrow_front(queue_lock);
        //Intentionally no pop_front or return_front to check if queue_lock is unlocked automatically
        //in the destructor at the end of this block
    }

    {
        std::unique_lock<std::mutex> queue_lock;
        auto borrowed_item = locked_queue.borrow_front(queue_lock);
        locked_queue.pop_front(queue_lock);
    }
}
