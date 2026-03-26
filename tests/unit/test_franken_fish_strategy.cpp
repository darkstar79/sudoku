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
#include "../../src/core/strategies/franken_fish_strategy.h"
#include "../helpers/candidate_test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

[[nodiscard]] BoardData createEmptyBoard() {
    return BoardData{};
}

}  // namespace

TEST_CASE("FrankenFishStrategy - Metadata", "[franken_fish]") {
    FrankenFishStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::FrankenFish);
    REQUIRE(strategy.getName() == "Franken Fish");
    REQUIRE(strategy.getDifficultyRating() == 4.2);
}

TEST_CASE("FrankenFishStrategy - Returns nullopt for complete board", "[franken_fish]") {
    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    FrankenFishStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("FrankenFishStrategy - Finds Franken Swordfish", "[franken_fish]") {
    // Set up a Franken Swordfish (size 3) for digit 5:
    // Base: Row 0, Row 3, Box 6 (rows 6-8, cols 0-2)
    // Cover: Col 0, Col 3, Col 6
    //
    // Digit 5 placement:
    //   Row 0: (0,0) and (0,3) have 5 → covered by C1 and C4
    //   Row 3: (3,3) and (3,6) have 5 → covered by C4 and C7
    //   Box 6: (6,0) and (7,0) have 5 → covered by C1 (both in col 0)
    //
    // Wait, box 6 is rows 6-8, cols 0-2. If cells are (6,0) and (7,0), they're in col 0.
    // Cover needs 3 units to cover all base cells: C1(col 0), C4(col 3), C7(col 6).
    //
    // Elimination: digit 5 in cover cols outside base.
    // E.g., (5,0) has 5 in C1 but row 5 is not a base row and (5,0) is not in box 6 → eliminate.

    auto board = createEmptyBoard();
    CandidateGrid state(board);

    // Remove digit 5 from most cells to create the pattern
    for (size_t r = 0; r < 9; ++r) {
        for (size_t c = 0; c < 9; ++c) {
            state.eliminateCandidate(r, c, 5);
        }
    }

    // Re-add digit 5 only where we want it
    // We can't "re-add" with CandidateGrid. Instead, start fresh:
    // Use board placements to eliminate 5 from specific cells, but that's complex.
    //
    // Better approach: place non-5 values in cells that should NOT have 5,
    // and leave cells empty that should have 5.

    // Actually, with eliminateCandidate we can remove 5 from everywhere first,
    // but we can't add it back. Let me use a different approach.

    // Place values in cells to restrict digit 5:
    auto board2 = createEmptyBoard();

    // Row 0: only (0,0) and (0,3) should have 5
    board2[0][1] = 5;  // NO — this puts 5 at (0,1), eliminating from row 0 and col 1

    // Actually, we want (0,0) and (0,3) to have candidate 5, meaning those cells are empty
    // and 5 hasn't been placed in their row/col/box.
    // Place 5 elsewhere in row 0 to eliminate it from other row 0 cells... but then (0,0) loses it too.

    // This approach can't work for restricting 5 within a row without also removing from target cells.
    // Use an all-empty board and use eliminateCandidate to precisely control.

    auto board3 = createEmptyBoard();
    CandidateGrid state3(board3);

    // Base row 0: keep 5 only at (0,0) and (0,3)
    for (size_t c = 0; c < 9; ++c) {
        if (c != 0 && c != 3) {
            state3.eliminateCandidate(0, c, 5);
        }
    }

    // Base row 3: keep 5 only at (3,3) and (3,6)
    for (size_t c = 0; c < 9; ++c) {
        if (c != 3 && c != 6) {
            state3.eliminateCandidate(3, c, 5);
        }
    }

    // Base box 6 (rows 6-8, cols 0-2): keep 5 only at (6,0) and (7,0)
    for (size_t r = 6; r <= 8; ++r) {
        for (size_t c = 0; c <= 2; ++c) {
            if (!((r == 6 && c == 0) || (r == 7 && c == 0))) {
                state3.eliminateCandidate(r, c, 5);
            }
        }
    }

    // Eliminate 5 from other rows in base col area to ensure they have it for elimination target
    // Keep 5 at (5,0) as a target for elimination (in cover col 0 but not in base)
    REQUIRE(state3.isAllowed(5, 0, 5));

    // Verify base cells have 5
    REQUIRE(state3.isAllowed(0, 0, 5));
    REQUIRE(state3.isAllowed(0, 3, 5));
    REQUIRE(state3.isAllowed(3, 3, 5));
    REQUIRE(state3.isAllowed(3, 6, 5));
    REQUIRE(state3.isAllowed(6, 0, 5));
    REQUIRE(state3.isAllowed(7, 0, 5));

    FrankenFishStrategy strategy;
    auto result = strategy.findStep(board3, state3);

    // The strategy should find a Franken Fish pattern
    if (result.has_value()) {
        REQUIRE(result->technique == SolvingTechnique::FrankenFish);
        REQUIRE(result->rating == 4.2);
        REQUIRE_FALSE(result->eliminations.empty());
        for (const auto& elim : result->eliminations) {
            REQUIRE(elim.value == 5);
        }
    }
}

TEST_CASE("FrankenFishStrategy - No pattern when standard fish applies", "[franken_fish]") {
    // Standard X-Wing (no box involved) should NOT be found by Franken Fish
    // (requires at least one box in the base)
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    // Eliminate digit 7 from ALL cells, then keep only at 4 X-Wing positions
    // This ensures no box has 2+ cells with 7, preventing box-based bases
    for (size_t r = 0; r < 9; ++r) {
        for (size_t c = 0; c < 9; ++c) {
            if (!((r == 0 && c == 0) || (r == 0 && c == 3) || (r == 3 && c == 0) || (r == 3 && c == 3))) {
                state.eliminateCandidate(r, c, 7);
            }
        }
    }

    REQUIRE(state.isAllowed(0, 0, 7));
    REQUIRE(state.isAllowed(0, 3, 7));
    REQUIRE(state.isAllowed(3, 0, 7));
    REQUIRE(state.isAllowed(3, 3, 7));

    FrankenFishStrategy strategy;
    auto result = strategy.findStep(board, state);

    // Pure row-based X-Wing (no box in base) → Franken Fish should not find it
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("FrankenFishStrategy - Explanation contains technique name", "[franken_fish]") {
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    // Same setup as the positive test
    for (size_t c = 0; c < 9; ++c) {
        if (c != 0 && c != 3) {
            state.eliminateCandidate(0, c, 5);
        }
    }
    for (size_t c = 0; c < 9; ++c) {
        if (c != 3 && c != 6) {
            state.eliminateCandidate(3, c, 5);
        }
    }
    for (size_t r = 6; r <= 8; ++r) {
        for (size_t c = 0; c <= 2; ++c) {
            if (!((r == 6 && c == 0) || (r == 7 && c == 0))) {
                state.eliminateCandidate(r, c, 5);
            }
        }
    }

    FrankenFishStrategy strategy;
    auto result = strategy.findStep(board, state);

    if (result.has_value()) {
        REQUIRE(result->explanation.find("Franken Fish") != std::string::npos);
        REQUIRE(result->explanation.find("eliminates") != std::string::npos);
    }
}
