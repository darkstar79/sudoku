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

#include "../../src/core/encryption_manager.h"
#include "../../src/core/i_time_provider.h"
#include "../../src/core/play_time_ledger.h"
#include "../helpers/test_utils.h"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using namespace std::chrono_literals;

namespace {

// A fixed instant at a UTC midnight, so day-rollover boundaries are exact under the mock clock.
[[nodiscard]] std::chrono::system_clock::time_point utcMidnight2025() {
    return std::chrono::sys_days{std::chrono::year{2025} / 1 / 1};
}

[[nodiscard]] std::shared_ptr<MockTimeProvider> mockAt(std::chrono::system_clock::time_point sys) {
    return std::make_shared<MockTimeProvider>(sys, std::chrono::steady_clock::now());
}

[[nodiscard]] std::vector<std::uint8_t> readBytes(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

}  // namespace

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("PlayTimeLedger - missing file is a clean fresh day", "[playlimit][ledger]") {
    sudoku::test::TempTestDir tmp;
    auto clock = mockAt(utcMidnight2025());

    PlayTimeLedger ledger(tmp.path() / "play_time.yaml", clock);

    CHECK(ledger.accruedToday() == 0ms);
    CHECK(ledger.isUnreadable() == false);
    CHECK(!ledger.lastSessionEnd().has_value());  // CHECK(!x): CHECK_FALSE trips clang-tidy EnumCastOutOfRange
    CHECK(ledger.cooldownRemaining(15) == 0ms);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("PlayTimeLedger - accrues active play and survives a reload (restart-proof)", "[playlimit][ledger]") {
    sudoku::test::TempTestDir tmp;
    auto path = tmp.path() / "play_time.yaml";
    auto clock = mockAt(utcMidnight2025() + 8h);  // mid-day

    {
        PlayTimeLedger ledger(path, clock);
        ledger.addActivePlay(20min);
        ledger.addActivePlay(10min);
        CHECK(ledger.accruedToday() == 30min);
    }

    // Simulate a restart: a fresh ledger at the same path, same wall-clock day.
    PlayTimeLedger reloaded(path, clock);
    CHECK(reloaded.accruedToday() == 30min);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("PlayTimeLedger - on-disk bytes are an encrypted EncryptionManager blob, not plaintext",
          "[playlimit][ledger][encryption]") {
    sudoku::test::TempTestDir tmp;
    auto path = tmp.path() / "play_time.yaml";
    auto clock = mockAt(utcMidnight2025() + 8h);

    PlayTimeLedger ledger(path, clock);
    ledger.addActivePlay(5min);

    const auto bytes = readBytes(path);
    REQUIRE(!bytes.empty());                       // REQUIRE(!x): REQUIRE_FALSE trips clang-tidy EnumCastOutOfRange
    CHECK(EncryptionManager::isEncrypted(bytes));  // carries the "SDKE" magic
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("PlayTimeLedger - day rollover resets accrual but preserves last_session_end", "[playlimit][ledger]") {
    sudoku::test::TempTestDir tmp;
    auto path = tmp.path() / "play_time.yaml";
    // Late in the day so a short cooldown spans midnight.
    auto clock = mockAt(utcMidnight2025() + 23h + 50min);

    PlayTimeLedger ledger(path, clock);
    ledger.addActivePlay(40min);
    ledger.recordSessionEnd();
    REQUIRE(ledger.accruedToday() == 40min);
    REQUIRE(ledger.cooldownRemaining(30) > 0ms);  // 30-min cooldown, 0 elapsed

    // Advance 20 minutes — past UTC midnight, still inside the cooldown.
    clock->advanceSystemTime(20min);

    CHECK(ledger.accruedToday() == 0ms);         // new day: budget reset
    CHECK(ledger.cooldownRemaining(30) > 0ms);   // cooldown spans midnight, still blocking
    CHECK(ledger.lastSessionEnd().has_value());  // last-session-end survives rollover
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("PlayTimeLedger - mid-session play after rollover attributes to the new day", "[playlimit][ledger]") {
    sudoku::test::TempTestDir tmp;
    auto path = tmp.path() / "play_time.yaml";
    auto clock = mockAt(utcMidnight2025() + 23h);

    PlayTimeLedger ledger(path, clock);
    ledger.addActivePlay(30min);
    REQUIRE(ledger.accruedToday() == 30min);

    clock->advanceSystemTime(2h);  // cross midnight
    REQUIRE(ledger.accruedToday() == 0ms);

    ledger.addActivePlay(15min);            // new-day play
    CHECK(ledger.accruedToday() == 15min);  // attributed to the new day, not added to yesterday's 30
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("PlayTimeLedger - cooldown counts down then clears", "[playlimit][ledger]") {
    sudoku::test::TempTestDir tmp;
    auto clock = mockAt(utcMidnight2025() + 8h);

    PlayTimeLedger ledger(tmp.path() / "play_time.yaml", clock);
    ledger.recordSessionEnd();

    CHECK(ledger.cooldownRemaining(15) == 15min);
    clock->advanceSystemTime(10min);
    CHECK(ledger.cooldownRemaining(15) == 5min);
    clock->advanceSystemTime(5min);
    CHECK(ledger.cooldownRemaining(15) == 0ms);
    clock->advanceSystemTime(1h);
    CHECK(ledger.cooldownRemaining(15) == 0ms);  // never goes negative
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("PlayTimeLedger - a backward clock correction never inflates the cooldown past its length",
          "[playlimit][ledger][regression]") {
    // Regression: if last_session_end_ ends up in the future (clock set forward at record time, then
    // corrected back), elapsed is negative and an unclamped `total - elapsed` would over-lock the user.
    sudoku::test::TempTestDir tmp;
    auto clock = mockAt(utcMidnight2025() + 12h);

    PlayTimeLedger ledger(tmp.path() / "play_time.yaml", clock);
    ledger.recordSessionEnd();  // stamps "now" (12:00)

    clock->advanceSystemTime(-1h);  // clock corrected backward; now < last_session_end_

    CHECK(ledger.cooldownRemaining(15) == 15min);  // clamped to the configured length, not 1h15m
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("PlayTimeLedger - a hand-edited file fails closed and is preserved (corrupt != missing)",
          "[playlimit][ledger][encryption][regression]") {
    // Regression: editing play_time.yaml must NOT reset the day. The AEAD MAC fails to verify, so
    // the ledger fails closed (reports exhausted) and never overwrites the tampered bytes.
    sudoku::test::TempTestDir tmp;
    auto path = tmp.path() / "play_time.yaml";
    auto clock = mockAt(utcMidnight2025() + 8h);

    {
        PlayTimeLedger ledger(path, clock);
        ledger.addActivePlay(45min);
    }

    // Flip a byte deep in the ciphertext (past the 6-byte header), then save the tampered file.
    auto bytes = readBytes(path);
    REQUIRE(bytes.size() > 64);
    bytes[40] ^= 0xFF;
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        for (const auto byte : bytes) {
            out.put(static_cast<char>(byte));
        }
    }

    PlayTimeLedger tampered(path, clock);
    CHECK(tampered.isUnreadable());
    CHECK(tampered.accruedToday() > 24h);  // fail-closed sentinel — NOT reset to a fresh 0

    // A no-op write must not clobber the preserved file.
    tampered.addActivePlay(5min);
    tampered.recordSessionEnd();
    CHECK(readBytes(path) == bytes);  // bytes unchanged — file preserved
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("PlayTimeLedger - a present-but-plaintext file fails closed (always encrypted)",
          "[playlimit][ledger][encryption]") {
    // The ledger is ALWAYS written encrypted, so a plaintext file on disk means it was hand-written
    // or stripped — do not trust it; fail closed.
    sudoku::test::TempTestDir tmp;
    auto path = tmp.path() / "play_time.yaml";
    {
        std::ofstream out(path);
        out << "version: 1\nday_key: 0\naccrued_today_ms: 0\n";
    }

    auto clock = mockAt(utcMidnight2025() + 8h);
    PlayTimeLedger ledger(path, clock);

    CHECK(ledger.isUnreadable());
    CHECK(ledger.accruedToday() > 24h);  // fail-closed sentinel
    CHECK(ledger.cooldownRemaining(15) > 24h);
}
