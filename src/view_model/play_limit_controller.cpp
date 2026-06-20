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

#include "play_limit_controller.h"

#include <algorithm>
#include <utility>

namespace sudoku::viewmodel {

PlayLimitController::PlayLimitController(std::shared_ptr<core::ISettingsManager> settings_manager,
                                         std::shared_ptr<core::IPlayTimeLedger> ledger)
    : settings_manager_(std::move(settings_manager)), ledger_(std::move(ledger)) {
}

void PlayLimitController::flush() {
    ticks_since_flush_ = 0;
    if (unflushed_ > std::chrono::milliseconds::zero()) {
        // Attribute to the day the time was earned on, not the day the flush happens to run — so a
        // flush straddling midnight keeps pre-midnight play on the old day (spec §2 / AC#4).
        ledger_->addActivePlay(unflushed_, accrual_day_key_);
        unflushed_ = std::chrono::milliseconds::zero();
    }
}

std::chrono::milliseconds PlayLimitController::liveDailyTotal() const {
    // unflushed_ counts toward today only while it belongs to today; across a rollover the
    // pre-midnight remainder is attributed to the old day (and flushed there on the next tick).
    const auto today_unflushed =
        (accrual_day_key_ == ledger_->currentDayKey()) ? unflushed_ : std::chrono::milliseconds::zero();
    return ledger_->accruedToday() + today_unflushed;
}

PlayLimitController::TickResult PlayLimitController::onTick(std::chrono::milliseconds current_puzzle_elapsed) {
    if (closed_) {
        return {};
    }

    // Day boundary: if we still hold pre-rollover active play, flush it to the day it was earned on
    // before accruing anything to the new day (spec §2 / AC#4 — pre-midnight time stays on its day).
    const auto today = ledger_->currentDayKey();
    if (unflushed_ > std::chrono::milliseconds::zero() && accrual_day_key_ != today) {
        flush();  // attributes unflushed_ to accrual_day_key_
    }

    // Active play added since the previous tick. A negative delta means the per-puzzle timer reset
    // (new game / restore / completion) — that boundary contributes nothing.
    auto delta = current_puzzle_elapsed - last_elapsed_;
    if (delta < std::chrono::milliseconds::zero()) {
        delta = std::chrono::milliseconds::zero();
    }
    last_elapsed_ = current_puzzle_elapsed;
    session_active_ += delta;
    if (unflushed_ == std::chrono::milliseconds::zero()) {
        accrual_day_key_ = today;  // start a fresh accrual block stamped to the current day
    }
    unflushed_ += delta;

    const auto& settings = settings_manager_->getSettings();

    // Flush on the auto-save cadence so a crash loses at most one interval of daily accrual.
    const int flush_every_ticks = std::max(1, settings.auto_save_interval_ms / 1000);
    if (++ticks_since_flush_ >= flush_every_ticks) {
        flush();
    }

    const auto decision = core::evaluate(settings, liveDailyTotal(), session_active_);

    if (decision.action == core::PlayLimitAction::Close) {
        flush();                      // persist the final active play before closing
        ledger_->recordSessionEnd();  // start the cooldown (harmless when no session limit is set)
        closed_ = true;
        return {.event = Event::Close, .limit = decision.limit, .minutes_left = 0};
    }

    if (decision.action == core::PlayLimitAction::Warn) {
        if (!warned_) {
            warned_ = true;
            return {.event = Event::Warn, .limit = decision.limit, .minutes_left = decision.minutes_left};
        }
        return {};
    }

    // None: either before the warn window or a limit was disabled/raised — re-arm the warn latch
    // so re-entering a warn window fires exactly one fresh warning.
    warned_ = false;
    return {};
}

}  // namespace sudoku::viewmodel
