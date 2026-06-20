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
#include <cstdint>
#include <optional>

namespace sudoku::core {

/// Restart-proof, date-keyed record of how much active play has accrued today and when the
/// last session ended (for the post-session cooldown). Persisted to its own encrypted file
/// outside the saves directory, so deleting a save cannot reset the day and hand-editing the
/// file cannot grant time (the AEAD MAC fails to decrypt → fail-closed).
///
/// Daily rollover uses the UTC calendar day; setting the system clock backward is a known,
/// accepted bypass (out of scope — see PlayTimeLedger).
class IPlayTimeLedger {
public:
    virtual ~IPlayTimeLedger() = default;

    /// Add active-play time, attributed to the current calendar day. Rolls the day over
    /// (resetting the accrual) when the stored day differs from today. Persists the change.
    /// A non-positive delta is a no-op.
    virtual void addActivePlay(std::chrono::milliseconds delta) = 0;

    /// Add active-play time attributed to an explicit calendar day, so a caller flushing across a
    /// day boundary can keep pre-midnight play on the day it was earned (instead of the day the
    /// flush happens to run). Rolls the stored day over to @p attribution_day_key when it differs.
    virtual void addActivePlay(std::chrono::milliseconds delta, std::int64_t attribution_day_key) = 0;

    /// Current UTC calendar-day key (days since the system_clock epoch). Lets a coordinator detect a
    /// day rollover and attribute accrual deterministically.
    [[nodiscard]] virtual std::int64_t currentDayKey() const = 0;

    /// Active play accrued for *today*. Returns 0 once the stored day has rolled over.
    [[nodiscard]] virtual std::chrono::milliseconds accruedToday() const = 0;

    /// Stamp "a session just ended now" so the cooldown begins. Persists.
    virtual void recordSessionEnd() = 0;

    /// The instant the last session ended, if one was ever recorded.
    [[nodiscard]] virtual std::optional<std::chrono::system_clock::time_point> lastSessionEnd() const = 0;

    /// Time left in the post-session break given a cooldown length in minutes. Returns 0 when
    /// no session-end was recorded, the cooldown length is non-positive, or it has elapsed.
    [[nodiscard]] virtual std::chrono::milliseconds cooldownRemaining(int cooldown_minutes) const = 0;

    /// True when the on-disk ledger was present but could not be read (corrupt / tampered /
    /// foreign key). The file is preserved, never overwritten, and the ledger fails closed:
    /// accruedToday() / cooldownRemaining() report "exhausted" so a configured limit keeps
    /// blocking rather than handing the user a fresh day.
    [[nodiscard]] virtual bool isUnreadable() const = 0;

protected:
    IPlayTimeLedger() = default;
    IPlayTimeLedger(const IPlayTimeLedger&) = default;
    IPlayTimeLedger& operator=(const IPlayTimeLedger&) = default;
    IPlayTimeLedger(IPlayTimeLedger&&) = default;
    IPlayTimeLedger& operator=(IPlayTimeLedger&&) = default;
};

}  // namespace sudoku::core
