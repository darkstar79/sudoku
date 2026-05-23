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
#include "../../src/core/strategies/wxyz_wing_strategy.h"
#include "../helpers/candidate_test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using sudoku::testing::createEmptyBoard;
using sudoku::testing::keepOnly;

TEST_CASE("WXYZWingStrategy - Metadata", "[wxyz_wing]") {
    WXYZWingStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::WXYZWing);
    REQUIRE(strategy.getName() == "WXYZ-Wing");
    REQUIRE(strategy.getDifficultyRating() == 4.6);
}

TEST_CASE("WXYZWingStrategy - Returns nullopt for complete board", "[wxyz_wing]") {
    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    WXYZWingStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("WXYZWingStrategy - Detects WXYZ-Wing with elimination", "[wxyz_wing]") {
    // Valid WXYZ-Wing: 4 cells, 4 values, exactly 1 non-restricted value (Z).
    //
    // Pivot at (0,0) {1,2,3,4} — 4 candidates
    // Wing1 at (0,3) {1,4}    — sees pivot via row 0
    // Wing2 at (3,0) {2,4}    — sees pivot via col 0
    // Wing3 at (1,1) {3,4}    — sees pivot via box 0
    // Union = {1,2,3,4}
    //
    // Restricted analysis:
    //   1: in (0,0) and (0,3) — same row → restricted ✓
    //   2: in (0,0) and (3,0) — same col → restricted ✓
    //   3: in (0,0) and (1,1) — same box → restricted ✓
    //   4: in (0,0),(0,3),(3,0),(1,1) — (0,3) and (3,0): diff row/col/box → NOT restricted
    // Z = 4 (unique non-restricted value)
    //
    // Eliminate 4 from cells seeing all Z-cells: (0,0),(0,3),(3,0),(1,1).
    // Target (3,3) sees (0,3) via col 3, (3,0) via row 3, (1,1) via box 1? No.
    // (3,3) is box 4: doesn't see (0,0) or (1,1).
    //
    // Better target: must see all 4 Z-cells. That's hard with spread cells.
    // Simpler: Z only in 3 cells (not pivot). Then target sees those 3.
    //
    // Pivot at (0,0) {1,2,3}  — 3 candidates, Z=4 NOT here
    // Wing1 at (0,3) {1,4}    — sees pivot via row
    // Wing2 at (3,0) {2,4}    — sees pivot via col
    // Wing3 at (1,1) {3,4}    — sees pivot via box
    // Union = {1,2,3,4}
    //
    // Restricted analysis:
    //   1: in (0,0) and (0,3) — same row → restricted ✓
    //   2: in (0,0) and (3,0) — same col → restricted ✓
    //   3: in (0,0) and (1,1) — same box → restricted ✓
    //   4: in (0,3),(3,0),(1,1) — (0,3)↔(3,0): diff row/col/box → NOT restricted
    // Z = 4
    //
    // Eliminate 4 from cells seeing all Z-cells {(0,3),(3,0),(1,1)}.
    // Target (3,3): sees (0,3) via col 3 ✓, sees (3,0) via row 3 ✓,
    //   sees (1,1)? box(3,3)=4, box(1,1)=0 → NO. Not valid target.
    // Target (0,1): sees (0,3) via row 0 ✓, sees (1,1) via box 0 ✓,
    //   sees (3,0)? row≠3, col≠0, box(0,1)=0≠box(3,0)=3 → NO.
    //
    // Use all Z-cells in the same row for easy targeting:
    // Pivot at (0,0) {1,2,3}
    // Wing1 at (0,3) {1,4}    — sees pivot via row
    // Wing2 at (0,6) {2,4}    — sees pivot via row
    // Wing3 at (1,1) {3,4}    — sees pivot via box
    // Union = {1,2,3,4}
    //
    // Restricted analysis:
    //   1: in (0,0) and (0,3) — same row → restricted ✓
    //   2: in (0,0) and (0,6) — same row → restricted ✓
    //   3: in (0,0) and (1,1) — same box → restricted ✓
    //   4: in (0,3),(0,6),(1,1) — (0,3)↔(1,1): diff row/col, box(0,3)=1≠box(1,1)=0 → NOT restricted
    // Z = 4
    //
    // Eliminate 4 from cells seeing {(0,3),(0,6),(1,1)}.
    // Target (0,7): sees (0,3) row ✓, (0,6) row ✓, (1,1)? col≠1, box(0,7)=2≠box(1,1)=0 → NO.
    // Target (0,1): sees (0,3) row ✓, (0,6) row ✓, (1,1) box 0 ✓ → YES!
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    keepOnly(state, 0, 0, {1, 2, 3});  // pivot (trivalue)
    keepOnly(state, 0, 3, {1, 4});     // wing1 — sees pivot via row
    keepOnly(state, 0, 6, {2, 4});     // wing2 — sees pivot via row
    keepOnly(state, 1, 1, {3, 4});     // wing3 — sees pivot via box
    keepOnly(state, 0, 1, {4, 8});     // target — has 4, sees all Z-cells

    // Eliminate 4 from other cells in row 0 to avoid spurious matches
    for (size_t col : {2, 4, 5, 7, 8}) {
        if (state.isAllowed(0, col, 4)) {
            state.eliminateCandidate(0, col, 4);
        }
    }

    WXYZWingStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->type == SolveStepType::Elimination);
    REQUIRE(result->technique == SolvingTechnique::WXYZWing);
    REQUIRE(result->rating == 4.6);

    // Check elimination of value 4 from target (0,1)
    bool found_target = false;
    for (const auto& elim : result->eliminations) {
        REQUIRE(elim.value == 4);
        if (elim.position.row == 0 && elim.position.col == 1) {
            found_target = true;
        }
    }
    REQUIRE(found_target);
}

TEST_CASE("WXYZWingStrategy - Returns nullopt when union != 4 values", "[wxyz_wing]") {
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    // Setup where union > 4
    keepOnly(state, 0, 0, {1, 2, 3});
    keepOnly(state, 0, 1, {1, 5});  // 5 is not in pivot
    keepOnly(state, 1, 0, {2, 6});  // 6 is not in pivot
    keepOnly(state, 1, 1, {3, 7});  // union = {1,2,3,5,6,7} = 6 values

    // Mark rest as filled
    for (size_t r = 0; r < 9; ++r) {
        for (size_t c = 0; c < 9; ++c) {
            if (!((r == 0 && c == 0) || (r == 0 && c == 1) || (r == 1 && c == 0) || (r == 1 && c == 1))) {
                board[r][c] = 1;
            }
        }
    }

    WXYZWingStrategy strategy;
    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("WXYZWingStrategy - Explanation contains technique name", "[wxyz_wing]") {
    // Same valid WXYZ-Wing pattern as the detection test
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    keepOnly(state, 0, 0, {1, 2, 3});
    keepOnly(state, 0, 3, {1, 4});
    keepOnly(state, 0, 6, {2, 4});
    keepOnly(state, 1, 1, {3, 4});
    keepOnly(state, 0, 1, {4, 8});

    for (size_t col : {2, 4, 5, 7, 8}) {
        if (state.isAllowed(0, col, 4)) {
            state.eliminateCandidate(0, col, 4);
        }
    }

    WXYZWingStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->explanation.find("WXYZ-Wing") != std::string::npos);
    REQUIRE(result->explanation.find("eliminates") != std::string::npos);
}

TEST_CASE("WXYZWingStrategy - Can be used through ISolvingStrategy interface", "[wxyz_wing]") {
    std::unique_ptr<ISolvingStrategy> strategy = std::make_unique<WXYZWingStrategy>();

    REQUIRE(strategy->getTechnique() == SolvingTechnique::WXYZWing);
    REQUIRE(strategy->getName() == "WXYZ-Wing");
    REQUIRE(strategy->getDifficultyRating() == 4.6);

    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    auto result = strategy->findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}
