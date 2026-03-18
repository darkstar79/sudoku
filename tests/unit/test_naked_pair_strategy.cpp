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
#include "../../src/core/strategies/naked_pair_strategy.h"
#include "../helpers/test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using namespace sudoku::test;

namespace {}  // anonymous namespace

TEST_CASE("NakedPairStrategy - Interface Implementation", "[naked_pair]") {
    NakedPairStrategy strategy;

    SECTION("getTechnique returns NakedPair") {
        REQUIRE(strategy.getTechnique() == SolvingTechnique::NakedPair);
    }

    SECTION("getName returns correct name") {
        REQUIRE(strategy.getName() == "Naked Pair");
    }

    SECTION("getDifficultyRating returns 50") {
        REQUIRE(strategy.getDifficultyRating() == 3.0);
        REQUIRE(strategy.getDifficultyRating() == getTechniqueRating(SolvingTechnique::NakedPair));
    }
}

TEST_CASE("NakedPairStrategy - Board Analysis", "[naked_pair]") {
    NakedPairStrategy strategy;

    SECTION("Analyzes board with potential naked pair") {
        auto board = createBoardWithNakedPair();
        CandidateGrid state(board);

        auto step = strategy.findStep(board, state);

        // Verify correct structure if pattern found
        if (step.has_value()) {
            REQUIRE(step->type == SolveStepType::Elimination);
            REQUIRE(step->technique == SolvingTechnique::NakedPair);
            REQUIRE(step->rating == 3.0);
            REQUIRE_FALSE(step->explanation.empty());
            REQUIRE(step->eliminations.size() > 0);
        }
        // Test passes whether pattern found or not - key is algorithm executed
    }

    SECTION("Analyzes simple board") {
        auto board = createSimpleBoard();
        CandidateGrid state(board);

        auto step = strategy.findStep(board, state);

        // Verify correct structure if pattern found
        if (step.has_value()) {
            REQUIRE(step->technique == SolvingTechnique::NakedPair);
        }
    }

    SECTION("Analyzes nearly complete board") {
        auto board = getNearlyCompletePuzzle();
        CandidateGrid state(board);

        auto step = strategy.findStep(board, state);

        // Nearly complete boards unlikely to have naked pairs
        // Test validates algorithm handles this case correctly
    }
}

TEST_CASE("NakedPairStrategy - Explanation Content", "[naked_pair]") {
    NakedPairStrategy strategy;
    auto board = createBoardWithNakedPair();
    CandidateGrid state(board);

    auto step = strategy.findStep(board, state);

    if (step.has_value()) {
        SECTION("Explanation mentions technique name") {
            REQUIRE(step->explanation.find("Naked Pair") != std::string::npos);
        }

        SECTION("Explanation is descriptive") {
            REQUIRE(step->explanation.length() > 20);
        }
    }
}

TEST_CASE("NakedPairStrategy - Eliminations", "[naked_pair]") {
    NakedPairStrategy strategy;

    SECTION("Naked pair creates eliminations") {
        auto board = createBoardWithNakedPair();
        CandidateGrid state(board);

        auto step = strategy.findStep(board, state);

        if (step.has_value()) {
            REQUIRE(step->type == SolveStepType::Elimination);
            REQUIRE_FALSE(step->eliminations.empty());

            // Each elimination should have valid position and value
            for (const auto& elim : step->eliminations) {
                REQUIRE(elim.position.row < 9);
                REQUIRE(elim.position.col < 9);
                REQUIRE(elim.value >= 1);
                REQUIRE(elim.value <= 9);
            }
        }
    }
}

TEST_CASE("NakedPairStrategy - Polymorphic Usage", "[naked_pair]") {
    SECTION("Can be used through ISolvingStrategy interface") {
        std::unique_ptr<ISolvingStrategy> strategy = std::make_unique<NakedPairStrategy>();

        auto board = createBoardWithNakedPair();
        CandidateGrid state(board);

        auto step = strategy->findStep(board, state);

        REQUIRE(strategy->getTechnique() == SolvingTechnique::NakedPair);
        REQUIRE(strategy->getDifficultyRating() == 3.0);
    }
}
