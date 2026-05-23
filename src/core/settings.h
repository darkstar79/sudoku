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
    bool collect_detailed_stats{false};
    bool encrypt_detailed_stats{true};

    // Language
    std::string language{"en"};

    // Experimental — gated UI; not part of the 1.0 stability commitment.
    // Default off. Reached via Settings → Experimental in the UI.
    bool experimental_training_mode{false};
    bool experimental_coaching_hints{false};

    bool operator==(const Settings&) const = default;
};

}  // namespace sudoku::core
