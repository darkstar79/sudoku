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

#include "candidate_grid.h"   // core::CandidateGrid
#include "core/board_data.h"  // core::BoardData
#include "solve_step.h"       // core::Elimination

#include <cstdint>
#include <expected>
#include <string>
#include <vector>

// NOTE (Wave-1 straddle): this file lives physically in src/core/ but declares
// namespace sudoku::engine on purpose (DEC-1). Phase A E1 encloses it into the
// sudoku_engine target and E3 relocates it to include/engine/ mechanically. Do
// not "fix" the namespace to match the directory. It must stay Qt-free,
// spdlog-free, and own no global/function-local static mutable state.
namespace sudoku::engine {

/// POD input to reconstruct(): a starting board (flat 81-char string) plus the
/// prior candidate eliminations to replay onto its base candidate set. Reuses
/// the existing core::Elimination representation — never invent a parallel one.
struct ReconstructionInput {
    std::string board_at_step;                          ///< flat 81-char board ('0'/'.' = empty)
    std::vector<core::Elimination> prior_eliminations;  ///< eliminations to replay onto the base set
};

/// A successfully reconstructed state: the parsed board and the candidate grid
/// with every prior elimination applied. Structured data only — no localized text.
struct ReconstructedState {
    core::BoardData board;
    core::CandidateGrid candidates;
};

/// Failure modes surfaced through the std::expected error channel (never thrown).
enum class ReconstructError : std::uint8_t {
    InvalidBoard,        ///< board_at_step parsed but violates Sudoku validity
    ParseFailed,         ///< board_at_step could not be parsed (bad length or illegal character)
    InvalidElimination,  ///< a prior_eliminations entry has an out-of-range cell or value
};

/// Rebuild a (board, candidates) state from a starting board + prior eliminations.
///
/// Qt-free and reentrant: all scratch is stack/parameter; no cached buffers.
///
/// board_at_step is a flat 81-char string ('0'/'.' = empty, '1'-'9' = value); it is
/// validated for both format (else ParseFailed) and Sudoku validity (else InvalidBoard).
/// Every prior_eliminations entry is range-checked — row/col in [0, BOARD_SIZE) and value
/// in [MIN_VALUE, MAX_VALUE] — so a malformed (e.g. deserialized) entry yields
/// InvalidElimination rather than out-of-bounds/shift UB in CandidateGrid.
[[nodiscard]] std::expected<ReconstructedState, ReconstructError> reconstruct(const ReconstructionInput& input);

}  // namespace sudoku::engine
