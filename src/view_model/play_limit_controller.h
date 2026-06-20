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

#include "../core/i_play_time_ledger.h"
#include "../core/i_settings_manager.h"
#include "../core/play_limit_policy.h"

#include <chrono>
#include <cstdint>
#include <memory>

namespace sudoku::viewmodel {

/// Qt-free coordinator that turns the 1-second game tick into warn / close events and keeps the
/// restart-proof ledger up to date. The View drives it (`onTick` per tick, `flush` on game
/// transitions / auto-save / close) and reacts to the returned event — it owns no Qt itself, so
/// the policy is unit-testable with a MockTimeProvider-backed ledger.
///
/// Accrual model: each tick contributes the *increase* in the per-puzzle active-play timer since
/// the previous tick (clamped at 0 when the timer resets on a new puzzle). That delta accumulates
/// into the per-run session total and, via the ledger, into the persisted daily total. Because
/// the per-puzzle timer already excludes paused/completed time, "timer stopped ⇒ no accrual" holds
/// for free.
class PlayLimitController {
public:
    enum class Event : std::uint8_t {
        None,  ///< Nothing to surface this tick.
        Warn,  ///< Show the one-time heads-up modal (binding limit + minutes left).
        Close  ///< Save and close: session-end already recorded so the cooldown has begun.
    };

    struct TickResult {
        Event event{Event::None};
        core::LimitKind limit{core::LimitKind::Session};  ///< Binding limit (Warn/Close).
        int minutes_left{0};                              ///< Warn only.

        bool operator==(const TickResult&) const = default;
    };

    PlayLimitController(std::shared_ptr<core::ISettingsManager> settings_manager,
                        std::shared_ptr<core::IPlayTimeLedger> ledger);

    /// Drive once per second with the current puzzle's active-play elapsed time. Reads settings
    /// fresh each call, so mid-session limit changes take effect on the next tick.
    [[nodiscard]] TickResult onTick(std::chrono::milliseconds current_puzzle_elapsed);

    /// Push any unflushed active play into the ledger now (call on game transitions, the auto-save
    /// cadence, and before close), bounding crash loss to one flush interval.
    void flush();

    /// Live daily total = persisted accrual + not-yet-flushed active play.
    [[nodiscard]] std::chrono::milliseconds liveDailyTotal() const;

    /// Cumulative active play this app session (across puzzles).
    [[nodiscard]] std::chrono::milliseconds sessionActive() const {
        return session_active_;
    }

private:
    std::shared_ptr<core::ISettingsManager> settings_manager_;
    std::shared_ptr<core::IPlayTimeLedger> ledger_;

    std::chrono::milliseconds last_elapsed_{0};    // previous tick's per-puzzle elapsed
    std::chrono::milliseconds session_active_{0};  // cumulative active play this run
    std::chrono::milliseconds unflushed_{0};       // accrued since the last ledger flush
    std::int64_t accrual_day_key_{0};              // calendar day the current unflushed_ belongs to
    int ticks_since_flush_{0};
    bool warned_{false};  // warn-once latch; re-arms when the warn condition clears
    bool closed_{false};  // latched after a Close so we emit it exactly once
};

}  // namespace sudoku::viewmodel
