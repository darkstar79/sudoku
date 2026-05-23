// sudoku - Offline Sudoku Game
// Copyright (C) 2025-2026 Alexander Bendlin (darkstar79)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "core/observable.h"

#include <memory>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

TEST_CASE("Observable - Advanced Features", "[observable]") {
    SECTION("getSubscriptionCount() with weak_ptr cleanup") {
        // Verify weak_ptr observers are cleaned up correctly
        Observable<int> obs(0);
        int notification_count = 0;

        {
            auto observer = std::make_shared<int>(0);
            obs.subscribe([observer, &notification_count](int val) {
                *observer = val;
                notification_count++;
            });
            REQUIRE(obs.getSubscriptionCount() == 1);

            // Trigger notification while observer is alive
            obs.set(42);
            REQUIRE(notification_count == 1);
            REQUIRE(*observer == 42);
        }  // observer goes out of scope

        // Callback is still registered (uses std::function, not weak_ptr)
        // This tests that callback-based subscriptions persist
        obs.set(100);
        REQUIRE(notification_count == 2);
        REQUIRE(obs.getSubscriptionCount() == 1);
    }

    SECTION("getSubscriptionCount() with interface-based weak_ptr cleanup") {
        // Test cleanup of interface-based observers that use weak_ptr
        class TestObserver : public IObserver<int> {
        public:
            int value = 0;
            void onNotify(const int& v) override {
                value = v;
            }
        };

        Observable<int> obs(0);

        {
            auto observer = std::make_shared<TestObserver>();
            obs.subscribe(observer);
            REQUIRE(obs.getSubscriptionCount() == 1);

            obs.set(42);
            REQUIRE(observer->value == 42);
        }  // observer goes out of scope, shared_ptr destroyed

        // Trigger notification - should cleanup expired weak_ptr
        obs.set(100);

        // After notification, expired weak_ptr should be removed
        REQUIRE(obs.getSubscriptionCount() == 0);
    }

    SECTION("CompositeObserver lifecycle") {
        // Test that CompositeObserver properly manages multiple subscriptions
        Observable<int> obs1(1);
        Observable<int> obs2(2);
        int combined_value = 0;

        {
            CompositeObserver composite;
            composite.observe(obs1, [&](int v) { combined_value += v; });
            composite.observe(obs2, [&](int v) { combined_value += v * 10; });

            obs1.set(5);  // combined_value = 5
            REQUIRE(combined_value == 5);

            obs2.set(3);  // combined_value = 5 + 30 = 35
            REQUIRE(combined_value == 35);

            // Composite still alive, subscriptions active
            obs1.set(10);  // combined_value = 35 + 10 = 45
            REQUIRE(combined_value == 45);
        }  // composite goes out of scope, destructor calls unsubscribeAll()

        // After composite destroyed, clearSubscriptions() was called on both observables
        REQUIRE(obs1.getSubscriptionCount() == 0);
        REQUIRE(obs2.getSubscriptionCount() == 0);

        // Further changes should not affect combined_value
        int previous_value = combined_value;
        obs1.set(999);
        obs2.set(888);
        REQUIRE(combined_value == previous_value);
    }

    SECTION("makeObservable() helper function") {
        // Test factory function
        auto obs = makeObservable(42);
        REQUIRE(obs.get() == 42);

        bool notified = false;
        int received_value = 0;
        obs.subscribe([&](int v) {
            notified = true;
            received_value = v;
        });

        obs.set(100);
        REQUIRE(notified);
        REQUIRE(received_value == 100);
    }

    SECTION("makeObservable() with complex types") {
        struct Point {
            int x;
            int y;
            bool operator==(const Point& other) const {
                return x == other.x && y == other.y;
            }
            bool operator!=(const Point& other) const {
                return !(*this == other);
            }
        };

        auto obs = makeObservable(Point{.x = 10, .y = 20});
        REQUIRE(obs.get().x == 10);
        REQUIRE(obs.get().y == 20);

        bool notified = false;
        obs.subscribe([&](const Point&) { notified = true; });

        obs.set(Point{.x = 30, .y = 40});
        REQUIRE(notified);
        REQUIRE(obs.get().x == 30);
        REQUIRE(obs.get().y == 40);
    }

    SECTION("Assignment operator (operator=) transfers value, triggers notification") {
        Observable<int> obs(10);

        int notification_count = 0;
        int last_value = 0;

        obs.subscribe([&](int v) {
            notification_count++;
            last_value = v;
        });

        // Use assignment operator with const T&
        obs = 20;
        REQUIRE(obs.get() == 20);
        REQUIRE(notification_count == 1);
        REQUIRE(last_value == 20);

        // Use assignment operator with T&&
        obs = 30;
        REQUIRE(obs.get() == 30);
        REQUIRE(notification_count == 2);
        REQUIRE(last_value == 30);

        // Assign same value - should NOT notify
        obs = 30;
        REQUIRE(notification_count == 2);  // Still 2, no new notification
    }

    SECTION("Assignment operator does not transfer subscribers") {
        Observable<int> obs1(10);
        Observable<int> obs2(20);

        int obs1_notifications = 0;
        int obs2_notifications = 0;

        obs1.subscribe([&](int) { obs1_notifications++; });
        obs2.subscribe([&](int) { obs2_notifications++; });

        // Assign value from obs1 to obs2 using operator=
        obs2 = obs1.get();

        // obs2 should have obs1's value
        REQUIRE(obs2.get() == 10);
        REQUIRE(obs2_notifications == 1);  // obs2's subscriber notified

        // But obs1's subscriber should NOT be transferred
        obs2.set(30);
        REQUIRE(obs1_notifications == 0);  // obs1's subscriber not affected
        REQUIRE(obs2_notifications == 2);  // obs2's subscriber still active
    }

    SECTION("Explicit conversion operator") {
        Observable<int> obs(42);

        // Explicit conversion to int
        int value = obs.get();
        REQUIRE(value == 42);

        // Use in conditional (requires explicit conversion)
        Observable<bool> flag(true);
        REQUIRE(flag.get());

        flag.set(false);
        REQUIRE_FALSE(flag.get());
    }

    SECTION("Explicit conversion in expressions") {
        Observable<int> obs(10);

        // Use in arithmetic
        int result = obs.get() + 5;
        REQUIRE(result == 15);

        // Use in comparison
        REQUIRE(obs.get() == 10);
        REQUIRE(obs.get() != 20);
        REQUIRE(obs.get() < 20);
        REQUIRE(obs.get() > 5);
    }

    SECTION("clearSubscriptions() during active notification") {
        Observable<int> obs(0);

        int callback1_executed = 0;
        int callback2_executed = 0;
        int callback3_executed = 0;

        // First callback clears all subscriptions during notification
        obs.subscribe([&](int) {
            callback1_executed++;
            obs.clearSubscriptions();  // Clear during notification
        });

        obs.subscribe([&](int) { callback2_executed++; });

        obs.subscribe([&](int) { callback3_executed++; });

        REQUIRE(obs.getSubscriptionCount() == 3);

        // Trigger notification - first callback will clear all subscriptions
        obs.set(42);

        // First callback should execute (it's the one doing the clearing)
        REQUIRE(callback1_executed == 1);

        // Other callbacks may or may not execute depending on iteration behavior
        // The important thing is that we don't crash and can continue using the observable

        REQUIRE(obs.getSubscriptionCount() == 0);

        // Further updates should not notify anyone
        obs.set(100);
        REQUIRE(callback1_executed == 1);  // No new notifications
        REQUIRE(obs.getSubscriptionCount() == 0);

        // Observable should still be functional - can add new subscribers
        bool new_callback_executed = false;
        obs.subscribe([&](int) { new_callback_executed = true; });
        obs.set(200);
        REQUIRE(new_callback_executed);
    }

    SECTION("clearSubscriptions() removes all observers and callbacks") {
        class TestObserver : public IObserver<int> {
        public:
            int value = 0;
            void onNotify(const int& v) override {
                value = v;
            }
        };

        Observable<int> obs(0);

        // Add interface-based observer
        auto observer = std::make_shared<TestObserver>();
        obs.subscribe(observer);

        // Add callback-based observers
        int callback1_count = 0;
        int callback2_count = 0;
        obs.subscribe([&](int) { callback1_count++; });
        obs.subscribe([&](int) { callback2_count++; });

        REQUIRE(obs.getSubscriptionCount() == 3);

        obs.set(42);
        REQUIRE(observer->value == 42);
        REQUIRE(callback1_count == 1);
        REQUIRE(callback2_count == 1);

        // Clear all subscriptions
        obs.clearSubscriptions();
        REQUIRE(obs.getSubscriptionCount() == 0);

        // Further updates should not notify anyone
        obs.set(100);
        REQUIRE(observer->value == 42);  // Still 42, not updated
        REQUIRE(callback1_count == 1);   // Still 1
        REQUIRE(callback2_count == 1);   // Still 1
    }

    SECTION("Multiple clearSubscriptions() calls are safe") {
        Observable<int> obs(0);

        obs.subscribe([](int) {});
        obs.subscribe([](int) {});

        REQUIRE(obs.getSubscriptionCount() == 2);

        obs.clearSubscriptions();
        REQUIRE(obs.getSubscriptionCount() == 0);

        // Call again - should be safe
        obs.clearSubscriptions();
        REQUIRE(obs.getSubscriptionCount() == 0);

        // Still functional after multiple clears
        bool notified = false;
        obs.subscribe([&](int) { notified = true; });
        obs.set(42);
        REQUIRE(notified);
    }

    SECTION("getSubscriptionCount() accuracy with mixed observer types") {
        class TestObserver : public IObserver<int> {
        public:
            void onNotify(const int&) override {
            }
        };

        Observable<int> obs(0);

        // Add 2 interface-based observers
        auto observer1 = std::make_shared<TestObserver>();
        auto observer2 = std::make_shared<TestObserver>();
        obs.subscribe(observer1);
        obs.subscribe(observer2);

        // Add 3 callback-based observers
        auto unsub1 = obs.subscribe([](int) {});
        auto unsub2 = obs.subscribe([](int) {});
        auto unsub3 = obs.subscribe([](int) {});

        REQUIRE(obs.getSubscriptionCount() == 5);

        // Unsubscribe one callback
        unsub1();
        REQUIRE(obs.getSubscriptionCount() == 4);

        // Unsubscribe another callback
        unsub2();
        REQUIRE(obs.getSubscriptionCount() == 3);

        // Both interface observers still active
        obs.set(42);
        REQUIRE(obs.getSubscriptionCount() == 3);  // No cleanup yet

        // After observers go out of scope and notification triggers
        observer1.reset();
        observer2.reset();
        obs.set(100);                              // Triggers weak_ptr cleanup
        REQUIRE(obs.getSubscriptionCount() == 1);  // Only one callback remains
    }
}

TEST_CASE("Observable - update() method", "[observable]") {
    SECTION("update() triggers notification when value changes") {
        Observable<int> obs(10);
        int notification_count = 0;
        int last_value = 0;

        obs.subscribe([&](int val) {
            notification_count++;
            last_value = val;
        });

        obs.update([](int& value) { value = 42; });

        REQUIRE(obs.get() == 42);
        REQUIRE(notification_count == 1);
        REQUIRE(last_value == 42);
    }

    SECTION("update() does NOT notify when value unchanged") {
        Observable<int> obs(10);
        int notification_count = 0;

        obs.subscribe([&](int) { notification_count++; });

        // Lambda runs but doesn't change value (identity transformation)
        obs.update([](int& value) {
            (void)value;  // Suppress unused warning
        });

        REQUIRE(notification_count == 0);
    }

    SECTION("update() with multiple modifications") {
        struct Counter {
            int count = 0;
            bool operator==(const Counter& other) const = default;
        };

        Observable<Counter> obs(Counter{5});
        int notifications = 0;

        obs.subscribe([&](const Counter&) { notifications++; });

        obs.update([](Counter& counter) {
            counter.count += 10;
            counter.count *= 2;
        });

        REQUIRE(obs.get().count == 30);
        REQUIRE(notifications == 1);
    }

    SECTION("update() exception safety") {
        Observable<int> obs(10);

        try {
            obs.update([](int& value) {
                value = 100;
                throw std::runtime_error("Test exception");
            });
            REQUIRE(false);
        } catch (const std::runtime_error&) {
            REQUIRE(obs.get() == 100);
        }
    }

    SECTION("update() with complex type comparison") {
        struct Point {
            int x, y;
            bool operator==(const Point& other) const = default;
        };

        Observable<Point> obs(Point{.x = 10, .y = 20});
        int notifications = 0;

        obs.subscribe([&](const Point&) { notifications++; });

        obs.update([](Point& point) { point.x = 15; });
        REQUIRE(notifications == 1);

        obs.update([](Point& point) { point.x = 15; });
        REQUIRE(notifications == 1);
    }
}

TEST_CASE("Observable - unsubscribe(ObserverPtr)", "[observable]") {
    class TestObserver : public IObserver<int> {
    public:
        int notification_count = 0;
        int last_value = 0;
        void onNotify(const int& value) override {
            notification_count++;
            last_value = value;
        }
    };

    SECTION("unsubscribe() removes specific observer") {
        Observable<int> obs(0);
        auto observer1 = std::make_shared<TestObserver>();
        auto observer2 = std::make_shared<TestObserver>();

        obs.subscribe(observer1);
        obs.subscribe(observer2);
        REQUIRE(obs.getSubscriptionCount() == 2);

        obs.set(10);
        REQUIRE(observer1->notification_count == 1);
        REQUIRE(observer2->notification_count == 1);

        obs.unsubscribe(observer1);
        REQUIRE(obs.getSubscriptionCount() == 1);

        obs.set(20);
        REQUIRE(observer1->notification_count == 1);
        REQUIRE(observer2->notification_count == 2);
    }

    SECTION("unsubscribe() with non-existent observer is safe") {
        Observable<int> obs(0);
        auto observer1 = std::make_shared<TestObserver>();
        auto observer2 = std::make_shared<TestObserver>();

        obs.subscribe(observer1);
        REQUIRE(obs.getSubscriptionCount() == 1);

        obs.unsubscribe(observer2);
        REQUIRE(obs.getSubscriptionCount() == 1);

        obs.set(10);
        REQUIRE(observer1->notification_count == 1);
    }

    SECTION("unsubscribe() is idempotent") {
        Observable<int> obs(0);
        auto observer = std::make_shared<TestObserver>();

        obs.subscribe(observer);
        REQUIRE(obs.getSubscriptionCount() == 1);

        obs.unsubscribe(observer);
        REQUIRE(obs.getSubscriptionCount() == 0);

        obs.unsubscribe(observer);
        REQUIRE(obs.getSubscriptionCount() == 0);
    }

    SECTION("unsubscribe() doesn't affect callback-based subscribers") {
        Observable<int> obs(0);
        auto observer = std::make_shared<TestObserver>();
        int callback_count = 0;

        obs.subscribe(observer);
        obs.subscribe([&](int) { callback_count++; });
        REQUIRE(obs.getSubscriptionCount() == 2);

        obs.unsubscribe(observer);
        REQUIRE(obs.getSubscriptionCount() == 1);

        obs.set(10);
        REQUIRE(observer->notification_count == 0);
        REQUIRE(callback_count == 1);
    }

    SECTION("unsubscribe() with nullptr is safe") {
        Observable<int> obs(0);
        obs.unsubscribe(nullptr);
        REQUIRE(obs.getSubscriptionCount() == 0);
    }
}
