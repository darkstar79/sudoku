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

#include "../helpers/strategy_test_utils.h"  // sudoku::testing::flatStringToBoard
#include "../helpers/test_utils.h"           // sudoku::test::isBoardValid
#include "core/board_data.h"
#include "core/candidate_grid.h"
#include "core/constants.h"
#include "core/reconstruction.h"
#include "core/solve_step.h"

#include <cstdint>
#include <string>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku;
using sudoku::core::BOARD_SIZE;

namespace {

// Canonical valid puzzle (unique solution). The empty cell (0,2) has base
// candidates {1,2,4}, so eliminating 1 and 2 leaves exactly {4} — a hand-known
// two-elimination case (guarded by the isAllowed preconditions below).
constexpr std::string_view kValidBoard = "530070000"
                                         "600195000"
                                         "098000060"
                                         "800060003"
                                         "400803001"
                                         "700020006"
                                         "060000280"
                                         "000419005"
                                         "000080079";

}  // namespace

TEST_CASE("Reconstruct_AppliesPriorEliminationsToBaseCandidates", "[reconstruction][engine]") {
    // Arrange
    const core::BoardData expected_board = testing::flatStringToBoard(std::string(kValidBoard));
    REQUIRE(sudoku::test::isBoardValid(expected_board));  // U-VALID precondition

    // The eliminations must be candidates in the fresh base grid, else the case
    // is fabricated — assert that up front so a wrong board fails loudly here.
    const core::CandidateGrid baseline(expected_board);
    constexpr size_t kRow = 0;
    constexpr size_t kCol = 2;
    REQUIRE(baseline.isAllowed(kRow, kCol, 1));
    REQUIRE(baseline.isAllowed(kRow, kCol, 2));

    const engine::ReconstructionInput input{
        .board_at_step = std::string(kValidBoard),
        .prior_eliminations =
            {
                core::Elimination{.position = core::Position{.row = kRow, .col = kCol}, .value = 1},
                core::Elimination{.position = core::Position{.row = kRow, .col = kCol}, .value = 2},
            },
    };

    // Act
    const auto result = engine::reconstruct(input);

    // Assert
    REQUIRE(result.has_value());
    const auto& state = result.value();

    // The board round-trips to the parsed input.
    CHECK(state.board == expected_board);

    // Exactly the two named candidates are gone at (0,2); nothing else there.
    CHECK_FALSE(state.candidates.isAllowed(kRow, kCol, 1));
    CHECK_FALSE(state.candidates.isAllowed(kRow, kCol, 2));
    const auto removed_bits = static_cast<uint16_t>((1U << 1U) | (1U << 2U));
    const auto expected_cell_mask = static_cast<uint16_t>(baseline.getPossibleValuesMask(kRow, kCol) & ~removed_bits);
    CHECK(state.candidates.getPossibleValuesMask(kRow, kCol) == expected_cell_mask);

    // No other cell changed versus a fresh baseline grid.
    for (size_t r = 0; r < BOARD_SIZE; ++r) {
        for (size_t c = 0; c < BOARD_SIZE; ++c) {
            if (r == kRow && c == kCol) {
                continue;
            }
            CHECK(state.candidates.getPossibleValuesMask(r, c) == baseline.getPossibleValuesMask(r, c));
        }
    }
}

TEST_CASE("Reconstruct_RejectsInvalidBoard", "[reconstruction][engine]") {
    // Arrange: a duplicate 5 in row 0 makes the board fail validity.
    const engine::ReconstructionInput input{
        .board_at_step = "550070000"
                         "600195000"
                         "098000060"
                         "800060003"
                         "400803001"
                         "700020006"
                         "060000280"
                         "000419005"
                         "000080079",
        .prior_eliminations = {},
    };

    // Act
    const auto result = engine::reconstruct(input);

    // Assert
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == engine::ReconstructError::InvalidBoard);
}

TEST_CASE("Reconstruct_RejectsOutOfRangeElimination", "[reconstruction][engine]") {
    // A malformed prior-elimination entry must surface as InvalidElimination rather than
    // being silently applied (an out-of-range value sets a wrong/unused candidate bit) or,
    // worse, driving an out-of-bounds write / shift-UB inside CandidateGrid. This guards
    // the primitive's future consumers (A.3 fixture replay, C.1 pool loader) that feed it
    // deserialized data.
    auto with_elim = [](const core::Elimination& elim) {
        return engine::ReconstructionInput{.board_at_step = std::string(kValidBoard), .prior_eliminations = {elim}};
    };

    SECTION("value below MIN_VALUE") {
        const auto result =
            engine::reconstruct(with_elim({.position = core::Position{.row = 0, .col = 2}, .value = 0}));
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == engine::ReconstructError::InvalidElimination);
    }

    SECTION("value above MAX_VALUE") {
        const auto result =
            engine::reconstruct(with_elim({.position = core::Position{.row = 0, .col = 2}, .value = 10}));
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == engine::ReconstructError::InvalidElimination);
    }

    SECTION("row out of range") {
        const auto result =
            engine::reconstruct(with_elim({.position = core::Position{.row = BOARD_SIZE, .col = 0}, .value = 1}));
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == engine::ReconstructError::InvalidElimination);
    }

    SECTION("col out of range") {
        const auto result =
            engine::reconstruct(with_elim({.position = core::Position{.row = 0, .col = BOARD_SIZE}, .value = 1}));
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == engine::ReconstructError::InvalidElimination);
    }
}
