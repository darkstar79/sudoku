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

#include "../../src/core/candidate_grid.h"
#include "../../src/core/strategies/grouped_x_cycles_strategy.h"
#include "../../src/core/strategies/x_cycles_strategy.h"
#include "../helpers/candidate_test_utils.h"

#include <array>
#include <optional>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

[[nodiscard]] BoardData createEmptyBoard() {
    return BoardData{};
}

/// Build a candidate grid in which only `digit` survives, and only at `keep` cells.
/// All other digits are eliminated everywhere so the strategy's digit loop bails
/// instantly on them, isolating the single-digit pattern under test.
[[nodiscard]] CandidateGrid singleDigitGrid(int digit, const std::vector<std::pair<size_t, size_t>>& keep) {
    BoardData board = createEmptyBoard();
    CandidateGrid state(board);
    std::array<bool, TOTAL_CELLS> keep_cell{};
    for (auto [r, c] : keep) {
        keep_cell[(r * BOARD_SIZE) + c] = true;
    }
    for (size_t idx = 0; idx < TOTAL_CELLS; ++idx) {
        for (int v = MIN_VALUE; v <= MAX_VALUE; ++v) {
            if (v != digit || !keep_cell[idx]) {
                state.eliminateCandidate(idx / BOARD_SIZE, idx % BOARD_SIZE, v);
            }
        }
    }
    return state;
}

/// True iff `step` is a Placement at (row, col). Short-circuits on `has_value()`
/// so the optional access is checked.
[[nodiscard]] bool placesAt(const std::optional<SolveStep>& step, size_t row, size_t col) {
    return step.has_value() && step->type == SolveStepType::Placement && step->position.row == row &&
           step->position.col == col;
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

    // The loop G=(0,0),(1,0) =strong(box0)= (2,2) =strong(row2)= (2,6) =weak= (6,6)
    // =strong(row6)= (6,0) =weak= G has a strong-strong discontinuity at the single
    // cell (2,2): 4 is forced there (Type 2 placement).
    REQUIRE(result.has_value());
    const auto& step = result.value();
    REQUIRE(step.technique == SolvingTechnique::GroupedXCycles);
    REQUIRE(step.rating == 6.8);
    REQUIRE(step.type == SolveStepType::Placement);
    REQUIRE(step.position.row == 2);
    REQUIRE(step.position.col == 2);
    REQUIRE(step.value == 4);
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

// Regression for GH #23 (finding 3): the strong-strong discontinuity (Type 2)
// closure was missing, so valid grouped placements were never emitted.
// The pattern below (digit 1) closes a grouped chain strong-strong onto the
// single cell (6,3); plain X-Cycles cannot recover it because the chain routes
// through a group node — which is what makes this a *grouped* Type 2.
TEST_CASE("GroupedXCyclesStrategy - emits Type 2 strong-strong placement",
          "[grouped_x_cycles][regression][bug-grouped-xcycles-type2]") {
    BoardData board = createEmptyBoard();
    CandidateGrid state = singleDigitGrid(1, {{1, 4}, {1, 7}, {5, 6}, {6, 3}, {6, 5}, {7, 5}, {8, 0}, {8, 3}});
    GroupedXCyclesStrategy strategy;

    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    const auto& step = result.value();
    REQUIRE(step.type == SolveStepType::Placement);
    REQUIRE(step.technique == SolvingTechnique::GroupedXCycles);
    REQUIRE(step.position.row == 6);
    REQUIRE(step.position.col == 3);
    REQUIRE(step.value == 1);
    REQUIRE(step.explanation.contains("strong-strong"));
}

// The chain genuinely depends on a group node: the same pattern that yields a
// grouped Type 2 placement at (6,3) is invisible to the non-grouped X-Cycles
// strategy — which is what makes this a *grouped* Type 2.
TEST_CASE("GroupedXCyclesStrategy - Type 2 placement requires a group node",
          "[grouped_x_cycles][regression][bug-grouped-xcycles-type2]") {
    BoardData board = createEmptyBoard();
    CandidateGrid state = singleDigitGrid(1, {{1, 4}, {1, 7}, {5, 6}, {6, 3}, {6, 5}, {7, 5}, {8, 0}, {8, 3}});

    GroupedXCyclesStrategy grouped;
    XCyclesStrategy plain;

    REQUIRE(placesAt(grouped.findStep(board, state), 6, 3));
    REQUIRE_FALSE(placesAt(plain.findStep(board, state), 6, 3));
}

// A strong-strong discontinuity whose discontinuity node is a GROUP must not
// produce a placement — a group cannot be "placed". With the isSingle() guard
// removed this exact board places digit 1 onto a group at (3,4); the guard
// suppresses it, so findStep yields no deduction here.
TEST_CASE("GroupedXCyclesStrategy - no Type 2 placement on group discontinuity",
          "[grouped_x_cycles][regression][bug-grouped-xcycles-type2]") {
    BoardData board = createEmptyBoard();
    CandidateGrid state =
        singleDigitGrid(1, {{0, 4}, {1, 3}, {1, 5}, {2, 4}, {3, 4}, {4, 3}, {5, 3}, {5, 4}, {5, 6}, {8, 1}, {8, 3}});
    GroupedXCyclesStrategy strategy;

    auto result = strategy.findStep(board, state);

    REQUIRE_FALSE(result.has_value());
}
