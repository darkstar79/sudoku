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

#include "core/i_time_provider.h"

#include <chrono>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

TEST_CASE("SystemTimeProvider - Delegates to std::chrono", "[time_provider]") {
    SystemTimeProvider provider;

    SECTION("systemNow returns current system time") {
        auto before = std::chrono::system_clock::now();
        auto provider_time = provider.systemNow();
        auto after = std::chrono::system_clock::now();

        // Provider time should be between before and after
        REQUIRE(provider_time >= before);
        REQUIRE(provider_time <= after);
    }

    SECTION("steadyNow returns current steady time") {
        auto before = std::chrono::steady_clock::now();
        auto provider_time = provider.steadyNow();
        auto after = std::chrono::steady_clock::now();

        // Provider time should be between before and after
        REQUIRE(provider_time >= before);
        REQUIRE(provider_time <= after);
    }

    SECTION("Multiple calls to systemNow return monotonically increasing values") {
        // SystemTimeProvider delegates to std::chrono, so multiple calls
        // will return different values due to CPU instruction timing.
        // We just verify they're ordered correctly (not necessarily different,
        // but second >= first is guaranteed by clock monotonicity)
        auto time1 = provider.systemNow();
        auto time2 = provider.systemNow();
        auto time3 = provider.systemNow();

        // Times should be non-decreasing (allowing for same value if calls are fast)
        REQUIRE(time2 >= time1);
        REQUIRE(time3 >= time2);
    }

    SECTION("Multiple calls to steadyNow return monotonically increasing values") {
        // steady_clock is guaranteed to be monotonic - time never goes backward
        auto time1 = provider.steadyNow();
        auto time2 = provider.steadyNow();
        auto time3 = provider.steadyNow();

        // Times should be non-decreasing
        REQUIRE(time2 >= time1);
        REQUIRE(time3 >= time2);
    }
}

TEST_CASE("MockTimeProvider - Controllable time advancement", "[time_provider]") {
    SECTION("Default constructor initializes to current time") {
        auto real_system_before = std::chrono::system_clock::now();
        auto real_steady_before = std::chrono::steady_clock::now();

        MockTimeProvider provider;

        auto mock_system = provider.systemNow();
        auto mock_steady = provider.steadyNow();

        auto real_system_after = std::chrono::system_clock::now();
        auto real_steady_after = std::chrono::steady_clock::now();

        // Mock times should be initialized close to real time
        REQUIRE(mock_system >= real_system_before);
        REQUIRE(mock_system <= real_system_after);
        REQUIRE(mock_steady >= real_steady_before);
        REQUIRE(mock_steady <= real_steady_after);
    }

    SECTION("Explicit constructor sets initial times") {
        auto system_start = std::chrono::system_clock::time_point(std::chrono::hours(100));
        auto steady_start = std::chrono::steady_clock::time_point(std::chrono::hours(200));

        MockTimeProvider provider(system_start, steady_start);

        REQUIRE(provider.systemNow() == system_start);
        REQUIRE(provider.steadyNow() == steady_start);
    }

    SECTION("advanceSystemTime moves system clock forward") {
        MockTimeProvider provider;
        auto initial = provider.systemNow();

        provider.advanceSystemTime(std::chrono::milliseconds(50));

        auto after_advance = provider.systemNow();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(after_advance - initial);

        REQUIRE(duration.count() == 50);
    }

    SECTION("advanceSteadyTime moves steady clock forward") {
        MockTimeProvider provider;
        auto initial = provider.steadyNow();

        provider.advanceSteadyTime(std::chrono::milliseconds(100));

        auto after_advance = provider.steadyNow();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(after_advance - initial);

        REQUIRE(duration.count() == 100);
    }

    SECTION("System and steady clocks advance independently") {
        MockTimeProvider provider;
        auto system_initial = provider.systemNow();
        auto steady_initial = provider.steadyNow();

        provider.advanceSystemTime(std::chrono::milliseconds(50));
        provider.advanceSteadyTime(std::chrono::milliseconds(100));

        auto system_after = provider.systemNow();
        auto steady_after = provider.steadyNow();

        auto system_duration = std::chrono::duration_cast<std::chrono::milliseconds>(system_after - system_initial);
        auto steady_duration = std::chrono::duration_cast<std::chrono::milliseconds>(steady_after - steady_initial);

        REQUIRE(system_duration.count() == 50);
        REQUIRE(steady_duration.count() == 100);
    }

    SECTION("advanceSystemTime can move time backward") {
        MockTimeProvider provider;
        auto initial = provider.systemNow();

        provider.advanceSystemTime(std::chrono::milliseconds(-30));

        auto after_advance = provider.systemNow();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(after_advance - initial);

        REQUIRE(duration.count() == -30);
        REQUIRE(after_advance < initial);
    }

    SECTION("Multiple advances accumulate") {
        MockTimeProvider provider;
        auto initial = provider.systemNow();

        provider.advanceSystemTime(std::chrono::milliseconds(10));
        provider.advanceSystemTime(std::chrono::milliseconds(20));
        provider.advanceSystemTime(std::chrono::milliseconds(30));

        auto final_time = provider.systemNow();
        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(final_time - initial);

        REQUIRE(total_duration.count() == 60);
    }

    SECTION("setSystemTime replaces current time") {
        MockTimeProvider provider;
        auto new_time = std::chrono::system_clock::time_point(std::chrono::hours(500));

        provider.setSystemTime(new_time);

        REQUIRE(provider.systemNow() == new_time);
    }

    SECTION("setSteadyTime replaces current time") {
        MockTimeProvider provider;
        auto new_time = std::chrono::steady_clock::time_point(std::chrono::hours(500));

        provider.setSteadyTime(new_time);

        REQUIRE(provider.steadyNow() == new_time);
    }

    SECTION("reset() reinitializes to current real time") {
        MockTimeProvider provider;

        // Advance time significantly
        provider.advanceSystemTime(std::chrono::hours(1000));
        provider.advanceSteadyTime(std::chrono::hours(2000));

        auto real_system_before = std::chrono::system_clock::now();
        auto real_steady_before = std::chrono::steady_clock::now();

        provider.reset();

        auto mock_system = provider.systemNow();
        auto mock_steady = provider.steadyNow();

        auto real_system_after = std::chrono::system_clock::now();
        auto real_steady_after = std::chrono::steady_clock::now();

        // After reset, mock times should be close to real time again
        REQUIRE(mock_system >= real_system_before);
        REQUIRE(mock_system <= real_system_after);
        REQUIRE(mock_steady >= real_steady_before);
        REQUIRE(mock_steady <= real_steady_after);
    }

    SECTION("Time does not advance on its own") {
        MockTimeProvider provider;
        auto system_initial = provider.systemNow();
        auto steady_initial = provider.steadyNow();

        // Call multiple times - mock time should never change without explicit advance
        for (int i = 0; i < 100; ++i) {
            REQUIRE(provider.systemNow() == system_initial);
            REQUIRE(provider.steadyNow() == steady_initial);
        }

        // Even after many calls, time remains frozen until explicitly advanced
        REQUIRE(provider.systemNow() == system_initial);
        REQUIRE(provider.steadyNow() == steady_initial);
    }

    SECTION("Large time advances work correctly") {
        MockTimeProvider provider;
        auto initial = provider.systemNow();

        // Advance by 24 hours
        provider.advanceSystemTime(std::chrono::hours(24));

        auto after_advance = provider.systemNow();
        auto duration = std::chrono::duration_cast<std::chrono::hours>(after_advance - initial);

        REQUIRE(duration.count() == 24);
    }

    SECTION("Precise millisecond control") {
        MockTimeProvider provider;
        auto initial = provider.steadyNow();

        provider.advanceSteadyTime(std::chrono::milliseconds(1));

        auto after_advance = provider.steadyNow();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(after_advance - initial);

        REQUIRE(duration.count() == 1);
    }
}

TEST_CASE("ITimeProvider - Polymorphic usage", "[time_provider]") {
    SECTION("Can use SystemTimeProvider through interface") {
        std::shared_ptr<ITimeProvider> provider = std::make_shared<SystemTimeProvider>();

        auto time = provider->systemNow();
        REQUIRE(time.time_since_epoch().count() > 0);
    }

    SECTION("Can use MockTimeProvider through interface") {
        std::shared_ptr<ITimeProvider> provider = std::make_shared<MockTimeProvider>();

        auto initial = provider->systemNow();

        // Downcast to access mock-specific methods
        auto mock_provider = std::dynamic_pointer_cast<MockTimeProvider>(provider);
        REQUIRE(mock_provider != nullptr);

        mock_provider->advanceSystemTime(std::chrono::milliseconds(100));

        auto after = provider->systemNow();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(after - initial);

        REQUIRE(duration.count() == 100);
    }

    SECTION("Can swap implementations at runtime") {
        std::shared_ptr<ITimeProvider> provider = std::make_shared<SystemTimeProvider>();

        auto real_time = provider->systemNow();
        REQUIRE(real_time.time_since_epoch().count() > 0);

        // Swap to mock
        provider = std::make_shared<MockTimeProvider>();
        auto mock_time = provider->systemNow();
        REQUIRE(mock_time.time_since_epoch().count() > 0);
    }
}
