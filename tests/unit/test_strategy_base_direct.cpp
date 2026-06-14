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

// Reachability tests for the Story 0b.4b "Direct" predicate (StrategyBase::eliminationsForcePlacement
// and its two detectors). Each case runs over a real CandidateGrid with a real, unapplied elimination
// set — never a hand-forced RatingContext{.forces_placement=true} — so this proves both faces of the
// predicate are satisfiable by legal boards (AC3b). The two detectors are exercised in isolation: the
// hidden-single board is built so the eliminated cell keeps ≥2 candidates (no naked single), and vice
// versa.

#include "../../src/core/candidate_grid.h"
#include "../../src/core/solve_step.h"
#include "../../src/core/strategy_base.h"

#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

// Exposes the protected static detectors for direct testing.
struct DirectProbe : StrategyBase {
    using StrategyBase::createsHiddenSingle;
    using StrategyBase::createsNakedSingle;
    using StrategyBase::eliminationsForcePlacement;
};

// R1C1 (0,0) has exactly two candidates {1,9} (row 0 holds 2..8). Eliminating 9 collapses it to {1}.
BoardData nakedSingleBoard() {
    return {{0, 2, 3, 4, 5, 6, 7, 8, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}};
}

// Digit 1 is blocked everywhere in box 0 except (0,0) and (0,1): a 1 in row 1 (at C5) and row 2 (at C6)
// blocks all of box 0's rows 1-2, and a 1 in column 2 (at R3) blocks (0,2). Both (0,0)/(0,1) are
// otherwise unconstrained (9 candidates each). Eliminating 1 from (0,1) leaves digit 1 with a single
// position in box 0 → a hidden single — while (0,1) still has 8 candidates (no naked single).
BoardData hiddenSingleBoard() {
    return {{0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 1, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 1, 0, 0},
            {0, 0, 1, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}};
}

}  // namespace

TEST_CASE("eliminationsForcePlacement - naked single face", "[direct][strategy_base]") {
    auto board = nakedSingleBoard();
    CandidateGrid state(board);
    std::vector<Elimination> elims = {Elimination{.position = {.row = 0, .col = 0}, .value = 9}};

    REQUIRE(DirectProbe::createsNakedSingle(state, elims));
    REQUIRE_FALSE(DirectProbe::createsHiddenSingle(state, elims));  // no unit drops to one position here
    REQUIRE(DirectProbe::eliminationsForcePlacement(state, elims));
}

TEST_CASE("eliminationsForcePlacement - hidden single face (no naked single)", "[direct][strategy_base]") {
    auto board = hiddenSingleBoard();
    CandidateGrid state(board);
    std::vector<Elimination> elims = {Elimination{.position = {.row = 0, .col = 1}, .value = 1}};

    REQUIRE(DirectProbe::createsHiddenSingle(state, elims));
    REQUIRE_FALSE(DirectProbe::createsNakedSingle(state, elims));  // (0,1) keeps 8 candidates
    REQUIRE(DirectProbe::eliminationsForcePlacement(state, elims));
}

TEST_CASE("eliminationsForcePlacement - does not fire when no single is created", "[direct][strategy_base]") {
    BoardData empty{};
    CandidateGrid state(empty);
    // Removing one candidate from a wide-open cell: the cell keeps 8 candidates and digit 5 keeps 8
    // positions in every unit — neither face fires.
    std::vector<Elimination> elims = {Elimination{.position = {.row = 0, .col = 0}, .value = 5}};

    REQUIRE_FALSE(DirectProbe::eliminationsForcePlacement(state, elims));
}
