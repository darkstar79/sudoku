# ITimeProvider Pattern: Time Abstraction for Testability

**Last Updated:** February 2026

## Overview

**CRITICAL**: All time-dependent code must use dependency-injected `ITimeProvider` interface to enable deterministic testing without timing-dependent flakiness.

The ITimeProvider pattern eliminates test flakiness by providing controllable, instant time advancement for tests while maintaining zero overhead in production code.

## Why Time Abstraction Matters

Traditional time-dependent tests are fragile and unreliable:

- `std::this_thread::sleep_for(50ms)` followed by `REQUIRE(elapsed >= 45ms)` can fail under system load
- CI/CD pipelines with high system load cause unpredictable timing variance
- 10-20% tolerance margins mask real bugs and make tests non-deterministic
- Tests run slowly (cumulative sleep time adds up)

The ITimeProvider pattern eliminates all these issues with controllable, instant time advancement.

## Architecture Overview

```cpp
// Interface abstraction provides two clocks:
class ITimeProvider {
    virtual std::chrono::system_clock::time_point systemNow() const = 0;  // Wall-clock time
    virtual std::chrono::steady_clock::time_point steadyNow() const = 0;  // Monotonic time
};

// Production implementation: zero overhead, inlines to direct std::chrono calls
class SystemTimeProvider : public ITimeProvider { /* delegates to std::chrono */ };

// Test implementation: controllable time advancement
class MockTimeProvider : public ITimeProvider {
    void advanceSystemTime(milliseconds duration);  // Move time forward/backward
    void advanceSteadyTime(milliseconds duration);
    // ... provides deterministic time for testing
};
```

## Usage in Production Code

All time-dependent classes accept `ITimeProvider` via constructor with default parameter for backward compatibility:

```cpp
// GameState with timer functionality
class GameState {
public:
    explicit GameState(
        std::shared_ptr<core::ITimeProvider> time_provider =
            std::make_shared<core::SystemTimeProvider>());

    void startTimer();
    void pauseTimer();
    std::chrono::milliseconds getElapsedTime() const;

private:
    std::shared_ptr<core::ITimeProvider> time_provider_;
    std::chrono::system_clock::time_point start_time_;
};

// Implementation uses injected time provider
std::chrono::milliseconds GameState::getElapsedTime() const {
    if (!is_timer_running_) return elapsed_time_;
    auto current = time_provider_->systemNow();  // Not std::chrono::system_clock::now()
    return elapsed_time_ +
           std::chrono::duration_cast<std::chrono::milliseconds>(current - start_time_);
}
```

## Components Using ITimeProvider

### 1. GameState ([src/model/game_state.h:27](../src/model/game_state.h#L27))

- Timer functionality: `startTimer()`, `pauseTimer()`, `getElapsedTime()`
- Uses `systemNow()` for wall-clock time

### 2. AutoSaveManager ([src/core/auto_save_manager.h:18](../src/core/auto_save_manager.h#L18))

- Periodic auto-save intervals
- Uses `steadyNow()` for monotonic time (immune to clock adjustments)

### 3. StatisticsManager ([src/core/statistics_manager.h:17](../src/core/statistics_manager.h#L17))

- Session tracking, completion times
- Uses `systemNow()` for timestamps

### 4. Solver spine — SudokuSolver / BacktrackingSolver / PuzzleRater (#24)

- `BacktrackingSolver` and `SudokuSolver` take an injected provider; deadline checks use
  `steadyNow()` so `MockTimeProvider` drives wall-clock budgets in tests (no real sleeps).
- `PuzzleRater` enforces a solve budget (default `kDefaultRatingBudget`, constructor-injectable) so a
  no-logical-path board cannot livelock generation-time rating (the AI Escargot class).
- `PuzzleGenerator` records the concrete RNG seed (incl. the `random_device` value chosen for
  seed-0) in `result.seed`, so every generated puzzle is reproducible.

### 5. SaveManager ([src/core/save_manager.h](../src/core/save_manager.h))

- Persisted wall-clock metadata (`last_modified`, `last_auto_save`, cleanup cutoff) and the
  corrupt-save `.corrupt-<ts>` archive suffix route through `systemNow()`.

### GameState default-provider decision (#24 H4)

`GameState` keeps its **default** `SystemTimeProvider` constructor argument. The game timer is
user-facing wall-clock time, outside the engine determinism contract, and `GameState` already routes
**all** time access through `time_provider_->systemNow()` (no raw `std::chrono::*::now()` calls).
Production constructs `GameState{}` and legitimately wants real time; timing-sensitive tests inject a
`MockTimeProvider`. Removing the default would force injection at every construction site for no
determinism gain, so it is retained by design.

## Usage in Tests

### ✅ CORRECT Pattern - Deterministic Time Control

```cpp
TEST_CASE("GameState timer tracks elapsed time", "[game_state]") {
    auto mock_time = std::make_shared<core::MockTimeProvider>();
    model::GameState state(mock_time);

    state.startTimer();
    mock_time->advanceSystemTime(std::chrono::milliseconds(50));

    auto elapsed = state.getElapsedTime();
    REQUIRE(elapsed.count() == 50);  // ✅ Exact assertion, no tolerance!
}

TEST_CASE("AutoSaveManager triggers save after interval", "[auto_save]") {
    auto mock_time = std::make_shared<core::MockTimeProvider>();
    auto mock_save_manager = std::make_shared<MockSaveManager>();
    core::AutoSaveManager auto_save(game_state, mock_save_manager, mock_time);

    auto_save.start(std::chrono::milliseconds(100));

    // Advance time just before interval
    mock_time->advanceSteadyTime(std::chrono::milliseconds(99));
    auto_save.update();
    REQUIRE_FALSE(mock_save_manager->save_called_);  // Not yet

    // Advance past interval
    mock_time->advanceSteadyTime(std::chrono::milliseconds(1));
    auto_save.update();
    REQUIRE(mock_save_manager->save_called_);  // ✅ Triggered!
}
```

### ❌ WRONG Pattern - Timing-Dependent Tests (DO NOT USE)

```cpp
TEST_CASE("GameState timer (FLAKY)", "[game_state]") {
    model::GameState state;  // Uses real time

    state.startTimer();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));  // ❌ System load affects timing

    auto elapsed = state.getElapsedTime();
    REQUIRE(elapsed.count() >= 45);  // ❌ 10% tolerance masks bugs, can still fail on slow CI
}
```

## Benefits of ITimeProvider Pattern

1. **Deterministic Testing**: Exact assertions (`== 50ms`) instead of fuzzy tolerances (`>= 45ms`)
2. **Zero Flakiness**: No dependence on system load, CPU scheduling, or clock resolution
3. **Instant Test Execution**: No actual sleep delays (tests run 10-100x faster)
4. **Edge Case Testing**: Easily test clock rollbacks, leap seconds, extreme time ranges
5. **Zero Production Overhead**: SystemTimeProvider inlines to direct `std::chrono` calls
6. **Backward Compatible**: Default parameters allow incremental adoption

## Performance Characteristics

- **SystemTimeProvider**: Zero overhead (inlined, optimizes to direct `std::chrono::now()` calls)
- **MockTimeProvider**: Only used in tests, performance irrelevant
- **No virtual dispatch overhead in Release builds**: Compiler devirtualizes when type is known

## Common Pitfalls

### ❌ WRONG: Directly calling std::chrono in time-dependent code

```cpp
class GameState {
    std::chrono::milliseconds getElapsedTime() const {
        auto now = std::chrono::system_clock::now();  // ❌ Not testable!
        // ...
    }
};
```

### ✅ CORRECT: Using injected time provider

```cpp
class GameState {
    std::chrono::milliseconds getElapsedTime() const {
        auto now = time_provider_->systemNow();  // ✅ Testable!
        // ...
    }
};
```

## When to Use systemNow() vs steadyNow()

### systemNow() - Wall-Clock Time

Can go backward (user adjusts clock, NTP sync)

**Use for:**
- Timestamps
- Session start/end times
- Save file metadata

**Example:** StatisticsManager session tracking

### steadyNow() - Monotonic Time

Never goes backward

**Use for:**
- Timers
- Intervals
- Elapsed time measurement
- Auto-save intervals

**Example:** AutoSaveManager periodic triggers, GameState timer (if pause/resume needs monotonic duration)

## Thread Safety

- ITimeProvider is **NOT thread-safe** (application is single-threaded by design)
- No mutex needed (no concurrent access possible in single-threaded architecture)

## Testing the Time Provider Itself

See [tests/unit/test_time_provider.cpp](../tests/unit/test_time_provider.cpp) for comprehensive tests validating:

- MockTimeProvider time advancement (forward and backward)
- Independence of system and steady clocks
- Reset functionality
- Polymorphic usage through interface

## Migration Guide

### Converting Existing Code

**Before:**
```cpp
class MyClass {
    void doSomething() {
        auto now = std::chrono::system_clock::now();  // Direct call
        // ...
    }
};
```

**After:**
```cpp
class MyClass {
public:
    explicit MyClass(std::shared_ptr<core::ITimeProvider> time_provider =
                     std::make_shared<core::SystemTimeProvider>())
        : time_provider_(time_provider) {}

    void doSomething() {
        auto now = time_provider_->systemNow();  // Injected call
        // ...
    }

private:
    std::shared_ptr<core::ITimeProvider> time_provider_;
};
```

### Writing Tests

```cpp
TEST_CASE("MyClass time-dependent behavior", "[my_class]") {
    auto mock_time = std::make_shared<core::MockTimeProvider>();
    MyClass obj(mock_time);

    // Control time precisely
    mock_time->advanceSystemTime(std::chrono::seconds(5));

    // Test with exact assertions
    REQUIRE(obj.getElapsedSeconds() == 5);
}
```

## Determinism Contract & CI Gate (#24)

The pattern is enforced, not just documented. [scripts/check_determinism.sh](../scripts/check_determinism.sh)
runs in CI (the **Determinism Gate** job, `needs: format-check`) and fails the build if any of these
**bypass calls** appear in `src/` outside the one legal bridge:

| Flagged call | Why it breaks determinism |
|--------------|---------------------------|
| `steady_clock::now()` / `system_clock::now()` | direct wall-clock read |
| `std::localtime` | non-reentrant (shared static buffer) |
| `random_device` | unseeded entropy |

The gate matches the **call**, not the type — `system_clock::time_point member_;` declarations are
fine. Two exemptions, both explicit (never a blanket directory exclude):

- `src/core/i_time_provider.h` — the single legal bridge (`SystemTimeProvider` / `MockTimeProvider`).
- A `// determinism-ok: <reason>` marker on the flagged line **or the line directly above it** — a
  documented, reviewed real-time exception (UI session timers, persisted wall-clock save metadata,
  save-id entropy, internal `SolutionCounter` timeout sampling). The above-line tolerance keeps the
  marker attached even when clang-format wraps a long statement.

Run it locally before pushing:

```bash
./scripts/check_determinism.sh
```

To add engine code that needs time or randomness, route it through `ITimeProvider`
(`steadyNow()` / `systemNow()`) or a seeded `std::mt19937` — do **not** reach for a raw marker unless
the value is genuinely real-time and outside the solver / generator / rater reproducibility surface.

## Additional Resources

- [TDD_GUIDE.md](TDD_GUIDE.md) - Testing best practices
- [src/core/i_time_provider.h](../src/core/i_time_provider.h) - Interface definition, plus both
  implementations: `SystemTimeProvider` (production) and `MockTimeProvider` (tests) live in this
  same header — there is no separate `system_time_provider.h` / `mock_time_provider.h`.
- [scripts/check_determinism.sh](../scripts/check_determinism.sh) - The CI determinism gate (below)
