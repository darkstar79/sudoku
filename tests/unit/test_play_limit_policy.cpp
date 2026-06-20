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

#include "../../src/core/play_limit_policy.h"

#include <chrono>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using namespace std::chrono_literals;

namespace {

[[nodiscard]] std::chrono::milliseconds minutes(int m) {
    return std::chrono::minutes{m};
}

[[nodiscard]] Settings sessionOnly(int limit_minutes, int lead) {
    Settings s;
    s.enable_session_limit = true;
    s.max_session_minutes = limit_minutes;
    s.warn_before_minutes = lead;
    return s;
}

[[nodiscard]] Settings dailyOnly(int limit_minutes, int lead) {
    Settings s;
    s.enable_daily_limit = true;
    s.max_daily_minutes = limit_minutes;
    s.warn_before_minutes = lead;
    return s;
}

}  // namespace

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("evaluate - default settings never warn or close", "[playlimit][policy]") {
    Settings s;  // all limits off by default

    CHECK(evaluate(s, 0ms, 0ms).action == PlayLimitAction::None);
    CHECK(evaluate(s, minutes(9999), minutes(9999)).action == PlayLimitAction::None);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("evaluate - enabled limit with 0 minutes is treated as unlimited", "[playlimit][policy]") {
    Settings s = sessionOnly(0, 5);  // enabled but 0 minutes
    s.enable_daily_limit = true;
    s.max_daily_minutes = 0;

    CHECK(evaluate(s, minutes(500), minutes(500)).action == PlayLimitAction::None);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("evaluate - session Close fires at exactly the limit even with daily budget left", "[playlimit][policy]") {
    Settings s = sessionOnly(30, 5);
    s.enable_daily_limit = true;
    s.max_daily_minutes = 120;  // plenty of daily budget remaining

    // Just before the session limit: still warning, not closing.
    CHECK(evaluate(s, minutes(10), minutes(29)).action == PlayLimitAction::Warn);

    // Exactly at the session limit: Close(Session).
    const auto at_limit = evaluate(s, minutes(10), minutes(30));
    CHECK(at_limit.action == PlayLimitAction::Close);
    CHECK(at_limit.limit == LimitKind::Session);

    // Past the limit: still Close.
    CHECK(evaluate(s, minutes(10), minutes(45)).action == PlayLimitAction::Close);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("evaluate - daily Close fires when the daily total reaches the limit", "[playlimit][policy]") {
    Settings s = dailyOnly(60, 5);

    CHECK(evaluate(s, minutes(59), 0ms).action == PlayLimitAction::Warn);
    const auto at_limit = evaluate(s, minutes(60), 0ms);
    CHECK(at_limit.action == PlayLimitAction::Close);
    CHECK(at_limit.limit == LimitKind::Daily);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("evaluate - tie at zero remaining binds to Session", "[playlimit][policy]") {
    Settings s = sessionOnly(30, 5);
    s.enable_daily_limit = true;
    s.max_daily_minutes = 30;

    // session active == 30 (remaining 0) AND daily total == 30 (remaining 0): tie -> Session.
    const auto d = evaluate(s, minutes(30), minutes(30));
    CHECK(d.action == PlayLimitAction::Close);
    CHECK(d.limit == LimitKind::Session);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("evaluate - warn lead clamp boundary table", "[playlimit][policy]") {
    // Session limit of 10 minutes; vary the lead. At session time 0 (remaining == limit),
    // and at session time 1 min (remaining == limit - 1).
    SECTION("lead == limit -> warns immediately (remaining == limit)") {
        Settings s = sessionOnly(10, 10);
        CHECK(evaluate(s, 0ms, 0ms).action == PlayLimitAction::Warn);
    }
    SECTION("lead == limit - 1 -> no warn at start, warns once 1 min in") {
        Settings s = sessionOnly(10, 9);
        CHECK(evaluate(s, 0ms, 0ms).action == PlayLimitAction::None);
        CHECK(evaluate(s, 0ms, minutes(1)).action == PlayLimitAction::Warn);
    }
    SECTION("lead == 0 -> never warns, only closes") {
        Settings s = sessionOnly(10, 0);
        CHECK(evaluate(s, 0ms, minutes(5)).action == PlayLimitAction::None);
        CHECK(evaluate(s, 0ms, minutes(9)).action == PlayLimitAction::None);
        CHECK(evaluate(s, 0ms, minutes(10)).action == PlayLimitAction::Close);
    }
    SECTION("lead > limit -> clamped to limit, warns immediately") {
        Settings s = sessionOnly(10, 99);
        CHECK(evaluate(s, 0ms, 0ms).action == PlayLimitAction::Warn);
    }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("evaluate - warn minutes_left is rounded up", "[playlimit][policy]") {
    Settings s = sessionOnly(10, 5);
    // 4 min 30 s of remaining (limit 10, played 5 min 30 s) -> "about 5 minutes".
    const auto d = evaluate(s, 0ms, std::chrono::milliseconds{5 * 60'000 + 30'000});
    CHECK(d.action == PlayLimitAction::Warn);
    CHECK(d.minutes_left == 5);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("evaluate - lowering a limit below played time closes on the next tick", "[playlimit][policy]") {
    // Was warning at 25 min played with a 30 min limit...
    CHECK(evaluate(sessionOnly(30, 5), 0ms, minutes(25)).action == PlayLimitAction::Warn);
    // ...user lowers the limit to 20 min: already over -> Close.
    CHECK(evaluate(sessionOnly(20, 5), 0ms, minutes(25)).action == PlayLimitAction::Close);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("evaluate - disabling a limit cancels its pending warn", "[playlimit][policy]") {
    Settings warning = sessionOnly(30, 5);
    CHECK(evaluate(warning, 0ms, minutes(28)).action == PlayLimitAction::Warn);

    Settings disabled = warning;
    disabled.enable_session_limit = false;
    CHECK(evaluate(disabled, 0ms, minutes(28)).action == PlayLimitAction::None);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("decide - allows a fresh launch when nothing is exhausted", "[playlimit][policy]") {
    Settings s = dailyOnly(60, 5);
    CHECK(decide(s, minutes(0), 0ms).action == LaunchAction::Allow);
    CHECK(decide(s, minutes(59), 0ms).action == LaunchAction::Allow);  // D - epsilon allows
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("decide - blocks when daily is exhausted (boundary is >= D)", "[playlimit][policy]") {
    Settings s = dailyOnly(60, 5);
    CHECK(decide(s, minutes(60), 0ms).action == LaunchAction::BlockedDailyExhausted);
    CHECK(decide(s, minutes(120), 0ms).action == LaunchAction::BlockedDailyExhausted);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("decide - blocks while in cooldown then allows after it clears", "[playlimit][policy]") {
    Settings s = sessionOnly(30, 5);
    s.session_cooldown_minutes = 15;

    const auto blocked = decide(s, 0ms, minutes(10));
    CHECK(blocked.action == LaunchAction::BlockedCooldown);
    CHECK(blocked.minutes_remaining == 10);

    // <1 minute remaining rounds up to 1.
    CHECK(decide(s, 0ms, 30s).minutes_remaining == 1);

    CHECK(decide(s, 0ms, 0ms).action == LaunchAction::Allow);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("decide - daily exhaustion wins over an active cooldown", "[playlimit][policy]") {
    Settings s = sessionOnly(30, 5);
    s.session_cooldown_minutes = 15;
    s.enable_daily_limit = true;
    s.max_daily_minutes = 60;

    CHECK(decide(s, minutes(60), minutes(10)).action == LaunchAction::BlockedDailyExhausted);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("decide - cooldown is ignored when the session limit is disabled", "[playlimit][policy]") {
    Settings s;  // session limit off
    s.session_cooldown_minutes = 15;
    CHECK(decide(s, 0ms, minutes(10)).action == LaunchAction::Allow);
}
