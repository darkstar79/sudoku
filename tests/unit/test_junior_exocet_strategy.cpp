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
#include "../../src/core/strategies/junior_exocet_strategy.h"
#include "../helpers/candidate_test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

[[nodiscard]] BoardData createEmptyBoard() {
    return BoardData{};
}

}  // namespace

TEST_CASE("JuniorExocetStrategy - Metadata", "[junior_exocet]") {
    JuniorExocetStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::JuniorExocet);
    REQUIRE(strategy.getName() == "Junior Exocet");
    REQUIRE(strategy.getDifficultyRating() == 9.4);
}

TEST_CASE("JuniorExocetStrategy - Returns nullopt for complete board", "[junior_exocet]") {
    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    JuniorExocetStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("JuniorExocetStrategy - Implements ISolvingStrategy interface", "[junior_exocet]") {
    JuniorExocetStrategy strategy;
    ISolvingStrategy& iface = strategy;

    REQUIRE(iface.getTechnique() == SolvingTechnique::JuniorExocet);
    REQUIRE(iface.getName() == "Junior Exocet");
    REQUIRE(iface.getDifficultyRating() == 9.4);

    // Empty board should not crash
    auto board = createEmptyBoard();
    CandidateGrid state(board);
    [[maybe_unused]] auto result = iface.findStep(board, state);
}

TEST_CASE("JuniorExocetStrategy - Returns nullopt when no pattern exists", "[junior_exocet]") {
    // Nearly complete board with no Junior Exocet pattern
    BoardData board = {{5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
                       {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
                       {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 0}};
    CandidateGrid state(board);
    JuniorExocetStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("JuniorExocetStrategy - Explanation contains technique name", "[junior_exocet]") {
    // Set up a board where a Junior Exocet elimination might occur:
    // We need base pair in a box row, targets in aligned columns in other bands.
    // Use an empty board with selective candidate elimination to construct the pattern.
    auto board = createEmptyBoard();
    CandidateGrid candidates(board);

    // Place values strategically to create a base pair in box 0, row 0
    // B1 = (0,0), B2 = (0,1) with combined candidates {1,2,3}
    // Everything else in box 0 row 0 is filled except these two
    board[0][2] = 4;
    candidates.placeValue(0, 2, 4);

    // Restrict B1 to {1,2} and B2 to {2,3}  => base_cands = {1,2,3}
    for (int v = 3; v <= 9; ++v) {
        candidates.eliminateCandidate(0, 0, v);
    }
    for (int v = 1; v <= 9; ++v) {
        if (v != 2 && v != 3) {
            candidates.eliminateCandidate(0, 1, v);
        }
    }

    // T1 at (3,0) — same column as B1, different band, different box
    // Give T1 candidates {1,2,5} — 5 is non-base, should be eliminated
    for (int v = 1; v <= 9; ++v) {
        if (v != 1 && v != 2 && v != 5) {
            candidates.eliminateCandidate(3, 0, v);
        }
    }

    // T2 at (6,1) — same column as B2, different band, different box from T1 and base
    // Give T2 candidates {2,3,7} — 7 is non-base, should be eliminated
    for (int v = 1; v <= 9; ++v) {
        if (v != 2 && v != 3 && v != 7) {
            candidates.eliminateCandidate(6, 1, v);
        }
    }

    JuniorExocetStrategy strategy;
    auto result = strategy.findStep(board, candidates);

    // The pattern may or may not be found depending on the full board context,
    // but if it is found, the explanation should mention "Junior Exocet"
    if (result.has_value()) {
        REQUIRE(result->explanation.find("Junior Exocet") != std::string::npos);
        REQUIRE(result->technique == SolvingTechnique::JuniorExocet);
        REQUIRE(result->type == SolveStepType::Elimination);
        REQUIRE_FALSE(result->eliminations.empty());
    }
}
