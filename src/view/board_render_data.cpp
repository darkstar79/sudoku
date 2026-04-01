// sudoku-cpp - Offline Sudoku Game
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

#include "board_render_data.h"

#include "core/constants.h"

#include <cstddef>

namespace sudoku::view {

BoardRenderData toBoardRenderData(const model::GameState& state) {
    BoardRenderData data{};
    for (size_t r = 0; r < core::BOARD_SIZE; ++r) {
        for (size_t c = 0; c < core::BOARD_SIZE; ++c) {
            auto cell = state.getCell(r, c);
            data[r][c] = RenderCell{
                .value = cell.value,
                .candidates = cell.notes,
                .is_given = cell.is_given,
                .has_conflict = cell.has_conflict,
                .is_hint_revealed = cell.is_hint_revealed,
                .analysis_color = state.getCellColor(r, c),
                .highlight_role = std::nullopt,
                .player_color = 0,
                .is_found = false,
            };
        }
    }
    return data;
}

BoardRenderData toBoardRenderData(const core::TrainingBoard& board) {
    BoardRenderData data{};
    for (size_t r = 0; r < core::BOARD_SIZE; ++r) {
        for (size_t c = 0; c < core::BOARD_SIZE; ++c) {
            const auto& cell = board[r][c];
            core::CellNotes notes;
            for (int v : cell.candidates) {
                notes.add(v);
            }

            std::optional<core::CellRole> role;
            if (cell.is_found) {
                role = core::CellRole::CorrectAnswer;
            } else if (cell.player_selected && cell.highlight_role != core::CellRole::Pattern) {
                role = cell.highlight_role;
            }

            data[r][c] = RenderCell{
                .value = cell.value,
                .candidates = notes,
                .is_given = cell.is_given,
                .highlight_role = role,
                .player_color = cell.player_color,
                .is_found = cell.is_found,
            };
        }
    }
    return data;
}

}  // namespace sudoku::view
