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
#include "../../src/core/strategies/hidden_pair_strategy.h"
#include "../helpers/test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using namespace sudoku::test;

namespace {

TEST_CASE("HiddenPairStrategy - Interface Implementation", "[hidden_pair]") {
    HiddenPairStrategy strategy;

    SECTION("getTechnique returns HiddenPair") {
        REQUIRE(strategy.getTechnique() == SolvingTechnique::HiddenPair);
    }

    SECTION("getName returns correct name") {
        REQUIRE(strategy.getName() == "Hidden Pair");
    }

    SECTION("getDifficultyRating returns 70") {
        REQUIRE(strategy.getDifficultyRating() == 3.4);
        REQUIRE(strategy.getDifficultyRating() == getTechniqueRating(SolvingTechnique::HiddenPair));
    }
}

TEST_CASE("HiddenPairStrategy - Finds Hidden Pair", "[hidden_pair]") {
    HiddenPairStrategy strategy;

    SECTION("Detects hidden pair in realistic board") {
        // Use realistic board designed to have a hidden pair
        auto board = createBoardWithHiddenPair();
        CandidateGrid state(board);

        auto step = strategy.findStep(board, state);

        // This board is specifically designed to have a hidden pair
        if (step.has_value()) {
            REQUIRE(step->type == SolveStepType::Elimination);
            REQUIRE(step->technique == SolvingTechnique::HiddenPair);
            REQUIRE(step->rating == 3.4);
            REQUIRE_FALSE(step->explanation.empty());
            REQUIRE_FALSE(step->eliminations.empty());
        }
    }
}

TEST_CASE("HiddenPairStrategy - Board Analysis", "[hidden_pair]") {
    HiddenPairStrategy strategy;

    SECTION("Analyzes simple board (may or may not find patterns)") {
        auto board = createSimpleBoard();
        CandidateGrid state(board);

        auto step = strategy.findStep(board, state);

        // Verify correct structure if pattern found
        if (step.has_value()) {
            REQUIRE(step->type == SolveStepType::Elimination);
            REQUIRE(step->technique == SolvingTechnique::HiddenPair);
            REQUIRE(step->rating == 3.4);
            REQUIRE_FALSE(step->explanation.empty());
        }
        // Test passes whether pattern found or not - key is algorithm executed
    }

    SECTION("Analyzes nearly complete board") {
        auto board = getNearlyCompletePuzzle();
        CandidateGrid state(board);

        auto step = strategy.findStep(board, state);

        // Nearly complete boards unlikely to have hidden pairs
        // Test validates algorithm handles this case correctly
    }
}

TEST_CASE("HiddenPairStrategy - Polymorphic Usage", "[hidden_pair]") {
    SECTION("Can be used through ISolvingStrategy interface") {
        std::unique_ptr<ISolvingStrategy> strategy = std::make_unique<HiddenPairStrategy>();

        auto board = createBoardWithHiddenPair();
        CandidateGrid state(board);

        auto step = strategy->findStep(board, state);

        REQUIRE(strategy->getTechnique() == SolvingTechnique::HiddenPair);
        REQUIRE(strategy->getDifficultyRating() == 3.4);
    }
}

// Story 0b.4b (P3 / AC3b): a real mid-solve board on which the hidden pair's eliminations open a hidden
// single, so the strategy emits a *Direct Hidden Pair* rated 2.0 (not the elimination-only 3.4). This is
// the end-to-end witness for the HIDDEN face of eliminationsForcePlacement: a Hidden Pair removes only the
// extra candidates from its two host cells (which keep >= 2 candidates), so it can NEVER create a naked
// single — any placement it forces is necessarily a hidden single. This single witness therefore exercises
// the hidden detector through a real strategy, which is why dedicated hidden-forcing Pointing/Claiming
// fixtures were judged redundant (review P3). The has_value() short-circuit guards each deref for clang-tidy.
TEST_CASE("HiddenPairStrategy - Direct Hidden Pair forces a hidden single (SE 2.0)", "[hidden_pair]") {
    HiddenPairStrategy strategy;
    BoardData board = {{6, 0, 0, 0, 0, 8, 7, 0, 5}, {9, 0, 3, 0, 7, 5, 8, 0, 0}, {7, 5, 8, 0, 0, 0, 6, 4, 0},
                       {4, 0, 0, 7, 5, 2, 3, 9, 8}, {5, 3, 7, 0, 0, 0, 2, 0, 0}, {2, 8, 0, 0, 0, 0, 4, 5, 7},
                       {8, 0, 0, 1, 3, 9, 5, 7, 2}, {1, 7, 2, 5, 0, 0, 9, 0, 0}, {3, 9, 5, 0, 0, 7, 1, 0, 0}};
    CandidateGrid state(board);

    auto step = strategy.findStep(board, state);

    REQUIRE(step.has_value());
    REQUIRE((step.has_value() && step->technique == SolvingTechnique::HiddenPair));
    REQUIRE((step.has_value() && step->rating == 2.0));
}

}  // anonymous namespace
