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

#include "../core/solve_step.h"
#include "../core/training_types.h"
#include "../model/game_state.h"

#include <array>
#include <cstdint>
#include <optional>

namespace sudoku::view {

/// Rendering-ready cell state used by the unified board widget.
/// Both game and training modes convert their data to this type.
struct RenderCell {
    int value{0};
    core::CellNotes candidates;  ///< Pencil marks (game) or solver candidates (training)
    bool is_given{false};
    bool has_conflict{false};
    bool is_hint_revealed{false};

    // Annotation layer
    uint8_t analysis_color{0};                     ///< 0=none, 1-6 for game analysis colors
    std::optional<core::CellRole> highlight_role;  ///< Technique highlight (nullopt = none)
    int player_color{0};                           ///< 0=none, 1=Color A, 2=Color B
    bool is_found{false};                          ///< Locked after correct answer in training
};

/// 9x9 board of rendering-ready cells.
using BoardRenderData = std::array<std::array<RenderCell, core::BOARD_SIZE>, core::BOARD_SIZE>;

/// Convert a game state to render data.
[[nodiscard]] BoardRenderData toBoardRenderData(const model::GameState& state);

/// Convert a training board to render data.
[[nodiscard]] BoardRenderData toBoardRenderData(const core::TrainingBoard& board);

}  // namespace sudoku::view
