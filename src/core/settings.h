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

#include "i_puzzle_generator.h"

#include <string>

namespace sudoku::core {

/// Application-wide persistent settings
struct Settings {
    // Gameplay
    int max_hints{10};
    int auto_save_interval_ms{30000};
    Difficulty default_difficulty{Difficulty::Medium};

    // Display
    bool show_conflicts{true};
    bool show_hints{true};
    bool show_session_timer{false};
    bool collect_detailed_stats{false};
    bool encrypt_detailed_stats{true};

    // Time limits (all off / unlimited by default — strictly opt-in). The app warns once
    // and then saves-and-closes when an active limit is reached; a restart cannot hand back
    // used-up time (see PlayTimeLedger). An *enabled* limit with a 0 minute count is treated
    // as unlimited — there is exactly one "off" state (the flag plus a >0 count).
    bool enable_session_limit{false};
    int max_session_minutes{0};        // active-play minutes in one continuous session
    int session_cooldown_minutes{15};  // mandatory break before a new session may start
    bool enable_daily_limit{false};
    int max_daily_minutes{0};    // cumulative active-play minutes per calendar day
    int warn_before_minutes{5};  // lead time for the pre-limit warning; clamped <= smallest active limit

    // Language
    std::string language{"en"};

    // Experimental — gated UI; not part of the 1.0 stability commitment.
    // Default off. Reached via Settings → Experimental in the UI.
    bool experimental_training_mode{false};
    bool experimental_coaching_hints{false};

    bool operator==(const Settings&) const = default;
};

}  // namespace sudoku::core
