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

#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

TEST_CASE("Observable - Basic Functionality", "[Observable]") {
    Observable<int> observable(10);

    SECTION("Initial value is set correctly") {
        REQUIRE(observable.get() == 10);
    }

    SECTION("Value can be updated") {
        observable.set(20);
        REQUIRE(observable.get() == 20);
    }

    SECTION("Observers are notified on value change") {
        int notification_count = 0;
        int last_value = 0;

        auto unsubscribe = observable.subscribe([&](const int& value) {
            notification_count++;
            last_value = value;
        });

        observable.set(30);

        REQUIRE(notification_count == 1);
        REQUIRE(last_value == 30);

        observable.set(40);

        REQUIRE(notification_count == 2);
        REQUIRE(last_value == 40);
    }

    SECTION("No notification when value doesn't change") {
        int notification_count = 0;

        auto unsubscribe = observable.subscribe([&]([[maybe_unused]] const int& value) { notification_count++; });

        observable.set(10);  // Same as initial value

        REQUIRE(notification_count == 0);
    }
}

TEST_CASE("Observable - Multiple Observers", "[Observable]") {
    Observable<std::string> observable("initial");

    SECTION("Multiple observers receive notifications") {
        std::vector<std::string> observer1_values;
        std::vector<std::string> observer2_values;

        auto unsubscribe1 = observable.subscribe([&](const std::string& value) { observer1_values.push_back(value); });

        auto unsubscribe2 = observable.subscribe([&](const std::string& value) { observer2_values.push_back(value); });

        observable.set("first");
        observable.set("second");

        REQUIRE(observer1_values.size() == 2);
        REQUIRE(observer2_values.size() == 2);
        REQUIRE(observer1_values[0] == "first");
        REQUIRE(observer1_values[1] == "second");
        REQUIRE(observer2_values[0] == "first");
        REQUIRE(observer2_values[1] == "second");
    }
}

TEST_CASE("Observable - Unsubscribe", "[Observable]") {
    Observable<int> observable(0);

    SECTION("Observer can unsubscribe") {
        int notification_count = 0;

        auto unsubscribe = observable.subscribe([&]([[maybe_unused]] const int& value) { notification_count++; });

        observable.set(1);
        REQUIRE(notification_count == 1);

        unsubscribe();  // Unsubscribe

        observable.set(2);
        REQUIRE(notification_count == 1);  // No additional notification
    }

    SECTION("One observer unsubscribes, others still receive notifications") {
        int observer1_count = 0;
        int observer2_count = 0;

        auto unsubscribe1 = observable.subscribe([&]([[maybe_unused]] const int& value) { observer1_count++; });

        auto unsubscribe2 = observable.subscribe([&]([[maybe_unused]] const int& value) { observer2_count++; });

        observable.set(1);
        REQUIRE(observer1_count == 1);
        REQUIRE(observer2_count == 1);

        unsubscribe1();  // Only first observer unsubscribes

        observable.set(2);
        REQUIRE(observer1_count == 1);  // No additional notification
        REQUIRE(observer2_count == 2);  // Still receiving notifications
    }
}

TEST_CASE("Observable - Complex Types", "[Observable]") {
    struct GameState {
        int score;
        std::string player;

        bool operator==(const GameState& other) const {
            return score == other.score && player == other.player;
        }

        bool operator!=(const GameState& other) const {
            return !(*this == other);
        }
    };

    Observable<GameState> observable({100, "Player1"});

    SECTION("Complex types work correctly") {
        REQUIRE(observable.get().score == 100);
        REQUIRE(observable.get().player == "Player1");

        int notification_count = 0;
        GameState last_state;

        auto unsubscribe = observable.subscribe([&](const GameState& state) {
            notification_count++;
            last_state = state;
        });

        observable.set({200, "Player2"});

        REQUIRE(notification_count == 1);
        REQUIRE(last_state.score == 200);
        REQUIRE(last_state.player == "Player2");

        // Set same value - no notification
        observable.set({200, "Player2"});
        REQUIRE(notification_count == 1);
    }
}

TEST_CASE("Observable - Thread Safety Considerations", "[Observable]") {
    // Note: This is a basic test. In a real multi-threaded environment,
    // more sophisticated testing would be needed
    Observable<int> observable(0);

    SECTION("Basic operations don't crash") {
        std::vector<std::function<void()>> unsubscribers;

        // Subscribe multiple observers
        for (int i = 0; i < 10; ++i) {
            auto unsubscribe = observable.subscribe([]([[maybe_unused]] const int& value) {
                // Do nothing
            });
            unsubscribers.push_back(unsubscribe);
        }

        // Update value multiple times
        for (int i = 0; i < 10; ++i) {
            observable.set(i);
        }

        // Unsubscribe all
        for (auto& unsubscribe : unsubscribers) {
            unsubscribe();
        }

        // Should not crash
        observable.set(100);
    }
}

TEST_CASE("Observable - clearSubscriptions Edge Cases", "[Observable]") {
    Observable<int> observable(42);

    SECTION("clearSubscriptions called inside observer callback") {
        int notification_count = 0;
        bool clear_called = false;

        observable.subscribe([&](const int& value) {
            notification_count++;
            if (value == 100 && !clear_called) {
                clear_called = true;
                observable.clearSubscriptions();  // Clear during notification
            }
        });

        observable.set(100);

        // First notification should complete
        REQUIRE(notification_count == 1);
        REQUIRE(clear_called);

        // Future notifications should not trigger (no subscribers)
        observable.set(200);
        REQUIRE(notification_count == 1);  // No additional notifications
    }

    SECTION("clearSubscriptions removes all observers") {
        int observer1_count = 0;
        int observer2_count = 0;

        observable.subscribe([&]([[maybe_unused]] const int& value) { observer1_count++; });

        observable.subscribe([&]([[maybe_unused]] const int& value) { observer2_count++; });

        // First notification - both observers active
        observable.set(50);
        REQUIRE(observer1_count == 1);
        REQUIRE(observer2_count == 1);

        // Clear all subscriptions
        observable.clearSubscriptions();
        REQUIRE(observable.getSubscriptionCount() == 0);

        // Future notifications should not trigger (no subscribers)
        observable.set(75);
        REQUIRE(observer1_count == 1);  // No additional notifications
        REQUIRE(observer2_count == 1);  // No additional notifications
    }

    SECTION("clearSubscriptions when no subscribers exist") {
        observable.clearSubscriptions();  // Should not crash

        // Can still subscribe after clearing empty list
        int notification_count = 0;
        observable.subscribe([&]([[maybe_unused]] const int& value) { notification_count++; });

        observable.set(200);
        REQUIRE(notification_count == 1);
    }
}

TEST_CASE("Observable - Subscription During Notification", "[Observable]") {
    Observable<int> observable(0);

    SECTION("New observer added during notification") {
        int observer1_count = 0;
        int observer2_count = 0;
        std::function<void()> unsubscribe2;

        observable.subscribe([&](const int& value) {
            observer1_count++;
            if (value == 10) {
                // Add observer2 during observer1's notification
                unsubscribe2 = observable.subscribe([&]([[maybe_unused]] const int& v) { observer2_count++; });
            }
        });

        observable.set(10);

        // Observer1 should get notification
        REQUIRE(observer1_count == 1);
        // Observer2 should NOT get current notification (added after iteration started)
        REQUIRE(observer2_count == 0);

        // Observer2 should get next notification
        observable.set(20);
        REQUIRE(observer1_count == 2);
        REQUIRE(observer2_count == 1);
    }

    SECTION("Observer unsubscribes itself during notification") {
        int notification_count = 0;
        std::function<void()> unsubscribe;

        unsubscribe = observable.subscribe([&](const int& value) {
            notification_count++;
            if (value == 5) {
                unsubscribe();  // Unsubscribe self during notification
            }
        });

        observable.set(5);
        REQUIRE(notification_count == 1);

        // Should not receive future notifications
        observable.set(10);
        REQUIRE(notification_count == 1);
    }
}

TEST_CASE("Observable - getSubscriptionCount", "[Observable]") {
    Observable<int> observable(0);

    SECTION("Reports correct count") {
        REQUIRE(observable.getSubscriptionCount() == 0);

        auto unsub1 = observable.subscribe([]([[maybe_unused]] const int& v) {});
        REQUIRE(observable.getSubscriptionCount() == 1);

        auto unsub2 = observable.subscribe([]([[maybe_unused]] const int& v) {});
        REQUIRE(observable.getSubscriptionCount() == 2);

        unsub1();
        REQUIRE(observable.getSubscriptionCount() == 1);

        unsub2();
        REQUIRE(observable.getSubscriptionCount() == 0);
    }
}

TEST_CASE("Observable - Assignment Operator", "[Observable]") {
    Observable<int> observable(10);

    SECTION("Assignment triggers notification") {
        int notification_count = 0;
        int last_value = 0;

        observable.subscribe([&](const int& value) {
            notification_count++;
            last_value = value;
        });

        observable = 50;  // Assignment operator

        REQUIRE(notification_count == 1);
        REQUIRE(last_value == 50);
        REQUIRE(observable.get() == 50);
    }

    SECTION("Assignment with same value does not notify") {
        int notification_count = 0;

        observable.subscribe([&]([[maybe_unused]] const int& value) { notification_count++; });

        observable = 10;  // Same as initial value

        REQUIRE(notification_count == 0);
    }
}

TEST_CASE("Observable - makeObservable Helper", "[Observable]") {
    SECTION("Creates observable with default value") {
        auto obs = makeObservable<int>();
        REQUIRE(obs.get() == 0);
    }

    SECTION("Creates observable with custom value") {
        auto obs = makeObservable<std::string>("Hello");
        REQUIRE(obs.get() == "Hello");
    }

    SECTION("Works with complex types") {
        struct Point {
            int x{0};
            int y{0};
            bool operator==(const Point& other) const = default;
        };

        auto obs = makeObservable<Point>({5, 10});
        REQUIRE(obs.get().x == 5);
        REQUIRE(obs.get().y == 10);
    }
}