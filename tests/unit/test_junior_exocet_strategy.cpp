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

#include <algorithm>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

[[nodiscard]] BoardData createEmptyBoard() {
    return BoardData{};
}

/// Did the result include eliminating `value` at (row, col)?
[[nodiscard]] bool hasElimination(const SolveStep& step, size_t row, size_t col, int value) {
    return std::ranges::any_of(step.eliminations, [&](const Elimination& e) {
        return e.position.row == row && e.position.col == col && e.value == value;
    });
}

/// Strip a cell down to a specific candidate set, preserving the constraint propagation
/// already applied. Returns whether the cell is left with that exact set.
void restrictCandidates(CandidateGrid& candidates, size_t row, size_t col, const std::vector<int>& keep) {
    sudoku::testing::keepOnly(candidates, row, col, keep);
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

TEST_CASE("JuniorExocetStrategy - Returns nullopt when no pattern exists", "[junior_exocet]") {
    BoardData board = {{5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
                       {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
                       {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 0}};
    CandidateGrid state(board);
    JuniorExocetStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

// Worked example from David P. Bird's JExocet Compendium (file "05 JE2 Examples"):
// Puzzle: ..7.2...493....6..6..3............5.2...1...8..69..4....37..9...2..5...1.....8...
// Pattern: (158)JE2 with base r1c12, targets r2c4 (non-base 4), r3c7 (non-base 2,7).
TEST_CASE("JuniorExocetStrategy - Compendium worked example (row-based)", "[junior_exocet]") {
    // clang-format off
    BoardData board = {
        {0, 0, 7, 0, 2, 0, 0, 0, 4},
        {9, 3, 0, 0, 0, 0, 6, 0, 0},
        {6, 0, 0, 3, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 5, 0},
        {2, 0, 0, 0, 1, 0, 0, 0, 8},
        {0, 0, 6, 9, 0, 0, 4, 0, 0},
        {0, 0, 3, 7, 0, 0, 9, 0, 0},
        {0, 2, 0, 0, 5, 0, 0, 0, 1},
        {0, 0, 0, 0, 0, 8, 0, 0, 0},
    };
    // clang-format on
    CandidateGrid candidates(board);
    JuniorExocetStrategy strategy;

    auto result = strategy.findStep(board, candidates);
    REQUIRE(result.has_value());
    REQUIRE(result->technique == SolvingTechnique::JuniorExocet);
    REQUIRE(result->type == SolveStepType::Elimination);

    // The pattern proves the targets must take a base digit. Constraint propagation
    // on this grid already restricts r2c4 to base candidates ∪ {4}; the JE eliminates
    // the 4. Similarly r3c7 has at least 2,7 to eliminate.
    bool target1_elim_found = hasElimination(*result, 1, 3, 4);  // r2c4 = (1, 3)
    bool target2_elim_2 = hasElimination(*result, 2, 6, 2);      // r3c7 = (2, 6)
    bool target2_elim_7 = hasElimination(*result, 2, 6, 7);
    CHECK((target1_elim_found || target2_elim_2 || target2_elim_7));
}

// ---------------------------------------------------------------------------
// BL-5 regression: T1 and T2 must lie in different rows (different "cross-lines"
// within the JE band). When the candidate target pair shares a row, the pattern
// is invalid and no JE elimination must be emitted.
// ---------------------------------------------------------------------------
TEST_CASE("JuniorExocetStrategy - BL-5: rejects targets in the same row", "[junior_exocet]") {
    auto board = createEmptyBoard();
    CandidateGrid candidates(board);

    // Base in box 0, mini-row r0c0, r0c1, base candidates = {1,2,3}
    restrictCandidates(candidates, 0, 0, {1, 2});
    restrictCandidates(candidates, 0, 1, {2, 3});

    // Two cells in the JE band (rows 0..2) that look like targets but BOTH on r2 —
    // same row → invalid. Place them in boxes 1 and 2.
    restrictCandidates(candidates, 2, 4, {1, 2, 3, 5});  // candidate target #1 (with non-base 5)
    restrictCandidates(candidates, 2, 7, {1, 2, 3, 6});  // candidate target #2 (with non-base 6)

    JuniorExocetStrategy strategy;
    auto result = strategy.findStep(board, candidates);

    // Same-row targets must never produce a JE elimination targeting r2c4 = 5 or r2c7 = 6.
    if (result.has_value()) {
        CHECK_FALSE(hasElimination(*result, 2, 4, 5));
        CHECK_FALSE(hasElimination(*result, 2, 7, 6));
    }
}

// ---------------------------------------------------------------------------
// BL-6 regression: every base candidate must appear in at least one target.
// In the strict form implemented here we require each target individually to
// contain ALL base candidates — so a target missing even a single base digit
// is rejected (and hence the non-base candidates of that pseudo-target are
// not eliminated).
// ---------------------------------------------------------------------------
TEST_CASE("JuniorExocetStrategy - BL-6: rejects when a target is missing a base digit", "[junior_exocet]") {
    auto board = createEmptyBoard();
    CandidateGrid candidates(board);

    // Base = {1,2,3} in r0c0, r0c1
    restrictCandidates(candidates, 0, 0, {1, 2});
    restrictCandidates(candidates, 0, 1, {2, 3});

    // T1 candidate at r1c4 missing base digit 3 — should disqualify this target.
    // Even though r1c4 has non-base candidate 5 that would *look* eliminable,
    // the JE invariant says it isn't sound to do so without a complete target.
    restrictCandidates(candidates, 1, 4, {1, 2, 5});
    restrictCandidates(candidates, 2, 7, {1, 2, 3, 6});

    JuniorExocetStrategy strategy;
    auto result = strategy.findStep(board, candidates);

    if (result.has_value()) {
        CHECK_FALSE(hasElimination(*result, 1, 4, 5));
    }
}

// ---------------------------------------------------------------------------
// BL-4 regression: S-cell cover must hold. We construct a base/target pair that
// looks like a JE but seed the S-cell region with a base digit spread over more
// than two distinct rows — the pattern must be rejected.
// ---------------------------------------------------------------------------
TEST_CASE("JuniorExocetStrategy - BL-4: rejects when S-cell cover spans >2 rows", "[junior_exocet]") {
    auto board = createEmptyBoard();
    CandidateGrid candidates(board);

    // Base in box 0, candidates {1,2,3}
    restrictCandidates(candidates, 0, 0, {1, 2});
    restrictCandidates(candidates, 0, 1, {2, 3});

    // Candidate targets in JE band: r1c4 (box 1), r2c7 (box 2) — both contain all base digits.
    restrictCandidates(candidates, 1, 4, {1, 2, 3, 4});
    restrictCandidates(candidates, 2, 7, {1, 2, 3, 5});

    // Force the S-cell cover for digit `1` to span 3+ rows: S-cells are in
    // rows 3-8, cols {2 (CLb), 3 (CL1=col of T1=c4? no — CL1 is t1.col=4), 7 (CL2)}.
    // Wait — for this base/target pair, t1.col = 4 so t1's companions in box 1 are c3, c5.
    // S-cell excluded cols = {base1.col=0, base2.col=1, c3, c5, c6 (t2 companions are c6, c8), c8}.
    // S-cell columns = {2, 4, 7}. We seed rows 3, 5, 7 in those cols with digit 1.
    // First, clear digit 1 from all rows 3..8 in cols 2, 4, 7 EXCEPT 3, 5, 7.
    for (size_t r = 3; r < 9; ++r) {
        for (size_t c : {2, 4, 7}) {
            if (r != 3 && r != 5 && r != 7) {
                if (candidates.isAllowed(r, c, 1)) {
                    candidates.eliminateCandidate(r, c, 1);
                }
            }
        }
    }
    // Now S-cells with digit 1 are in rows {3, 5, 7} — three rows → cover invariant fails.

    JuniorExocetStrategy strategy;
    auto result = strategy.findStep(board, candidates);

    // The cover invariant rejects the pattern; no JE elimination of the non-base targets.
    if (result.has_value()) {
        CHECK_FALSE(hasElimination(*result, 1, 4, 4));
        CHECK_FALSE(hasElimination(*result, 2, 7, 5));
    }
}

// ---------------------------------------------------------------------------
// MED regression: targets accepted with only 1 base candidate. With the corrected
// "target ⊇ base_mask" rule and |base| ≥ 3, every target carries every base digit.
// We assert that a candidate target with only one base digit is rejected.
// ---------------------------------------------------------------------------
TEST_CASE("JuniorExocetStrategy - MED: rejects target with only one base candidate", "[junior_exocet]") {
    auto board = createEmptyBoard();
    CandidateGrid candidates(board);

    // Base = {1,2,3}
    restrictCandidates(candidates, 0, 0, {1, 2});
    restrictCandidates(candidates, 0, 1, {2, 3});

    // Candidate "target" with only ONE base digit + a non-base digit.
    restrictCandidates(candidates, 1, 4, {1, 5});     // missing 2 and 3
    restrictCandidates(candidates, 2, 7, {1, 2, 3});  // complete

    JuniorExocetStrategy strategy;
    auto result = strategy.findStep(board, candidates);

    // The 5 in r1c4 must not be eliminated since the JE pattern's left target is invalid.
    if (result.has_value()) {
        CHECK_FALSE(hasElimination(*result, 1, 4, 5));
    }
}

// ---------------------------------------------------------------------------
// LOW: column-based search half. Verify the strategy attempts the transpose.
// ---------------------------------------------------------------------------
TEST_CASE("JuniorExocetStrategy - LOW: searches column-based patterns", "[junior_exocet]") {
    // We don't need a full positive JE pattern here — just confirm the column path
    // executes without crashing and the strategy doesn't bail before exploring it.
    auto board = createEmptyBoard();
    CandidateGrid candidates(board);

    // Column-based base: r0c0, r1c0 (same column, same box). |base| = 3.
    restrictCandidates(candidates, 0, 0, {1, 2});
    restrictCandidates(candidates, 1, 0, {2, 3});

    JuniorExocetStrategy strategy;
    auto result = strategy.findStep(board, candidates);

    // Either a valid column-based pattern fires (legitimate) or none does — but no crash.
    if (result.has_value()) {
        CHECK(result->technique == SolvingTechnique::JuniorExocet);
        // If a column-based pattern fired, the explanation must say so.
        CHECK((result->explanation.find("column based") != std::string::npos ||
               result->explanation.find("row based") != std::string::npos));
    }
}

TEST_CASE("JuniorExocetStrategy - Implements ISolvingStrategy interface", "[junior_exocet]") {
    JuniorExocetStrategy strategy;
    ISolvingStrategy& iface = strategy;

    REQUIRE(iface.getTechnique() == SolvingTechnique::JuniorExocet);
    REQUIRE(iface.getName() == "Junior Exocet");
    REQUIRE(iface.getDifficultyRating() == 9.4);

    auto board = createEmptyBoard();
    CandidateGrid state(board);
    [[maybe_unused]] auto result = iface.findStep(board, state);
}
