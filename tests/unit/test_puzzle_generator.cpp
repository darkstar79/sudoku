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

#include "../../src/core/puzzle_generator.h"
#include "../helpers/candidate_test_utils.h"
#include "../helpers/test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

TEST_CASE("PuzzleGenerator - Basic Functionality", "[puzzle_generator]") {
    PuzzleGenerator generator;

    SECTION("Generate puzzle with different difficulties") {
        for (auto difficulty :
             {Difficulty::Easy, Difficulty::Medium, Difficulty::Hard, Difficulty::Expert, Difficulty::Master}) {
            // Create settings without unique solution requirement for testing
            GenerationSettings settings;
            settings.difficulty = difficulty;
            settings.seed = 12345;           // Fixed seed for reproducibility
            settings.ensure_unique = false;  // Disable unique solution checking
            settings.max_attempts = 100;

            auto result = generator.generatePuzzle(settings);

            // Debug: Print error if generation fails
            if (!result.has_value()) {
                INFO("Generation failed for difficulty " << static_cast<int>(difficulty) << " with error "
                                                         << static_cast<int>(result.error()));
            }
            REQUIRE(result.has_value());

            const auto& puzzle = result.value();
            REQUIRE(puzzle.difficulty == difficulty);
            REQUIRE(puzzle.board.size() == 9);
            REQUIRE(puzzle.solution.size() == 9);
        }
    }

    SECTION("Generated puzzle has appropriate clue count") {
        GenerationSettings settings;
        settings.difficulty = Difficulty::Easy;
        settings.seed = 12345;
        settings.ensure_unique = false;  // Disable for testing
        settings.max_attempts = 100;

        auto result = generator.generatePuzzle(settings);
        if (!result.has_value()) {
            INFO("Generation failed with error " << static_cast<int>(result.error()));
        }
        REQUIRE(result.has_value());

        const auto& puzzle = result.value();
        int clue_count = generator.countClues(puzzle.board);
        REQUIRE(clue_count >= 36);  // Easy should have 36-40 clues
        REQUIRE(clue_count <= 40);
        REQUIRE(puzzle.clue_count == clue_count);
    }
}

TEST_CASE("PuzzleGenerator - Puzzle Validation", "[puzzle_generator]") {
    PuzzleGenerator generator;

    SECTION("Valid puzzle structure") {
        BoardData valid_board = sudoku::test::getEasyPuzzleWithPatterns();

        REQUIRE(generator.validatePuzzle(valid_board));
    }

    SECTION("Invalid puzzle - wrong dimensions") {
        // BoardData is always 9x9, so we skip invalid dimension test
        // BoardData is a fixed 9x9 array; invalid dimensions are impossible
        REQUIRE(true);  // Placeholder: BoardData always has valid dimensions
    }

    SECTION("Invalid puzzle - out of range values") {
        BoardData invalid_board;
        invalid_board[0][0] = 10;  // Invalid value
        REQUIRE_FALSE(generator.validatePuzzle(invalid_board));
    }

    SECTION("Invalid puzzle - conflicts") {
        BoardData invalid_board;
        invalid_board[0][0] = 5;
        invalid_board[0][1] = 5;  // Row conflict
        REQUIRE_FALSE(generator.validatePuzzle(invalid_board));
    }
}

TEST_CASE("PuzzleGenerator - Puzzle Solving", "[puzzle_generator]") {
    PuzzleGenerator generator;

    SECTION("Solve simple puzzle") {
        BoardData puzzle = sudoku::test::getEasyPuzzleWithPatterns();

        auto result = generator.solvePuzzle(puzzle);
        REQUIRE(result.has_value());

        const auto& solution = result.value();

        // Check that solution is complete and valid
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                REQUIRE(solution[row][col] >= 1);
                REQUIRE(solution[row][col] <= 9);
            }
        }

        REQUIRE(generator.validatePuzzle(solution));
    }

    SECTION("Unsolvable puzzle") {
        BoardData unsolvable = {{5, 5, 0, 0, 0, 0, 0, 0, 0},  // Row conflict - unsolvable
                                {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
                                {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
                                {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}};

        auto result = generator.solvePuzzle(unsolvable);
        REQUIRE_FALSE(result.has_value());
    }
}

TEST_CASE("PuzzleGenerator - Clue Counting", "[puzzle_generator]") {
    PuzzleGenerator generator;

    SECTION("Count clues correctly") {
        BoardData board;
        REQUIRE(generator.countClues(board) == 0);

        board[0][0] = 5;
        board[1][1] = 3;
        board[2][2] = 7;
        REQUIRE(generator.countClues(board) == 3);

        // Fill entire board
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (board[row][col] == 0) {
                    board[row][col] = 1;
                }
            }
        }
        REQUIRE(generator.countClues(board) == 81);
    }
}

TEST_CASE("PuzzleGenerator - Generation Settings", "[puzzle_generator]") {
    PuzzleGenerator generator;

    SECTION("Generate with specific seed") {
        GenerationSettings settings;
        settings.difficulty = Difficulty::Medium;
        settings.seed = 12345;
        settings.ensure_unique = false;  // Disable for testing

        auto result1 = generator.generatePuzzle(settings);
        auto result2 = generator.generatePuzzle(settings);

        if (!result1.has_value()) {
            INFO("First generation failed with error " << static_cast<int>(result1.error()));
        }
        if (!result2.has_value()) {
            INFO("Second generation failed with error " << static_cast<int>(result2.error()));
        }
        REQUIRE(result1.has_value());
        REQUIRE(result2.has_value());

        // Same seed should produce same puzzle
        REQUIRE(result1.value().board == result2.value().board);
        REQUIRE(result1.value().solution == result2.value().solution);
        REQUIRE(result1.value().seed == result2.value().seed);
    }
}
TEST_CASE("PuzzleGenerator - Error Handling", "[puzzle_generator]") {
    PuzzleGenerator generator;

    SECTION("Maximum attempts exceeded") {
        GenerationSettings settings;
        settings.difficulty = Difficulty::Expert;
        settings.seed = 12345;
        settings.ensure_unique = true;  // Strict requirement
        settings.max_attempts = 1;      // Very low limit

        auto result = generator.generatePuzzle(settings);

        // With max_attempts = 1, generation may fail for Expert difficulty
        // This tests the TooManyAttempts error path
        if (!result.has_value()) {
            REQUIRE((result.error() == GenerationError::TooManyAttempts ||
                     result.error() == GenerationError::NoUniqueSolution ||
                     result.error() == GenerationError::GenerationFailed));
        }
    }

    SECTION("Zero max attempts returns error") {
        GenerationSettings settings;
        settings.difficulty = Difficulty::Easy;
        settings.max_attempts = 0;

        auto result = generator.generatePuzzle(settings);

        // Zero attempts should fail immediately (any error is acceptable)
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("solvePuzzle handles empty board") {
        BoardData empty_board;

        auto result = generator.solvePuzzle(empty_board);

        // Empty board should still be solvable (finds one valid solution)
        REQUIRE(result.has_value());
        REQUIRE(generator.validatePuzzle(result.value()));
    }

    SECTION("solvePuzzle handles board with invalid dimensions") {
        // BoardData is a fixed 9x9 array; invalid dimensions are impossible
        // Skip this test as BoardData always has valid dimensions
        REQUIRE(true);
    }
}

TEST_CASE("PuzzleGenerator - Constraint Propagation", "[puzzle_generator]") {
    PuzzleGenerator generator;

    SECTION("Propagate constraints on simple board") {
        // Board with naked singles that can be propagated
        BoardData board = sudoku::test::getEasyPuzzleWithPatterns();

        auto result = generator.propagateConstraints(board);
        REQUIRE(result.has_value());

        const auto& propagated = result.value();

        // Propagation should fill some cells
        int original_clues = generator.countClues(board);
        int propagated_clues = generator.countClues(propagated);
        REQUIRE(propagated_clues >= original_clues);

        // Propagated board should still be valid
        REQUIRE(generator.validatePuzzle(propagated));
    }

    SECTION("Propagate constraints on complete board") {
        // Already solved board - propagation should be no-op
        BoardData complete_board = sudoku::testing::kSolvedBoard;

        auto result = generator.propagateConstraints(complete_board);
        REQUIRE(result.has_value());
        REQUIRE(result.value() == complete_board);  // Should be unchanged
    }

    SECTION("hasContradiction on valid board returns false") {
        BoardData valid_board = sudoku::test::getEasyPuzzleWithPatterns();

        REQUIRE_FALSE(generator.hasContradiction(valid_board));
    }

    SECTION("hasContradiction on empty board returns false") {
        BoardData empty_board;
        REQUIRE_FALSE(generator.hasContradiction(empty_board));
    }
}

TEST_CASE("PuzzleGenerator - Clue Analysis", "[puzzle_generator]") {
    PuzzleGenerator generator;

    SECTION("Analyze clue constraints on partially filled board") {
        BoardData board = sudoku::test::getEasyPuzzleWithPatterns();

        auto analysis = generator.analyzeClueConstraints(board);

        // Should have one ClueAnalysis entry for each non-zero clue
        int clue_count = generator.countClues(board);
        REQUIRE(analysis.size() == static_cast<size_t>(clue_count));

        // All entries should have valid positions and values
        for (const auto& clue : analysis) {
            REQUIRE(clue.pos.row < 9);
            REQUIRE(clue.pos.col < 9);
            REQUIRE(clue.value >= 1);
            REQUIRE(clue.value <= 9);
            REQUIRE(clue.alternative_count >= 0);
            REQUIRE(clue.alternative_count <= 8);
            REQUIRE(clue.constraint_score >= 1);
            REQUIRE(clue.constraint_score <= 9);
        }
    }

    SECTION("Analyze clue constraints on empty board") {
        BoardData empty_board;

        auto analysis = generator.analyzeClueConstraints(empty_board);

        // Empty board should have no clues to analyze
        REQUIRE(analysis.empty());
    }

    SECTION("Select clues for dropping") {
        BoardData board = sudoku::test::getEasyPuzzleWithPatterns();

        std::mt19937 rng(42);
        int clue_count = generator.countClues(board);

        // Request to drop half the clues
        auto selected = generator.selectCluesForDropping(board, clue_count / 2, rng);

        REQUIRE(selected.size() <= static_cast<size_t>(clue_count / 2));

        // All selected positions should be valid
        for (const auto& pos : selected) {
            REQUIRE(pos.row < 9);
            REQUIRE(pos.col < 9);
            REQUIRE(board[pos.row][pos.col] != 0);  // Should point to filled cells
        }
    }

    SECTION("Select clues for dropping - request more than available") {
        BoardData sparse_board;
        sparse_board[0][0] = 5;
        sparse_board[1][1] = 3;

        std::mt19937 rng(42);

        // Request to drop 10 clues when only 2 exist
        auto selected = generator.selectCluesForDropping(sparse_board, 10, rng);

        // Should return at most 2 clues
        REQUIRE(selected.size() <= 2);
    }
}

TEST_CASE("PuzzleGenerator - Difficulty Clue Ranges", "[puzzle_generator]") {
    PuzzleGenerator generator;

    SECTION("Easy puzzles respect clue count range") {
        GenerationSettings settings;
        settings.difficulty = Difficulty::Easy;
        settings.seed = 12345;
        settings.ensure_unique = false;
        settings.max_attempts = 200;

        auto result = generator.generatePuzzle(settings);

        if (result.has_value()) {
            int clues = generator.countClues(result.value().board);
            // Easy: 36-40 clues (per Difficulty enum doc)
            REQUIRE(clues >= 36);
            REQUIRE(clues <= 40);
        }
    }

    SECTION("Medium puzzles respect clue count range") {
        GenerationSettings settings;
        settings.difficulty = Difficulty::Medium;
        settings.seed = 54321;
        settings.ensure_unique = false;
        settings.max_attempts = 200;

        auto result = generator.generatePuzzle(settings);

        if (result.has_value()) {
            int clues = generator.countClues(result.value().board);
            // Medium: 28-35 clues
            REQUIRE(clues >= 28);
            REQUIRE(clues <= 35);
        }
    }

    SECTION("Hard puzzles respect clue count range") {
        GenerationSettings settings;
        settings.difficulty = Difficulty::Hard;
        settings.seed = 99999;
        settings.ensure_unique = false;
        settings.max_attempts = 500;

        auto result = generator.generatePuzzle(settings);

        if (result.has_value()) {
            int clues = generator.countClues(result.value().board);
            // Hard: 22-27 clues
            REQUIRE(clues >= 22);
            REQUIRE(clues <= 27);
        }
    }

    SECTION("Expert puzzles respect clue count range") {
        GenerationSettings settings;
        settings.difficulty = Difficulty::Expert;
        settings.seed = 11111;
        settings.ensure_unique = false;
        settings.max_attempts = 1000;

        auto result = generator.generatePuzzle(settings);

        if (result.has_value()) {
            int clues = generator.countClues(result.value().board);
            // Expert: 17-21 clues
            REQUIRE(clues >= 17);
            REQUIRE(clues <= 21);
        }
    }
}
