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

#include "reconstruction.h"

#include "core/constants.h"
#include "game_validator.h"

#include <optional>
#include <string_view>
#include <utility>

namespace sudoku::engine {

namespace {

/// Parse a flat 81-char board ('0'/'.' = empty, '1'-'9' = value) into a BoardData.
/// Dependency-free by design (Option B): avoids PuzzleAnalyzer's three-dependency
/// constructor so this engine keystone stays lightweight and reentrant. Returns
/// nullopt on wrong length or an illegal character (surfaced as ParseFailed).
[[nodiscard]] std::optional<core::BoardData> parseFlatBoard(std::string_view flat) {
    if (flat.size() != core::TOTAL_CELLS) {
        return std::nullopt;
    }
    core::BoardData board;
    for (size_t i = 0; i < core::TOTAL_CELLS; ++i) {
        const char ch = flat[i];
        if (ch == '.' || ch == '0') {
            board[i / core::BOARD_SIZE][i % core::BOARD_SIZE] = core::EMPTY_CELL;
        } else if (ch >= '1' && ch <= '9') {
            board[i / core::BOARD_SIZE][i % core::BOARD_SIZE] = ch - '0';
        } else {
            return std::nullopt;
        }
    }
    return board;
}

}  // namespace

std::expected<ReconstructedState, ReconstructError> reconstruct(const ReconstructionInput& input) {
    const auto parsed = parseFlatBoard(input.board_at_step);
    if (!parsed.has_value()) {
        return std::unexpected(ReconstructError::ParseFailed);
    }

    const core::GameValidator validator;
    if (!validator.validateBoard(parsed.value())) {
        return std::unexpected(ReconstructError::InvalidBoard);
    }

    // Replay each prior elimination onto the base candidate set. CandidateGrid's
    // overlay is monotonic (no un-eliminate), which is exactly what replay needs.
    // Range-check every entry first: eliminateCandidate indexes an 81-element array
    // and shifts by `value`, so an out-of-range (e.g. deserialized) entry would be
    // out-of-bounds / shift UB — surface it as a clean error instead.
    core::CandidateGrid candidates(parsed.value());
    for (const auto& elim : input.prior_eliminations) {
        if (elim.position.row >= core::BOARD_SIZE || elim.position.col >= core::BOARD_SIZE ||
            elim.value < core::MIN_VALUE || elim.value > core::MAX_VALUE) {
            return std::unexpected(ReconstructError::InvalidElimination);
        }
        candidates.eliminateCandidate(elim.position.row, elim.position.col, elim.value);
    }

    return ReconstructedState{parsed.value(), std::move(candidates)};
}

}  // namespace sudoku::engine
