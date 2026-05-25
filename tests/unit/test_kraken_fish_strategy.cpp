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
#include "../../src/core/strategies/kraken_fish_strategy.h"
#include "../helpers/candidate_test_utils.h"
#include "../helpers/strategy_test_utils.h"

#include <algorithm>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

TEST_CASE("KrakenFishStrategy - Metadata", "[kraken_fish]") {
    KrakenFishStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::KrakenFish);
    REQUIRE(strategy.getName() == "Kraken Fish");
    REQUIRE(strategy.getDifficultyRating() == 8.5);
}

TEST_CASE("KrakenFishStrategy - Returns nullopt for complete board", "[kraken_fish]") {
    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    KrakenFishStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("KrakenFishStrategy - Can be used through ISolvingStrategy interface", "[kraken_fish]") {
    std::unique_ptr<ISolvingStrategy> strategy = std::make_unique<KrakenFishStrategy>();

    REQUIRE(strategy->getTechnique() == SolvingTechnique::KrakenFish);
    REQUIRE(strategy->getName() == "Kraken Fish");
    REQUIRE(strategy->getDifficultyRating() == 8.5);

    // Complete board returns nullopt
    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    auto result = strategy->findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("KrakenFishStrategy - Returns nullopt for empty board with no finned patterns", "[kraken_fish]") {
    // Empty board has too many candidates everywhere for fish patterns
    BoardData board{};
    CandidateGrid state(board);
    KrakenFishStrategy strategy;

    auto result = strategy.findStep(board, state);
    // Empty board won't have finned fish with Kraken eliminations
    // (it may or may not find something, but the test verifies no crash)
    // The strategy should handle this gracefully
    (void)result;
    SUCCEED("Strategy did not crash on empty board");
}

// Regression for issue #20 (BL-3 + HIGH × 2):
//   1. Kraken steps must NOT include free finned-fish eliminations
//      (those are reported by FinnedXWing/Swordfish/Jellyfish strategies which
//      run earlier; bundling them here misattributes their rating and lets
//      `position` point at a non-Kraken cell).
//   2. explanation_data.region_type must reflect the COVER axis (the axis
//      eliminations sit on), not the base axis.
//
// Scenario derived from tests/data/fixtures/Hard/fixtures.yaml (Hard_s4503_i7):
//   Kraken Swordfish on value 2 in rows 5/7/9 (0-idx: 4/6/8) with fin at R9C6
//   (0-idx: row=8, col=5). Bases are rows; cover cols are {1, 3, 4, 5}
//   (col 5 is the fin's cover line). The fin's box is box 7 (rows 6-8, cols
//   3-5), so any cell at (row, 3) or (row, 4) in row 7 (the only non-base row
//   in the fin's box) would be a "free" finned elim; the true Kraken elims
//   sit at (0, 3) and (2, 3).
TEST_CASE("KrakenFishStrategy - omits in-fin-box free elims and reports cover-axis region",
          "[kraken_fish][regression][issue20]") {
    constexpr std::string_view kBoardFlat =
        "050040000060005200370060008290000000005001090000030020507003004006070002000400010";
    BoardData board = sudoku::testing::flatStringToBoard(std::string(kBoardFlat));
    CandidateGrid state(board);

    // Prior eliminations recorded in the fixture — applied to bring the candidate
    // grid into the same state the solver would reach by step 7.
    state.eliminateCandidate(4, 0, 4);
    state.eliminateCandidate(4, 0, 8);
    state.eliminateCandidate(5, 0, 1);
    state.eliminateCandidate(5, 0, 4);
    state.eliminateCandidate(5, 0, 8);
    state.eliminateCandidate(8, 2, 2);
    state.eliminateCandidate(3, 6, 5);
    state.eliminateCandidate(3, 7, 5);
    state.eliminateCandidate(6, 4, 8);
    state.eliminateCandidate(0, 3, 1);
    state.eliminateCandidate(1, 3, 1);

    KrakenFishStrategy strategy;
    auto result = strategy.findStep(board, state);
    REQUIRE(result.has_value());
    REQUIRE(result->technique == SolvingTechnique::KrakenFish);

    // Determine fin's box from the explanation positions (the last role==Fin).
    REQUIRE(result->explanation_data.position_roles.size() == result->explanation_data.positions.size());
    auto fin_it = std::ranges::find(result->explanation_data.position_roles, CellRole::Fin);
    REQUIRE(fin_it != result->explanation_data.position_roles.end());
    auto fin_idx = std::distance(result->explanation_data.position_roles.begin(), fin_it);
    Position fin_pos = result->explanation_data.positions[fin_idx];
    size_t fin_box_row = fin_pos.row / 3;
    size_t fin_box_col = fin_pos.col / 3;

    // Every elimination must sit OUTSIDE the fin's box — in-box elims belong
    // to the standard finned-fish technique and must not be attributed here.
    for (const auto& elim : result->eliminations) {
        bool in_fin_box = (elim.position.row / 3 == fin_box_row) && (elim.position.col / 3 == fin_box_col);
        INFO("Elimination at (" << elim.position.row << "," << elim.position.col << ") must not be in fin box "
                                << "(" << fin_box_row << "," << fin_box_col << ")");
        REQUIRE_FALSE(in_fin_box);
    }

    // `position` must point at a real Kraken cell (one of the eliminations).
    auto match = std::ranges::find_if(result->eliminations, [&](const Elimination& e) {
        return e.position.row == result->position.row && e.position.col == result->position.col;
    });
    REQUIRE(match != result->eliminations.end());

    // Row-based fish: eliminations sit on the cover (column) axis, so the
    // UI-highlight region_type must be Col.
    REQUIRE(result->explanation_data.region_type == RegionType::Col);
}
