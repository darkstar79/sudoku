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

#include "../../src/core/i_time_provider.h"
#include "../../src/core/play_time_ledger.h"
#include "../../src/core/settings_manager.h"
#include "../../src/view_model/play_limit_controller.h"
#include "../helpers/test_utils.h"

#include <chrono>
#include <memory>
#include <tuple>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using namespace sudoku::viewmodel;
using namespace std::chrono_literals;

namespace {

struct Harness {
    sudoku::test::TempTestDir tmp;
    std::shared_ptr<SettingsManager> settings;
    std::shared_ptr<MockTimeProvider> clock;
    std::shared_ptr<PlayTimeLedger> ledger;
    std::unique_ptr<PlayLimitController> controller;

    Harness()
        : settings(std::make_shared<SettingsManager>(tmp.path() / "settings.yaml")),
          clock(std::make_shared<MockTimeProvider>(std::chrono::sys_days{std::chrono::year{2025} / 1 / 1} + 8h,
                                                   std::chrono::steady_clock::now())),
          ledger(std::make_shared<PlayTimeLedger>(tmp.path() / "play_time.yaml", clock)),
          controller(std::make_unique<PlayLimitController>(settings, ledger)) {
    }
};

}  // namespace

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("PlayLimitController - default settings never warn or close", "[playlimit][controller]") {
    Harness h;

    for (int sec = 0; sec <= 600; sec += 60) {
        const auto res = h.controller->onTick(std::chrono::seconds{sec});
        CHECK(res.event == PlayLimitController::Event::None);
    }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("PlayLimitController - session limit warns once then closes and starts cooldown", "[playlimit][controller]") {
    Harness h;
    h.settings->setEnableSessionLimit(true);
    h.settings->setMaxSessionMinutes(2);
    h.settings->setWarnBeforeMinutes(1);

    CHECK(h.controller->onTick(0s).event == PlayLimitController::Event::None);

    const auto warn = h.controller->onTick(60s);
    CHECK(warn.event == PlayLimitController::Event::Warn);
    CHECK(warn.limit == LimitKind::Session);
    CHECK(warn.minutes_left == 1);

    // Still inside the warn window — must NOT warn again (once per session).
    CHECK(h.controller->onTick(90s).event == PlayLimitController::Event::None);

    const auto close = h.controller->onTick(120s);
    CHECK(close.event == PlayLimitController::Event::Close);
    CHECK(close.limit == LimitKind::Session);

    // AC#7: session-end recorded before close so the cooldown begins.
    CHECK(h.ledger->lastSessionEnd().has_value());
    CHECK(h.ledger->cooldownRemaining(15) > 0ms);

    // Latched closed: further ticks emit nothing.
    CHECK(h.controller->onTick(180s).event == PlayLimitController::Event::None);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("PlayLimitController - a daily-only close does not arm a session cooldown", "[playlimit][controller]") {
    // A daily-limit close must not stamp session-end: otherwise enabling a session limit later would
    // surface a phantom BlockedCooldown derived from a session that never had one.
    Harness h;
    h.settings->setEnableDailyLimit(true);
    h.settings->setMaxDailyMinutes(2);  // session limit stays OFF

    std::ignore = h.controller->onTick(0s);
    const auto close = h.controller->onTick(120s);
    REQUIRE(close.event == PlayLimitController::Event::Close);
    REQUIRE(close.limit == LimitKind::Daily);

    CHECK(!h.ledger->lastSessionEnd().has_value());  // no cooldown armed
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("PlayLimitController - disabling a limit cancels the pending warn; re-enabling re-arms",
          "[playlimit][controller]") {
    Harness h;
    h.settings->setEnableSessionLimit(true);
    h.settings->setMaxSessionMinutes(10);
    h.settings->setWarnBeforeMinutes(5);

    // Enter the warn window and fire once.
    std::ignore = h.controller->onTick(0s);
    CHECK(h.controller->onTick(6min).event == PlayLimitController::Event::Warn);
    CHECK(h.controller->onTick(7min).event == PlayLimitController::Event::None);  // suppressed

    // Disable mid-session: the pending warn is cancelled (None re-arms the latch).
    h.settings->setEnableSessionLimit(false);
    CHECK(h.controller->onTick(8min).event == PlayLimitController::Event::None);

    // Re-enable: the warning re-arms and fires once more.
    h.settings->setEnableSessionLimit(true);
    CHECK(h.controller->onTick(9min).event == PlayLimitController::Event::Warn);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("PlayLimitController - active play accrues across puzzles; a timer reset does not subtract",
          "[playlimit][controller]") {
    Harness h;

    std::ignore = h.controller->onTick(60s);
    std::ignore = h.controller->onTick(120s);  // puzzle 1: 2 min
    std::ignore = h.controller->onTick(0s);    // new puzzle: timer reset to 0
    std::ignore = h.controller->onTick(60s);   // puzzle 2: 1 min

    CHECK(h.controller->sessionActive() == 3min);
    CHECK(h.controller->liveDailyTotal() == 3min);  // not yet flushed, but live total is correct
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("PlayLimitController - only active play accrues; a stopped timer keeps totals flat",
          "[playlimit][controller]") {
    Harness h;

    std::ignore = h.controller->onTick(120s);
    const auto session_before = h.controller->sessionActive();
    const auto daily_before = h.controller->liveDailyTotal();

    // Timer stopped (e.g. puzzle complete): elapsed stays constant while wall-clock advances.
    h.clock->advanceSystemTime(30min);
    std::ignore = h.controller->onTick(120s);
    std::ignore = h.controller->onTick(120s);

    CHECK(h.controller->sessionActive() == session_before);
    CHECK(h.controller->liveDailyTotal() == daily_before);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("PlayLimitController - flush persists accrual; crash loses at most the unflushed tail",
          "[playlimit][controller]") {
    Harness h;

    std::ignore = h.controller->onTick(60s);
    std::ignore = h.controller->onTick(120s);
    h.controller->flush();  // auto-save cadence / transition flush

    // Ledger now holds the flushed daily total; a fresh ledger (restart) sees it.
    PlayTimeLedger reloaded(h.tmp.path() / "play_time.yaml", h.clock);
    CHECK(reloaded.accruedToday() == 2min);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("PlayLimitController - mid-session day rollover keeps pre-midnight play on the old day",
          "[playlimit][controller]") {
    // AC#4 / spec §2: time earned before midnight stays attributed to its day, even though the flush
    // that persists it may run after midnight. So the new day's live total must NOT include it.
    sudoku::test::TempTestDir tmp;
    auto settings = std::make_shared<SettingsManager>(tmp.path() / "settings.yaml");
    auto clock = std::make_shared<MockTimeProvider>(
        std::chrono::sys_days{std::chrono::year{2025} / 1 / 1} + 23h + 30min, std::chrono::steady_clock::now());
    auto ledger = std::make_shared<PlayTimeLedger>(tmp.path() / "play_time.yaml", clock);
    PlayLimitController controller(settings, ledger);

    std::ignore = controller.onTick(0s);
    std::ignore = controller.onTick(60s);  // 1 min of pre-midnight play, still unflushed

    clock->advanceSystemTime(40min);        // cross UTC midnight (now 00:10 the next day)
    std::ignore = controller.onTick(120s);  // +1 min of new-day play; pre-midnight min flushed to old day

    // Only the new-day minute counts toward today; the pre-midnight minute did not roll forward.
    CHECK(controller.liveDailyTotal() == 1min);
    CHECK(ledger->accruedToday() == 0ms);  // old day's accrual is not today's
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("PlayLimitController - periodic auto-flush persists on the auto-save cadence", "[playlimit][controller]") {
    Harness h;
    h.settings->setAutoSaveInterval(10'000);  // flush every 10 ticks (clamped minimum)

    // 10 one-second ticks of steady accrual: the 10th crosses the cadence and auto-flushes.
    for (int sec = 1; sec <= 10; ++sec) {
        std::ignore = h.controller->onTick(std::chrono::seconds{sec});
    }

    // A fresh ledger (simulating a crash with no explicit flush) sees the auto-flushed accrual.
    PlayTimeLedger reloaded(h.tmp.path() / "play_time.yaml", h.clock);
    CHECK(reloaded.accruedToday() == std::chrono::seconds{10});
}
