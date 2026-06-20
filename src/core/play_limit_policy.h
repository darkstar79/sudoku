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

#include "settings.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>

namespace sudoku::core {

/// Which configured play-time limit a decision is about.
enum class LimitKind : std::uint8_t {
    Session,  ///< Active play in one continuous app session.
    Daily     ///< Cumulative active play per calendar day.
};

/// What the in-session policy says should happen on this tick.
enum class PlayLimitAction : std::uint8_t {
    None,  ///< Keep playing; nothing to surface.
    Warn,  ///< The binding limit is within the warning lead — surface the one-time heads-up.
    Close  ///< The binding limit is reached — save and close.
};

/// Result of evaluating the in-session limit state. Pure data; the warn-once
/// dedupe and the actual save/close live in the controller / View, not here.
struct PlayLimitDecision {
    PlayLimitAction action{PlayLimitAction::None};
    LimitKind limit{LimitKind::Session};  ///< Binding limit (meaningful when action != None).
    int minutes_left{0};                  ///< Warn only: minutes remaining, rounded up.

    bool operator==(const PlayLimitDecision&) const = default;
};

/// What the launch gate should do before the window is shown.
enum class LaunchAction : std::uint8_t {
    Allow,                  ///< Open normally.
    BlockedDailyExhausted,  ///< Today's budget is spent — "come back tomorrow".
    BlockedCooldown         ///< Still inside the post-session break.
};

/// Result of the launch-gate decision. Pure data.
struct LaunchDecision {
    LaunchAction action{LaunchAction::Allow};
    int minutes_remaining{0};  ///< Cooldown only: minutes left in the break, rounded up.

    bool operator==(const LaunchDecision&) const = default;
};

namespace detail {

[[nodiscard]] constexpr std::chrono::milliseconds minutesToMs(int minutes) {
    return std::chrono::milliseconds{static_cast<std::int64_t>(minutes) * 60'000};
}

/// Round a positive remaining duration up to whole minutes (so 30 s reads as "1 minute",
/// 4 min 30 s as "5 minutes"). Non-positive input yields 0.
[[nodiscard]] constexpr int ceilToMinutes(std::chrono::milliseconds remaining) {
    if (remaining <= std::chrono::milliseconds::zero()) {
        return 0;
    }
    return static_cast<int>((remaining.count() + 59'999) / 60'000);
}

/// An enabled limit with a non-positive minute count is treated as unlimited — there is
/// exactly one "off" state (the enable flag plus a >0 minute count), never two.
[[nodiscard]] constexpr bool sessionLimitActive(const Settings& s) {
    return s.enable_session_limit && s.max_session_minutes > 0;
}
[[nodiscard]] constexpr bool dailyLimitActive(const Settings& s) {
    return s.enable_daily_limit && s.max_daily_minutes > 0;
}

}  // namespace detail

/// Pure, deterministic evaluation of the in-session limit state.
///
/// @param settings    limit configuration (enable flags + minutes + warn lead).
/// @param daily_total cumulative active play today (across app runs), live.
/// @param session     active play in this continuous app session, live.
///
/// Tie-break: when the session and daily limits are reached or warn at exactly the same
/// remaining time, the **session** limit binds (locked decision — the session close always
/// starts the cooldown). The warn lead is clamped to the smallest active limit, and a lead
/// of 0 disables the warning entirely.
[[nodiscard]] inline PlayLimitDecision evaluate(const Settings& settings, std::chrono::milliseconds daily_total,
                                                std::chrono::milliseconds session) {
    using std::chrono::milliseconds;

    const bool session_on = detail::sessionLimitActive(settings);
    const bool daily_on = detail::dailyLimitActive(settings);
    if (!session_on && !daily_on) {
        return {};
    }

    std::optional<milliseconds> session_remaining;
    std::optional<milliseconds> daily_remaining;
    if (session_on) {
        session_remaining = detail::minutesToMs(settings.max_session_minutes) - session;
    }
    if (daily_on) {
        daily_remaining = detail::minutesToMs(settings.max_daily_minutes) - daily_total;
    }

    // Binding limit = the one with the smaller remaining time; ties go to Session.
    LimitKind binding{LimitKind::Session};
    milliseconds binding_remaining{0};
    if (session_remaining && daily_remaining) {
        if (*session_remaining <= *daily_remaining) {
            binding = LimitKind::Session;
            binding_remaining = *session_remaining;
        } else {
            binding = LimitKind::Daily;
            binding_remaining = *daily_remaining;
        }
    } else if (session_remaining) {
        binding = LimitKind::Session;
        binding_remaining = *session_remaining;
    } else {
        binding = LimitKind::Daily;
        binding_remaining = *daily_remaining;
    }

    if (binding_remaining <= milliseconds::zero()) {
        return {.action = PlayLimitAction::Close, .limit = binding, .minutes_left = 0};
    }

    // Clamp the warn lead to the smallest active limit; lead 0 disables warning.
    int smallest_limit_minutes = 0;
    if (session_on && daily_on) {
        smallest_limit_minutes = std::min(settings.max_session_minutes, settings.max_daily_minutes);
    } else if (session_on) {
        smallest_limit_minutes = settings.max_session_minutes;
    } else {
        smallest_limit_minutes = settings.max_daily_minutes;
    }
    const int lead = std::clamp(settings.warn_before_minutes, 0, smallest_limit_minutes);
    if (lead > 0 && binding_remaining <= detail::minutesToMs(lead)) {
        return {.action = PlayLimitAction::Warn,
                .limit = binding,
                .minutes_left = detail::ceilToMinutes(binding_remaining)};
    }

    return {.action = PlayLimitAction::None, .limit = binding, .minutes_left = 0};
}

/// Pure launch-gate decision. Daily exhaustion wins over an active cooldown (locked
/// precedence — the longer, honest wait). The cooldown only applies when the session
/// limit is enabled.
///
/// @param settings           limit configuration.
/// @param accrued_today       active play already accrued today (from the ledger).
/// @param cooldown_remaining  time left in the post-session break (from the ledger), 0 if none.
[[nodiscard]] inline LaunchDecision decide(const Settings& settings, std::chrono::milliseconds accrued_today,
                                           std::chrono::milliseconds cooldown_remaining) {
    using std::chrono::milliseconds;

    if (detail::dailyLimitActive(settings) && accrued_today >= detail::minutesToMs(settings.max_daily_minutes)) {
        return {.action = LaunchAction::BlockedDailyExhausted, .minutes_remaining = 0};
    }

    const bool cooldown_armed = detail::sessionLimitActive(settings) && settings.session_cooldown_minutes > 0;
    if (cooldown_armed && cooldown_remaining > milliseconds::zero()) {
        return {.action = LaunchAction::BlockedCooldown,
                .minutes_remaining = detail::ceilToMinutes(cooldown_remaining)};
    }

    return {.action = LaunchAction::Allow, .minutes_remaining = 0};
}

}  // namespace sudoku::core
