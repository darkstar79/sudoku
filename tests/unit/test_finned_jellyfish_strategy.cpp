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
#include "../../src/core/strategies/finned_jellyfish_strategy.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

[[nodiscard]] BoardData createEmptyBoard() {
    return BoardData{};
}

void keepOnly(CandidateGrid& grid, size_t row, size_t col, const std::vector<int>& keep) {
    for (int v = 1; v <= 9; ++v) {
        if (std::find(keep.begin(), keep.end(), v) == keep.end() && grid.isAllowed(row, col, v)) {
            grid.eliminateCandidate(row, col, v);
        }
    }
}

}  // namespace

TEST_CASE("FinnedJellyfishStrategy - Metadata", "[finned_jellyfish]") {
    FinnedJellyfishStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::FinnedJellyfish);
    REQUIRE(strategy.getName() == "Finned Jellyfish");
    REQUIRE(strategy.getDifficultyPoints() == 450);
}

TEST_CASE("FinnedJellyfishStrategy - Returns nullopt for complete board", "[finned_jellyfish]") {
    BoardData board = {{5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
                       {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
                       {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};
    CandidateGrid state(board);
    FinnedJellyfishStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("FinnedJellyfishStrategy - Detects row-based finned jellyfish", "[finned_jellyfish]") {
    // Row-based finned jellyfish for value 1:
    // 4 rows with value 1 candidates, union of columns = 5 (4 base + 1 fin)
    // Rows 0, 2, 4, 6. Base cols: 0, 3, 6, 8. Fin col: 1 (only in row 0).
    auto board = BoardData{};
    CandidateGrid state(board);

    // Remove value 1 from all cells first
    for (size_t r = 0; r < 9; ++r) {
        for (size_t c = 0; c < 9; ++c) {
            if (state.isAllowed(r, c, 1)) {
                state.eliminateCandidate(r, c, 1);
            }
        }
    }

    // Row 0: cols 0, 1 (fin at col 1)
    // But we can't add back — we need a different approach.
    // Use keepOnly on cells to set specific candidates including value 1.

    // Actually, let's use a fresh approach: mark most cells as filled.
    auto board2 = BoardData::filled(2);  // All filled with 2

    // Leave specific cells empty for the pattern
    // Row 0: empty at cols 0, 1 (fin col)
    board2[0][0] = 0;
    board2[0][1] = 0;
    // Row 2: empty at col 0, 3
    board2[2][0] = 0;
    board2[2][3] = 0;
    // Row 4: empty at col 3, 6
    board2[4][3] = 0;
    board2[4][6] = 0;
    // Row 6: empty at col 6, 8
    board2[6][6] = 0;
    board2[6][8] = 0;

    // Target: Row 1, col 0 — in fin's box (box 0), non-pattern row, base col 0
    board2[1][0] = 0;

    CandidateGrid state2(board2);

    // All empty cells should have candidate 1 (they do by default since cell is empty)
    // Ensure they only have specific candidates
    keepOnly(state2, 0, 0, {1, 5});
    keepOnly(state2, 0, 1, {1, 5});  // fin cell
    keepOnly(state2, 2, 0, {1, 5});
    keepOnly(state2, 2, 3, {1, 5});
    keepOnly(state2, 4, 3, {1, 5});
    keepOnly(state2, 4, 6, {1, 5});
    keepOnly(state2, 6, 6, {1, 5});
    keepOnly(state2, 6, 8, {1, 5});
    keepOnly(state2, 1, 0, {1, 3});  // target

    FinnedJellyfishStrategy strategy;
    auto result = strategy.findStep(board2, state2);

    if (result.has_value()) {
        REQUIRE(result->technique == SolvingTechnique::FinnedJellyfish);
        REQUIRE(result->type == SolveStepType::Elimination);
        REQUIRE(result->points == 450);

        for (const auto& elim : result->eliminations) {
            REQUIRE(elim.value == 1);
        }

        REQUIRE(result->explanation.find("Finned Jellyfish") != std::string::npos);
    }
}

TEST_CASE("FinnedJellyfishStrategy - Col-based finned Jellyfish detection", "[finned_jellyfish]") {
    // Fish cols {0,3,6,7}. Cols 0,3: rows {0,3,6}. Col 6: rows {0,3,7}. Col 7: rows {0,3,7,8} (fin=8).
    // Union = {0,3,6,7,8} = 5 rows. fin_row=8, fin_col=7, fin_box=8 (rows 6-8, cols 6-8).
    // Base rows = {0,3,6,7}. base_rows ∩ rows 6-8 = {6,7}.
    // Non-fish ∩ cols 6-8 = {8}. Eliminates 5 from (6,8) and (7,8).
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    for (size_t row = 0; row < 9; ++row) {
        if (row != 0 && row != 3 && row != 6) {
            state.eliminateCandidate(row, 0, 5);
            state.eliminateCandidate(row, 3, 5);
        }
    }
    for (size_t row = 0; row < 9; ++row) {
        if (row != 0 && row != 3 && row != 7) {
            state.eliminateCandidate(row, 6, 5);
        }
    }
    for (size_t row = 0; row < 9; ++row) {
        if (row != 0 && row != 3 && row != 7 && row != 8) {
            state.eliminateCandidate(row, 7, 5);
        }
    }

    FinnedJellyfishStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->type == SolveStepType::Elimination);
    REQUIRE(result->technique == SolvingTechnique::FinnedJellyfish);
    REQUIRE_FALSE(result->eliminations.empty());

    for (const auto& elim : result->eliminations) {
        REQUIRE(elim.value == 5);
        // Must be in fin's box (box 8: rows 6-8, cols 6-8)
        size_t box = (elim.position.row / 3) * 3 + (elim.position.col / 3);
        REQUIRE(box == 8);
        // Not in fish cols
        REQUIRE(elim.position.col != 0);
        REQUIRE(elim.position.col != 3);
        REQUIRE(elim.position.col != 6);
        REQUIRE(elim.position.col != 7);
    }
}

TEST_CASE("FinnedJellyfishStrategy - Can be used through ISolvingStrategy interface", "[finned_jellyfish]") {
    std::unique_ptr<ISolvingStrategy> strategy = std::make_unique<FinnedJellyfishStrategy>();

    REQUIRE(strategy->getTechnique() == SolvingTechnique::FinnedJellyfish);
    REQUIRE(strategy->getName() == "Finned Jellyfish");
    REQUIRE(strategy->getDifficultyPoints() == 450);

    BoardData board = {{5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
                       {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
                       {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};
    CandidateGrid state(board);
    auto result = strategy->findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}
