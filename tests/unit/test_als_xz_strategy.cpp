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
#include "../../src/core/strategies/als_xz_strategy.h"
#include "../helpers/candidate_test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using sudoku::testing::keepOnly;

TEST_CASE("ALSxZStrategy - Metadata", "[als_xz]") {
    ALSxZStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::ALSxZ);
    REQUIRE(strategy.getName() == "ALS-XZ");
    REQUIRE(strategy.getDifficultyRating() == 6.8);
}

TEST_CASE("ALSxZStrategy - Returns nullopt for complete board", "[als_xz]") {
    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    ALSxZStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("ALSxZStrategy - Detects ALS-XZ with two 2-cell ALSs", "[als_xz]") {
    // ALS A: 2 cells in row 0 with 3 candidates total (N=2, N+1=3)
    //   (0,0) {1,2} and (0,1) {2,3} → union = {1,2,3}
    // ALS B: 2 cells in row 2 with 3 candidates total
    //   (2,0) {2,4} and (2,1) {3,4} → union = {2,3,4}
    // Common = {2,3}. Try X=2: A's cells with 2 = {(0,0),(0,1)}, B's cells with 2 = {(2,0)}.
    //   (0,0) sees (2,0) via col 0 ✓, (0,1) sees (2,0) via box 0 ✓. All see = yes. X=2 is restricted.
    // Z=3: A's cells with 3 = {(0,1)}, B's cells with 3 = {(2,1)}.
    //   Eliminate 3 from cells outside both ALSs that see (0,1) and (2,1).
    //   Target (1,1): sees (0,1) via col 1, sees (2,1) via col 1. In box 0. Has candidate 3.
    auto board = BoardData::filled(5);  // all filled

    board[0][0] = 0;
    board[0][1] = 0;
    board[2][0] = 0;
    board[2][1] = 0;
    board[1][1] = 0;  // target

    CandidateGrid state(board);
    keepOnly(state, 0, 0, {1, 2});
    keepOnly(state, 0, 1, {2, 3});
    keepOnly(state, 2, 0, {2, 4});
    keepOnly(state, 2, 1, {3, 4});
    keepOnly(state, 1, 1, {3, 7});  // target with candidate 3

    ALSxZStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->technique == SolvingTechnique::ALSxZ);
    REQUIRE(result->type == SolveStepType::Elimination);
    REQUIRE(result->rating == 6.8);

    bool found_target = false;
    for (const auto& elim : result->eliminations) {
        REQUIRE(elim.value == 3);
        if (elim.position.row == 1 && elim.position.col == 1) {
            found_target = true;
        }
    }
    REQUIRE(found_target);
}

TEST_CASE("ALSxZStrategy - Explanation contains technique name", "[als_xz]") {
    auto board = BoardData::filled(5);
    board[0][0] = 0;
    board[0][1] = 0;
    board[2][0] = 0;
    board[2][1] = 0;
    board[1][1] = 0;

    CandidateGrid state(board);
    keepOnly(state, 0, 0, {1, 2});
    keepOnly(state, 0, 1, {2, 3});
    keepOnly(state, 2, 0, {2, 4});
    keepOnly(state, 2, 1, {3, 4});
    keepOnly(state, 1, 1, {3, 7});

    ALSxZStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->explanation.find("ALS-XZ") != std::string::npos);
    REQUIRE(result->explanation.find("eliminates") != std::string::npos);
}

TEST_CASE("ALSxZStrategy - Returns nullopt when no restricted common", "[als_xz]") {
    // Two ALSs with common candidates but X-cells don't all see each other
    auto board = BoardData::filled(5);
    board[0][0] = 0;
    board[0][1] = 0;
    board[8][7] = 0;  // Far away — won't see ALS A cells
    board[8][8] = 0;

    CandidateGrid state(board);
    keepOnly(state, 0, 0, {1, 2});
    keepOnly(state, 0, 1, {2, 3});
    keepOnly(state, 8, 7, {2, 4});
    keepOnly(state, 8, 8, {3, 4});

    ALSxZStrategy strategy;
    auto result = strategy.findStep(board, state);

    // (0,0){1,2} and (0,1){2,3} vs (8,7){2,4} and (8,8){3,4}
    // X=2: A's 2-cells = {(0,0),(0,1)}, B's 2-cells = {(8,7)}
    // (0,1) sees (8,7)? row 0 vs 8, col 1 vs 7, box 0 vs 8 → NO. Not restricted.
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("ALSxZStrategy - Detects ALS-XZ with 3-cell ALS", "[als_xz]") {
    // ALS A: 3 cells in row 0 with 4 candidates total (N=3, N+1=4)
    //   (0,0) {1,2}, (0,1) {2,3}, (0,2) {3,4} → union = {1,2,3,4}
    // ALS B: 2 cells in row 2 with 3 candidates total
    //   (2,0) {2,5} and (2,1) {4,5} → union = {2,4,5}
    // Common = {1,2,3,4} & {2,4,5} = {2,4}
    // Try X=2: A's 2-cells = {(0,0),(0,1)}, B's 2-cells = {(2,0)}
    //   (0,0) sees (2,0) via col 0 ✓, (0,1) sees (2,0) via box 0 ✓. Restricted ✓
    // Z=4: A's 4-cells = {(0,2)}, B's 4-cells = {(2,1)}
    //   Eliminate 4 from cells seeing (0,2) and (2,1)
    //   Target (1,2): sees (0,2) via col 2 ✓, sees (2,1) via box 0 ✓. Has candidate 4. ✓
    auto board = BoardData::filled(5);
    board[0][0] = 0;
    board[0][1] = 0;
    board[0][2] = 0;
    board[2][0] = 0;
    board[2][1] = 0;
    board[1][2] = 0;  // target

    CandidateGrid state(board);
    keepOnly(state, 0, 0, {1, 2});
    keepOnly(state, 0, 1, {2, 3});
    keepOnly(state, 0, 2, {3, 4});
    keepOnly(state, 2, 0, {2, 5});
    keepOnly(state, 2, 1, {4, 5});
    keepOnly(state, 1, 2, {4, 7});  // target with candidate 4

    ALSxZStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->technique == SolvingTechnique::ALSxZ);

    bool found_target = false;
    for (const auto& elim : result->eliminations) {
        if (elim.position.row == 1 && elim.position.col == 2 && elim.value == 4) {
            found_target = true;
        }
    }
    REQUIRE(found_target);
}

TEST_CASE("ALSxZStrategy - Detects column-based ALSs", "[als_xz]") {
    // Place ALS cells in different boxes so they can only form column-based ALSs.
    // ALS A: 2 cells in col 0 (different boxes)
    //   (0,0) {1,2} and (3,0) {2,3} → union = {1,2,3}
    // ALS B: 2 cells in col 1 (different boxes)
    //   (0,1) {2,4} and (3,1) {3,4} → union = {2,3,4}
    // Common = {2,3}. X=2: A's 2-cells = {(0,0),(3,0)}, B's 2-cells = {(0,1)}
    //   (0,0) sees (0,1) via row 0 ✓, (3,0) sees (0,1)? row 3≠0, col 0≠1, box 0≠0(3,0 is box 3) → NO
    // X=3: A's 3-cells = {(3,0)}, B's 3-cells = {(3,1)}
    //   (3,0) sees (3,1) via row 3 ✓. Restricted ✓
    // Z=2: A's 2-cells = {(0,0),(3,0)}, B's 2-cells = {(0,1)}
    //   Eliminate 2 from cells seeing (0,0),(3,0),(0,1)
    //   Target (1,0): sees (0,0) col 0 ✓, (3,0) col 0 ✓, (0,1) box 0 ✓. ✓
    auto board = BoardData::filled(5);
    board[0][0] = 0;
    board[3][0] = 0;
    board[0][1] = 0;
    board[3][1] = 0;
    board[1][0] = 0;  // target

    CandidateGrid state(board);
    keepOnly(state, 0, 0, {1, 2});
    keepOnly(state, 3, 0, {2, 3});
    keepOnly(state, 0, 1, {2, 4});
    keepOnly(state, 3, 1, {3, 4});
    keepOnly(state, 1, 0, {2, 8});  // target

    ALSxZStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->technique == SolvingTechnique::ALSxZ);

    bool found_target = false;
    for (const auto& elim : result->eliminations) {
        if (elim.position.row == 1 && elim.position.col == 0 && elim.value == 2) {
            found_target = true;
        }
    }
    REQUIRE(found_target);
}

TEST_CASE("ALSxZStrategy - Detects box-based ALSs", "[als_xz]") {
    // ALS A: 2 cells in box 0 with 3 candidates
    //   (0,0) {1,2} and (1,1) {2,3} → union = {1,2,3}, both in box 0
    // ALS B: 2 cells in box 1 with 3 candidates
    //   (0,3) {2,4} and (1,4) {3,4} → union = {2,3,4}, both in box 1
    // Common = {2,3}. X=2: A's 2-cells = {(0,0),(1,1)}, B's 2-cells = {(0,3)}
    //   (0,0) sees (0,3) via row 0 ✓, (1,1) sees (0,3)? row 1≠0, col 1≠3, box 0≠1 → NO.
    // X=3: A's 3-cells = {(1,1)}, B's 3-cells = {(1,4)}
    //   (1,1) sees (1,4) via row 1 ✓. Restricted ✓
    // Z=2: A's 2-cells = {(0,0),(1,1)}, B's 2-cells = {(0,3)}
    //   Eliminate 2 from cells seeing (0,0),(1,1),(0,3)
    //   Target (0,1): sees (0,0) row ✓, sees (1,1) box 0 ✓, sees (0,3) row ✓. Has 2. ✓
    auto board = BoardData::filled(5);
    board[0][0] = 0;
    board[1][1] = 0;
    board[0][3] = 0;
    board[1][4] = 0;
    board[0][1] = 0;  // target

    CandidateGrid state(board);
    keepOnly(state, 0, 0, {1, 2});
    keepOnly(state, 1, 1, {2, 3});
    keepOnly(state, 0, 3, {2, 4});
    keepOnly(state, 1, 4, {3, 4});
    keepOnly(state, 0, 1, {2, 6});  // target

    ALSxZStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->technique == SolvingTechnique::ALSxZ);

    bool found_target = false;
    for (const auto& elim : result->eliminations) {
        if (elim.position.row == 0 && elim.position.col == 1 && elim.value == 2) {
            found_target = true;
        }
    }
    REQUIRE(found_target);
}

TEST_CASE("ALSxZStrategy - Returns nullopt when ALSs share cells", "[als_xz]") {
    // Two ALS candidates that share cell (0,1) — should be rejected
    auto board = BoardData::filled(5);
    board[0][0] = 0;
    board[0][1] = 0;  // shared
    board[0][2] = 0;

    CandidateGrid state(board);
    keepOnly(state, 0, 0, {1, 2});
    keepOnly(state, 0, 1, {2, 3});  // shared by both "ALSs"
    keepOnly(state, 0, 2, {3, 4});
    // ALS A would be {(0,0),(0,1)} and ALS B {(0,1),(0,2)} — they share (0,1)

    ALSxZStrategy strategy;
    auto result = strategy.findStep(board, state);

    // With only 3 cells, impossible to form 2 non-overlapping ALSs → nullopt
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("ALSxZStrategy - Returns nullopt when valid pair but no targets", "[als_xz]") {
    // Valid ALS pair with restricted common, but no external cell sees all Z-cells
    // ALS A in row 0: (0,0) {1,2}, (0,1) {2,3} → union {1,2,3}
    // ALS B in row 8: (8,0) {2,4}, (8,1) {3,4} → union {2,3,4}
    // X=2: (0,0),(0,1) see (8,0) via col 0 (0,0) and box? (0,1) sees (8,0)? No.
    // This won't form a restricted link so nullopt anyway.
    //
    // Better: ALSs far apart with restricted X but Z-cells that no external cell sees.
    // ALS A in row 0: (0,0) {1,2}, (0,1) {2,3} → {1,2,3}
    // ALS B in row 0: (0,7) {2,4}, (0,8) {3,4} → {2,3,4}
    // X=2: A-2-cells={(0,0),(0,1)}, B-2-cells={(0,7)}. All see via row 0 ✓
    // Z=3: A-3-cells={(0,1)}, B-3-cells={(0,8)}.
    //   Need cell seeing (0,1) and (0,8): same row 0 works but those are in the ALSs.
    //   External cell seeing both via col: col 1 and col 8 — only (0,1) and (0,8) themselves.
    //   Box: (0,1) in box 0, (0,8) in box 2 — no external cell is in both.
    //   Row 0: cells (0,2)-(0,6) see both via row, but are they empty and have candidate 3?
    //   We'll fill them to prevent any targets.
    auto board = BoardData::filled(5);
    board[0][0] = 0;
    board[0][1] = 0;
    board[0][7] = 0;
    board[0][8] = 0;
    // No other empty cells in row 0 with Z candidate

    CandidateGrid state(board);
    keepOnly(state, 0, 0, {1, 2});
    keepOnly(state, 0, 1, {2, 3});
    keepOnly(state, 0, 7, {2, 4});
    keepOnly(state, 0, 8, {3, 4});

    ALSxZStrategy strategy;
    auto result = strategy.findStep(board, state);

    // No external empty cell with candidate 3 sees both (0,1) and (0,8) → nullopt
    REQUIRE_FALSE(result.has_value());
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — Catch2 TEST_CASE with multiple REQUIRE/loop checks; complexity is inherent to test coverage
TEST_CASE("ALSxZStrategy - Multiple elimination targets", "[als_xz]") {
    // ALS A in row 0: (0,0) {1,2}, (0,1) {2,3} → union {1,2,3}
    // ALS B in row 2: (2,0) {2,4}, (2,1) {3,4} → union {2,3,4}
    // X=2, Z=3. Z-cells: A=(0,1), B=(2,1)
    // Two targets in col 1 that see both Z-cells:
    //   Target1 (1,1): sees (0,1) col 1 ✓, sees (2,1) col 1 ✓
    //   Target2: needs to see (0,1) and (2,1). Another cell in box 0 col 1 = (1,1) only.
    //   Box 0 has rows 0-2, col 1 → (0,1) is in ALS, (1,1) target, (2,1) in ALS.
    //   Use target in same col but different box won't see both.
    //   Actually any cell in col 1 sees both: (3,1),(4,1),...,(8,1)
    auto board = BoardData::filled(5);
    board[0][0] = 0;
    board[0][1] = 0;
    board[2][0] = 0;
    board[2][1] = 0;
    board[1][1] = 0;  // target 1
    board[3][1] = 0;  // target 2

    CandidateGrid state(board);
    keepOnly(state, 0, 0, {1, 2});
    keepOnly(state, 0, 1, {2, 3});
    keepOnly(state, 2, 0, {2, 4});
    keepOnly(state, 2, 1, {3, 4});
    keepOnly(state, 1, 1, {3, 7});  // target 1
    keepOnly(state, 3, 1, {3, 8});  // target 2

    ALSxZStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->technique == SolvingTechnique::ALSxZ);

    bool found_t1 = false;
    bool found_t2 = false;
    for (const auto& elim : result->eliminations) {
        REQUIRE(elim.value == 3);
        if (elim.position.row == 1 && elim.position.col == 1) {
            found_t1 = true;
        }
        if (elim.position.row == 3 && elim.position.col == 1) {
            found_t2 = true;
        }
    }
    REQUIRE(found_t1);
    REQUIRE(found_t2);
}

TEST_CASE("ALSxZStrategy - Explanation data has correct roles", "[als_xz]") {
    // Reuse the basic 2-cell ALS test setup
    auto board = BoardData::filled(5);
    board[0][0] = 0;
    board[0][1] = 0;
    board[2][0] = 0;
    board[2][1] = 0;
    board[1][1] = 0;

    CandidateGrid state(board);
    keepOnly(state, 0, 0, {1, 2});
    keepOnly(state, 0, 1, {2, 3});
    keepOnly(state, 2, 0, {2, 4});
    keepOnly(state, 2, 1, {3, 4});
    keepOnly(state, 1, 1, {3, 7});

    ALSxZStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());

    // ALS A has 2 cells → SetA, ALS B has 2 cells → SetB
    const auto& roles = result->explanation_data.position_roles;
    REQUIRE(roles.size() == 4);
    REQUIRE(roles[0] == CellRole::SetA);
    REQUIRE(roles[1] == CellRole::SetA);
    REQUIRE(roles[2] == CellRole::SetB);
    REQUIRE(roles[3] == CellRole::SetB);

    // Values should include X, Z, and ALS sizes
    REQUIRE(result->explanation_data.values.size() == 4);
}

TEST_CASE("ALSxZStrategy - Can be used through ISolvingStrategy interface", "[als_xz]") {
    std::unique_ptr<ISolvingStrategy> strategy = std::make_unique<ALSxZStrategy>();

    REQUIRE(strategy->getTechnique() == SolvingTechnique::ALSxZ);
    REQUIRE(strategy->getName() == "ALS-XZ");
    REQUIRE(strategy->getDifficultyRating() == 6.8);

    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    auto result = strategy->findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}
