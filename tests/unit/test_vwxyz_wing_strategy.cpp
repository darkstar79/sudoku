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
#include "../../src/core/strategies/vwxyz_wing_strategy.h"
#include "../helpers/candidate_test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using sudoku::testing::createEmptyBoard;
using sudoku::testing::keepOnly;

TEST_CASE("VWXYZWingStrategy - Metadata", "[vwxyz_wing]") {
    VWXYZWingStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::VWXYZWing);
    REQUIRE(strategy.getName() == "VWXYZ-Wing");
    REQUIRE(strategy.getDifficultyRating() == 4.8);
}

TEST_CASE("VWXYZWingStrategy - Returns nullopt for complete board", "[vwxyz_wing]") {
    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    VWXYZWingStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("VWXYZWingStrategy - Detects VWXYZ-Wing with elimination", "[vwxyz_wing]") {
    // VWXYZ-Wing: 5 cells, 5 values, exactly 1 non-restricted value (Z).
    //
    // Pivot at (0,0) {1,2,3,4} — 4 candidates
    // Wing1 at (0,3) {1,5}     — sees pivot via row 0
    // Wing2 at (3,0) {2,5}     — sees pivot via col 0
    // Wing3 at (1,1) {3,5}     — sees pivot via box 0
    // Wing4 at (2,2) {4,5}     — sees pivot via box 0
    // Union = {1,2,3,4,5} — 5 values ✓
    //
    // Restricted analysis:
    //   1: in (0,0) and (0,3) — same row → restricted ✓
    //   2: in (0,0) and (3,0) — same col → restricted ✓
    //   3: in (0,0) and (1,1) — same box → restricted ✓
    //   4: in (0,0) and (2,2) — same box → restricted ✓
    //   5: in (0,3),(3,0),(1,1),(2,2) — (0,3) and (3,0): diff row/col/box → NOT restricted
    // Z = 5 (unique non-restricted value)
    //
    // Eliminate 5 from external cells seeing ALL Z-cells: {(0,3),(3,0),(1,1),(2,2)}
    // Target: must see all 4 Z-cells. Hard with spread cells, but (0,0) is excluded.
    //
    // Let's place a target at (1,0) which sees:
    //   (0,3): diff row/col, but same... no, (1,0) box 0, (0,3) box 1 → doesn't see
    //
    // Better approach: Z only in cells within same box + one in same row/col.
    // Redesign: put Z-cells closer together.
    //
    // Pivot at (0,0) {1,2,3,4}
    // Wing1 at (0,1) {1,5}     — sees pivot via row 0 + box 0
    // Wing2 at (1,0) {2,5}     — sees pivot via col 0 + box 0
    // Wing3 at (2,0) {3,5}     — sees pivot via col 0 + box 0
    // Wing4 at (0,2) {4,5}     — sees pivot via row 0 + box 0
    // Union = {1,2,3,4,5} — 5 values ✓
    //
    // Restricted analysis:
    //   1: in (0,0) and (0,1) — same row + box → restricted ✓
    //   2: in (0,0) and (1,0) — same col + box → restricted ✓
    //   3: in (0,0) and (2,0) — same col + box → restricted ✓
    //   4: in (0,0) and (0,2) — same row + box → restricted ✓
    //   5: in (0,1),(1,0),(2,0),(0,2) — all in box 0 → all mutually visible → restricted!
    //
    // All restricted → no Z → no VWXYZ-Wing. Need non-restricted Z.
    //
    // Move one wing out of the box:
    // Pivot at (0,0) {1,2,3,4}
    // Wing1 at (0,1) {1,5}     — sees pivot via row + box
    // Wing2 at (1,0) {2,5}     — sees pivot via col + box
    // Wing3 at (2,0) {3,5}     — sees pivot via col + box
    // Wing4 at (0,6) {4,5}     — sees pivot via row 0 (different box)
    // Union = {1,2,3,4,5} ✓
    //
    // Restricted:
    //   1: (0,0)+(0,1) — same row+box → restricted ✓
    //   2: (0,0)+(1,0) — same col+box → restricted ✓
    //   3: (0,0)+(2,0) — same col+box → restricted ✓
    //   4: (0,0)+(0,6) — same row → restricted ✓
    //   5: (0,1),(1,0),(2,0),(0,6) — (0,6) is box 2, others box 0
    //      (0,1) sees (1,0)? col diff, row diff, box 0 same → yes
    //      (0,1) sees (0,6)? same row → yes
    //      (1,0) sees (0,6)? diff row/col/box → NO!
    //      → NOT all mutually visible → 5 is non-restricted ✓
    // Z = 5, exactly 1 non-restricted ✓
    //
    // Z-cells: (0,1), (1,0), (2,0), (0,6)
    // Target must see all 4 Z-cells and have candidate 5.
    // (0,0) is excluded (it's the pivot).
    // Candidates at any cell seeing all Z-cells:
    //   Need to see (0,1)=row0/box0, (1,0)=col0/box0, (2,0)=col0/box0, (0,6)=row0/box2
    //   A cell in row 0 col 0 sees all via row+col → but that's the pivot.
    //   Other cells: hard to find one seeing all 4 unless in row 0 AND col 0.
    //
    // Let me try another arrangement where Z-cells are more concentrated.
    // Pivot at (4,4) {1,2,3,4}
    // Wing1 at (4,0) {1,5}     — sees pivot via row 4
    // Wing2 at (0,4) {2,5}     — sees pivot via col 4
    // Wing3 at (3,3) {3,5}     — sees pivot via box 4
    // Wing4 at (5,5) {4,5}     — sees pivot via box 4
    // Union = {1,2,3,4,5} ✓
    //
    // Restricted:
    //   1: (4,4)+(4,0) — same row → restricted ✓
    //   2: (4,4)+(0,4) — same col → restricted ✓
    //   3: (4,4)+(3,3) — same box → restricted ✓
    //   4: (4,4)+(5,5) — same box → restricted ✓
    //   5: (4,0),(0,4),(3,3),(5,5) —
    //      (4,0) sees (0,4)? diff row/col; box: (4,0)=box3, (0,4)=box1 → NO
    //      → NOT all mutually visible → 5 is non-restricted ✓
    //
    // Z-cells: (4,0), (0,4), (3,3), (5,5)
    // Target at (3,4) — sees (0,4) via col, (3,3) via box4/row3, (5,5) via box4
    //   sees (4,0)? row diff, col diff, box: (3,4)=box4, (4,0)=box3 → NO
    //
    // This is tricky. Let me concentrate Z-cells in fewer regions.
    // Pivot at (4,4) {1,2,3,4,5} — 5 candidates (pivot has Z too)
    // Wing1 at (4,0) {1,5}     — sees pivot via row 4
    // Wing2 at (0,4) {2,5}     — sees pivot via col 4
    // Wing3 at (3,3) {3,5}     — sees pivot via box 4
    // Wing4 at (5,5) {4,5}     — sees pivot via box 4
    // Union = {1,2,3,4,5} ✓ (same)
    //
    // Z-cells for 5: (4,4),(4,0),(0,4),(3,3),(5,5) — 5 cells, very hard for external cell to see all.
    //
    // Simplest approach: Z in only 2 cells, both in same row.
    // Pivot at (0,0) {1,2,3,5} — 4 candidates (has Z=5)
    // Wing1 at (0,3) {1,4}     — sees pivot via row (no Z)
    // Wing2 at (3,0) {2,4}     — sees pivot via col (no Z)
    // Wing3 at (1,1) {3,4}     — sees pivot via box (no Z)
    // Wing4 at (0,6) {4,5}     — sees pivot via row (has Z=5)
    // Wait, union = {1,2,3,4,5}? Pivot={1,2,3,5}, wings contribute {4}. Yes ✓
    //
    // But wing4 {4,5} must share candidate with pivot. pivot_mask & {4,5} = {5} → OK.
    //
    // Restricted:
    //   1: (0,0) only → trivially restricted ✓
    //   2: (0,0) only → trivially restricted ✓
    //   3: (0,0) only → trivially restricted ✓
    //   4: (0,3),(3,0),(1,1),(0,6) — (0,3) sees (3,0)? NO → non-restricted
    //   5: (0,0),(0,6) — same row → restricted ✓
    // Z = 4 (the single non-restricted value)
    //
    // Z-cells for 4: (0,3),(3,0),(1,1),(0,6) — 4 cells, hard to find target seeing all.
    //
    // Even simpler: make Z in just 2 cells in same box (non-restricted because not all see each other?
    // No, 2 cells in same box DO mutually see each other).
    //
    // OK let me try: Z in 2 cells in different boxes, same column.
    // Pivot at (0,0) {1,2,3,4}
    // Wing1 at (0,1) {1,5}     — row+box → restricted
    // Wing2 at (1,0) {2,5}     — col+box → restricted
    // Wing3 at (2,0) {3,5}     — col+box → restricted
    // Wing4 at (3,0) {4,5}     — col (diff box) → restricted for 4
    // 5: in (0,1),(1,0),(2,0),(3,0) —
    //    (0,1) sees (1,0)? box0 yes. (0,1) sees (2,0)? box0 yes. (0,1) sees (3,0)? no!
    //    → non-restricted ✓
    // Z = 5, Z-cells = (0,1),(1,0),(2,0),(3,0)
    // Target seeing all 4: must see row/col/box of each.
    //   (1,0),(2,0),(3,0) all in col 0 → target in col 0 sees them.
    //   (0,1) → target must see it too. If target in col 0 row X, needs row 0 or box 0.
    //   Target (2,1) — col 0? No. Target (0,0) is pivot.
    //   Target in col 0: sees (1,0),(2,0),(3,0). Sees (0,1) if in box 0 or row 0.
    //   (1,0) is in box 0 row 1 → doesn't see (0,1) via row. But (1,0) box 0, (0,1) box 0 → sees.
    //   Wait I need TARGET to see all Z-cells.
    //   Target at (4,0): col 0 → sees (1,0),(2,0),(3,0). Sees (0,1)? row diff, col diff, box diff → NO.
    //   Target at (1,1): box 0 → sees (0,1),(1,0),(2,0)? (2,0) same box → yes. (3,0)? diff box/row/col → NO.
    //
    // I need to find an elimination target. Let me try fewer Z-cells.
    // Make Z appear in only 2 cells that don't mutually see each other.
    //
    // FINAL APPROACH: Keep it simple. 5 cells in a row with carefully chosen candidates.
    //
    // All 5 cells in row 0 (pivot sees all wings via shared row):
    // Pivot at (0,0) {1,2,3,4}
    // Wing1 at (0,1) {1,5}
    // Wing2 at (0,2) {2,5}
    // Wing3 at (0,3) {3,5}
    // Wing4 at (0,4) {4,5}
    // Union = {1,2,3,4,5} ✓
    //
    // All in row 0, so all cells mutually visible → ALL values restricted → no Z → no pattern.
    //
    // Need wings spread across different visibility groups. Let me use a mixed approach.
    //
    // Pivot at (0,0) {1,2,3,4}
    // Wing1 at (0,4) {1,5}     — same row → sees pivot
    // Wing2 at (0,5) {2,5}     — same row → sees pivot
    // Wing3 at (1,1) {3,5}     — same box → sees pivot
    // Wing4 at (4,0) {4,5}     — same col → sees pivot
    //
    // Union = {1,2,3,4,5} ✓
    //
    // Restricted:
    //   1: (0,0)+(0,4) → same row → restricted ✓
    //   2: (0,0)+(0,5) → same row → restricted ✓
    //   3: (0,0)+(1,1) → same box → restricted ✓
    //   4: (0,0)+(4,0) → same col → restricted ✓
    //   5: in (0,4),(0,5),(1,1),(4,0)
    //      (0,4) sees (0,5)? same row → yes
    //      (0,4) sees (1,1)? diff row, diff col, diff box (box0 vs box1) → NO
    //      → non-restricted ✓
    //
    // Z=5, Z-cells = (0,4),(0,5),(1,1),(4,0)
    // Target must see all 4 and have candidate 5.
    //   (0,4) row 0, (0,5) row 0, (1,1) box 0, (4,0) col 0
    //   Target at (0,0) = pivot, excluded.
    //   Target at (1,0): sees (4,0) col, sees (1,1) box/row, sees (0,4)? no, sees (0,5)? no
    //   Hard to find target seeing both box 0 cells AND row 0 cells.
    //
    // Try: Z only in 2 cells not seeing each other.
    // Pivot {1,2,3,4,5}, 4 wings, 2 wings have Z, 2 don't.
    // Pivot at (4,4) {1,2,3,4,5}
    // Wing1 at (4,0) {1,5}     — row → sees pivot. Z-cell.
    // Wing2 at (0,4) {2,5}     — col → sees pivot. Z-cell.
    // Wing3 at (3,3) {3,4}     — box → sees pivot. No Z.
    // Wing4 at (5,5) {1,4}     — box → sees pivot. No Z.
    //
    // Union: {1,2,3,4,5} ✓
    // Restricted:
    //   1: (4,4),(4,0),(5,5) — (4,0) sees (5,5)? diff everything → NO → non-restricted
    //   Hmm, too many non-restricted. Let me fix.
    //
    // Wing4 at (5,5) {3,4}. Then 1 only in (4,4),(4,0) → same row → restricted.
    // Union: pivot={1,2,3,4,5}, w1={1,5}, w2={2,5}, w3={3,4}, w4={3,4}
    //   = {1,2,3,4,5} ✓
    //
    // Restricted:
    //   1: (4,4),(4,0) → same row → restricted ✓
    //   2: (4,4),(0,4) → same col → restricted ✓
    //   3: (4,4),(3,3),(5,5) → (3,3) sees (5,5)? box4 both → yes. (4,4) sees both → yes → restricted ✓
    //   4: (4,4),(3,3),(5,5) → same as 3 → restricted ✓
    //   5: (4,4),(4,0),(0,4) → (4,0) sees (0,4)? diff row/col/box → NO → non-restricted ✓
    //
    // Z=5, Z-cells = {(4,4),(4,0),(0,4)}
    // Target: must see all 3. (4,4)=box4, (4,0)=row4, (0,4)=col4
    // Target at (4,4) is pivot, excluded.
    // Target needs: row 4 or col 4 or box 4 to see (4,4);
    //               row 4 or col 0 or box 3 to see (4,0);
    //               row 0 or col 4 or box 1 to see (0,4).
    // Target at (4,4) sees all but excluded.
    // Target at (4,8): row 4 sees (4,4) and (4,0). col 8 → doesn't see (0,4). box 5 → no. → NO.
    // Target at (4,4) only candidate. Too restrictive.
    //
    // Simplify further: Z in just 2 cells sharing a row.
    // Pivot at (4,4) {1,2,3,4}
    // Wing1 at (4,0) {1,5}     — row → sees pivot
    // Wing2 at (4,8) {2,5}     — row → sees pivot
    // Wing3 at (3,3) {3,5}     — box → sees pivot
    // Wing4 at (5,5) {4,5}     — box → sees pivot
    //
    // Hmm, all wings have 5, giving Z=5 in all 4 wings (not in pivot).
    // That means pivot has {1,2,3,4} without 5, but union must be {1,2,3,4,5} → yes.
    // But (4,0),(4,8): same row → see each other.
    // (4,0) sees (3,3)? no. So not all Z-cells mutually visible → non-restricted.
    //
    // Check other values:
    //   1: (4,4),(4,0) → same row → restricted ✓
    //   2: (4,4),(4,8) → same row → restricted ✓
    //   3: (4,4),(3,3) → same box → restricted ✓
    //   4: (4,4),(5,5) → same box → restricted ✓
    //   5: (4,0),(4,8),(3,3),(5,5) → (4,0) sees (3,3)? box3 vs box4, diff row/col → NO
    //      → non-restricted ✓
    //
    // Z=5, exactly 1 non-restricted ✓
    // Z-cells: (4,0),(4,8),(3,3),(5,5)
    //
    // Target must see all 4 Z-cells AND have candidate 5.
    // (4,0) in row 4, (4,8) in row 4 → target in row 4 sees both.
    // (3,3) in box 4 → target in box 4 or row 3 or col 3.
    // (5,5) in box 4 → target in box 4 or row 5 or col 5.
    // If target in row 4 AND box 4: row 4 cells in box 4 are (4,3),(4,4),(4,5).
    //   (4,4) is pivot, excluded.
    //   (4,3): sees (3,3) via col 3 → yes! sees (5,5)? diff row, diff col, box4 → yes!
    //          sees (4,0) via row → yes! sees (4,8) via row → yes!
    //   So (4,3) is a valid elimination target.
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    // Pivot at (4,4) {1,2,3,4}
    keepOnly(state, 4, 4, {1, 2, 3, 4});
    // Wing1 at (4,0) {1,5}
    keepOnly(state, 4, 0, {1, 5});
    // Wing2 at (4,8) {2,5}
    keepOnly(state, 4, 8, {2, 5});
    // Wing3 at (3,3) {3,5}
    keepOnly(state, 3, 3, {3, 5});
    // Wing4 at (5,5) {4,5}
    keepOnly(state, 5, 5, {4, 5});

    // Target at (4,3) must have candidate 5
    REQUIRE(state.isAllowed(4, 3, 5));

    VWXYZWingStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->type == SolveStepType::Elimination);
    REQUIRE(result->technique == SolvingTechnique::VWXYZWing);
    REQUIRE(result->rating == 4.8);

    // Check that eliminations include value 5
    bool found_target = false;
    for (const auto& elim : result->eliminations) {
        REQUIRE(elim.value == 5);
        if (elim.position.row == 4 && elim.position.col == 3) {
            found_target = true;
        }
    }
    REQUIRE(found_target);
}

TEST_CASE("VWXYZWingStrategy - No pattern when union has wrong count", "[vwxyz_wing]") {
    // 5 cells but union = 4 values (not 5)
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    keepOnly(state, 4, 4, {1, 2, 3});
    keepOnly(state, 4, 0, {1, 4});
    keepOnly(state, 4, 8, {2, 4});
    keepOnly(state, 3, 3, {3, 4});
    keepOnly(state, 5, 5, {1, 4});  // Union = {1,2,3,4} = 4 values, not 5

    VWXYZWingStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("VWXYZWingStrategy - No pattern when multiple non-restricted values", "[vwxyz_wing]") {
    // 5 cells, 5 values, but 2+ non-restricted values
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    // Pivot at (0,0) {1,2,3}, wings spread so both 4 and 5 are non-restricted
    keepOnly(state, 0, 0, {1, 2, 3});
    keepOnly(state, 0, 3, {1, 4});  // row — 1 restricted
    keepOnly(state, 3, 0, {2, 5});  // col — 2 restricted
    keepOnly(state, 6, 6, {3, 4});  // diff box, diff row/col — doesn't see pivot!

    // Actually wing3 must see pivot. (6,6) doesn't see (0,0). Skip this test arrangement.
    // Wing3 at (1,1) {3,4}, Wing4 at (2,2) {1,5}
    keepOnly(state, 6, 6, {1, 2, 3, 4, 5, 6, 7, 8, 9});  // Reset
    keepOnly(state, 1, 1, {3, 4});
    keepOnly(state, 2, 2, {4, 5});  // Union = {1,2,3,4,5} ✓

    // Check: 4 is in (0,3),(1,1),(2,2) — (0,3) sees (1,1)? no → non-restricted
    //        5 is in (3,0),(2,2) — (3,0) sees (2,2)? no → non-restricted
    // 2 non-restricted values → no pattern

    VWXYZWingStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("VWXYZWingStrategy - Explanation contains technique name", "[vwxyz_wing]") {
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    keepOnly(state, 4, 4, {1, 2, 3, 4});
    keepOnly(state, 4, 0, {1, 5});
    keepOnly(state, 4, 8, {2, 5});
    keepOnly(state, 3, 3, {3, 5});
    keepOnly(state, 5, 5, {4, 5});

    VWXYZWingStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->explanation.find("VWXYZ-Wing") != std::string::npos);
    REQUIRE(result->explanation.find("eliminates") != std::string::npos);
}
