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
#include "../../src/core/strategies/unique_rectangle_strategy.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

[[nodiscard]] BoardData createEmptyBoard() {
    return BoardData{};
}

/// Helper: keep only specified candidates for a cell
void keepOnly(CandidateGrid& grid, size_t row, size_t col, const std::vector<int>& keep) {
    for (int v = 1; v <= 9; ++v) {
        if (std::find(keep.begin(), keep.end(), v) == keep.end() && grid.isAllowed(row, col, v)) {
            grid.eliminateCandidate(row, col, v);
        }
    }
}

}  // namespace

TEST_CASE("UniqueRectangleStrategy - Col-based Type 1 detection", "[unique_rectangle]") {
    // Two bivalue {1,2} cells in same column (col 0), different boxes (box 0 and box 3):
    //   c1 = (0,0) bivalue {1,2}  → box 0
    //   c2 = (3,0) bivalue {1,2}  → box 3
    // findRectangleCompletionCol tries col 1:
    //   c3 = (0,1) bivalue {1,2}  → box 0  (roof)
    //   c4 = (3,1) {1,2,3}        → box 3  (floor)
    // Rectangle spans exactly boxes 0 and 3 → valid.
    // Type 1: 3 roof cells + 1 floor → eliminates {1,2} from c4.
    // Row-based loop skips (0,0)+(0,1) because they share box 0.
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    keepOnly(state, 0, 0, {1, 2});
    keepOnly(state, 3, 0, {1, 2});
    keepOnly(state, 0, 1, {1, 2});
    keepOnly(state, 3, 1, {1, 2, 3});

    UniqueRectangleStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->technique == SolvingTechnique::UniqueRectangle);
    REQUIRE(result->type == SolveStepType::Elimination);
    REQUIRE_FALSE(result->eliminations.empty());

    // Floor cell (3,1) should have {1,2} eliminated
    bool elim1 = false;
    bool elim2 = false;
    for (const auto& elim : result->eliminations) {
        REQUIRE(elim.position.row == 3);
        REQUIRE(elim.position.col == 1);
        if (elim.value == 1) {
            elim1 = true;
        }
        if (elim.value == 2) {
            elim2 = true;
        }
    }
    REQUIRE(elim1);
    REQUIRE(elim2);
}

TEST_CASE("UniqueRectangleStrategy - Col-based Type 3 naked subset in Col unit", "[unique_rectangle]") {
    // c1=(0,0) and c2=(3,0) bivalue {1,2} in col 0, different boxes (0 and 3).
    // findRectangleCompletionCol tries col 1:
    //   c3=(0,1) {1,2,5}, c4=(3,1) {1,2,6}  → floor with different extras
    // Floor cells (0,1) and (3,1) share col 1 → unit_type = Col.
    // extras_union = {5,6}. Partner (6,1) with candidates {5,6} forms naked pair.
    // → Eliminate 5 and 6 from other cells in col 1.
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    keepOnly(state, 0, 0, {1, 2});
    keepOnly(state, 3, 0, {1, 2});
    keepOnly(state, 0, 1, {1, 2, 5});
    keepOnly(state, 3, 1, {1, 2, 6});

    // Partner cell in col 1: candidates subset of {5,6}
    keepOnly(state, 6, 1, {5, 6});

    // Target cell in col 1 with 5 or 6 to be eliminated
    keepOnly(state, 1, 1, {5, 7, 9});

    UniqueRectangleStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->technique == SolvingTechnique::UniqueRectangle);
    REQUIRE(result->type == SolveStepType::Elimination);
    REQUIRE(result->explanation.find("Type 3") != std::string::npos);
    REQUIRE(result->explanation_data.technique_subtype == 2);

    for (const auto& elim : result->eliminations) {
        REQUIRE((elim.value == 5 || elim.value == 6));
        // Not floor cells (0,1) or (3,1), not partner (6,1)
        REQUIRE_FALSE((elim.position.row == 0 && elim.position.col == 1));
        REQUIRE_FALSE((elim.position.row == 3 && elim.position.col == 1));
        REQUIRE_FALSE((elim.position.row == 6 && elim.position.col == 1));
    }
}

TEST_CASE("UniqueRectangleStrategy - Col-based Type 4 strong link in Col unit", "[unique_rectangle]") {
    // c1=(0,0) and c2=(3,0) bivalue {1,2} in col 0, different boxes (0 and 3).
    // findRectangleCompletionCol tries col 1:
    //   c3=(0,1) {1,2,5}, c4=(3,1) {1,2,6}  → floor cells
    // Floor cells (0,1) and (3,1) share col 1 → unit_type = Col.
    // Eliminate value 1 from all other cells in col 1 → strong link on 1.
    // → Eliminate 2 from floor cells (0,1) and (3,1).
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    keepOnly(state, 0, 0, {1, 2});
    keepOnly(state, 3, 0, {1, 2});
    keepOnly(state, 0, 1, {1, 2, 5});
    keepOnly(state, 3, 1, {1, 2, 6});

    // Eliminate value 1 from all other cells in col 1
    for (size_t row = 0; row < 9; ++row) {
        if (row != 0 && row != 3 && state.isAllowed(row, 1, 1)) {
            state.eliminateCandidate(row, 1, 1);
        }
    }

    // Ensure no Type 3 naked pair partner exists (no cell in col 1 with candidates ⊆ {5,6})
    // The remaining cells in col 1 still have many candidates so no partner forms

    UniqueRectangleStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->technique == SolvingTechnique::UniqueRectangle);
    REQUIRE(result->type == SolveStepType::Elimination);
    REQUIRE(result->explanation.find("Type 4") != std::string::npos);
    REQUIRE(result->explanation_data.technique_subtype == 3);

    // Should eliminate 2 from both floor cells (strong link on 1)
    for (const auto& elim : result->eliminations) {
        REQUIRE(elim.value == 2);
        REQUIRE((elim.position.col == 1));
        REQUIRE((elim.position.row == 0 || elim.position.row == 3));
    }
    REQUIRE(result->eliminations.size() == 2);
}

TEST_CASE("UniqueRectangleStrategy - Col-based Type 2 with Col unit", "[unique_rectangle]") {
    // Two bivalue {1,2} cells in same column (col 0), different boxes:
    //   c1 = (0,0) bivalue {1,2}  → box 0  (roof)
    //   c2 = (3,0) bivalue {1,2}  → box 3  (roof)
    // findRectangleCompletionCol tries col 1:
    //   c3 = (0,1) {1,2,3}        → box 0  (floor)
    //   c4 = (3,1) {1,2,3}        → box 3  (floor)
    // Rectangle spans exactly boxes 0 and 3 → valid.
    // Type 2: 2 roof + 2 floor, both floor have same extra C=3.
    // Floor cells (0,1) and (3,1) share col 1 → unit_type = RegionType::Col.
    // Eliminates 3 from all other cells in col 1.
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    keepOnly(state, 0, 0, {1, 2});
    keepOnly(state, 3, 0, {1, 2});
    keepOnly(state, 0, 1, {1, 2, 3});
    keepOnly(state, 3, 1, {1, 2, 3});

    UniqueRectangleStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->technique == SolvingTechnique::UniqueRectangle);
    REQUIRE(result->type == SolveStepType::Elimination);
    REQUIRE_FALSE(result->eliminations.empty());

    // All eliminations must be value 3, in col 1, not at rows 0 or 3
    for (const auto& elim : result->eliminations) {
        REQUIRE(elim.value == 3);
        REQUIRE(elim.position.col == 1);
        REQUIRE(elim.position.row != 0);
        REQUIRE(elim.position.row != 3);
    }
}
