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

#pragma once

#include <chrono>
#include <memory>

namespace sudoku::core {

/**
 * @brief Interface for time providers to enable testable time-dependent code.
 *
 * This interface abstracts time-related functionality to allow dependency injection
 * of mock time providers in tests, eliminating timing-dependent test flakiness.
 *
 * Production code uses SystemTimeProvider (zero overhead, delegates to std::chrono).
 * Test code uses MockTimeProvider (controllable time advancement).
 */
class ITimeProvider {
public:
    virtual ~ITimeProvider() = default;

    /**
     * @brief Get current system clock time point.
     * @return Current time according to system_clock.
     */
    [[nodiscard]] virtual std::chrono::system_clock::time_point systemNow() const = 0;

    /**
     * @brief Get current steady clock time point.
     * @return Current time according to steady_clock (monotonic).
     */
    [[nodiscard]] virtual std::chrono::steady_clock::time_point steadyNow() const = 0;

protected:
    // Protected special member functions to prevent slicing while allowing derived classes
    ITimeProvider() = default;
    ITimeProvider(const ITimeProvider&) = default;
    ITimeProvider& operator=(const ITimeProvider&) = default;
    ITimeProvider(ITimeProvider&&) = default;
    ITimeProvider& operator=(ITimeProvider&&) = default;
};

/**
 * @brief Production implementation that delegates to std::chrono.
 *
 * This is a zero-overhead abstraction - all methods are inline and will be
 * optimized away by the compiler in release builds.
 */
class SystemTimeProvider final : public ITimeProvider {
public:
    SystemTimeProvider() = default;
    ~SystemTimeProvider() override = default;

    // Prevent copy/move
    SystemTimeProvider(const SystemTimeProvider&) = delete;
    SystemTimeProvider& operator=(const SystemTimeProvider&) = delete;
    SystemTimeProvider(SystemTimeProvider&&) = delete;
    SystemTimeProvider& operator=(SystemTimeProvider&&) = delete;

    [[nodiscard]] std::chrono::system_clock::time_point systemNow() const override {
        return std::chrono::system_clock::now();
    }

    [[nodiscard]] std::chrono::steady_clock::time_point steadyNow() const override {
        return std::chrono::steady_clock::now();
    }
};

/**
 * @brief Test implementation with controllable time advancement.
 *
 * Allows tests to precisely control time without sleep() calls, making tests:
 * - Deterministic (no timing variance)
 * - Fast (no actual waiting)
 * - Reliable (no CI/CD flakiness from system load)
 *
 * Example usage:
 * @code
 * auto mock_time = std::make_shared<MockTimeProvider>();
 * GameState state(mock_time);
 * state.startTimer();
 * mock_time->advanceSystemTime(std::chrono::milliseconds(50));
 * REQUIRE(state.getElapsedTime().count() == 50);  // Exact assertion!
 * @endcode
 */
class MockTimeProvider final : public ITimeProvider {
public:
    /**
     * @brief Construct with current real time as starting point.
     */
    MockTimeProvider()
        : fake_system_time_(std::chrono::system_clock::now()), fake_steady_time_(std::chrono::steady_clock::now()) {
    }

    /**
     * @brief Construct with explicit starting times.
     * @param system_start Initial system clock time point
     * @param steady_start Initial steady clock time point
     */
    MockTimeProvider(std::chrono::system_clock::time_point system_start,
                     std::chrono::steady_clock::time_point steady_start)
        : fake_system_time_(system_start), fake_steady_time_(steady_start) {
    }

    ~MockTimeProvider() override = default;

    // Prevent copy/move
    MockTimeProvider(const MockTimeProvider&) = delete;
    MockTimeProvider& operator=(const MockTimeProvider&) = delete;
    MockTimeProvider(MockTimeProvider&&) = delete;
    MockTimeProvider& operator=(MockTimeProvider&&) = delete;

    /**
     * @brief Advance system clock time by specified duration.
     * @param duration Time to advance (can be negative to go backward)
     */
    void advanceSystemTime(std::chrono::milliseconds duration) {
        fake_system_time_ += duration;
    }

    /**
     * @brief Advance steady clock time by specified duration.
     * @param duration Time to advance (typically positive, steady clock is monotonic)
     */
    void advanceSteadyTime(std::chrono::milliseconds duration) {
        fake_steady_time_ += duration;
    }

    /**
     * @brief Set system clock to specific time point.
     * @param time_point New system clock time
     */
    void setSystemTime(std::chrono::system_clock::time_point time_point) {
        fake_system_time_ = time_point;
    }

    /**
     * @brief Set steady clock to specific time point.
     * @param time_point New steady clock time
     */
    void setSteadyTime(std::chrono::steady_clock::time_point time_point) {
        fake_steady_time_ = time_point;
    }

    /**
     * @brief Reset both clocks to current real time.
     */
    void reset() {
        fake_system_time_ = std::chrono::system_clock::now();
        fake_steady_time_ = std::chrono::steady_clock::now();
    }

    [[nodiscard]] std::chrono::system_clock::time_point systemNow() const override {
        return fake_system_time_;
    }

    [[nodiscard]] std::chrono::steady_clock::time_point steadyNow() const override {
        return fake_steady_time_;
    }

private:
    std::chrono::system_clock::time_point fake_system_time_;
    std::chrono::steady_clock::time_point fake_steady_time_;
};

}  // namespace sudoku::core
