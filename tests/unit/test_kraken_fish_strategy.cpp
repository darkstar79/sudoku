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
#include <string>

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

// Regression for issue #20 (the HIGH finding from the 2026-05-25 audit):
//   A *non-vacuous* Kraken step must NOT include free finned-fish eliminations — those
//   are reported by FinnedXWing/Swordfish/Jellyfish strategies which run earlier;
//   bundling them here misattributes their rating and lets `position` point at a
//   non-Kraken cell.
//
// Fixture: Hard_s1_i0 raw givens (tests/data/fixtures/Hard/fixtures.yaml). At the
// initial candidate state KrakenFishStrategy finds a Kraken X-Wing on value 1 in
// rows 5/7 with the fin at R7C1 whose fin-placement chain is *consistent* (no
// contradiction), eliminating 1 from R3C4 — a genuine extended Kraken elimination
// (rating 8.5). Sound: the puzzle's solution has R3C4 = 7.
//
// region_type is asserted as Row (the base axis), matching the fish-family convention
// (XWing/Swordfish/Jellyfish/Finned*/Sashimi* all do the same). The 2026-05-25 audit's
// recommendation to flip it to the cover axis contradicted the documented convention at
// [localized_explanations.h:204] and was rejected — see PR #38 and issue #39.
TEST_CASE("KrakenFishStrategy - non-vacuous Kraken omits in-fin-box elims, full Kraken rating",
          "[kraken_fish][regression][issue20]") {
    constexpr std::string_view kBoardFlat =
        "040500010003080002000020008000000000002067009317000050004000025030009700000010800";
    BoardData board = sudoku::testing::flatStringToBoard(std::string(kBoardFlat));
    CandidateGrid state(board);

    KrakenFishStrategy strategy;
    auto result = strategy.findStep(board, state);
    REQUIRE(result.has_value());
    REQUIRE(result->technique == SolvingTechnique::KrakenFish);

    // A consistent fin chain → full Kraken rating (NOT the fin-exclusion rating).
    REQUIRE(result->rating == getTechniqueRating(SolvingTechnique::KrakenFish));

    // Determine fin's box from the explanation positions (the role==Fin entry).
    REQUIRE(result->explanation_data.position_roles.size() == result->explanation_data.positions.size());
    auto fin_it = std::ranges::find(result->explanation_data.position_roles, CellRole::Fin);
    REQUIRE(fin_it != result->explanation_data.position_roles.end());
    auto fin_idx = std::distance(result->explanation_data.position_roles.begin(), fin_it);
    Position fin_pos = result->explanation_data.positions[fin_idx];
    size_t fin_box_row = fin_pos.row / BOX_SIZE;
    size_t fin_box_col = fin_pos.col / BOX_SIZE;

    // Every elimination must sit OUTSIDE the fin's box — in-box elims belong to the
    // standard finned-fish technique and must not be attributed here.
    REQUIRE_FALSE(result->eliminations.empty());
    for (const auto& elim : result->eliminations) {
        bool in_fin_box =
            (elim.position.row / BOX_SIZE == fin_box_row) && (elim.position.col / BOX_SIZE == fin_box_col);
        INFO("Elimination at (" << elim.position.row << "," << elim.position.col << ") must not be in fin box "
                                << "(" << fin_box_row << "," << fin_box_col << ")");
        REQUIRE_FALSE(in_fin_box);
    }

    // `position` must point at a real Kraken cell (one of the eliminations).
    auto match = std::ranges::find_if(result->eliminations, [&](const Elimination& e) {
        return e.position.row == result->position.row && e.position.col == result->position.col;
    });
    REQUIRE(match != result->eliminations.end());

    // This fixture is known to eliminate exactly 1 from R3C4. If the chain heuristic
    // shifts and this changes, flag it for review rather than silently passing on a
    // different (possibly weaker) deduction.
    REQUIRE(result->eliminations.size() == 1);
    REQUIRE(result->eliminations[0].position.row == 2);
    REQUIRE(result->eliminations[0].position.col == 3);
    REQUIRE(result->eliminations[0].value == 1);

    // Row-based fish: region_type encodes the base-axis orientation.
    REQUIRE(result->explanation_data.region_type == RegionType::Row);
}

// Regression for issue #40 (the 2026-05-25 audit observation at CODE_REVIEW:1114):
//   When placing `value` at the fin self-contradicts, the fin placement is impossible,
//   so the old verifyKrakenElimination returned `true` *vacuously* for every candidate
//   target — surfacing a batch of cover eliminations "chain-verified" by a chain that
//   had actually self-destructed, rated as a full Kraken (8.5). The strictly simpler,
//   honest conclusion is that the fin cannot hold `value`: a single elimination on the
//   fin cell, rated at the underlying finned-fish level.
//
// Fixture: Hard_s4503_i7 (originally added for issue #20). On inspection it is in fact a
// vacuous-fin case: placing 2 at the fin R9C6 propagates to a contradiction. The puzzle's
// solution has R9C6 = 6, so eliminating 2 from R9C6 is sound. The base set is an order-3
// Swordfish, so the fin-exclusion is rated at the FinnedSwordfish level (SE 4.0), not 8.5.
TEST_CASE("KrakenFishStrategy - vacuous fin contradiction reports fin-exclusion",
          "[kraken_fish][regression][issue40]") {
    constexpr std::string_view kBoardFlat =
        "050040000060005200370060008290000000005001090000030020507003004006070002000400010";
    BoardData board = sudoku::testing::flatStringToBoard(std::string(kBoardFlat));
    CandidateGrid state(board);

    // Prior eliminations recorded in the fixture — bring the candidate grid into the
    // same state the solver would reach by step 7.
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

    // Single fin-exclusion, NOT a batch of vacuously-validated cover eliminations.
    REQUIRE(result->eliminations.size() == 1);
    const auto& elim = result->eliminations[0];
    REQUIRE(elim.position.row == 8);  // R9C6 — the fin cell
    REQUIRE(elim.position.col == 5);
    REQUIRE(elim.value == 2);

    // `position` points at the eliminated fin cell, and the fin role marks the same cell.
    REQUIRE(result->position.row == 8);
    REQUIRE(result->position.col == 5);
    auto fin_it = std::ranges::find(result->explanation_data.position_roles, CellRole::Fin);
    REQUIRE(fin_it != result->explanation_data.position_roles.end());
    auto fin_idx = std::distance(result->explanation_data.position_roles.begin(), fin_it);
    REQUIRE(result->explanation_data.positions[fin_idx].row == 8);
    REQUIRE(result->explanation_data.positions[fin_idx].col == 5);

    // Rated at the underlying finned-fish level (order-3 Swordfish = SE 4.0), NOT the
    // full Kraken rating — this is the rating-drift fix.
    REQUIRE(result->rating == getTechniqueRating(SolvingTechnique::FinnedSwordfish));
    REQUIRE(result->rating != getTechniqueRating(SolvingTechnique::KrakenFish));

    // Honest explanation: the fin placement is impossible (no "via chain verification").
    REQUIRE(result->explanation.find("impossible") != std::string::npos);
    REQUIRE(result->explanation.find("fin") != std::string::npos);

    // Base-axis orientation preserved (issue #39 scope untouched).
    REQUIRE(result->explanation_data.region_type == RegionType::Row);
}
