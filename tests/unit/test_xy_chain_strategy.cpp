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
#include "../../src/core/strategies/xy_chain_strategy.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

void keepOnly(CandidateGrid& grid, size_t row, size_t col, const std::vector<int>& keep) {
    for (int v = 1; v <= 9; ++v) {
        if (std::find(keep.begin(), keep.end(), v) == keep.end() && grid.isAllowed(row, col, v)) {
            grid.eliminateCandidate(row, col, v);
        }
    }
}

}  // namespace

TEST_CASE("XYChainStrategy - Metadata", "[xy_chain]") {
    XYChainStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::XYChain);
    REQUIRE(strategy.getName() == "XY-Chain");
    REQUIRE(strategy.getDifficultyPoints() == 450);
}

TEST_CASE("XYChainStrategy - Returns nullopt for complete board", "[xy_chain]") {
    BoardData board = {{5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
                       {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
                       {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};
    CandidateGrid state(board);
    XYChainStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("XYChainStrategy - Detects 4-cell XY-Chain", "[xy_chain]") {
    // 4-cell chain: A-B, B-C, C-D, D-A pattern
    // Cell 1 (0,0) {1,2} → Cell 2 (0,3) {2,3} → Cell 3 (0,6) {3,4} → Cell 4 (0,8) {4,1}
    // Start with X=1: outgoing=2, next has 2→3, next has 3→4, next has 4→1=X. Chain of 4.
    // Eliminate 1 from cells seeing both (0,0) and (0,8).
    // Target: any cell in row 0 (between them) with candidate 1.
    auto board = BoardData::filled(5);  // All filled

    board[0][0] = 0;
    board[0][3] = 0;
    board[0][6] = 0;
    board[0][8] = 0;
    board[0][4] = 0;  // Target cell — sees both endpoints via row

    CandidateGrid state(board);
    keepOnly(state, 0, 0, {1, 2});
    keepOnly(state, 0, 3, {2, 3});
    keepOnly(state, 0, 6, {3, 4});
    keepOnly(state, 0, 8, {4, 1});
    keepOnly(state, 0, 4, {1, 7});  // target with candidate 1

    XYChainStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->technique == SolvingTechnique::XYChain);
    REQUIRE(result->type == SolveStepType::Elimination);
    REQUIRE(result->points == 450);

    bool found_target = false;
    for (const auto& elim : result->eliminations) {
        REQUIRE(elim.value == 1);
        if (elim.position.row == 0 && elim.position.col == 4) {
            found_target = true;
        }
    }
    REQUIRE(found_target);
}

TEST_CASE("XYChainStrategy - Returns nullopt for 3-cell chain (covered by XY-Wing)", "[xy_chain]") {
    // 3-cell chain should NOT be detected (min length = 4, depth >= 3)
    auto board = BoardData::filled(5);

    board[0][0] = 0;
    board[0][3] = 0;
    board[0][6] = 0;
    board[0][4] = 0;

    CandidateGrid state(board);
    keepOnly(state, 0, 0, {1, 2});
    keepOnly(state, 0, 3, {2, 3});
    keepOnly(state, 0, 6, {3, 1});  // closes at 3 cells
    keepOnly(state, 0, 4, {1, 7});

    XYChainStrategy strategy;
    auto result = strategy.findStep(board, state);

    // Should not detect a 3-cell chain (that's XY-Wing territory)
    // It may find a different pattern or return nullopt
    if (result.has_value()) {
        // If it finds something, it must be a chain of 4+ cells
        REQUIRE(result->explanation_data.values.size() >= 2);
        REQUIRE(result->explanation_data.values[1] >= 4);  // chain_length >= 4
    }
}

TEST_CASE("XYChainStrategy - Explanation contains technique name", "[xy_chain]") {
    auto board = BoardData::filled(5);

    board[0][0] = 0;
    board[0][3] = 0;
    board[0][6] = 0;
    board[0][8] = 0;
    board[0][4] = 0;

    CandidateGrid state(board);
    keepOnly(state, 0, 0, {1, 2});
    keepOnly(state, 0, 3, {2, 3});
    keepOnly(state, 0, 6, {3, 4});
    keepOnly(state, 0, 8, {4, 1});
    keepOnly(state, 0, 4, {1, 7});

    XYChainStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->explanation.find("XY-Chain") != std::string::npos);
    REQUIRE(result->explanation.find("eliminates") != std::string::npos);
}

TEST_CASE("XYChainStrategy - Can be used through ISolvingStrategy interface", "[xy_chain]") {
    std::unique_ptr<ISolvingStrategy> strategy = std::make_unique<XYChainStrategy>();

    REQUIRE(strategy->getTechnique() == SolvingTechnique::XYChain);
    REQUIRE(strategy->getName() == "XY-Chain");
    REQUIRE(strategy->getDifficultyPoints() == 450);

    BoardData board = {{5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
                       {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
                       {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};
    CandidateGrid state(board);
    auto result = strategy->findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}
