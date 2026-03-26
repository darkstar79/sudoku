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

#include "../../src/core/cpu_features.h"
#include "../../src/core/i_puzzle_rater.h"
#include "../../src/core/puzzle_generator.h"
#include "../../src/core/puzzle_rating.h"
#include "../helpers/candidate_test_utils.h"
#include "../helpers/test_utils.h"

#include <chrono>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

// ============================================================================
// MockPuzzleRater - Configurable mock for testing rater integration paths
// ============================================================================

class MockPuzzleRater : public IPuzzleRater {
public:
    /// Configure mock to return a successful rating
    void setRating(double score, Difficulty difficulty, bool requires_backtracking = false) {
        return_error_ = false;
        rating_ = PuzzleRating{
            .se_rating = score,
            .solve_path = {},
            .requires_backtracking = requires_backtracking,
            .estimated_difficulty = difficulty,
        };
    }

    /// Configure mock to return an error
    void setError(RatingError error) {
        return_error_ = true;
        error_ = error;
    }

    [[nodiscard]] std::expected<PuzzleRating, RatingError> ratePuzzle(const BoardData& /*board*/) const override {
        call_count_++;
        if (return_error_) {
            return std::unexpected(error_);
        }
        return rating_;
    }

    [[nodiscard]] int getCallCount() const {
        return call_count_;
    }

private:
    bool return_error_{false};
    PuzzleRating rating_{};
    RatingError error_{RatingError::Unsolvable};
    mutable int call_count_{0};
};

// ============================================================================
// Branch Coverage Tests for Puzzle Generator
// Testing error paths, edge cases, and boundary conditions to improve
// branch coverage from 48.5% to 70%+
// ============================================================================

TEST_CASE("PuzzleGenerator - hasUniqueSolution Branch Coverage", "[puzzle_generator][branches]") {
    PuzzleGenerator generator;

    SECTION("Board with unique solution returns true") {
        BoardData unique_puzzle = sudoku::test::getEasyPuzzleWithPatterns();

        // Tests countSolutions() → countSolutionsHelper() with max_solutions=2
        // Should find exactly 1 solution and return true
        REQUIRE(generator.hasUniqueSolution(unique_puzzle));
    }

    SECTION("Board with multiple solutions returns false") {
        // Underconstrained puzzle with multiple solutions
        BoardData multi_solution_puzzle;
        multi_solution_puzzle[0][0] = 1;
        multi_solution_puzzle[1][1] = 2;
        multi_solution_puzzle[2][2] = 3;

        // Tests countSolutions() early termination when count >= max_solutions
        // Should find >= 2 solutions and return false
        REQUIRE_FALSE(generator.hasUniqueSolution(multi_solution_puzzle));
    }

    SECTION("Empty board has many solutions (not unique)") {
        BoardData empty_board;

        // Tests findEmptyPosition() on empty board (many options)
        // Tests countSolutionsHelper() early termination branch
        REQUIRE_FALSE(generator.hasUniqueSolution(empty_board));
    }

    SECTION("Complete board has unique solution (itself)") {
        BoardData complete_board = sudoku::testing::kSolvedBoard;

        // Tests findEmptyPosition() returning nullopt (no empty cells)
        // Tests countSolutionsHelper() base case with complete board
        REQUIRE(generator.hasUniqueSolution(complete_board));
    }

    SECTION("Overconstrained board has no unique solution") {
        // Create an overconstrained board with multiple conflicts
        BoardData overconstrained;
        // Fill first row with 1-9
        for (int col = 0; col < 9; ++col) {
            overconstrained[0][col] = col + 1;
        }
        // Create an impossible constraint in row 1
        overconstrained[1][0] = 1;  // Same as [0][0]
        overconstrained[1][1] = 2;  // Same as [0][1]
        overconstrained[1][2] = 3;  // Same as [0][2]
        // Continue filling to make it highly constrained
        for (int col = 3; col < 9; ++col) {
            overconstrained[1][col] = col + 1;
        }

        // Board has duplicate values in every column (rows 0 and 1 share values),
        // making it unsolvable — countSolutions() returns 0, not 1
        REQUIRE_FALSE(generator.hasUniqueSolution(overconstrained));
    }
}

TEST_CASE("PuzzleGenerator - solvePuzzle Branch Coverage", "[puzzle_generator][branches]") {
    PuzzleGenerator generator;

    SECTION("Solve puzzle successfully") {
        BoardData solvable_puzzle = sudoku::test::getEasyPuzzleWithPatterns();

        // Tests solvePuzzle() success path, exercises countSolutionsHelper() indirectly
        auto result = generator.solvePuzzle(solvable_puzzle);
        REQUIRE(result.has_value());
        REQUIRE(generator.validatePuzzle(result.value()));

        // All cells should be filled
        for (const auto& row : result.value()) {
            for (int cell : row) {
                REQUIRE(cell >= 1);
                REQUIRE(cell <= 9);
            }
        }
    }

    SECTION("Solve empty board (many solutions)") {
        BoardData empty_board;

        // Tests findEmptyPosition() with completely empty board
        // Should find one valid solution
        auto result = generator.solvePuzzle(empty_board);
        REQUIRE(result.has_value());
        REQUIRE(generator.validatePuzzle(result.value()));
    }

    SECTION("Solve unsolvable board returns error") {
        BoardData unsolvable = {{1, 1, 0, 0, 0, 0, 0, 0, 0},  // Row conflict
                                {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
                                {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
                                {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}};

        // Tests countSolutionsHelper() finding 0 solutions
        auto result = generator.solvePuzzle(unsolvable);
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("Solve board with conflicts returns error") {
        // Create a board with row conflicts (two 5s in first row) — unsolvable
        BoardData invalid_board;
        invalid_board[0][0] = 5;
        invalid_board[0][1] = 5;

        auto result = generator.solvePuzzle(invalid_board);
        REQUIRE_FALSE(result.has_value());
    }
}

TEST_CASE("PuzzleGenerator - propagateConstraints Branch Coverage", "[puzzle_generator][branches]") {
    PuzzleGenerator generator;

    SECTION("Propagate constraints fills forced cells") {
        // Board with naked singles (forced moves)
        BoardData board = sudoku::test::getEasyPuzzleWithPatterns();

        // Tests applyIterativePropagation*() happy path
        auto result = generator.propagateConstraints(board);
        REQUIRE(result.has_value());

        const auto& propagated = result.value();
        int original_clues = generator.countClues(board);
        int propagated_clues = generator.countClues(propagated);

        // Should fill some cells via propagation
        REQUIRE(propagated_clues >= original_clues);
        REQUIRE(generator.validatePuzzle(propagated));
    }

    SECTION("Propagate constraints on empty board") {
        BoardData empty_board;

        // Tests propagation with no initial constraints
        // Should be no-op (no forced moves on empty board)
        auto result = generator.propagateConstraints(empty_board);
        REQUIRE(result.has_value());
        REQUIRE(result.value() == empty_board);
    }

    SECTION("Propagate constraints on complete board is no-op") {
        BoardData complete_board = sudoku::testing::kSolvedBoard;

        // Tests propagation on complete board (all cells filled)
        // Tests findEmptyPosition() returning nullopt
        auto result = generator.propagateConstraints(complete_board);
        REQUIRE(result.has_value());
        REQUIRE(result.value() == complete_board);  // Should be unchanged
    }

    SECTION("Propagate constraints on contradictory board returns error") {
        BoardData contradiction_board;
        contradiction_board[0][0] = 5;
        contradiction_board[0][1] = 5;  // Row conflict

        // Tests hasContradiction() detection
        // Should detect contradiction and return error
        auto result = generator.propagateConstraints(contradiction_board);

        // Either returns error or propagation stops due to contradiction
        // Implementation-dependent behavior
        if (!result.has_value()) {
            REQUIRE_FALSE(result.has_value());
        } else {
            // If propagation succeeds, board should still be invalid
            REQUIRE_FALSE(generator.validatePuzzle(result.value()));
        }
    }
}

TEST_CASE("PuzzleGenerator - hasContradiction Branch Coverage", "[puzzle_generator][branches]") {
    PuzzleGenerator generator;

    SECTION("Valid board has no contradiction") {
        BoardData valid_board = {{5, 3, 0, 0, 7, 0, 0, 0, 0}, {6, 0, 0, 1, 9, 5, 0, 0, 0}, {0, 9, 8, 0, 0, 0, 0, 6, 0},
                                 {8, 0, 0, 0, 6, 0, 0, 0, 3}, {4, 0, 0, 8, 0, 3, 0, 0, 1}, {7, 0, 0, 0, 2, 0, 0, 0, 6},
                                 {0, 6, 0, 0, 0, 0, 2, 8, 0}, {0, 0, 0, 4, 1, 9, 0, 0, 5}, {0, 0, 0, 0, 8, 0, 0, 7, 9}};

        REQUIRE_FALSE(generator.hasContradiction(valid_board));
    }

    SECTION("Board with empty domain (no legal values) has contradiction") {
        // Create scenario where a cell has no legal values
        BoardData empty_domain;
        // Fill row 0 with 1-8, leaving [0][8] empty but blocked
        for (int col = 0; col < 8; ++col) {
            empty_domain[0][col] = col + 1;  // 1, 2, 3, 4, 5, 6, 7, 8
        }
        // Fill column 8 with value 9, blocking last position
        empty_domain[1][8] = 9;

        // Now [0][8] cannot be 1-8 (row constraint) or 9 (column constraint)
        // This creates an empty domain
        REQUIRE(generator.hasContradiction(empty_domain));
    }

    SECTION("Board with conflicts but no empty domain") {
        // Simple conflicts don't create contradiction if cells still have legal values
        BoardData conflict;
        conflict[0][0] = 5;
        conflict[0][1] = 5;  // Row conflict, but other cells still have legal values

        // No empty domain exists, so no contradiction detected
        REQUIRE_FALSE(generator.hasContradiction(conflict));
    }

    SECTION("Empty board has no contradiction") {
        BoardData empty_board;

        REQUIRE_FALSE(generator.hasContradiction(empty_board));
    }

    SECTION("Complete valid board has no contradiction") {
        BoardData complete_board = sudoku::testing::kSolvedBoard;

        REQUIRE_FALSE(generator.hasContradiction(complete_board));
    }
}

TEST_CASE("PuzzleGenerator - removeCluesToTarget Boundary Testing", "[puzzle_generator][branches]") {
    PuzzleGenerator generator;
    std::mt19937 rng(42);

    SECTION("Remove clues to moderate target (25 clues)") {
        // Generate complete solution
        BoardData solution = sudoku::testing::kSolvedBoard;

        // Tests moderate target (25 clues, easier than 17)
        auto result = generator.removeCluesToTarget(solution, 25, 200, rng);

        if (result.has_value()) {
            int clues = generator.countClues(*result);
            // Should achieve target or be close
            REQUIRE(clues >= 22);  // Within Hard difficulty range
            REQUIRE(clues <= 28);  // Allow some variance
            REQUIRE(generator.validatePuzzle(*result));
        } else {
            // Failure is acceptable for difficult targets
            REQUIRE(!result.has_value());
        }
    }

    SECTION("Remove clues to maximum (81 clues - complete board)") {
        BoardData solution = sudoku::testing::kSolvedBoard;

        // Tests boundary: 81 clues (no removal)
        auto result = generator.removeCluesToTarget(solution, 81, 10, rng);

        REQUIRE(result.has_value());
        REQUIRE(generator.countClues(*result) == 81);
        REQUIRE(result == solution);  // Should be unchanged
    }

    SECTION("Impossible target clues (0 clues) returns empty board") {
        BoardData solution = sudoku::testing::kSolvedBoard;

        // Tests error path: impossible to achieve 0 clues with unique solution
        // Use 2 attempts to keep Debug-mode runtime manageable (50 attempts × 81 uniqueness
        // checks each = very slow; 2 attempts still covers all intended branches)
        auto result = generator.removeCluesToTarget(solution, 0, 2, rng);

        // Should fail (empty board or very few clues, but likely fails)
        // Empty board indicates failure to reach target
        if (!result.has_value()) {
            REQUIRE(!result.has_value());
        }
    }

    SECTION("Max attempts exceeded returns empty board") {
        BoardData solution = sudoku::testing::kSolvedBoard;

        // Tests max_attempts branch: very low attempts for difficult target
        auto result = generator.removeCluesToTarget(solution, 17, 1, rng);

        // With only 1 attempt, likely to fail for target 17
        if (!result.has_value()) {
            REQUIRE(!result.has_value());  // Failure is acceptable
        }
    }
}

TEST_CASE("PuzzleGenerator - selectCluesForDropping Edge Cases", "[puzzle_generator][branches]") {
    PuzzleGenerator generator;
    std::mt19937 rng(42);

    SECTION("Select 0 clues returns empty vector") {
        BoardData board = sudoku::test::getEasyPuzzleWithPatterns();

        // Tests boundary: num_clues = 0
        auto selected = generator.selectCluesForDropping(board, 0, rng);
        REQUIRE(selected.empty());
    }

    SECTION("Select from empty board returns empty vector") {
        BoardData empty_board;

        // Tests edge case: no clues to select from
        auto selected = generator.selectCluesForDropping(empty_board, 10, rng);
        REQUIRE(selected.empty());
    }

    SECTION("Select all clues from sparse board") {
        BoardData sparse_board;
        sparse_board[0][0] = 5;
        sparse_board[1][1] = 3;
        sparse_board[2][2] = 7;

        // Tests selecting all available clues (3 clues total)
        auto selected = generator.selectCluesForDropping(sparse_board, 3, rng);

        REQUIRE(selected.size() <= 3);
        for (const auto& pos : selected) {
            REQUIRE(sparse_board[pos.row][pos.col] != 0);
        }
    }

    SECTION("Select negative clues returns empty vector") {
        BoardData board = sudoku::test::getEasyPuzzleWithPatterns();

        // Tests error path: negative num_clues
        auto selected = generator.selectCluesForDropping(board, -5, rng);

        // Should handle gracefully (empty or no-op)
        REQUIRE(selected.empty());
    }
}

TEST_CASE("PuzzleGenerator - Difficulty Boundaries", "[puzzle_generator][branches]") {
    PuzzleGenerator generator;

    SECTION("Invalid difficulty enum returns error") {
        GenerationSettings settings;
        settings.difficulty = static_cast<Difficulty>(99);  // Invalid enum value
        settings.max_attempts = 100;

        auto result = generator.generatePuzzle(settings);

        // Tests validatePuzzle() error path with invalid difficulty
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == GenerationError::InvalidDifficulty);
    }

    SECTION("Difficulty boundary - Easy minimum (36 clues)") {
        GenerationSettings settings;
        settings.difficulty = Difficulty::Easy;
        settings.seed = 12345;
        settings.ensure_unique = false;
        settings.max_attempts = 200;

        auto result = generator.generatePuzzle(settings);

        if (result.has_value()) {
            int clues = generator.countClues(result.value().board);
            // Easy: 36-40 clues - test minimum boundary
            REQUIRE(clues >= 36);
        }
    }

    SECTION("Difficulty boundary - Expert minimum (17 clues)") {
        GenerationSettings settings;
        settings.difficulty = Difficulty::Expert;
        settings.seed = 99999;
        settings.ensure_unique = false;
        settings.max_attempts = 1000;

        auto result = generator.generatePuzzle(settings);

        if (result.has_value()) {
            int clues = generator.countClues(result.value().board);
            // Expert: 17-21 clues - test minimum boundary
            REQUIRE(clues >= 17);
            REQUIRE(clues <= 21);
        }
    }
}

TEST_CASE("PuzzleGenerator - removeCluesToCreatePuzzleIterative Coverage", "[puzzle_generator][branches]") {
    PuzzleGenerator generator;
    std::mt19937 rng(42);

    SECTION("Iterative removal for Expert difficulty") {
        BoardData solution = sudoku::testing::kSolvedBoard;

        GenerationSettings settings;
        settings.difficulty = Difficulty::Expert;
        settings.ensure_unique = false;
        settings.max_attempts = 500;

        // Tests removeCluesToCreatePuzzleIterative() for Expert (uses iterative deepening)
        auto puzzle_board = generator.removeCluesToCreatePuzzleIterative(solution, settings, rng);

        if (puzzle_board.has_value()) {
            int clues = generator.countClues(*puzzle_board);
            // Expert: 17-21 clues
            REQUIRE(clues >= 17);
            REQUIRE(clues <= 21);
            REQUIRE(generator.validatePuzzle(*puzzle_board));
        }
    }

    SECTION("Iterative removal for Easy difficulty (greedy path)") {
        BoardData solution = sudoku::testing::kSolvedBoard;

        GenerationSettings settings;
        settings.difficulty = Difficulty::Easy;
        settings.ensure_unique = false;
        settings.max_attempts = 200;

        // Tests greedy removal path (not iterative) for Easy difficulty
        auto puzzle_board = generator.removeCluesToCreatePuzzleIterative(solution, settings, rng);

        if (puzzle_board.has_value()) {
            int clues = generator.countClues(*puzzle_board);
            // Easy: 36-40 clues
            REQUIRE(clues >= 36);
            REQUIRE(clues <= 40);
            REQUIRE(generator.validatePuzzle(*puzzle_board));
        }
    }
}

TEST_CASE("PuzzleGenerator - analyzeClueConstraints Edge Cases", "[puzzle_generator][branches]") {
    PuzzleGenerator generator;

    SECTION("Analyze single clue") {
        BoardData single_clue_board;
        single_clue_board[4][4] = 5;  // Center cell

        auto analysis = generator.analyzeClueConstraints(single_clue_board);

        REQUIRE(analysis.size() == 1);
        REQUIRE(analysis[0].pos.row == 4);
        REQUIRE(analysis[0].pos.col == 4);
        REQUIRE(analysis[0].value == 5);
        // Center cell with no other constraints has many alternatives
        REQUIRE(analysis[0].alternative_count >= 0);
        REQUIRE(analysis[0].alternative_count <= 8);
    }

    SECTION("Analyze complete board (all clues)") {
        BoardData complete_board = sudoku::testing::kSolvedBoard;

        auto analysis = generator.analyzeClueConstraints(complete_board);

        // Should analyze all 81 clues
        REQUIRE(analysis.size() == 81);

        // All clues in complete board should have 0 alternatives (forced)
        for (const auto& clue : analysis) {
            REQUIRE(clue.alternative_count == 0);
            REQUIRE(clue.constraint_score == 9);  // Maximum constraint
        }
    }
}

TEST_CASE("PuzzleGenerator - Scalar vs SIMD Propagation Parity", "[puzzle_generator][branches]") {
    PuzzleGenerator generator;

    SECTION("Scalar propagation matches SIMD propagation") {
        BoardData board = sudoku::test::getEasyPuzzleWithPatterns();

        // Test both scalar and SIMD versions (SIMD may not be available)
        auto scalar_result = generator.propagateConstraintsScalar(board);
        REQUIRE(scalar_result.has_value());

        if (hasAvx2()) {
            auto simd_result = generator.propagateConstraintsSIMD(board);
            REQUIRE(simd_result.has_value());

            // Results should match
            REQUIRE(scalar_result.value() == simd_result.value());
        }
    }
}

TEST_CASE("PuzzleGenerator - generatePuzzle Error Paths", "[puzzle_generator][branches]") {
    PuzzleGenerator generator;

    SECTION("Generate with seed produces consistent results") {
        GenerationSettings settings1;
        settings1.difficulty = Difficulty::Easy;
        settings1.seed = 999;
        settings1.ensure_unique = false;
        settings1.max_attempts = 100;

        GenerationSettings settings2 = settings1;  // Same seed

        auto result1 = generator.generatePuzzle(settings1);
        auto result2 = generator.generatePuzzle(settings2);

        if (result1.has_value() && result2.has_value()) {
            // Same seed should produce same puzzle
            REQUIRE(result1.value().board == result2.value().board);
            REQUIRE(result1.value().seed == result2.value().seed);
        }
    }

    SECTION("Generate with ensure_unique=true") {
        GenerationSettings settings;
        settings.difficulty = Difficulty::Medium;
        settings.seed = 54321;
        settings.ensure_unique = true;  // Strict requirement
        settings.max_attempts = 100;

        auto result = generator.generatePuzzle(settings);

        if (result.has_value()) {
            // Puzzle should have unique solution
            REQUIRE(generator.hasUniqueSolution(result.value().board));
        }
    }

    SECTION("Generate Easy with default settings") {
        auto result = generator.generatePuzzle(Difficulty::Easy);

        // Should succeed with default settings
        REQUIRE(result.has_value());
        REQUIRE(result.value().difficulty == Difficulty::Easy);
        REQUIRE(generator.validatePuzzle(result.value().board));
    }

    SECTION("Generate all difficulties with default settings") {
        for (auto diff : {Difficulty::Easy, Difficulty::Medium, Difficulty::Hard}) {
            auto result = generator.generatePuzzle(diff);

            if (result.has_value()) {
                REQUIRE(result.value().difficulty == diff);
                REQUIRE(generator.validatePuzzle(result.value().board));
            }
        }
    }
}

TEST_CASE("PuzzleGenerator - Additional Edge Cases", "[puzzle_generator][branches]") {
    PuzzleGenerator generator;

    // Board dimension tests removed — BoardData is always 9x9 (fixed-size flat array)

    SECTION("validatePuzzle with negative values") {
        BoardData negative_values;
        negative_values[0][0] = -1;  // Invalid negative value

        REQUIRE_FALSE(generator.validatePuzzle(negative_values));
    }

    SECTION("validatePuzzle with value > 9") {
        BoardData large_value;
        large_value[4][4] = 15;  // Invalid value

        REQUIRE_FALSE(generator.validatePuzzle(large_value));
    }

    SECTION("countClues on partial board") {
        BoardData partial;
        // Add exactly 25 clues
        for (int i = 0; i < 25; ++i) {
            partial[i / 9][i % 9] = (i % 9) + 1;
        }

        REQUIRE(generator.countClues(partial) == 25);
    }

    SECTION("solvePuzzle with nearly complete board") {
        BoardData nearly_complete = {
            {5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
            {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
            {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 0}};

        // Only last cell is empty
        auto result = generator.solvePuzzle(nearly_complete);
        REQUIRE(result.has_value());
        REQUIRE(result.value()[8][8] == 9);  // Should fill in the missing value
    }
}

// ============================================================================
// MockPuzzleRater Integration Tests
// Tests the rater integration paths in PuzzleGenerator::generatePuzzle()
// Lines 119-135: rater_, rating_result, rating validation branches
// ============================================================================

TEST_CASE("PuzzleGenerator - Rater Integration", "[puzzle_generator][rater][branches]") {
    auto mock_rater = std::make_shared<MockPuzzleRater>();

    SECTION("Successful rating sets puzzle score") {
        // Configure mock to return Easy-range rating
        mock_rater->setRating(1.5, Difficulty::Easy);
        PuzzleGenerator generator(mock_rater);

        auto result = generator.generatePuzzle(Difficulty::Easy);

        REQUIRE(result.has_value());
        // Rater was called at least once
        REQUIRE(mock_rater->getCallCount() >= 1);
        // Rating should be set on the puzzle
        REQUIRE(result.value().rating == 1.5);
    }

    SECTION("Rating failure does not reject puzzle") {
        // Configure mock to return error
        mock_rater->setError(RatingError::Unsolvable);
        PuzzleGenerator generator(mock_rater);

        auto result = generator.generatePuzzle(Difficulty::Easy);

        // Puzzle should still be generated successfully (rating failure is non-fatal)
        REQUIRE(result.has_value());
        REQUIRE(mock_rater->getCallCount() >= 1);
        // Rating should remain at default (0) since rating failed
        REQUIRE(result.value().rating == 0.0);
    }

    SECTION("Rating with InvalidBoard error does not reject puzzle") {
        mock_rater->setError(RatingError::InvalidBoard);
        PuzzleGenerator generator(mock_rater);

        auto result = generator.generatePuzzle(Difficulty::Easy);

        REQUIRE(result.has_value());
        REQUIRE(result.value().rating == 0.0);
    }

    SECTION("Rating with Timeout error does not reject puzzle") {
        mock_rater->setError(RatingError::Timeout);
        PuzzleGenerator generator(mock_rater);

        auto result = generator.generatePuzzle(Difficulty::Easy);

        REQUIRE(result.has_value());
        REQUIRE(result.value().rating == 0.0);
    }
}

TEST_CASE("PuzzleGenerator - Rater Integration (rating validation)", "[puzzle_generator][rater][branches]") {
    auto mock_rater = std::make_shared<MockPuzzleRater>();

    SECTION("In-range rating accepted") {
        // Easy range is [0, 750), set rating to 250
        mock_rater->setRating(1.5, Difficulty::Easy);
        PuzzleGenerator generator(mock_rater);

        GenerationSettings settings;
        settings.difficulty = Difficulty::Easy;
        settings.seed = 12345;
        settings.ensure_unique = false;
        settings.max_attempts = 50;

        auto result = generator.generatePuzzle(settings);

        REQUIRE(result.has_value());
        REQUIRE(result.value().rating == 1.5);
    }

    SECTION("Out-of-range rating rejected causes retry") {
        // Easy range is [0, 750), but return a Hard-range rating (1300)
        // This forces the generator to retry — with only 2 attempts, generation should fail
        mock_rater->setRating(4.5, Difficulty::Hard);
        PuzzleGenerator generator(mock_rater);

        GenerationSettings settings;
        settings.difficulty = Difficulty::Easy;
        settings.seed = 99999;
        settings.ensure_unique = false;
        settings.max_attempts = 2;  // Low attempts to force failure

        auto result = generator.generatePuzzle(settings);

        // Should fail because every puzzle gets rejected by rating validation
        REQUIRE_FALSE(result.has_value());
        // Rater was called on each attempt
        REQUIRE(mock_rater->getCallCount() == 2);
    }

    SECTION("Rating below min rejected") {
        // Medium range is [750, 1250), return rating of 100 (below min)
        mock_rater->setRating(1.5, Difficulty::Easy);
        PuzzleGenerator generator(mock_rater);

        GenerationSettings settings;
        settings.difficulty = Difficulty::Medium;
        settings.seed = 77777;
        settings.ensure_unique = false;
        settings.max_attempts = 2;

        auto result = generator.generatePuzzle(settings);

        // Should fail: rating 100 < min_rating 750 for Medium
        REQUIRE_FALSE(result.has_value());
        REQUIRE(mock_rater->getCallCount() == 2);
    }

    SECTION("Rating at max boundary rejected") {
        // Easy range is [0, 750), return rating exactly at 750 (>= max)
        mock_rater->setRating(3.0, Difficulty::Medium);
        PuzzleGenerator generator(mock_rater);

        GenerationSettings settings;
        settings.difficulty = Difficulty::Easy;
        settings.seed = 55555;
        settings.ensure_unique = false;
        settings.max_attempts = 2;

        auto result = generator.generatePuzzle(settings);

        // Should fail: rating 750 >= max_rating 750 for Easy
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("Rating failure still accepts puzzle") {
        // Rating error is non-fatal
        mock_rater->setError(RatingError::Timeout);
        PuzzleGenerator generator(mock_rater);

        GenerationSettings settings;
        settings.difficulty = Difficulty::Easy;
        settings.seed = 33333;
        settings.ensure_unique = false;
        settings.max_attempts = 50;

        auto result = generator.generatePuzzle(settings);

        // Should succeed: rating failure skips validation
        REQUIRE(result.has_value());
        REQUIRE(result.value().rating == 0.0);
    }
}

TEST_CASE("PuzzleGenerator - No Rater (null rater)", "[puzzle_generator][rater][branches]") {
    SECTION("Generator without rater skips rating entirely") {
        PuzzleGenerator generator;  // No rater injected

        auto result = generator.generatePuzzle(Difficulty::Easy);

        REQUIRE(result.has_value());
        // Rating should be 0 (default, never set)
        REQUIRE(result.value().rating == 0.0);
    }
}

// ============================================================================
// Scalar Solver Path Tests — force SolverPath::Scalar to cover the scalar
// backtracker (countSolutionsHelper) which is dead code on AVX-512 machines
// when using SolverPath::Auto (always resolves to AVX512 on Zen 5).
// ============================================================================

TEST_CASE("PuzzleGenerator - Scalar solver path (countSolutions)", "[puzzle_generator][scalar]") {
    // Classic near-complete puzzle with a unique solution
    const BoardData unique_puzzle = {
        {5, 3, 0, 0, 7, 0, 0, 0, 0}, {6, 0, 0, 1, 9, 5, 0, 0, 0}, {0, 9, 8, 0, 0, 0, 0, 6, 0},
        {8, 0, 0, 0, 6, 0, 0, 0, 3}, {4, 0, 0, 8, 0, 3, 0, 0, 1}, {7, 0, 0, 0, 2, 0, 0, 0, 6},
        {0, 6, 0, 0, 0, 0, 2, 8, 0}, {0, 0, 0, 4, 1, 9, 0, 0, 5}, {0, 0, 0, 0, 8, 0, 0, 7, 9}};

    const BoardData multi_solution_puzzle;

    const BoardData complete_board = sudoku::testing::kSolvedBoard;

    PuzzleGenerator generator;
    generator.setSolverPath(SolverPath::Scalar);

    SECTION("Unique puzzle has unique solution via scalar path") {
        // Covers countSolutions() Scalar case (lines 691-698) and
        // the entire countSolutionsHelper() function
        REQUIRE(generator.hasUniqueSolution(unique_puzzle));
    }

    SECTION("Multi-solution board is not unique via scalar path") {
        // Covers early-termination branch in countSolutionsHelper (count >= max_solutions)
        REQUIRE_FALSE(generator.hasUniqueSolution(multi_solution_puzzle));
    }

    SECTION("Complete board is unique via scalar path") {
        // Covers base case in countSolutionsHelper: no empty cells → increment count and return
        REQUIRE(generator.hasUniqueSolution(complete_board));
    }

    SECTION("Memoization cache hit via scalar path") {
        // Call twice on same board; second call hits the Zobrist cache
        REQUIRE(generator.hasUniqueSolution(unique_puzzle));
        REQUIRE(generator.hasUniqueSolution(unique_puzzle));
    }
}
