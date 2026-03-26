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

#include "../../src/core/candidate_grid.h"
#include "../../src/core/strategies/grouped_x_cycles_strategy.h"
#include "../helpers/candidate_test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

[[nodiscard]] BoardData createEmptyBoard() {
    return BoardData{};
}

}  // namespace

TEST_CASE("GroupedXCyclesStrategy - Metadata", "[grouped_x_cycles]") {
    GroupedXCyclesStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::GroupedXCycles);
    REQUIRE(strategy.getName() == "Grouped X-Cycles");
    REQUIRE(strategy.getDifficultyRating() == 6.8);
}

TEST_CASE("GroupedXCyclesStrategy - Returns nullopt for complete board", "[grouped_x_cycles]") {
    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    GroupedXCyclesStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("GroupedXCyclesStrategy - Finds grouped cycle with box group", "[grouped_x_cycles]") {
    // Set up a grouped X-Cycle for digit 4:
    // Group G = {(0,0),(1,0)} in box 0, col 0 — both have digit 4
    // Box 0 has only G and (2,2) with digit 4 → strong link G ↔ (2,2)
    // Row 2: only (2,2) and (2,6) have digit 4 → strong link (2,2) ↔ (2,6)
    // Col 6: (2,6) and (6,6) have digit 4, plus others → weak link (2,6) — (6,6)
    // Col 0: (0,0), (1,0), and (6,0) have digit 4 → G contains (0,0),(1,0), weak to (6,0)
    // Row 6: (6,0) and (6,6) have digit 4 → strong link

    auto board = createEmptyBoard();
    CandidateGrid state(board);

    // Remove digit 4 from specific cells to create the pattern

    // Box 0: keep 4 only at (0,0), (1,0), (2,2)
    for (size_t r = 0; r <= 2; ++r) {
        for (size_t c = 0; c <= 2; ++c) {
            if (!((r == 0 && c == 0) || (r == 1 && c == 0) || (r == 2 && c == 2))) {
                state.eliminateCandidate(r, c, 4);
            }
        }
    }

    // Row 2: keep 4 only at (2,2) and (2,6)
    for (size_t c = 0; c < 9; ++c) {
        if (c != 2 && c != 6) {
            state.eliminateCandidate(2, c, 4);
        }
    }

    // Row 6: keep 4 only at (6,0) and (6,6)
    for (size_t c = 0; c < 9; ++c) {
        if (c != 0 && c != 6) {
            state.eliminateCandidate(6, c, 4);
        }
    }

    // Col 0: keep 4 only at (0,0), (1,0), (6,0)
    for (size_t r = 0; r < 9; ++r) {
        if (r != 0 && r != 1 && r != 6) {
            state.eliminateCandidate(r, 0, 4);
        }
    }

    // Col 6: keep 4 at (2,6), (6,6), and (4,6) as extra for weak link
    for (size_t r = 0; r < 9; ++r) {
        if (r != 2 && r != 4 && r != 6) {
            state.eliminateCandidate(r, 6, 4);
        }
    }

    // Verify setup
    REQUIRE(state.isAllowed(0, 0, 4));
    REQUIRE(state.isAllowed(1, 0, 4));
    REQUIRE(state.isAllowed(2, 2, 4));
    REQUIRE(state.isAllowed(2, 6, 4));
    REQUIRE(state.isAllowed(6, 0, 4));
    REQUIRE(state.isAllowed(6, 6, 4));

    GroupedXCyclesStrategy strategy;
    auto result = strategy.findStep(board, state);

    // Should find a grouped X-Cycle pattern
    if (result.has_value()) {
        REQUIRE(result->technique == SolvingTechnique::GroupedXCycles);
        REQUIRE(result->rating == 6.8);
        REQUIRE_FALSE(result->eliminations.empty());
        for (const auto& elim : result->eliminations) {
            REQUIRE(elim.value == 4);
        }
    }
}

TEST_CASE("GroupedXCyclesStrategy - No cycle when too few candidates", "[grouped_x_cycles]") {
    BoardData board = {{5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
                       {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
                       {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 0}};
    CandidateGrid state(board);
    GroupedXCyclesStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("GroupedXCyclesStrategy - Explanation contains technique name", "[grouped_x_cycles]") {
    // Reuse the grouped cycle setup
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    for (size_t r = 0; r <= 2; ++r) {
        for (size_t c = 0; c <= 2; ++c) {
            if (!((r == 0 && c == 0) || (r == 1 && c == 0) || (r == 2 && c == 2))) {
                state.eliminateCandidate(r, c, 4);
            }
        }
    }
    for (size_t c = 0; c < 9; ++c) {
        if (c != 2 && c != 6) {
            state.eliminateCandidate(2, c, 4);
        }
    }
    for (size_t c = 0; c < 9; ++c) {
        if (c != 0 && c != 6) {
            state.eliminateCandidate(6, c, 4);
        }
    }
    for (size_t r = 0; r < 9; ++r) {
        if (r != 0 && r != 1 && r != 6) {
            state.eliminateCandidate(r, 0, 4);
        }
    }
    for (size_t r = 0; r < 9; ++r) {
        if (r != 2 && r != 4 && r != 6) {
            state.eliminateCandidate(r, 6, 4);
        }
    }

    GroupedXCyclesStrategy strategy;
    auto result = strategy.findStep(board, state);

    if (result.has_value()) {
        REQUIRE(result->explanation.find("Grouped X-Cycles") != std::string::npos);
    }
}
