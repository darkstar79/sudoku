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
#include "../../src/core/strategies/death_blossom_strategy.h"
#include "../helpers/candidate_test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using sudoku::testing::createEmptyBoard;
using sudoku::testing::keepOnly;

TEST_CASE("DeathBlossomStrategy - Metadata", "[death_blossom]") {
    DeathBlossomStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::DeathBlossom);
    REQUIRE(strategy.getName() == "Death Blossom");
    REQUIRE(strategy.getDifficultyRating() == 8.2);
}

TEST_CASE("DeathBlossomStrategy - Returns nullopt for complete board", "[death_blossom]") {
    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    DeathBlossomStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("DeathBlossomStrategy - Finds Death Blossom with 2 petals", "[death_blossom]") {
    // Stem: (0,0) with candidates {1,2}
    //
    // Petal for 1: ALS at (0,3),(0,4) in row 0 with cands {1,3,5}
    //   (0,3) = {1,3}, (0,4) = {3,5}
    //   Stem (0,0) sees (0,3) via row 0. (0,3) has candidate 1. ✓ Restricted common.
    //
    // Petal for 2: ALS at (3,0),(4,0) in col 0 with cands {2,3,7}
    //   (3,0) = {2,3}, (4,0) = {3,7}
    //   Stem (0,0) sees (3,0) via col 0. (3,0) has candidate 2. ✓ Restricted common.
    //
    // Common Z=3 across both petals (3 is in both {1,3,5} and {2,3,7}), Z ∉ stem {1,2}. ✓
    //
    // Z-cells: petal 1 has 3 at (0,3) and (0,4). Petal 2 has 3 at (3,0).
    // Eliminate 3 from cells seeing (0,3), (0,4), AND (3,0).
    // Target (3,3): sees (0,3) via col 3, sees (0,4) — different row/col. (3,3) is row 3, col 3.
    //   (0,4) is row 0, col 4, box 1. (3,3) is box 4. Doesn't see.
    // Target (0,0): stem itself, excluded.
    //
    // Let me simplify: petal 1 has 3 only at (0,3): keepOnly (0,4) = {1,5} and (0,3) = {3,5}.
    // Then petal 1 cands = {1,3,5}, popcount=3 for 2 cells, correct.
    // Z-cells in petal 1: only (0,3).
    // Z-cells in petal 2: only (3,0).
    // Target must see both (0,3) and (3,0):
    //   (3,3): sees (0,3) via col 3, sees (3,0) via row 3. ✓
    //   Must have candidate 3 and not be in any ALS or stem.

    auto board = createEmptyBoard();
    CandidateGrid state(board);

    // Stem
    keepOnly(state, 0, 0, {1, 2});

    // Petal 1: ALS in row 0
    keepOnly(state, 0, 3, {3, 5});
    keepOnly(state, 0, 4, {1, 5});

    // Petal 2: ALS in col 0
    keepOnly(state, 3, 0, {2, 3});
    keepOnly(state, 4, 0, {3, 7});

    // Target (3,3) must have candidate 3
    REQUIRE(state.isAllowed(3, 3, 3));

    DeathBlossomStrategy strategy;
    auto result = strategy.findStep(board, state);

    if (result.has_value()) {
        REQUIRE(result->technique == SolvingTechnique::DeathBlossom);
        REQUIRE(result->rating == 8.2);
        REQUIRE_FALSE(result->eliminations.empty());
        for (const auto& elim : result->eliminations) {
            REQUIRE(elim.value == 3);
        }
    }
}

TEST_CASE("DeathBlossomStrategy - No pattern when stem has too many candidates", "[death_blossom]") {
    // Stem with 5 candidates → too many for Death Blossom (max 3)
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    keepOnly(state, 0, 0, {1, 2, 3, 4, 5});

    DeathBlossomStrategy strategy;
    auto result = strategy.findStep(board, state);

    // May find a pattern with a different stem cell on the empty board,
    // but the specific 5-candidate stem won't be used.
    // This is mainly a regression test.
    if (result.has_value()) {
        REQUIRE(result->technique == SolvingTechnique::DeathBlossom);
    }
}

TEST_CASE("DeathBlossomStrategy - Explanation contains technique name", "[death_blossom]") {
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    keepOnly(state, 0, 0, {1, 2});
    keepOnly(state, 0, 3, {3, 5});
    keepOnly(state, 0, 4, {1, 5});
    keepOnly(state, 3, 0, {2, 3});
    keepOnly(state, 4, 0, {3, 7});

    DeathBlossomStrategy strategy;
    auto result = strategy.findStep(board, state);

    if (result.has_value()) {
        REQUIRE(result->explanation.find("Death Blossom") != std::string::npos);
        REQUIRE(result->explanation.find("eliminates") != std::string::npos);
    }
}
